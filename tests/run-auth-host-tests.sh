#!/usr/bin/env bash
set -euo pipefail

# Native Jansson headers/library are required; Switch libraries cannot run on the host.
: "${JANSSON_SOURCE:?Set JANSSON_SOURCE to the native Jansson source directory}"
: "${JANSSON_BUILD:?Set JANSSON_BUILD to its native CMake build directory}"
cd "$(dirname "$0")/.."
test_binary="$(mktemp "${TMPDIR:-/tmp}/lunanx-auth-test.XXXXXX")"
trap 'rm -f "$test_binary"' EXIT
gc_flags=(-Wl,--gc-sections)
if [[ "$(uname -s)" == Darwin ]]; then
    gc_flags=(-Wl,-dead_strip)
fi
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
    -Iapp/src -I"$JANSSON_SOURCE/src" -I"$JANSSON_BUILD/include" \
    -I"${MBEDTLS_INCLUDE_DIR:-${DEVKITPRO:-/opt/devkitpro}/portlibs/switch/include}" \
    tests/amazon_device_sign_in_test.cpp app/src/luna/amazon_auth.cpp app/src/core/i18n.cpp \
    "$JANSSON_BUILD/lib/libjansson.a" "${gc_flags[@]}" -o "$test_binary"
"$test_binary"
