#!/usr/bin/env bash
# Gera os mutantes com a universalmutator e descarta os que nao compilam.
set -eu
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
rm -rf "$RAIZ/mutantes" "$RAIZ/mutantes_invalidos"; mkdir -p "$RAIZ/mutantes"
cd "$RAIZ"

# 1) geracao (sem a checagem interna --cmd, que tem bug na versao 1.14.1)
mutate src/sudoku.cpp cpp --noCheck --mutantDir "$RAIZ/mutantes"
echo "Candidatos gerados: $(ls "$RAIZ"/mutantes/*.mutant.*.cpp | wc -l)"

# 2) filtro: mutantes que nao compilam vao para mutantes_invalidos/
mkdir -p "$RAIZ/mutantes_invalidos"
export INV="$RAIZ/mutantes_invalidos"
ls "$RAIZ"/mutantes/*.mutant.*.cpp | xargs -P "$(nproc)" -I{} bash -c \
  'g++ -std=c++17 -fsyntax-only -w "{}" 2>/dev/null || mv "{}" "$INV"/'

echo "Validos (compilam): $(ls "$RAIZ"/mutantes/*.mutant.*.cpp | wc -l)"
echo "Invalidos         : $(ls "$INV" | wc -l)"
