#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${1:-build}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
JOBS="${JOBS:-$(nproc)}"

# Clean common cross-compilation environment variables.
unset CC CXX AR AS LD RANLIB STRIP
unset CFLAGS CXXFLAGS CPPFLAGS LDFLAGS
unset CMAKE_TOOLCHAIN_FILE
unset PKG_CONFIG_PATH PKG_CONFIG_LIBDIR PKG_CONFIG_SYSROOT_DIR
unset OPENSSL_ROOT_DIR OPENSSL_INCLUDE_DIR OPENSSL_LIBRARIES OPENSSL_SSL_LIBRARY OPENSSL_CRYPTO_LIBRARY

# Strip rk3588 toolchain entries from LD_LIBRARY_PATH to avoid link/runtime pollution.
if [[ -n "${LD_LIBRARY_PATH:-}" ]]; then
  CLEAN_LD_LIBRARY_PATH=""
  IFS=':' read -ra LD_PARTS <<< "${LD_LIBRARY_PATH}"
  for p in "${LD_PARTS[@]}"; do
    if [[ -z "$p" || "$p" == *"rk3588_toolchain"* ]]; then
      continue
    fi
    if [[ -z "$CLEAN_LD_LIBRARY_PATH" ]]; then
      CLEAN_LD_LIBRARY_PATH="$p"
    else
      CLEAN_LD_LIBRARY_PATH="${CLEAN_LD_LIBRARY_PATH}:$p"
    fi
  done
  export LD_LIBRARY_PATH="$CLEAN_LD_LIBRARY_PATH"
fi

GENERATOR="Unix Makefiles"
if command -v ninja >/dev/null 2>&1; then
  GENERATOR="Ninja"
fi

echo "[build] root       : ${ROOT_DIR}"
echo "[build] build dir  : ${BUILD_DIR}"
echo "[build] build type : ${BUILD_TYPE}"
echo "[build] generator  : ${GENERATOR}"

cmake --fresh \
  -S "${ROOT_DIR}" \
  -B "${ROOT_DIR}/${BUILD_DIR}" \
  -G "${GENERATOR}" \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DOPENSSL_INCLUDE_DIR=/usr/include \
  -DOPENSSL_SSL_LIBRARY=/usr/lib/x86_64-linux-gnu/libssl.so \
  -DOPENSSL_CRYPTO_LIBRARY=/usr/lib/x86_64-linux-gnu/libcrypto.so \
  -DOPENSSL_ROOT_DIR=/usr \
  -DPKG_CONFIG_EXECUTABLE=/usr/bin/pkg-config \
  -DCMAKE_IGNORE_PATH="/home/ss/rk3588_toolchain;/home/ss/rk3588_toolchain/aarch64-buildroot-linux-gnu/sysroot"

cmake --build "${ROOT_DIR}/${BUILD_DIR}" -j"${JOBS}"

echo "[build] done: ${ROOT_DIR}/${BUILD_DIR}/lbl_server"
