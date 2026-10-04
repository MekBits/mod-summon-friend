#!/usr/bin/env bash
# Logic test for mod-summon-friend. Needs only g++ -- no AzerothCore.
#
#   ./test/run.sh
#
# The rules live in src/summon_friend_rules.h without AC types, so there are
# no stubs to keep in sync. The test proves the DECISION is right; that
# mod_summon_friend.cpp still compiles against the core is shown by a real
# build (the core-build workflow).

set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

g++ -std=c++17 -Wall -Wextra -Werror -I "$HERE/../src" "$HERE/test_rules.cpp" -o "$OUT/test_rules"
"$OUT/test_rules"
