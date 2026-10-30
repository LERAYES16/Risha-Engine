#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD="$ROOT/build"
cmake -S "$ROOT" -B "$BUILD" -DRISHA_BUILD_TESTS=ON
cmake --build "$BUILD" --parallel
ctest --test-dir "$BUILD" --output-on-failure
cmake --install "$BUILD" --prefix "$HOME/.local"
printf '%s\n' "تم تثبيت محرك ريشة في $HOME/.local/bin/risha_editor"
printf '%s\n' "إذا لم يظهر الأمر فوراً، افتح طرفية جديدة أو أضف $HOME/.local/bin إلى PATH."
