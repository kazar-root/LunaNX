#!/usr/bin/env bash
# Compile LunaNX et produit build/LunaNX.nro.
set -euo pipefail

export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
export DEVKITA64="${DEVKITA64:-$DEVKITPRO/devkitA64}"
export PATH="$DEVKITPRO/tools/bin:$DEVKITA64/bin:$DEVKITPRO/portlibs/switch/bin:$(python3 -m site --user-base)/bin:$PATH"

cd "$(dirname "$0")"
jobs="$(sysctl -n hw.ncpu 2>/dev/null || nproc)"

cmake -B build -G Ninja
# Les dépendances WebRTC (projets externes) doivent exister avant l'édition de liens.
cmake --build build --target cjson mbedtls srtp2 usrsctp -j"$jobs"
cmake --build build --target LunaNX.nro -j"$jobs"

printf '\nCompilé : %s\n' "$(pwd)/build/LunaNX.nro"
