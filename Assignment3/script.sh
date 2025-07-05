#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# licm_pipeline.sh  –  Compila, canonicalizza e applica il custom-LICM
# Uso:   ./licm_pipeline.sh  nome_file_senza_estensione
# Esempio (per test/test_licm_advanced.c):
#        ./licm_pipeline.sh test_licm_advanced
# ---------------------------------------------------------------------------

set -euo pipefail        # exit on error, unset vars are errors, propagate pipes

# 1. Parsec & check argomento
if [[ $# -ne 1 ]]; then
  echo "Uso: $0 <nome_file_senza_estensione>"
  exit 1
fi
SRC_BASE="$1"                    # es. test_licm_advanced
SRC_PATH="test/${SRC_BASE}.c"    # percorso completo al .c

if [[ ! -f "${SRC_PATH}" ]]; then
  echo "Errore: file sorgente '${SRC_PATH}' non trovato."
  exit 2
fi

# 2. File temporanei / output
BEFORE_LL="before.ll"
CLEAN_LL="before.clean.ll"
OPT_LL="optimized.ll"

# 3. Pipeline
echo "➜ Genero IR non ottimizzato..."
clang-18 -O0 -S -emit-llvm -Xclang -disable-O0-optnone \
         "${SRC_PATH}" -o "${BEFORE_LL}"

echo "➜ Canonicalizzo (mem2reg, loop-simplify, lcssa)..."
opt-18 -passes="mem2reg,loop-simplify,lcssa" \
       -S "${BEFORE_LL}" -o "${CLEAN_LL}"

echo "➜ Eseguo il Custom LICM..."
opt-18 -load-pass-plugin=./build/libMyLLVMPasses.so \
       -passes="loop(custom-licm),verify" \
       -S "${CLEAN_LL}" -o "${OPT_LL}"

echo "➜ Differenza fra IR prima e dopo il LICM:"
code --diff "${CLEAN_LL}" "${OPT_LL}" &

echo "Done ✓"
