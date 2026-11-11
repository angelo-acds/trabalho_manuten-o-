#!/usr/bin/env bash
# Compila e executa a suite de testes contra UM arquivo-fonte.
# Uso: scripts/rodar_testes.sh <arquivo.cpp> [--failfast]
# Codigo de saida: 0 = testes passaram | !=0 = falharam (ou nao compilou)
set -u
FONTE="$(realpath "$1")"
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

g++ -std=c++17 -O0 -w -D_GLIBCXX_ASSERTIONS -DSUT="\"$FONTE\"" -o "$TMP/teste" "$RAIZ/tests/test_sudoku.cpp" 2> "$TMP/erro_compilacao.txt" \
  || { echo "ERRO_DE_COMPILACAO"; exit 100; }

cd "$TMP"   # o jogo grava partida.save no diretorio corrente
if [ "${2:-}" = "--failfast" ]; then export SUDOKU_FAILFAST=1; fi
timeout 20 ./teste
exit $?
