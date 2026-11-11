# Trabalho Final P2 - Teste baseado em defeitos (Sudoku em C++)

Programa: `src/sudoku.cpp` (jogo de Sudoku, versao limpa v3).
Ferramenta oficial: **universalmutator** (`pip install universalmutator`).

## Estrutura
- `src/sudoku.cpp` - codigo original
- `tests/test_sudoku.cpp` - suite de testes (83 testes, ~7400 verificacoes)
- `scripts/rodar_testes.sh` - compila e roda a suite contra UM arquivo-fonte
- `scripts/gerar_mutantes.sh` - gera os mutantes com a universalmutator -> `mutantes/`
- `scripts/analisar_mutantes.sh` - roda a suite em todos os mutantes e calcula o escore
- `scripts/mini_mutator.py` - mutador auxiliar (reserva, so para validar testes)
- `resultados_validacao_mini/` - resultados do ciclo de validacao feito com o mutador auxiliar
- `MUTANTES_EQUIVALENTES.md` - analise dos mutantes que nao podem ser mortos

## Como executar (Linux/macOS/WSL, g++ com C++17)
```bash
pip install universalmutator
scripts/rodar_testes.sh src/sudoku.cpp          # deve terminar com 0 falhas
scripts/gerar_mutantes.sh                       # cria mutantes/sudoku.mutant.N.cpp
scripts/analisar_mutantes.sh mutantes           # cria resultados/ (resumo, vivos.txt)
```
Um mutante e MORTO quando a suite falha (ou da timeout), VIVO quando passa,
INVALIDO quando nao compila (fora do escore).

## Detalhes da suite
- O `main` do jogo e renomeado (`#define main ...`) para o arquivo de teste incluir o `.cpp`.
- Cada execucao roda em diretorio temporario (o jogo grava `partida.save` no diretorio atual),
  entao as execucoes podem ser paralelizadas.
- Compila com `-D_GLIBCXX_ASSERTIONS` (acessos fora dos limites viram falha) e sem `NDEBUG`
  (os `assert` ficam ativos).
- Testes por camada: Tabuleiro (inclusive teste exaustivo 81x81 contra um oraculo das regras),
  desafios, registro em arquivo (formato binario byte a byte, arquivos corrompidos, erro de
  gravacao), Partida, apresentacao (saida exata) e interface de texto (entrada simulada).

## Resultado da validacao (mutador auxiliar, NAO e a universalmutator)
| Ciclo | Gerados | Invalidos | Validos | Mortos | Vivos | Escore |
|---|---|---|---|---|---|---|
| 1 (suite inicial) | 420 | 94 | 326 | 305 | 21 | 93,56% |
| 2 (testes novos) | 420 | 94 | 326 | 317 | 9 | 97,24% |

Os 9 vivos restantes foram analisados e sao equivalentes: escore ajustado = 317/317 = 100%.
No ciclo 2 apenas os 21 vivos do ciclo 1 foram reexecutados (os demais ja estavam mortos
e a suite so recebeu testes novos).
Os numeros finais do trabalho devem vir da execucao com a universalmutator.
