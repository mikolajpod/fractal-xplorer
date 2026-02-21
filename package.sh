#!/usr/bin/env bash
# Build fractal_xplorer and produce a self-contained portable ZIP.
# Run from the repo root inside an MSYS2 MinGW-w64 shell.
set -euo pipefail

export PATH="/c/msys64/mingw64/bin:/c/msys64/usr/bin:$PATH"

NAME="fractal_xplorer"
VERSION="1.4"
DIST="${NAME}-${VERSION}-win64"
BUILD_DIR="build"
ZIP_NAME="${DIST}.zip"

echo "=== Building ${NAME} v${VERSION} ==="
cmake --build "${BUILD_DIR}" -- -j$(nproc)

echo "=== Collecting files into ${DIST}/ ==="
rm -rf "${DIST}"
mkdir -p "${DIST}"

cp "${BUILD_DIR}/${NAME}.exe" "${DIST}/"
cp LICENSE  "${DIST}/"
cp README.md "${DIST}/"

# Copy every MinGW DLL the exe depends on (skip Windows system DLLs).
ldd "${BUILD_DIR}/${NAME}.exe" \
    | grep -i '/mingw64/' \
    | awk '{print $3}' \
    | sort -u \
    | while read -r dll; do
        echo "  + $(basename "${dll}")"
        cp "${dll}" "${DIST}/"
    done

echo "=== Creating ${ZIP_NAME} ==="
python -c "
import zipfile, os, sys

dist  = sys.argv[1]
zname = sys.argv[2]

with zipfile.ZipFile(zname, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
    for root, dirs, files in os.walk(dist):
        for f in sorted(files):
            path = os.path.join(root, f)
            zf.write(path, path.replace(os.sep, '/'))
" "${DIST}" "${ZIP_NAME}"

SIZE=$(python -c "import os; s=os.path.getsize('${ZIP_NAME}'); print(f'{s/1024/1024:.1f} MB')")
echo ""
echo "=== Done ==="
echo "  ${ZIP_NAME}  (${SIZE})"
echo ""
echo "Contents of ${DIST}/:"
ls -1 "${DIST}/"
