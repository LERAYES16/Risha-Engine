#!/bin/sh
set -eu
PREFIX="$HOME/.local"
rm -f "$PREFIX/bin/risha_editor"
rm -f "$PREFIX/share/applications/risha.desktop"
rm -rf "$PREFIX/share/risha"
rm -rf "$PREFIX/include/risha"
printf '%s\n' "تمت إزالة ملفات تثبيت محرك ريشة من $PREFIX."
