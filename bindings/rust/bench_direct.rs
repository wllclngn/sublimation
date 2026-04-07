// bench_direct.rs -- Direct comparison: sublimation vs Rust sort_unstable
//
// Links libsublimation.a statically. Both sorts run in the same process
// on the same data with the same clock. No subprocess overhead.
//
// Build: rustc -O --edition 2021 -L ../../build -l static=sublimation -l pthread -l m bench_direct.rs -o bench_direct
use std::env;
use std::time::Instant;

extern "C" {
    fn sublimation_i64(arr: *mut i64, n: usize);
}

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

fn bench_sublimation(arr: &mut [i64], pattern: &str, runs: usize) -> f64 {
    let n = arr.len();
    let mut best = f64::MAX;
    for r in 0..runs {
        fill_pattern(arr, pattern, 42 + r as u64);
        let t0 = Instant::now();
        unsafe { sublimation_i64(arr.as_mut_ptr(), n); }
        let ns = t0.elapsed().as_nanos() as f64 / n as f64;
        if ns < best { best = ns; }
    }
    best
}

fn bench_rust(arr: &mut [i64], pattern: &str, runs: usize) -> f64 {
    let n = arr.len();
    let mut best = f64::MAX;
    for r in 0..runs {
        fill_pattern(arr, pattern, 42 + r as u64);
        let t0 = Instant::now();
        arr.sort_unstable();
        let ns = t0.elapsed().as_nanos() as f64 / n as f64;
        if ns < best { best = ns; }
    }
    best
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

    // warm up both
    fill_pattern(&mut arr, pattern, 42);
    unsafe { sublimation_i64(arr.as_mut_ptr(), n); }
    fill_pattern(&mut arr, pattern, 42);
    arr.sort_unstable();

    let ns_sub = bench_sublimation(&mut arr, pattern, runs);
    let ns_rust = bench_rust(&mut arr, pattern, runs);

    println!("{{\"algo\":\"sublimation_via_rust\",\"size\":{},\"pattern\":\"{}\",\"ns_per_elem\":{:.2}}}", n, pattern, ns_sub);
    println!("{{\"algo\":\"rust_sort_unstable_direct\",\"size\":{},\"pattern\":\"{}\",\"ns_per_elem\":{:.2}}}", n, pattern, ns_rust);
}
