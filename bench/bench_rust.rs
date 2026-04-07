// bench_rust.rs -- Benchmark: Rust slice::sort_unstable (pdqsort)
// Protocol: argv[1]=size argv[2]=pattern argv[3]=runs
// Output: JSON line with ns_per_element
use std::env;
use std::time::Instant;

fn fill_random(arr: &mut [i64], seed: u64) {
    let mut s = seed;
    for v in arr.iter_mut() {
        s = s.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        *v = (s >> 16) as i64;
    }
}

fn fill_pattern(arr: &mut [i64], pattern: &str, seed: u64) {
    let n = arr.len();
    match pattern {
        "random" => fill_random(arr, seed),
        "sorted" => { for i in 0..n { arr[i] = i as i64; } }
        "reversed" => { for i in 0..n { arr[i] = (n - i) as i64; } }
        "nearly" => {
            for i in 0..n { arr[i] = i as i64; }
            let swaps = std::cmp::max(n / 100, 2);
            let mut s = seed;
            for _ in 0..swaps {
                s = s.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
                let i = (s >> 33) as usize % n;
                s = s.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
                let j = (s >> 33) as usize % n;
                arr.swap(i, j);
            }
        }
        "few_unique" => {
            let mut s = seed;
            for v in arr.iter_mut() {
                s = s.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
                *v = ((s >> 33) % 5) as i64;
            }
        }
        "pipe_organ" => {
            let half = n / 2;
            for i in 0..half { arr[i] = i as i64; }
            for i in half..n { arr[i] = (n - i) as i64; }
        }
        "phased" => {
            let boundary = n * 3 / 4;
            for i in 0..boundary { arr[i] = i as i64; }
            fill_random(&mut arr[boundary..], seed);
        }
        "equal" => { for v in arr.iter_mut() { *v = 42; } }
        _ => fill_random(arr, seed),
    }
}

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 4 {
        eprintln!("Usage: {} <size> <pattern> <runs>", args[0]);
        std::process::exit(1);
    }
    let n: usize = args[1].parse().unwrap();
    let pattern = &args[2];
    let runs: usize = args[3].parse().unwrap();

    let mut arr = vec![0i64; n];
    let mut best_ns = f64::MAX;

    // warm up
    fill_pattern(&mut arr, pattern, 42);
    arr.sort_unstable();

    for r in 0..runs {
        fill_pattern(&mut arr, pattern, 42 + r as u64);
        let t0 = Instant::now();
        arr.sort_unstable();
        let elapsed = t0.elapsed().as_nanos() as f64;
        let ns_per = elapsed / n as f64;
        if ns_per < best_ns { best_ns = ns_per; }
    }

    println!("{{\"algo\":\"rust_sort_unstable\",\"size\":{},\"pattern\":\"{}\",\"ns_per_elem\":{:.2}}}",
             n, pattern, best_ns);
}
