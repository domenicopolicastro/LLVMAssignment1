#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# fusion_pipeline.sh – Compila, canonicalizza e applica il Loop-Fusion Pass
# Uso:   ./fusion_pipeline.sh  nome_file_senza_estensione
# Esempio (per test/test_loop_fusion.c):
#        ./fusion_pipeline.sh test_loop_fusion
# ---------------------------------------------------------------------------

set -euo pipefail            # interrompe su errore, pipe, variabili unset

# 1. Parse & check argomento
if [[ $# -ne 1 ]]; then
  echo "Uso: $0 <nome_file_senza_estensione>"
  exit 1
fi
SRC_BASE="$1"                        # es. test_loop_fusion
SRC_PATH="test/${SRC_BASE}.c"        # percorso completo

if [[ ! -f "${SRC_PATH}" ]]; then
  echo "Errore: file sorgente '${SRC_PATH}' non trovato."
  exit 2
fi

# 2. File temporanei / output (stessi nomi)
BEFORE_LL="before.ll"
CLEAN_LL="before.clean.ll"
OPT_LL="optimized.ll"

# 3. Pipeline
echo "➜ Genero IR non ottimizzato..."
clang-18 -O0 -S -emit-llvm -Xclang -disable-O0-optnone \
         "${SRC_PATH}" -o "${BEFORE_LL}"

echo "➜ Canonicalizzo (mem2reg, loop-simplify)..."
opt-18 -passes="mem2reg,loop-simplify" \
       -S "${BEFORE_LL}" -o "${CLEAN_LL}"

echo "➜ Eseguo il Loop-Fusion Pass..."
opt-18 -load-pass-plugin=./build/libMyLLVMPasses.so \
       -passes="loop-fusion-pass" \
       -S "${CLEAN_LL}" -o "${OPT_LL}"

echo "➜ Differenza fra IR prima e dopo la fusion:"
code --diff "${CLEAN_LL}" "${OPT_LL}" &

echo "Done ✓"
