#!/usr/bin/env bash
# Roda a suite contra todos os mutantes de uma pasta e calcula o escore.
# Uso: scripts/analisar_mutantes.sh <pasta_mutantes> [pasta_resultados]
# Mutante: INVALIDO (nao compila) | MORTO (algum teste falhou/timeout) | VIVO
set -u
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
MUT="$(realpath "$1")"
OUT="$(realpath -m "${2:-$RAIZ/resultados}")"
mkdir -p "$OUT"
export RAIZ OUT

um_mutante() {
  f="$1"; id="$(basename "$f" .cpp)"
  "$RAIZ/scripts/rodar_testes.sh" "$f" --failfast >/dev/null 2>&1
  rc=$?
  if   [ $rc -eq 100 ]; then echo "$id,INVALIDO"
  elif [ $rc -eq 0 ];   then echo "$id,VIVO"
  else                       echo "$id,MORTO"; fi
}
export -f um_mutante

ls "$MUT"/*.mutant.*.cpp | xargs -P "$(nproc)" -I{} bash -c 'um_mutante {}' > "$OUT/resultados.csv"
sort -t. -k3 -n "$OUT/resultados.csv" -o "$OUT/resultados.csv"

mortos=$(grep -c ',MORTO' "$OUT/resultados.csv")
vivos=$(grep -c ',VIVO' "$OUT/resultados.csv")
invalidos=$(grep -c ',INVALIDO' "$OUT/resultados.csv")
grep ',VIVO' "$OUT/resultados.csv" | cut -d, -f1 > "$OUT/vivos.txt"
validos=$((mortos + vivos))
{
  echo "Mutantes gerados : $((validos + invalidos))"
  echo "Invalidos        : $invalidos (nao compilam, fora do escore)"
  echo "Validos          : $validos"
  echo "Mortos           : $mortos"
  echo "Vivos            : $vivos"
  [ $validos -gt 0 ] && echo "Escore de mutacao: $(python3 -c "print(f'{$mortos/$validos*100:.2f}')")%  (mortos / validos)"
  echo "Obs.: se houver mutantes equivalentes, escore ajustado = mortos / (validos - equivalentes)."
} | tee "$OUT/resumo.txt"
