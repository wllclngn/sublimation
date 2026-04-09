# Maintainer: Will C.
pkgname=sublimation
pkgver=1.1.0
pkgrel=1
pkgdesc='Adaptive sorting library using spectral graph theory and flow-model architecture'
arch=('x86_64')
url='https://www.github.com/wllclngn/sublimation'
license=('GPL-2.0-only')
depends=('glibc')
makedepends=('gcc' 'python')
source=("$pkgname-$pkgver.tar.gz::$url/archive/v$pkgver.tar.gz")
sha256sums=('SKIP')

build() {
    cd "$srcdir/$pkgname-$pkgver"
    python3 install.py build
}

check() {
    cd "$srcdir/$pkgname-$pkgver"
    python3 tests/test.py --quick
}

package() {
    cd "$srcdir/$pkgname-$pkgver"

    # Libraries
    install -Dm644 build/libsublimation.a "$pkgdir/usr/lib/libsublimation.a"
    install -Dm755 build/libsublimation.so "$pkgdir/usr/lib/libsublimation.so"

    # Public header
    install -Dm644 src/include/sublimation.h "$pkgdir/usr/include/sublimation/sublimation.h"

    # Internal headers
    install -dm755 "$pkgdir/usr/include/sublimation/internal"
    install -Dm644 src/include/internal/*.h -t "$pkgdir/usr/include/sublimation/internal/"

    # License
    install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
