# Análise de Mutantes Equivalentes (Sobreviventes)

Durante o Ciclo 2 de testes, restaram 106 mutantes vivos. A análise manual revelou que a grande maioria consiste em **Mutantes Equivalentes**, que alteram a sintaxe, mas mantêm o comportamento exato do programa, tornando impossível criar testes que os matem. 

Principais categorias encontradas:

1. **Alteração em Comentários e Visibilidade**
   * *Exemplos:* Troca de `public:` por `/*public:*/` ou remoção de `// FACIL`.
   * *Justificativa:* Comentários são ignorados pelo compilador. Visibilidade em `structs` C++ já é `public` por padrão. O binário gerado é idêntico.

2. **Sintaxe de Loops e Contadores**
   * *Exemplos:* Troca de `ordem < TOTAL_CELULAS` por `ordem != TOTAL_CELULAS`.
   * *Justificativa:* Como a variável `ordem` é incrementada estritamente de 1 em 1, ela nunca ultrapassa o valor sem antes ser igual a ele. A condição de parada ocorre exatamente no mesmo momento.

3. **Lógica Matemática Estrita**
   * *Exemplos:* Troca de `numero >= NUMERO_MINIMO` (que vale 1) por `numero >= (1==1)`.
   * *Justificativa:* A expressão `(1==1)` resulta em `true`, que em C++ é convertido implicitamente para o inteiro `1`. A regra lógica permanece estritamente idêntica (`>= 1`).
   * *Exemplos:* Troca de `contar_celulas_vazias() == 0` por `<= 0`.
   * *Justificativa:* A quantidade de células vazias nunca pode ser negativa, então o comportamento final é o mesmo.

4. **Simetria de Argumentos**
   * *Exemplos:* Inversão de argumentos em `compartilham_regiao(Posicao a, Posicao b)` para `(Posicao b, Posicao a)`.
   * *Justificativa:* A verificação de pertencimento à mesma região é comutativa (a ordem dos fatores não altera o resultado booleano).
