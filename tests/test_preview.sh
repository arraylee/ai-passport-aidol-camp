#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT
flags="-std=c11 -Wall -Wextra -Werror -g -fsanitize=undefined"
includes="-Itests/host/cards_fakes -Imain"
rename="-Dhr_preview_tick=b_hr_preview_tick -Dhr_preview_reset=b_hr_preview_reset
-Dhr_preview_send=b_hr_preview_send -Dhr_preview_receive=b_hr_preview_receive
-Dhr_preview_image=b_hr_preview_image -Dhr_profile=b_hr_profile"
cc $flags $includes $rename -c main/hr_preview.c -o "$out/preview_b.o"
cc $flags $includes tests/host/test_preview.c main/hr_preview.c main/hr_core.c main/hr_identity.c \
 "$out/preview_b.o" -o "$out/test_preview"
"$out/test_preview"
