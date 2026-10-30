#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD="$ROOT/build"
command -v cmake >/dev/null 2>&1 || { echo "cmake غير مثبت: sudo apt install build-essential cmake libx11-dev"; exit 1; }
cmake -S "$ROOT" -B "$BUILD" -DRISHA_BUILD_TESTS=ON
cmake --build "$BUILD" --parallel
ctest --test-dir "$BUILD" --output-on-failure
echo "تم البناء والاختبار بنجاح: $BUILD/risha_editor"
if [ "${1:-}" != "--no-run" ]; then
    exec "$BUILD/risha_editor"
fi
