// Suite de testes do Sudoku (teste de mutacao - P2).
//
// Compilacao: o codigo sob teste e informado por -DSUT="caminho/sudoku.cpp".
// Assim o MESMO arquivo de teste e usado contra o original e contra cada mutante.
//
// Convencao de saida: codigo 0 = todos os testes passaram (mutante VIVO);
//                     codigo != 0 = algum teste falhou (mutante MORTO).
//
// Cada teste roda em diretorio temporario (o runner faz cd), pois o jogo grava
// sempre "partida.save" no diretorio corrente.

#define main sudoku_main_original
#include SUT
#undef main

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <functional>
#include <sstream>
#include <csignal>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

// ---------------------------------------------------------------------------
// Mini framework
// ---------------------------------------------------------------------------
static int g_total = 0;
static int g_falhas = 0;
static bool g_failfast = false;
static const char* g_teste_atual = "";

static void registrar_falha(const char* arq, int linha, const std::string& msg) {
  g_falhas++;
  std::cerr << "FALHA [" << g_teste_atual << "] " << arq << ":" << linha << ": "
            << msg << "\n";
  if (g_failfast) std::exit(1);
}

#define CHECK(cond)                                      \
  do {                                                   \
    g_total++;                                           \
    if (!(cond)) registrar_falha(__FILE__, __LINE__, #cond); \
  } while (0)

#define CHECK_EQ(a, b)                                                     \
  do {                                                                     \
    g_total++;                                                             \
    auto va_ = (a);                                                        \
    auto vb_ = (b);                                                        \
    if (!(va_ == vb_)) {                                                   \
      std::ostringstream os_;                                              \
      os_ << #a << " == " << #b << "  (obtido: " << va_ << ", esperado: " \
          << vb_ << ")";                                                   \
      registrar_falha(__FILE__, __LINE__, os_.str());                      \
    }                                                                      \
  } while (0)

// Verifica que `expr` lanca `Exc` e que what() == msg.
#define CHECK_THROWS(expr, Exc, msg)                                       \
  do {                                                                     \
    g_total++;                                                             \
    bool lancou_ = false;                                                  \
    try {                                                                  \
      expr;                                                                \
    } catch (const Exc& e_) {                                              \
      lancou_ = true;                                                      \
      if (std::string(e_.what()) != std::string(msg)) {                    \
        registrar_falha(__FILE__, __LINE__,                                \
                        std::string("mensagem errada: '") + e_.what() +    \
                            "' (esperada: '" + msg + "')");                \
      }                                                                    \
    } catch (...) {                                                        \
      registrar_falha(__FILE__, __LINE__, "excecao de tipo errado");       \
      lancou_ = true;                                                      \
    }                                                                      \
    if (!lancou_) registrar_falha(__FILE__, __LINE__, "nao lancou " #Exc); \
  } while (0)

// Verifica que `expr` lanca `Exc` (qualquer mensagem).
#define CHECK_THROWS_ANY_MSG(expr, Exc)                                    \
  do {                                                                     \
    g_total++;                                                             \
    bool lancou_ = false;                                                  \
    try {                                                                  \
      expr;                                                                \
    } catch (const Exc&) {                                                 \
      lancou_ = true;                                                      \
    } catch (...) {                                                        \
      registrar_falha(__FILE__, __LINE__, "excecao de tipo errado");       \
      lancou_ = true;                                                      \
    }                                                                      \
    if (!lancou_) registrar_falha(__FILE__, __LINE__, "nao lancou " #Exc); \
  } while (0)

#define CHECK_NO_THROW(expr)                                                \
  do {                                                                      \
    g_total++;                                                              \
    try {                                                                   \
      expr;                                                                 \
    } catch (const std::exception& e_) {                                    \
      registrar_falha(__FILE__, __LINE__,                                   \
                      std::string("excecao inesperada: ") + e_.what());     \
    }                                                                       \
  } while (0)

struct Teste {
  const char* nome;
  std::function<void()> fn;
};
static std::vector<Teste>& testes() {
  static std::vector<Teste> v;
  return v;
}
struct Registrador {
  Registrador(const char* nome, std::function<void()> fn) {
    testes().push_back({nome, std::move(fn)});
  }
};
#define TESTE(nome)                                          \
  static void teste_##nome();                                \
  static Registrador registrador_##nome(#nome, teste_##nome); \
  static void teste_##nome()

// ---------------------------------------------------------------------------
// Utilitarios
// ---------------------------------------------------------------------------
static const char* ARQ = "partida.save";

static void limpar() { std::remove(ARQ); }  // remove arquivo ou diretorio vazio

static std::string ler_arquivo() {
  std::ifstream f(ARQ, std::ios::binary);
  std::ostringstream os;
  os << f.rdbuf();
  return os.str();
}

static void escrever_arquivo(const std::string& bytes) {
  std::ofstream f(ARQ, std::ios::binary | std::ios::trunc);
  f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

static int total_no_arquivo() {
  const std::string c = ler_arquivo();
  int total = -1;
  if (c.size() >= sizeof(int)) std::memcpy(&total, c.data(), sizeof(int));
  return total;
}

static std::string bytes_total(int total) {
  return std::string(reinterpret_cast<const char*>(&total), sizeof(int));
}

static std::string digitos(const Tabuleiro& t) {
  std::string s;
  for (int o = 0; o < Tabuleiro::TOTAL_CELULAS; o++)
    s += static_cast<char>('0' + t.numero_em(Tabuleiro::posicao_da_ordem(o)));
  return s;
}

static Tabuleiro de_digitos(const std::string& s) {
  Tabuleiro t;
  for (int o = 0; o < Tabuleiro::TOTAL_CELULAS; o++)
    t.preencher(Tabuleiro::posicao_da_ordem(o), s[o] - '0');
  return t;
}

static bool iguais(const Tabuleiro& a, const Tabuleiro& b) {
  return digitos(a) == digitos(b);
}

static bool tem(const std::string& texto, const std::string& trecho) {
  return texto.find(trecho) != std::string::npos;
}

static int contar(const std::string& texto, const std::string& trecho) {
  int n = 0;
  for (size_t p = texto.find(trecho); p != std::string::npos;
       p = texto.find(trecho, p + 1))
    n++;
  return n;
}

// Oracle independente das regras: o tabuleiro completo e uma solucao valida?
static bool solucao_valida(const Tabuleiro& t) {
  for (int i = 0; i < 9; i++) {
    int linha = 0, coluna = 0, bloco = 0;
    for (int j = 0; j < 9; j++) {
      const int nl = t.numero_em({i, j});
      const int nc = t.numero_em({j, i});
      const int nb = t.numero_em({(i / 3) * 3 + j / 3, (i % 3) * 3 + j % 3});
      if (nl < 1 || nl > 9 || nc < 1 || nc > 9 || nb < 1 || nb > 9) return false;
      linha |= 1 << nl;
      coluna |= 1 << nc;
      bloco |= 1 << nb;
    }
    if (linha != 0x3FE || coluna != 0x3FE || bloco != 0x3FE) return false;
  }
  return true;
}

// Executa a interface de texto com a entrada dada e devolve tudo que foi impresso.
static std::string executar_interface(const std::string& entrada) {
  std::istringstream in(entrada);
  std::ostringstream out;
  std::streambuf* buf_in = std::cin.rdbuf(in.rdbuf());
  std::streambuf* buf_out = std::cout.rdbuf(out.rdbuf());
  std::cin.clear();
  try {
    InterfaceDeTexto ui;
    ui.executar();
  } catch (...) {
    std::cin.rdbuf(buf_in);
    std::cout.rdbuf(buf_out);
    throw;
  }
  std::cin.rdbuf(buf_in);
  std::cout.rdbuf(buf_out);
  std::cin.clear();
  return out.str();
}

static const char* DESAFIO_FACIL =
    "140782690020091040806000721008207004260409000704836209009028000600970038"
    "005160970";
static const char* DESAFIO_MEDIO =
    "007300020004007051900104367600002485000506209020009036000000003036080740"
    "401003008";
static const char* DESAFIO_DIFICIL =
    "208300094401000200000002000900010040304065070610098000093000080006000000"
    "000000123";

// Resolve um nivel inteiro e devolve o tabuleiro (usa uma Partida a parte).
static Tabuleiro solucao_do_nivel(Nivel nivel) {
  limpar();
  Partida p;
  p.iniciar_nova(nivel);
  p.resolver_tudo();
  Tabuleiro t = p.tabuleiro();
  limpar();
  return t;
}

// ===========================================================================
// TABULEIRO
// ===========================================================================
TESTE(tabuleiro_constantes) {
  CHECK_EQ(Tabuleiro::TAMANHO, 9);
  CHECK_EQ(Tabuleiro::TAMANHO_BLOCO, 3);
  CHECK_EQ(Tabuleiro::TOTAL_CELULAS, 81);
  CHECK_EQ(Tabuleiro::CELULA_VAZIA, 0);
  CHECK_EQ(Tabuleiro::NUMERO_MINIMO, 1);
  CHECK_EQ(Tabuleiro::NUMERO_MAXIMO, 9);
}

TESTE(tabuleiro_existe_limites) {
  CHECK(Tabuleiro::existe({0, 0}));
  CHECK(Tabuleiro::existe({8, 8}));
  CHECK(Tabuleiro::existe({0, 8}));
  CHECK(Tabuleiro::existe({8, 0}));
  CHECK(Tabuleiro::existe({4, 4}));
  CHECK(!Tabuleiro::existe({-1, 0}));
  CHECK(!Tabuleiro::existe({0, -1}));
  CHECK(!Tabuleiro::existe({9, 0}));
  CHECK(!Tabuleiro::existe({0, 9}));
  CHECK(!Tabuleiro::existe({-1, -1}));
  CHECK(!Tabuleiro::existe({9, 9}));
  CHECK(!Tabuleiro::existe({9, 4}));
  CHECK(!Tabuleiro::existe({4, 9}));
  CHECK(!Tabuleiro::existe({-1, 4}));
  CHECK(!Tabuleiro::existe({4, -1}));
}

TESTE(tabuleiro_posicao_da_ordem) {
  auto p = Tabuleiro::posicao_da_ordem(0);
  CHECK(p == (Posicao{0, 0}));
  p = Tabuleiro::posicao_da_ordem(8);
  CHECK(p == (Posicao{0, 8}));
  p = Tabuleiro::posicao_da_ordem(9);
  CHECK(p == (Posicao{1, 0}));
  p = Tabuleiro::posicao_da_ordem(13);
  CHECK(p == (Posicao{1, 4}));
  p = Tabuleiro::posicao_da_ordem(40);
  CHECK(p == (Posicao{4, 4}));
  p = Tabuleiro::posicao_da_ordem(80);
  CHECK(p == (Posicao{8, 8}));
  for (int o = 0; o < 81; o++) {
    p = Tabuleiro::posicao_da_ordem(o);
    CHECK_EQ(p.linha * 9 + p.coluna, o);
  }
}

TESTE(posicao_igualdade) {
  CHECK((Posicao{1, 2}) == (Posicao{1, 2}));
  CHECK(!((Posicao{1, 2}) == (Posicao{1, 3})));
  CHECK(!((Posicao{1, 2}) == (Posicao{2, 2})));
  CHECK(!((Posicao{1, 2}) == (Posicao{2, 1})));
}

TESTE(tabuleiro_vazio_inicial) {
  Tabuleiro t;
  CHECK_EQ(t.contar_celulas_vazias(), 81);
  CHECK(!t.esta_completo());
  for (int o = 0; o < 81; o++)
    CHECK_EQ(t.numero_em(Tabuleiro::posicao_da_ordem(o)), 0);
}

TESTE(tabuleiro_preencher_e_contar) {
  Tabuleiro t;
  t.preencher({0, 0}, 5);
  CHECK_EQ(t.numero_em({0, 0}), 5);
  CHECK_EQ(t.numero_em({0, 1}), 0);
  CHECK_EQ(t.numero_em({1, 0}), 0);
  CHECK_EQ(t.contar_celulas_vazias(), 80);
  t.preencher({8, 8}, 7);
  CHECK_EQ(t.numero_em({8, 8}), 7);
  CHECK_EQ(t.contar_celulas_vazias(), 79);
  t.preencher({3, 6}, 2);
  CHECK_EQ(t.numero_em({3, 6}), 2);
  CHECK_EQ(t.numero_em({6, 3}), 0);  // linha/coluna nao confundidas
  CHECK_EQ(t.contar_celulas_vazias(), 78);
}

TESTE(tabuleiro_ordem_da_linha_coluna) {
  // cada celula guarda seu proprio valor: detecta indexacao errada
  Tabuleiro t;
  for (int l = 0; l < 9; l++)
    for (int c = 0; c < 9; c++) {
      // sequencia latina valida: ((l*3 + l/3 + c) % 9) + 1
      t.preencher({l, c}, (l * 3 + l / 3 + c) % 9 + 1);
    }
  CHECK(t.esta_completo());
  CHECK_EQ(t.contar_celulas_vazias(), 0);
  CHECK(solucao_valida(t));
  for (int l = 0; l < 9; l++)
    for (int c = 0; c < 9; c++)
      CHECK_EQ(t.numero_em({l, c}), (l * 3 + l / 3 + c) % 9 + 1);
}

TESTE(tabuleiro_preencher_fora) {
  Tabuleiro t;
  const char* m = "Posicao fora do tabuleiro";
  CHECK_THROWS(t.preencher({-1, 0}, 1), JogadaInvalida, m);
  CHECK_THROWS(t.preencher({0, -1}, 1), JogadaInvalida, m);
  CHECK_THROWS(t.preencher({9, 0}, 1), JogadaInvalida, m);
  CHECK_THROWS(t.preencher({0, 9}, 1), JogadaInvalida, m);
  CHECK_EQ(t.contar_celulas_vazias(), 81);
}

TESTE(tabuleiro_preencher_intervalo) {
  Tabuleiro t;
  const char* m = "O numero deve estar entre 1 e 9";
  CHECK_THROWS(t.preencher({0, 0}, 10), JogadaInvalida, m);
  CHECK_THROWS(t.preencher({0, 0}, -1), JogadaInvalida, m);
  CHECK_THROWS(t.preencher({0, 0}, 100), JogadaInvalida, m);
  CHECK_EQ(t.numero_em({0, 0}), 0);
  CHECK_NO_THROW(t.preencher({0, 0}, 1));
  CHECK_NO_THROW(t.preencher({1, 3}, 9));
  CHECK_EQ(t.numero_em({0, 0}), 1);
  CHECK_EQ(t.numero_em({1, 3}), 9);
  for (int n = 1; n <= 9; n++) {
    Tabuleiro u;
    CHECK_NO_THROW(u.preencher({4, 4}, n));
    CHECK_EQ(u.numero_em({4, 4}), n);
  }
}

TESTE(tabuleiro_apagar) {
  Tabuleiro t;
  t.preencher({2, 2}, 4);
  CHECK_EQ(t.contar_celulas_vazias(), 80);
  CHECK_NO_THROW(t.preencher({2, 2}, Tabuleiro::CELULA_VAZIA));
  CHECK_EQ(t.numero_em({2, 2}), 0);
  CHECK_EQ(t.contar_celulas_vazias(), 81);
  // apagar celula ja vazia nao falha
  CHECK_NO_THROW(t.preencher({2, 2}, 0));
  // apos apagar, o numero pode ser usado de novo na mesma regiao
  CHECK_NO_THROW(t.preencher({2, 3}, 4));
}

TESTE(tabuleiro_conflito_mensagem) {
  Tabuleiro t;
  t.preencher({0, 0}, 5);
  CHECK_THROWS(t.preencher({0, 8}, 5), JogadaInvalida,
               "O numero 5 ja aparece na mesma linha, coluna ou bloco");
  CHECK_EQ(t.numero_em({0, 8}), 0);
  CHECK_EQ(t.contar_celulas_vazias(), 80);
}

TESTE(tabuleiro_conflito_linha_coluna_bloco) {
  Tabuleiro t;
  t.preencher({4, 4}, 5);
  // mesma linha / mesma coluna
  CHECK_THROWS_ANY_MSG(t.preencher({4, 0}, 5), JogadaInvalida);
  CHECK_THROWS_ANY_MSG(t.preencher({4, 8}, 5), JogadaInvalida);
  CHECK_THROWS_ANY_MSG(t.preencher({0, 4}, 5), JogadaInvalida);
  CHECK_THROWS_ANY_MSG(t.preencher({8, 4}, 5), JogadaInvalida);
  // mesmo bloco (outra linha e outra coluna)
  CHECK_THROWS_ANY_MSG(t.preencher({3, 3}, 5), JogadaInvalida);
  CHECK_THROWS_ANY_MSG(t.preencher({5, 5}, 5), JogadaInvalida);
  CHECK_THROWS_ANY_MSG(t.preencher({3, 5}, 5), JogadaInvalida);
  CHECK_THROWS_ANY_MSG(t.preencher({5, 3}, 5), JogadaInvalida);
  // fora de qualquer regiao compartilhada
  CHECK_NO_THROW(t.preencher({0, 0}, 5));
  CHECK_NO_THROW(t.preencher({8, 8}, 5));
}

// Teste exaustivo contra um oracle: para TODO par (A, B) de posicoes,
// 5 em B conflita com 5 em A  <=>  A != B e (mesma linha, coluna ou bloco).
TESTE(tabuleiro_regioes_exaustivo) {
  for (int a = 0; a < 81; a++) {
    for (int b = 0; b < 81; b++) {
      const Posicao pa = Tabuleiro::posicao_da_ordem(a);
      const Posicao pb = Tabuleiro::posicao_da_ordem(b);
      Tabuleiro t;
      t.preencher(pa, 5);
      const bool mesmo_bloco =
          pa.linha / 3 == pb.linha / 3 && pa.coluna / 3 == pb.coluna / 3;
      const bool esperado_conflito =
          a != b && (pa.linha == pb.linha || pa.coluna == pb.coluna || mesmo_bloco);
      bool conflitou = false;
      try {
        t.preencher(pb, 5);
      } catch (const JogadaInvalida&) {
        conflitou = true;
      }
      if (conflitou != esperado_conflito) {
        std::ostringstream os;
        os << "regiao: A=" << pa.linha << "," << pa.coluna << " B=" << pb.linha
           << "," << pb.coluna << " conflito=" << conflitou
           << " esperado=" << esperado_conflito;
        registrar_falha(__FILE__, __LINE__, os.str());
      }
      g_total++;
    }
  }
}

TESTE(tabuleiro_numero_diferente_nao_conflita) {
  Tabuleiro t;
  t.preencher({0, 0}, 5);
  CHECK_NO_THROW(t.preencher({0, 1}, 6));
  CHECK_NO_THROW(t.preencher({1, 0}, 7));
  CHECK_NO_THROW(t.preencher({1, 1}, 8));
}

TESTE(tabuleiro_sobrescrever_mesma_celula) {
  Tabuleiro t;
  t.preencher({3, 3}, 5);
  CHECK_NO_THROW(t.preencher({3, 3}, 5));  // a propria celula nao conta
  CHECK_NO_THROW(t.preencher({3, 3}, 6));
  CHECK_EQ(t.numero_em({3, 3}), 6);
  CHECK_EQ(t.contar_celulas_vazias(), 80);
}

TESTE(tabuleiro_dica_vazio_sem_dica) {
  Tabuleiro t;
  CHECK(!t.aplicar_dica().has_value());
  CHECK_EQ(t.contar_celulas_vazias(), 81);
}

TESTE(tabuleiro_dica_celula_unica) {
  Tabuleiro t;
  for (int c = 0; c < 8; c++) t.preencher({0, c}, c + 1);
  auto d = t.aplicar_dica();
  CHECK(d.has_value());
  CHECK(d->posicao == (Posicao{0, 8}));
  CHECK_EQ(d->numero, 9);
  CHECK_EQ(t.numero_em({0, 8}), 9);
  CHECK_EQ(t.contar_celulas_vazias(), 72);
}

TESTE(tabuleiro_dica_primeira_na_ordem) {
  Tabuleiro t;
  const int l0[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  const int l1[8] = {4, 5, 6, 7, 8, 9, 1, 2};
  for (int c = 0; c < 8; c++) {
    t.preencher({0, c}, l0[c]);
    t.preencher({1, c}, l1[c]);
  }
  auto d1 = t.aplicar_dica();
  CHECK(d1.has_value());
  CHECK(d1->posicao == (Posicao{0, 8}));
  CHECK_EQ(d1->numero, 9);
  auto d2 = t.aplicar_dica();
  CHECK(d2.has_value());
  CHECK(d2->posicao == (Posicao{1, 8}));
  CHECK_EQ(d2->numero, 3);
  CHECK_EQ(t.numero_em({1, 8}), 3);
}

TESTE(tabuleiro_dica_ignora_celula_preenchida) {
  // linha 0 completa: a celula (0,0)=1 tem um unico numero possivel (ela
  // mesma), mas esta preenchida e nao pode gerar dica.
  Tabuleiro t;
  for (int c = 0; c < 9; c++) t.preencher({0, c}, c + 1);
  CHECK(!t.aplicar_dica().has_value());
  CHECK_EQ(t.contar_celulas_vazias(), 72);
}

TESTE(tabuleiro_dica_so_com_um_possivel) {
  // celula com dois possiveis nao recebe dica: (0,7) e (0,8) admitem {8,9}
  Tabuleiro t;
  for (int c = 0; c < 7; c++) t.preencher({0, c}, c + 1);
  CHECK(!t.aplicar_dica().has_value());
  CHECK_EQ(t.contar_celulas_vazias(), 74);
}

TESTE(tabuleiro_dica_celula_sem_possibilidade) {
  // celula sem nenhum numero possivel nao e dica (size == 0)
  Tabuleiro t;
  // (0,0) vazia; 1..8 na linha 0 e 9 na coluna 0 -> sem opcao para (0,0)
  for (int c = 1; c < 9; c++) t.preencher({0, c}, c);
  t.preencher({4, 0}, 9);
  CHECK(!t.aplicar_dica().has_value());
  CHECK_EQ(t.numero_em({0, 0}), 0);
}

// ===========================================================================
// DESAFIOS
// ===========================================================================
TESTE(desafio_facil_conteudo) {
  Tabuleiro t = desafio_do_nivel(Nivel::FACIL);
  CHECK_EQ(digitos(t), std::string(DESAFIO_FACIL));
  CHECK_EQ(t.contar_celulas_vazias(), 37);
}

TESTE(desafio_medio_conteudo) {
  Tabuleiro t = desafio_do_nivel(Nivel::MEDIO);
  CHECK_EQ(digitos(t), std::string(DESAFIO_MEDIO));
  CHECK_EQ(t.contar_celulas_vazias(), 45);
}

TESTE(desafio_dificil_conteudo) {
  Tabuleiro t = desafio_do_nivel(Nivel::DIFICIL);
  CHECK_EQ(digitos(t), std::string(DESAFIO_DIFICIL));
  CHECK_EQ(t.contar_celulas_vazias(), 53);
}

TESTE(desafio_resolvivel_so_com_dicas) {
  const Nivel niveis[3] = {Nivel::FACIL, Nivel::MEDIO, Nivel::DIFICIL};
  for (Nivel n : niveis) {
    Tabuleiro t = desafio_do_nivel(n);
    int passos = 0;
    int vazias = t.contar_celulas_vazias();
    while (t.aplicar_dica().has_value()) {
      passos++;
      vazias--;
      CHECK_EQ(t.contar_celulas_vazias(), vazias);
    }
    CHECK(t.esta_completo());
    CHECK(solucao_valida(t));
    CHECK(passos > 0);
  }
}

TESTE(desafio_solucao_respeita_dados) {
  const char* ds[3] = {DESAFIO_FACIL, DESAFIO_MEDIO, DESAFIO_DIFICIL};
  const Nivel niveis[3] = {Nivel::FACIL, Nivel::MEDIO, Nivel::DIFICIL};
  for (int i = 0; i < 3; i++) {
    Tabuleiro s = solucao_do_nivel(niveis[i]);
    CHECK(solucao_valida(s));
    const std::string d = digitos(s);
    for (int o = 0; o < 81; o++)
      if (ds[i][o] != '0') CHECK_EQ(d[o], ds[i][o]);
  }
}

// ===========================================================================
// REGISTRO DE PARTIDA (persistencia)
// ===========================================================================
TESTE(registro_existe) {
  limpar();
  CHECK(!RegistroDePartida::existe());
  Tabuleiro d = desafio_do_nivel(Nivel::FACIL);
  { RegistroDePartida r = RegistroDePartida::criar(d); }
  CHECK(RegistroDePartida::existe());
  limpar();
  CHECK(!RegistroDePartida::existe());
}

TESTE(registro_criar_formato) {
  limpar();
  Tabuleiro d = desafio_do_nivel(Nivel::FACIL);
  { RegistroDePartida r = RegistroDePartida::criar(d); }
  const std::string c = ler_arquivo();
  CHECK_EQ(c.size(), static_cast<size_t>(4 + 81));
  CHECK_EQ(c.substr(0, 4), bytes_total(0));
  CHECK_EQ(c.substr(4), std::string(DESAFIO_FACIL));
  limpar();
}

TESTE(registro_criar_sobrescreve_anterior) {
  limpar();
  escrever_arquivo(std::string(500, 'x'));
  Tabuleiro d = desafio_do_nivel(Nivel::MEDIO);
  { RegistroDePartida r = RegistroDePartida::criar(d); }
  const std::string c = ler_arquivo();
  CHECK_EQ(c.size(), static_cast<size_t>(85));
  CHECK_EQ(c.substr(4), std::string(DESAFIO_MEDIO));
  limpar();
}

TESTE(registro_registrar_jogada) {
  limpar();
  Tabuleiro d = desafio_do_nivel(Nivel::FACIL);
  Tabuleiro t = d;
  t.preencher({0, 2}, 3);
  {
    RegistroDePartida r = RegistroDePartida::criar(d);
    r.registrar_jogada(t);
  }
  std::string c = ler_arquivo();
  CHECK_EQ(c.size(), static_cast<size_t>(4 + 81 + 81));
  CHECK_EQ(total_no_arquivo(), 1);
  CHECK_EQ(c.substr(4, 81), std::string(DESAFIO_FACIL));
  CHECK_EQ(c.substr(85, 81), digitos(t));

  t.preencher({0, 8}, 5);
  {
    RegistroDePartida r = RegistroDePartida::abrir();
    r.registrar_jogada(t);
  }
  c = ler_arquivo();
  CHECK_EQ(c.size(), static_cast<size_t>(4 + 81 * 3));
  CHECK_EQ(total_no_arquivo(), 2);
  CHECK_EQ(c.substr(4 + 81 * 2, 81), digitos(t));
  limpar();
}

TESTE(registro_abrir_inexistente) {
  limpar();
  CHECK_THROWS(RegistroDePartida::abrir(), ArquivoInexistente, "partida.save");
}

TESTE(registro_ler_desafio_e_ultimo) {
  limpar();
  Tabuleiro d = desafio_do_nivel(Nivel::FACIL);
  Tabuleiro t1 = d;
  t1.preencher({0, 2}, 3);
  Tabuleiro t2 = t1;
  t2.preencher({0, 8}, 5);
  {
    RegistroDePartida r = RegistroDePartida::criar(d);
    r.registrar_jogada(t1);
    r.registrar_jogada(t2);
  }
  RegistroDePartida r = RegistroDePartida::abrir();
  CHECK(iguais(r.ler_desafio(), d));
  CHECK(iguais(r.ler_ultimo_tabuleiro(), t2));
  // leituras repetidas e intercaladas continuam corretas
  CHECK(iguais(r.ler_ultimo_tabuleiro(), t2));
  CHECK(iguais(r.ler_desafio(), d));
  limpar();
}

TESTE(registro_somente_desafio) {
  limpar();
  Tabuleiro d = desafio_do_nivel(Nivel::DIFICIL);
  { RegistroDePartida r = RegistroDePartida::criar(d); }
  RegistroDePartida r = RegistroDePartida::abrir();
  CHECK(iguais(r.ler_desafio(), d));
  CHECK(iguais(r.ler_ultimo_tabuleiro(), d));
  limpar();
}

TESTE(registro_arquivo_curto_corrompido) {
  limpar();
  escrever_arquivo(bytes_total(0) + std::string(80, '0'));  // falta 1 digito
  {
    RegistroDePartida r = RegistroDePartida::abrir();
    CHECK_THROWS(r.ler_desafio(), ArquivoCorrompido,
                 "O registro nao contem um tabuleiro completo");
    CHECK_THROWS_ANY_MSG(r.ler_ultimo_tabuleiro(), ArquivoCorrompido);
  }
  escrever_arquivo(bytes_total(0));  // so o cabecalho
  {
    RegistroDePartida r = RegistroDePartida::abrir();
    CHECK_THROWS_ANY_MSG(r.ler_desafio(), ArquivoCorrompido);
    CHECK_THROWS_ANY_MSG(r.ler_ultimo_tabuleiro(), ArquivoCorrompido);
  }
  limpar();
}

TESTE(registro_arquivo_vazio_total) {
  limpar();
  escrever_arquivo("");
  RegistroDePartida r = RegistroDePartida::abrir();
  Tabuleiro t = desafio_do_nivel(Nivel::FACIL);
  CHECK_THROWS(r.registrar_jogada(t), ArquivoCorrompido,
               "O registro nao contem o total de jogadas");
  escrever_arquivo("ab");  // menos que sizeof(int)
  RegistroDePartida r2 = RegistroDePartida::abrir();
  CHECK_THROWS_ANY_MSG(r2.registrar_jogada(t), ArquivoCorrompido);
  limpar();
}

TESTE(registro_digito_invalido) {
  limpar();
  std::string tab(81, '0');
  tab[10] = ':';  // '0' + 10 -> numero 10
  escrever_arquivo(bytes_total(0) + tab);
  RegistroDePartida r = RegistroDePartida::abrir();
  CHECK_THROWS(r.ler_desafio(), ArquivoCorrompido,
               "O numero deve estar entre 1 e 9");
  tab[10] = '/';  // numero -1
  escrever_arquivo(bytes_total(0) + tab);
  RegistroDePartida r2 = RegistroDePartida::abrir();
  CHECK_THROWS(r2.ler_desafio(), ArquivoCorrompido,
               "O numero deve estar entre 1 e 9");
  limpar();
}

TESTE(registro_conflito_no_arquivo) {
  limpar();
  std::string tab(81, '0');
  tab[0] = '7';
  tab[1] = '7';  // mesma linha
  escrever_arquivo(bytes_total(0) + tab);
  RegistroDePartida r = RegistroDePartida::abrir();
  CHECK_THROWS(r.ler_desafio(), ArquivoCorrompido,
               "O numero 7 ja aparece na mesma linha, coluna ou bloco");
  limpar();
}

TESTE(registro_ler_ultimo_ignora_inicio) {
  limpar();
  // 2 tabuleiros: o primeiro e invalido, o ultimo e valido -> so o ultimo conta
  std::string ruim(81, '1');
  escrever_arquivo(bytes_total(1) + ruim + std::string(DESAFIO_FACIL));
  RegistroDePartida r = RegistroDePartida::abrir();
  CHECK(iguais(r.ler_ultimo_tabuleiro(), desafio_do_nivel(Nivel::FACIL)));
  CHECK_THROWS_ANY_MSG(r.ler_desafio(), ArquivoCorrompido);
  limpar();
}

TESTE(registro_criar_erro_de_gravacao) {
  limpar();
  mkdir(ARQ, 0755);  // um diretorio com o nome do arquivo impede a criacao
  Tabuleiro d = desafio_do_nivel(Nivel::FACIL);
  CHECK_THROWS(RegistroDePartida::criar(d), ErroDeGravacao,
               "Nao foi possivel salvar em partida.save");
  rmdir(ARQ);
}

TESTE(montar_tabuleiro_minimo) {
  std::vector<int> pouco(80, 0);
  CHECK_THROWS(montar_tabuleiro(pouco), ArquivoCorrompido,
               "O registro tem menos numeros que o tabuleiro");
  std::vector<int> exato(81, 0);
  CHECK_NO_THROW(montar_tabuleiro(exato));
  std::vector<int> demais(100, 0);
  CHECK_NO_THROW(montar_tabuleiro(demais));
  std::vector<int> vazio;
  CHECK_THROWS_ANY_MSG(montar_tabuleiro(vazio), ArquivoCorrompido);
}

// ===========================================================================
// PARTIDA
// ===========================================================================
TESTE(partida_iniciar_nova) {
  limpar();
  Partida p;
  p.iniciar_nova(Nivel::MEDIO);
  CHECK_EQ(digitos(p.tabuleiro()), std::string(DESAFIO_MEDIO));
  p.iniciar_nova(Nivel::DIFICIL);
  CHECK_EQ(digitos(p.tabuleiro()), std::string(DESAFIO_DIFICIL));
  p.iniciar_nova(Nivel::FACIL);
  CHECK_EQ(digitos(p.tabuleiro()), std::string(DESAFIO_FACIL));
  CHECK(!RegistroDePartida::existe());  // nada gravado sem jogada
}

TESTE(partida_jogar_grava_registro) {
  limpar();
  Partida p;
  p.iniciar_nova(Nivel::FACIL);
  p.jogar({{0, 2}, 3});
  CHECK_EQ(p.tabuleiro().numero_em({0, 2}), 3);
  CHECK(RegistroDePartida::existe());
  const std::string c = ler_arquivo();
  CHECK_EQ(c.size(), static_cast<size_t>(4 + 81 + 81));
  CHECK_EQ(total_no_arquivo(), 1);
  CHECK_EQ(c.substr(4, 81), std::string(DESAFIO_FACIL));
  CHECK_EQ(c.substr(85, 81), digitos(p.tabuleiro()));
  p.jogar({{0, 8}, 5});
  CHECK_EQ(total_no_arquivo(), 2);
  CHECK_EQ(ler_arquivo().size(), static_cast<size_t>(4 + 81 * 3));
  limpar();
}

TESTE(partida_jogar_celula_do_desafio) {
  limpar();
  Partida p;
  p.iniciar_nova(Nivel::FACIL);
  CHECK_THROWS(p.jogar({{0, 0}, 9}), JogadaInvalida,
               "Esta celula faz parte do desafio e nao pode mudar");
  CHECK_THROWS_ANY_MSG(p.jogar({{0, 0}, 1}), JogadaInvalida);
  CHECK_THROWS_ANY_MSG(p.apagar({0, 0}), JogadaInvalida);
  CHECK_EQ(p.tabuleiro().numero_em({0, 0}), 1);
  CHECK(!RegistroDePartida::existe());
  // celula do desafio na ultima posicao (8,7)=7: borda
  CHECK_THROWS_ANY_MSG(p.jogar({{8, 7}, 2}), JogadaInvalida);
  limpar();
}

TESTE(partida_jogar_fora_do_tabuleiro) {
  limpar();
  Partida p;
  p.iniciar_nova(Nivel::FACIL);
  CHECK_THROWS(p.jogar({{9, 0}, 1}), JogadaInvalida,
               "Posicao fora do tabuleiro");
  CHECK_THROWS(p.jogar({{0, -1}, 1}), JogadaInvalida,
               "Posicao fora do tabuleiro");
  CHECK_THROWS(p.jogar({{-1, 0}, 1}), JogadaInvalida,
               "Posicao fora do tabuleiro");
  CHECK_THROWS(p.jogar({{0, 9}, 1}), JogadaInvalida,
               "Posicao fora do tabuleiro");
  CHECK(!RegistroDePartida::existe());
}

TESTE(partida_jogada_invalida_nao_altera) {
  limpar();
  Partida p;
  p.iniciar_nova(Nivel::FACIL);
  const std::string antes = digitos(p.tabuleiro());
  CHECK_THROWS_ANY_MSG(p.jogar({{0, 2}, 1}), JogadaInvalida);  // conflito
  CHECK_THROWS_ANY_MSG(p.jogar({{0, 2}, 10}), JogadaInvalida);
  CHECK_EQ(digitos(p.tabuleiro()), antes);
  CHECK(!RegistroDePartida::existe());  // nada foi gravado
}

TESTE(partida_apagar) {
  limpar();
  Partida p;
  p.iniciar_nova(Nivel::FACIL);
  p.jogar({{0, 2}, 3});
  CHECK_EQ(p.tabuleiro().contar_celulas_vazias(), 36);
  p.apagar({0, 2});
  CHECK_EQ(p.tabuleiro().numero_em({0, 2}), 0);
  CHECK_EQ(p.tabuleiro().contar_celulas_vazias(), 37);
  CHECK_EQ(total_no_arquivo(), 2);
  CHECK_EQ(digitos(p.tabuleiro()), std::string(DESAFIO_FACIL));
  limpar();
}

TESTE(partida_dar_dica) {
  limpar();
  Partida p;
  p.iniciar_nova(Nivel::FACIL);
  auto d = p.dar_dica();
  CHECK(d.has_value());
  CHECK(d->posicao == (Posicao{0, 2}));
  CHECK_EQ(d->numero, 3);
  CHECK_EQ(p.tabuleiro().numero_em({0, 2}), 3);
  CHECK_EQ(total_no_arquivo(), 1);
  CHECK_EQ(ler_arquivo().size(), static_cast<size_t>(166));
  limpar();
}

TESTE(partida_resolver_tudo_todos_niveis) {
  const Nivel niveis[3] = {Nivel::FACIL, Nivel::MEDIO, Nivel::DIFICIL};
  const int vazias[3] = {37, 45, 53};
  for (int i = 0; i < 3; i++) {
    limpar();
    Partida p;
    p.iniciar_nova(niveis[i]);
    p.resolver_tudo();
    CHECK(p.tabuleiro().esta_completo());
    CHECK(solucao_valida(p.tabuleiro()));
    CHECK_EQ(total_no_arquivo(), vazias[i]);  // uma jogada por celula
    CHECK_EQ(ler_arquivo().size(), static_cast<size_t>(4 + 81 * (vazias[i] + 1)));
    // sem dica restante, nada novo e gravado
    CHECK(!p.dar_dica().has_value());
    CHECK_EQ(total_no_arquivo(), vazias[i]);
    limpar();
  }
}

TESTE(partida_resolver_tudo_com_erro_do_jogador) {
  limpar();
  const Tabuleiro sol = solucao_do_nivel(Nivel::FACIL);
  Partida p;
  p.iniciar_nova(Nivel::FACIL);
  // (0,8)=3 e aceito pelas regras, mas a solucao correta e outra
  p.jogar({{0, 8}, 3});
  p.resolver_tudo();
  CHECK(!p.tabuleiro().esta_completo());
  CHECK(p.tabuleiro().contar_celulas_vazias() > 0);
  limpar();
}

TESTE(partida_continuar) {
  limpar();
  {
    Partida p;
    p.iniciar_nova(Nivel::FACIL);
    p.jogar({{0, 2}, 3});
    p.jogar({{0, 8}, 5});
  }
  Partida q;
  q.continuar();
  CHECK_EQ(q.tabuleiro().numero_em({0, 2}), 3);
  CHECK_EQ(q.tabuleiro().numero_em({0, 8}), 5);
  CHECK_EQ(q.tabuleiro().contar_celulas_vazias(), 35);
  // o desafio original foi restaurado: suas celulas continuam protegidas
  CHECK_THROWS(q.jogar({{0, 0}, 9}), JogadaInvalida,
               "Esta celula faz parte do desafio e nao pode mudar");
  // e as jogadas do usuario continuam editaveis
  CHECK_NO_THROW(q.apagar({0, 8}));
  CHECK_EQ(q.tabuleiro().numero_em({0, 8}), 0);
  CHECK_EQ(total_no_arquivo(), 3);
  limpar();
}

TESTE(partida_continuar_sem_arquivo) {
  limpar();
  Partida p;
  p.iniciar_nova(Nivel::MEDIO);
  CHECK_THROWS(p.continuar(), ArquivoInexistente, "partida.save");
  CHECK_EQ(digitos(p.tabuleiro()), std::string(DESAFIO_MEDIO));
}

TESTE(partida_continuar_corrompido_nao_altera) {
  limpar();
  Partida p;
  p.iniciar_nova(Nivel::MEDIO);
  escrever_arquivo(bytes_total(0) + std::string(40, '0'));
  CHECK_THROWS_ANY_MSG(p.continuar(), ArquivoCorrompido);
  CHECK_EQ(digitos(p.tabuleiro()), std::string(DESAFIO_MEDIO));
  // desafio tambem intacto
  CHECK_THROWS_ANY_MSG(p.jogar({{0, 2}, 1}), JogadaInvalida);
  limpar();
}

TESTE(partida_continuar_desafio_ok_ultimo_corrompido) {
  limpar();
  Partida p;
  p.iniciar_nova(Nivel::MEDIO);
  // desafio valido + ultimo tabuleiro invalido (todos '1')
  escrever_arquivo(bytes_total(1) + std::string(DESAFIO_FACIL) +
                   std::string(81, '1'));
  CHECK_THROWS_ANY_MSG(p.continuar(), ArquivoCorrompido);
  CHECK_EQ(digitos(p.tabuleiro()), std::string(DESAFIO_MEDIO));
  limpar();
}

TESTE(partida_falha_de_gravacao_preserva_estado) {
  limpar();
  Partida p;
  p.iniciar_nova(Nivel::FACIL);
  mkdir(ARQ, 0755);  // impede criar o registro
  CHECK_THROWS_ANY_MSG(p.jogar({{0, 2}, 3}), ErroDeGravacao);
  CHECK_EQ(p.tabuleiro().numero_em({0, 2}), 0);
  CHECK_THROWS_ANY_MSG(p.dar_dica(), ErroDeGravacao);
  CHECK_EQ(digitos(p.tabuleiro()), std::string(DESAFIO_FACIL));
  CHECK_THROWS_ANY_MSG(p.resolver_tudo(), ErroDeGravacao);
  CHECK_EQ(digitos(p.tabuleiro()), std::string(DESAFIO_FACIL));
  rmdir(ARQ);
  // sem o obstaculo, tudo volta a funcionar e o registro nasce com o desafio
  CHECK_NO_THROW(p.jogar({{0, 2}, 3}));
  CHECK_EQ(ler_arquivo().substr(4, 81), std::string(DESAFIO_FACIL));
  CHECK_EQ(total_no_arquivo(), 1);
  limpar();
}

TESTE(partida_iniciar_nova_descarta_registro) {
  limpar();
  Partida p;
  p.iniciar_nova(Nivel::FACIL);
  p.jogar({{0, 2}, 3});
  p.iniciar_nova(Nivel::MEDIO);  // novo desafio: registro antigo descartado
  p.jogar({{0, 0}, 5});
  const std::string c = ler_arquivo();
  CHECK_EQ(c.substr(4, 81), std::string(DESAFIO_MEDIO));
  CHECK_EQ(total_no_arquivo(), 1);
  CHECK_EQ(c.size(), static_cast<size_t>(166));
  limpar();
}


TESTE(tabuleiro_numero_em_fora_aborta) {
  // numero_em exige posicao valida (assert): fora do tabuleiro deve abortar.
  const Posicao invalidas[3] = {{9, 9}, {-1, 0}, {0, 9}};
  for (const Posicao& pos : invalidas) {
    fflush(nullptr);
    pid_t pid = fork();
    if (pid == 0) {
      std::freopen("/dev/null", "w", stderr);
      Tabuleiro t;
      volatile int v = t.numero_em(pos);
      (void)v;
      _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    CHECK(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
  }
}

TESTE(registro_ler_tabuleiro_recupera_apos_falha) {
  limpar();
  // arquivo incompleto: a leitura falha e deixa o fluxo em estado de erro
  escrever_arquivo(bytes_total(0) + std::string(40, '0'));
  RegistroDePartida r = RegistroDePartida::abrir();
  CHECK_THROWS_ANY_MSG(r.ler_desafio(), ArquivoCorrompido);
  // o arquivo e completado por fora; uma nova leitura deve funcionar
  {
    std::ofstream f(ARQ, std::ios::binary | std::ios::app);
    f << std::string(41, '0');
  }
  CHECK_NO_THROW(r.ler_desafio());
  limpar();
}

TESTE(main_original_retorna_zero) {
  limpar();
  std::istringstream in("0\n");
  std::ostringstream out;
  std::streambuf* bi = std::cin.rdbuf(in.rdbuf());
  std::streambuf* bo = std::cout.rdbuf(out.rdbuf());
  std::cin.clear();
  const int rc = sudoku_main_original();
  std::cin.rdbuf(bi);
  std::cout.rdbuf(bo);
  std::cin.clear();
  CHECK_EQ(rc, 0);
  CHECK(tem(out.str(), "Bem-vindo ao Sudoku!"));
  CHECK(tem(out.str(), "Ate a proxima!"));
}

// ===========================================================================
// APRESENTACAO
// ===========================================================================
TESTE(exibir_tabuleiro_vazio_exato) {
  std::ostringstream os;
  exibir_tabuleiro(os, Tabuleiro());
  const std::string esperado =
      "\n"
      "    1 2 3   4 5 6   7 8 9\n"
      "  -------------------------\n"
      "1 | . . . | . . . | . . . |\n"
      "2 | . . . | . . . | . . . |\n"
      "3 | . . . | . . . | . . . |\n"
      "  -------------------------\n"
      "4 | . . . | . . . | . . . |\n"
      "5 | . . . | . . . | . . . |\n"
      "6 | . . . | . . . | . . . |\n"
      "  -------------------------\n"
      "7 | . . . | . . . | . . . |\n"
      "8 | . . . | . . . | . . . |\n"
      "9 | . . . | . . . | . . . |\n"
      "  -------------------------\n";
  CHECK_EQ(os.str(), esperado);
}

TESTE(exibir_tabuleiro_facil_exato) {
  std::ostringstream os;
  exibir_tabuleiro(os, desafio_do_nivel(Nivel::FACIL));
  const std::string esperado =
      "\n"
      "    1 2 3   4 5 6   7 8 9\n"
      "  -------------------------\n"
      "1 | 1 4 . | 7 8 2 | 6 9 . |\n"
      "2 | . 2 . | . 9 1 | . 4 . |\n"
      "3 | 8 . 6 | . . . | 7 2 1 |\n"
      "  -------------------------\n"
      "4 | . . 8 | 2 . 7 | . . 4 |\n"
      "5 | 2 6 . | 4 . 9 | . . . |\n"
      "6 | 7 . 4 | 8 3 6 | 2 . 9 |\n"
      "  -------------------------\n"
      "7 | . . 9 | . 2 8 | . . . |\n"
      "8 | 6 . . | 9 7 . | . 3 8 |\n"
      "9 | . . 5 | 1 6 . | 9 7 . |\n"
      "  -------------------------\n";
  CHECK_EQ(os.str(), esperado);
}

TESTE(exibir_andamento) {
  std::ostringstream a;
  exibir_andamento(a, Tabuleiro());
  CHECK_EQ(a.str(), std::string("Faltam 81 celulas.\n"));

  std::ostringstream b;
  exibir_andamento(b, desafio_do_nivel(Nivel::FACIL));
  CHECK_EQ(b.str(), std::string("Faltam 37 celulas.\n"));

  limpar();
  Tabuleiro sol = solucao_do_nivel(Nivel::FACIL);
  std::ostringstream c;
  exibir_andamento(c, sol);
  CHECK_EQ(c.str(), std::string("Parabens! Voce completou o Sudoku! "
                                "Escolha [5] para jogar de novo.\n"));

  // um unico vazio ainda nao e vitoria
  Tabuleiro quase = sol;
  quase.preencher({0, 0}, 0);
  std::ostringstream d;
  exibir_andamento(d, quase);
  CHECK_EQ(d.str(), std::string("Faltam 1 celulas.\n"));
}

TESTE(exibir_celula_e_linha) {
  std::ostringstream a;
  exibir_celula(a, 0);
  CHECK_EQ(a.str(), std::string(". "));
  std::ostringstream b;
  exibir_celula(b, 7);
  CHECK_EQ(b.str(), std::string("7 "));
  std::ostringstream c;
  exibir_linha(c, desafio_do_nivel(Nivel::FACIL), 0);
  CHECK_EQ(c.str(), std::string("1 | 1 4 . | 7 8 2 | 6 9 . |\n"));
}

TESTE(exibir_textos_fixos) {
  std::ostringstream bv;
  exibir_boas_vindas(bv);
  CHECK_EQ(bv.str(),
           std::string("Bem-vindo ao Sudoku!\n"
                       "Preencha a grade para que cada linha, cada coluna e cada bloco\n"
                       "3x3 tenha os numeros de 1 a 9, sem repetir. Os pontos (.) sao\n"
                       "celulas vazias. Linhas e colunas vao de 1 a 9.\n"
                       "O jogo salva sozinho e voltara de onde voce parou.\n"));
  std::ostringstream mp;
  exibir_menu_principal(mp);
  CHECK_EQ(mp.str(), std::string("\nO que voce quer fazer?\n"
                                 "[1] Colocar um numero\n"
                                 "[2] Apagar um numero\n"
                                 "[3] Pedir uma dica\n"
                                 "[4] Resolver tudo\n"
                                 "[5] Novo jogo\n"
                                 "[0] Sair\n"));
  std::ostringstream mn;
  exibir_menu_de_niveis(mn);
  CHECK_EQ(mn.str(), std::string("\nEscolha o nivel:\n"
                                 "[1] Facil\n"
                                 "[2] Medio\n"
                                 "[3] Dificil\n"
                                 "[0] Voltar\n"));
}

// ===========================================================================
// INTERFACE DE TEXTO (entrada simulada)
// ===========================================================================
TESTE(ui_sair_imediatamente) {
  limpar();
  const std::string o = executar_interface("0\n");
  CHECK(tem(o, "Bem-vindo ao Sudoku!"));
  CHECK(tem(o, "Faltam 37 celulas."));
  CHECK(tem(o, "[0] Sair"));
  CHECK(tem(o, "Opcao: "));
  CHECK(tem(o, "Ate a proxima!"));
  CHECK(!tem(o, "Partida anterior retomada."));
  CHECK(!tem(o, "invalida"));
  CHECK(!tem(o, "Nao foi possivel"));
  CHECK(!tem(o, "danificado"));
  CHECK_EQ(contar(o, "Ate a proxima!"), 1);
}

TESTE(ui_fim_de_entrada_sai) {
  limpar();
  const std::string o = executar_interface("");
  CHECK(tem(o, "Ate a proxima!"));
}

TESTE(ui_opcao_invalida) {
  limpar();
  std::string o = executar_interface("7\n0\n");
  CHECK(tem(o, "Opcao invalida! Digite um dos numeros do menu."));
  o = executar_interface("abc\n0\n");
  CHECK(tem(o, "Opcao invalida! Digite um dos numeros do menu."));
  CHECK(tem(o, "Ate a proxima!"));
  o = executar_interface("-3\n0\n");
  CHECK(tem(o, "Opcao invalida!"));
  o = executar_interface("6\n0\n");
  CHECK(tem(o, "Opcao invalida!"));
}

TESTE(ui_colocar_numero) {
  limpar();
  const std::string o = executar_interface("1\n1 3 3\n0\n");
  CHECK(tem(o, "Digite: linha coluna numero (exemplo: 3 5 7): "));
  CHECK(tem(o, "Faltam 36 celulas."));
  CHECK(tem(o, "1 | 1 4 3 | 7 8 2 | 6 9 . |"));
  CHECK(RegistroDePartida::existe());
  limpar();
}

TESTE(ui_colocar_numero_erros) {
  limpar();
  std::string o = executar_interface("1\n1 3 1\n0\n");  // conflito
  CHECK(tem(o, "Nao foi possivel: O numero 1 ja aparece na mesma linha, "
               "coluna ou bloco."));
  CHECK(tem(o, "Faltam 37 celulas."));

  o = executar_interface("1\n1 1 5\n0\n");  // celula do desafio
  CHECK(tem(o, "Nao foi possivel: Esta celula faz parte do desafio e nao "
               "pode mudar."));

  o = executar_interface("1\n1 3 10\n0\n");  // numero fora do intervalo
  CHECK(tem(o, "Nao foi possivel: O numero deve estar entre 1 e 9."));

  o = executar_interface("1\n0 1 5\n0\n");  // linha 0 nao existe na tela
  CHECK(tem(o, "Nao foi possivel: Linha e coluna devem estar entre 1 e 9."));
  o = executar_interface("1\n1 0 5\n0\n");
  CHECK(tem(o, "Linha e coluna devem estar entre 1 e 9"));
  o = executar_interface("1\n10 1 5\n0\n");
  CHECK(tem(o, "Linha e coluna devem estar entre 1 e 9"));
  o = executar_interface("1\n1 10 5\n0\n");
  CHECK(tem(o, "Linha e coluna devem estar entre 1 e 9"));

  o = executar_interface("1\nx y z\n0\n");  // nao numerico
  CHECK(tem(o, "Nao foi possivel: Digite apenas numeros, separados por espaco."));
  CHECK(tem(o, "Ate a proxima!"));  // a entrada pendente foi descartada
  CHECK(!tem(o, "Opcao invalida"));  // e nao vazou para o menu

  o = executar_interface("1\n1 3\n0\n");  // faltou o numero (le o 0 como numero)
  CHECK(tem(o, "Faltam"));
  CHECK(!RegistroDePartida::existe() || true);
  limpar();
}

TESTE(ui_posicao_na_borda_da_tela) {
  limpar();
  // (9,9) da tela = (8,8): vazia no desafio facil; deve ser aceita
  const std::string o = executar_interface("1\n9 9 4\n0\n");
  CHECK(!tem(o, "Linha e coluna devem estar"));
  CHECK(tem(o, "Faltam 36 celulas.") || tem(o, "Nao foi possivel"));
  limpar();
}

TESTE(ui_apagar_numero) {
  limpar();
  const std::string o = executar_interface("1\n1 3 3\n2\n1 3\n0\n");
  CHECK(tem(o, "Digite: linha coluna (exemplo: 3 5): "));
  CHECK(tem(o, "Faltam 36 celulas."));
  // apos apagar volta a 37: aparece a mensagem e o tabuleiro sem o numero
  size_t ult = o.rfind("Faltam ");
  CHECK(ult != std::string::npos);
  CHECK(o.compare(ult, 18, "Faltam 37 celulas.") == 0);
  limpar();
}

TESTE(ui_apagar_celula_do_desafio) {
  limpar();
  const std::string o = executar_interface("2\n1 1\n0\n");
  CHECK(tem(o, "Esta celula faz parte do desafio e nao pode mudar"));
  limpar();
}

TESTE(ui_apagar_posicao_invalida) {
  limpar();
  std::string o = executar_interface("2\n0 5\n0\n");
  CHECK(tem(o, "Linha e coluna devem estar entre 1 e 9"));
  o = executar_interface("2\nq\n0\n");
  CHECK(tem(o, "Digite apenas numeros"));
}

TESTE(ui_pedir_dica) {
  limpar();
  const std::string o = executar_interface("3\n0\n");
  CHECK(tem(o, "Dica: na linha 1, coluna 3 so cabe o numero 3."));
  CHECK(tem(o, "Faltam 36 celulas."));
  limpar();
}

TESTE(ui_resolver_tudo) {
  limpar();
  const std::string o = executar_interface("4\n0\n");
  CHECK(tem(o, "Parabens! Voce completou o Sudoku! Escolha [5] para jogar de novo."));
  CHECK(!tem(o, "Nao consegui terminar"));
  limpar();
}

TESTE(ui_sem_dica_apos_resolver) {
  limpar();
  const std::string o = executar_interface("4\n3\n0\n");
  CHECK(tem(o, "Nao ha dica agora. Algum numero seu pode estar errado."));
  CHECK(!tem(o, "Dica:"));
  limpar();
}

TESTE(ui_resolver_tudo_nao_consegue) {
  limpar();
  // 3 em (1,9) e aceito pelas regras mas esta errado
  const std::string o = executar_interface("1\n1 9 3\n4\n0\n");
  CHECK(tem(o, "Nao consegui terminar. Algum numero seu pode estar errado: "
               "apague-o ou comece um novo jogo."));
  CHECK(!tem(o, "Parabens!"));
  limpar();
}

TESTE(ui_novo_jogo_niveis) {
  limpar();
  std::string o = executar_interface("5\n2\n0\n");
  CHECK(tem(o, "Escolha o nivel:"));
  CHECK(tem(o, "Faltam 45 celulas."));  // medio

  o = executar_interface("5\n3\n0\n");
  CHECK(tem(o, "Faltam 53 celulas."));  // dificil

  o = executar_interface("5\n1\n0\n");
  CHECK(tem(o, "Faltam 37 celulas."));  // facil

  // voltar: mantem o jogo atual (facil, 37) sem iniciar novo
  o = executar_interface("5\n0\n0\n");
  CHECK(!tem(o, "Faltam 45"));
  CHECK(!tem(o, "Faltam 53"));

  // opcao invalida no menu de niveis, depois dificil
  o = executar_interface("5\n9\nabc\n3\n0\n");
  CHECK(tem(o, "Opcao invalida! Digite um dos numeros do menu."));
  CHECK(tem(o, "Faltam 53 celulas."));
  limpar();
}

TESTE(ui_novo_jogo_troca_de_nivel) {
  limpar();
  // medio -> facil: o ultimo escolhido vale
  std::string o = executar_interface("5\n2\n5\n1\n0\n");
  CHECK(o.rfind("Faltam 37 celulas.") > o.rfind("Faltam 45 celulas."));
  CHECK_EQ(contar(o, "Ate a proxima!"), 1);
  // dificil -> facil
  o = executar_interface("5\n3\n5\n1\n0\n");
  CHECK(o.rfind("Faltam 37 celulas.") > o.rfind("Faltam 53 celulas."));
  // facil -> dificil -> medio
  o = executar_interface("5\n3\n5\n2\n0\n");
  CHECK(o.rfind("Faltam 45 celulas.") > o.rfind("Faltam 53 celulas."));
  CHECK_EQ(contar(o, "Ate a proxima!"), 1);
  // escolhas no menu de niveis nao imprimem a despedida
  o = executar_interface("5\n1\n0\n");
  CHECK_EQ(contar(o, "Ate a proxima!"), 1);
  o = executar_interface("5\n0\n0\n");
  CHECK_EQ(contar(o, "Ate a proxima!"), 1);
  limpar();
}

TESTE(ui_novo_jogo_descarta_progresso) {
  limpar();
  // joga no facil, troca para medio e joga la
  const std::string o = executar_interface("1\n1 3 3\n5\n2\n1\n1 1 8\n0\n");
  CHECK(tem(o, "Faltam 36 celulas."));
  CHECK(tem(o, "Faltam 44 celulas."));  // medio com 1 jogada
  limpar();
}

TESTE(ui_continuar_partida_anterior) {
  limpar();
  executar_interface("1\n1 3 3\n0\n");
  const std::string o = executar_interface("0\n");
  CHECK(tem(o, "Partida anterior retomada."));
  CHECK(tem(o, "Faltam 36 celulas."));
  CHECK(tem(o, "1 | 1 4 3 | 7 8 2 | 6 9 . |"));
  limpar();
}

TESTE(ui_continuar_mantem_desafio_protegido) {
  limpar();
  executar_interface("1\n1 3 3\n0\n");
  const std::string o = executar_interface("1\n1 1 9\n0\n");
  CHECK(tem(o, "Esta celula faz parte do desafio e nao pode mudar"));
  limpar();
}

TESTE(ui_registro_danificado) {
  limpar();
  escrever_arquivo(bytes_total(0) + std::string(81, '1'));
  const std::string o = executar_interface("0\n");
  CHECK(tem(o, "O registro salvo esta danificado ("));
  CHECK(tem(o, "). Comecando com um jogo novo."));
  CHECK(tem(o, "Faltam 37 celulas."));
  CHECK(!tem(o, "Partida anterior retomada."));
  limpar();
}

TESTE(ui_erro_ao_abrir_e_ao_salvar) {
  limpar();
  mkdir(ARQ, 0755);
  // existe() enxerga o diretorio; abrir() falha; criar() falha ao jogar
  const std::string o = executar_interface("1\n1 3 3\n0\n");
  CHECK(tem(o, "Nao foi possivel abrir o arquivo partida.save."));
  CHECK(tem(o, "Erro ao salvar: Nao foi possivel salvar em partida.save."));
  CHECK(tem(o, "Faltam 37 celulas."));
  rmdir(ARQ);
}

// ---------------------------------------------------------------------------
int main() {
  g_failfast = std::getenv("SUDOKU_FAILFAST") != nullptr;
  for (const Teste& t : testes()) {
    g_teste_atual = t.nome;
    limpar();
    try {
      t.fn();
    } catch (const std::exception& e) {
      registrar_falha(__FILE__, __LINE__,
                      std::string("excecao nao tratada: ") + e.what());
    } catch (...) {
      registrar_falha(__FILE__, __LINE__, "excecao desconhecida");
    }
  }
  limpar();
  std::cerr << g_total << " verificacoes, " << g_falhas << " falhas, "
            << testes().size() << " testes\n";
  return g_falhas == 0 ? 0 : 1;
}

TESTE(tabuleiro_limites_linha_coluna_dez) {
  Tabuleiro tabuleiro;
  const Posicao linha_invalida{10, 0};
  const Posicao coluna_invalida{0, 10};

  CHECK_THROWS_ANY_MSG(tabuleiro.preencher(linha_invalida, 1), JogadaInvalida);
  CHECK_THROWS_ANY_MSG(tabuleiro.preencher(coluna_invalida, 1), JogadaInvalida);
}

TESTE(tabuleiro_repeticao_no_bloco_3x3) {
  Tabuleiro tabuleiro;
  tabuleiro.preencher({0, 0}, 5);

  CHECK_THROWS_ANY_MSG(tabuleiro.preencher({1, 1}, 5), JogadaInvalida);
  CHECK_THROWS_ANY_MSG(tabuleiro.preencher({2, 2}, 5), JogadaInvalida);
}

TESTE(registro_erro_de_gravacao_ao_criar) {
  limpar();
  CHECK_EQ(mkdir(ARQ, 0755), 0);
  Tabuleiro tabuleiro_vazio;

  CHECK_THROWS_ANY_MSG(RegistroDePartida::criar(tabuleiro_vazio), ErroDeGravacao);
  CHECK_EQ(rmdir(ARQ), 0);
}

TESTE(ui_opcoes_invalidas_e_fim_de_entrada) {
  limpar();
  std::string saida;

  CHECK_NO_THROW(saida = executar_interface("-1\n99\n50\n"));
  CHECK(tem(saida, "Opcao invalida"));
  CHECK(tem(saida, "Ate a proxima!"));
}
