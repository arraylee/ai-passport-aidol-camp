#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
out="$(mktemp -d)"
trap 'rm -rf "$out"' EXIT
cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=undefined \
  -Itests/host/cards_fakes -Itests/vendor/cjson -Imain \
  tests/host/test_cards.c main/hr_cards.c main/hr_friend_store.c \
  main/hr_edit.c main/hr_core.c main/hr_social.c \
  tests/vendor/cjson/cJSON.c -lm -o "$out/test_cards"
"$out/test_cards"
flags="-std=c11 -Wall -Wextra -Werror -g -fsanitize=undefined"
includes="-Itests/host/cards_fakes -Itests/vendor/cjson -Imain"
rename="-Dhr_cards_init=b_hr_cards_init -Dhr_cards_reset=b_hr_cards_reset
-Dhr_cards_tick=b_hr_cards_tick -Dhr_cards_send=b_hr_cards_send
-Dhr_cards_receive=b_hr_cards_receive -Dhr_cards_ready=b_hr_cards_ready
-Dhr_cards_progress=b_hr_cards_progress
-Dhr_cards_stats=b_hr_cards_stats -Dhr_cards_commit=b_hr_cards_commit -Dhr_friend_store_init=b_hr_friend_store_init
-Dhr_friend_store_bind=b_hr_friend_store_bind
-Dhr_friend_store_matches=b_hr_friend_store_matches
-Dhr_friend_store_abort=b_hr_friend_store_abort -Dhr_friend_store_begin=b_hr_friend_store_begin
-Dhr_friend_store_append=b_hr_friend_store_append -Dhr_friend_store_finish=b_hr_friend_store_finish
-Dhr_friend_profile=b_hr_friend_profile -Dhr_friend_crc_update=b_hr_friend_crc_update
-Dhr_profile=b_hr_profile -Dhr_profile_current=b_hr_profile_current"
# Build a second independent firmware instance in the same network simulator.
cc $flags $includes $rename -c main/hr_cards.c -o "$out/cards_b.o"
cc $flags $includes $rename -c main/hr_friend_store.c -o "$out/store_b.o"
cc $flags $includes tests/host/test_cards_pair.c main/hr_cards.c main/hr_friend_store.c \
  main/hr_edit.c main/hr_core.c main/hr_identity.c main/hr_social.c tests/vendor/cjson/cJSON.c \
  "$out/cards_b.o" "$out/store_b.o" -lm -o "$out/test_cards_pair"
"$out/test_cards_pair"

"$out/test_cards_pair" resume-saved

"$out/test_cards_pair" resume-saved full-hello

"$out/test_cards_pair" resume-saved full-hello text-avatar

"$out/test_cards_pair" normal full-hello avatar-avatar tail-loss
"$out/test_cards_pair" normal full-hello avatar-avatar speed

"$out/test_cards_pair" normal full-hello avatar-avatar roster-retry
"$out/test_cards_pair" normal full-hello avatar-avatar multiline
"$out/test_cards_pair" normal full-hello avatar-avatar manual

# Reproducible variable loop timing, 20% loss plus duplicates and reordering.
for seed in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
 "$out/test_cards_pair" normal full-hello avatar-avatar fuzz "$seed"
done

# New firmware paired with an actual frozen single-chunk v0.7.4 peer.
cc $flags $includes $rename -c tests/fixtures/cards-v074.c -o "$out/cards_b.o"
cc $flags $includes tests/host/test_cards_pair.c main/hr_cards.c main/hr_friend_store.c \
 main/hr_edit.c main/hr_core.c main/hr_identity.c main/hr_social.c tests/vendor/cjson/cJSON.c \
 "$out/cards_b.o" "$out/store_b.o" -lm -o "$out/test_cards_mixed"
"$out/test_cards_mixed" normal full-hello avatar-avatar

"$out/test_cards_mixed" normal full-hello avatar-avatar multiline
