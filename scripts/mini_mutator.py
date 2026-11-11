#!/usr/bin/env python3
"""Mutador auxiliar (reserva) para C++.

Serve para validar a suite de testes quando a universalmutator nao esta
disponivel. NAO substitui a ferramenta oficial do trabalho.
Uso: mini_mutator.py <fonte.cpp> <pasta_saida>
Gera <pasta_saida>/sudoku.mutant.N.cpp e mutantes.csv (id, linha, operador).
"""
import re, sys, os

fonte, saida = sys.argv[1], sys.argv[2]
os.makedirs(saida, exist_ok=True)
linhas = open(fonte).read().split("\n")

REL = {"<=": [">", "<", "=="], ">=": ["<", ">", "=="], "==": ["!=", "<", ">"],
       "!=": ["==", "<", ">"], "<": ["<=", ">=", "=="], ">": [">=", "<=", "=="]}
ARIT = {"+": ["-", "*"], "-": ["+", "*"], "*": ["/", "+"], "/": ["*", "%"], "%": ["/", "*"]}
LOG = {"&&": ["||"], "||": ["&&"]}

TOKEN = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])\'|//.*$|'
                   r'(<=|>=|==|!=|&&|\|\||\+\+|--|\+=|-=|<<|>>|->|::|[<>+\-*/%!])|'
                   r'\b(\d+)\b')

mutantes = []  # (linha, operador, texto_da_linha_mutada)

def mutar_linha(i, linha):
    s = linha.strip()
    if s.startswith("#") or s.startswith("//") or s.startswith("*") or s.startswith("/*"):
        return
    for m in TOKEN.finditer(linha):
        tok = m.group(0)
        if tok.startswith('"') or tok.startswith("'") or tok.startswith("//"):
            continue
        a, b = m.span()
        def novo(t):
            return linha[:a] + t + linha[b:]
        if m.group(2) is not None:  # constante inteira
            v = int(tok)
            for nv in {v + 1, v - 1, 0} - {v}:
                if nv >= 0:
                    mutantes.append((i, "CONST", novo(str(nv))))
            continue
        if tok in REL:
            # evita template/stream: so se ha espacos ao redor
            if linha[a-1:a] == " " and linha[b:b+1] == " ":
                for r in REL[tok]: mutantes.append((i, "REL", novo(r)))
        elif tok in ARIT:
            if linha[a-1:a] == " " and linha[b:b+1] == " ":
                for r in ARIT[tok]: mutantes.append((i, "ARIT", novo(r)))
        elif tok in LOG:
            for r in LOG[tok]: mutantes.append((i, "LOG", novo(r)))
        elif tok == "!" and linha[b:b+1] not in ("=",):
            mutantes.append((i, "NEG", novo("")))
        elif tok == "++":
            mutantes.append((i, "INC", novo("--")))
        elif tok == "--":
            mutantes.append((i, "INC", novo("++")))
    # remocao de comando simples
    if re.match(r"^(return\b.*|continue|break|[\w:\.\->\[\]\(\)\s,+=*&]+);$", s) and not s.startswith(("for", "while", "if")):
        ind = linha[:len(linha) - len(linha.lstrip())]
        if s.startswith("return"):
            mutantes.append((i, "SDL", ind + ";"))
        elif "=" in s or s.endswith(");") or s in ("continue;", "break;"):
            mutantes.append((i, "SDL", ind + ";"))
    if s.startswith("return true;"):
        mutantes.append((i, "RET", linha.replace("true", "false")))
    if s.startswith("return false;"):
        mutantes.append((i, "RET", linha.replace("false", "true")))

for i, l in enumerate(linhas):
    mutar_linha(i, l)

vistos, n = set(), 0
base = os.path.basename(fonte).rsplit(".", 1)[0]
with open(os.path.join(saida, "mutantes.csv"), "w") as csv:
    csv.write("id,linha,operador\n")
    for (i, op, nova) in mutantes:
        if nova == linhas[i] or (i, nova) in vistos:
            continue
        vistos.add((i, nova))
        copia = linhas[:i] + [nova] + linhas[i + 1:]
        open(os.path.join(saida, f"{base}.mutant.{n}.cpp"), "w").write("\n".join(copia))
        csv.write(f"{n},{i+1},{op}\n")
        n += 1
print(f"{n} mutantes gerados em {saida}")
