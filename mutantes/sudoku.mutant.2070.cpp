// Sudoku em modo texto.
//
// GLOSSARIO (linguagem ubiqua): as palavras que o jogador usa sao as mesmas
// que aparecem no codigo, com um nome para cada conceito.
//   Tabuleiro : grade 9x9 que contem as celulas.
//   Celula    : uma posicao do tabuleiro; guarda um numero ou esta vazia.
//   Posicao   : par (linha, coluna). No codigo vai de 0 a 8; na tela, de 1 a 9.
//   Bloco     : regiao 3x3 do tabuleiro.
//   Numero    : de 1 a 9; e o que o jogador coloca em uma celula.
//   Jogada    : colocar (ou apagar) um numero em uma celula.
//   Dica      : preenchimento de uma celula que so admite um numero.
//   Desafio   : tabuleiro inicial; seus numeros nao podem ser alterados.
//   Nivel     : dificuldade do desafio (facil, medio, dificil).
//   Partida   : tabuleiro em jogo junto com o seu registro.
//   Registro  : arquivo que guarda o historico da partida automaticamente.

#include <array>
#include <cassert>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// ===========================================================================
// Excecoes: representam o fluxo excepcional
// ===========================================================================

class JogadaInvalida : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class ArquivoInexistente : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class ArquivoCorrompido : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class ErroDeGravacao : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

// ===========================================================================
// Dominio: regras do Sudoku (nao sabe nada sobre tela ou arquivos)
// ===========================================================================

struct Posicao {
  int linha;
  int coluna;

  bool operator==(const Posicao& outra) const {
    return linha == outra.linha && coluna == outra.coluna;
  }
};

struct Jogada {
  Posicao posicao;
  int numero;
};

class Tabuleiro {
 public:
  static constexpr int TAMANHO = 9;
  static constexpr int TAMANHO_BLOCO = 3;
  static constexpr int TOTAL_CELULAS = TAMANHO * TAMANHO;
  static constexpr int CELULA_VAZIA = 0;
  static constexpr int NUMERO_MINIMO = 1;
  static constexpr int NUMERO_MAXIMO = 9;

  static bool existe(Posicao posicao);
  static Posicao posicao_da_ordem(int ordem) {
    return {ordem / TAMANHO, ordem % TAMANHO};
  }

  int numero_em(Posicao posicao) const;
  int contar_celulas_vazias() const;
  bool esta_completo() const { return contar_celulas_vazias() == 0; }
  void preencher(Posicao posicao, int numero);
  std::optional<Jogada> aplicar_dica();

 private:
  static bool esta_no_intervalo(int numero);
  static int ordem_da(Posicao posicao);
  static int bloco_de(Posicao posicao);
  static bool compartilham_regiao(Posicao a, Posicao b);

  bool pode_receber(Posicao posicao, int numero) const;
  std::vector<int> numeros_possiveis(Posicao posicao) const;

  std::array<int, TOTAL_CELULAS> celulas_{};
};

bool Tabuleiro::existe(Posicao posicao) {
  return posicao.linha >= 0 && posicao.linha < TAMANHO &&
         posicao.coluna >= 0 && posicao.coluna < TAMANHO;
}

bool Tabuleiro::esta_no_intervalo(int numero) {
  return numero >= NUMERO_MINIMO && numero <= NUMERO_MAXIMO;
}

int Tabuleiro::ordem_da(Posicao posicao) {
  return posicao.linha * TAMANHO + posicao.coluna;
}

int Tabuleiro::bloco_de(Posicao posicao) {
  constexpr int BLOCOS_POR_LINHA = TAMANHO / TAMANHO_BLOCO;
  return posicao.linha / TAMANHO_BLOCO * BLOCOS_POR_LINHA +
         posicao.coluna / TAMANHO_BLOCO;
}

// Duas posicoes compartilham regiao se estao na mesma linha, coluna ou bloco.
bool Tabuleiro::compartilham_regiao(Posicao a, Posicao b) {
  return a.linha == b.linha || a.coluna == b.coluna ||
         bloco_de(a) == bloco_de(b);
}

int Tabuleiro::numero_em(Posicao posicao) const {
  assert(existe(posicao));
  return celulas_[ordem_da(posicao)];
}

int Tabuleiro::contar_celulas_vazias() const {
  int quantidade = 0;
  for (int numero : celulas_) {
    if (numero == CELULA_VAZIA) {
      quantidade++;
    }
  }
  return quantidade;
}

// Pre-condicao: a posicao existe e o numero esta no intervalo permitido.
bool Tabuleiro::pode_receber(Posicao posicao, int numero) const {
  for (int ordem = 0; ordem < TOTAL_CELULAS; ordem++) {
    const Posicao outra = posicao_da_ordem(ordem);
    if (outra == posicao || !compartilham_regiao(posicao, outra)) {
      continue;
    }
    if (numero_em(outra) == numero) {
      return false;
    }
  }
  return true;
}

// Preencher com CELULA_VAZIA apaga o numero da celula.
void Tabuleiro::preencher(Posicao posicao, int numero) {
  if (!existe(posicao)) {
    throw JogadaInvalida("Posicao fora do tabuleiro");
  }
  if (numero != CELULA_VAZIA && !esta_no_intervalo(numero)) {
    throw JogadaInvalida("O numero deve estar entre " +
                         std::to_string(NUMERO_MINIMO) + " e " +
                         std::to_string(NUMERO_MAXIMO));
  }
  if (numero != CELULA_VAZIA && !pode_receber(posicao, numero)) {
    throw JogadaInvalida("O numero " + std::to_string(numero) +
                         " ja aparece na mesma linha, coluna ou bloco");
  }
  celulas_[ordem_da(posicao)] = numero;
}

std::vector<int> Tabuleiro::numeros_possiveis(Posicao posicao) const {
  std::vector<int> numeros;
  for (int numero = NUMERO_MINIMO; numero <= NUMERO_MAXIMO; numero++) {
    if (pode_receber(posicao, numero)) {
      numeros.push_back(numero);
    }
  }
  return numeros;
}

// Preenche a primeira celula vazia que so admite um numero e devolve a jogada
// feita. Sem dica disponivel devolve std::nullopt (resultado normal).
std::optional<Jogada> Tabuleiro::aplicar_dica() {
  for (int ordem = 0; ordem < TOTAL_CELULAS; ordem++) {
    if (celulas_[ordem] != CELULA_VAZIA) {
      continue;
    }
    const Posicao posicao = posicao_da_ordem(ordem);
    const std::vector<int> numeros = numeros_possiveis(posicao);
    if (numeros.size() != 1) {
      continue;
    }
    celulas_[ordem] = numeros.front();
    return Jogada{posicao, numeros.front()};
  }
  return std::nullopt;
}

// ===========================================================================
// Desafios embutidos (0 = celula vazia). Todos tem solucao unica que pode ser
// encontrada apenas com dicas.
// ===========================================================================

enum class Nivel { FACIL, MEDIO, DIFICIL };

constexpr const char* DESAFIOS[] = {
    // FACIL
    "140782690"
    "020091040"
    "806000721"
    "008207004"
    "260409000"
    "704836209"
    "009028000"
    "600970038"
    "005160970",
    // MEDIO
    "007300020"
    "004007051"
    "900104367"
    "600002485"
    "000506209"
    "020009036"
    "000000003"
    "036080740"
    "401003008",
    // DIFICIL
    "208300094"
    "401000200"
    "000002000"
    "900010040"
    "304065070"
    "610098000"
    "093000080"
    "006000000"
    "000000123"};

Tabuleiro desafio_do_nivel(Nivel nivel) {
  const std::string texto = DESAFIOS[static_cast<int>(nivel)];

  Tabuleiro desafio;
  for (int ordem = 0; ordem < Tabuleiro::TOTAL_CELULAS; ordem++) {
    desafio.preencher(Tabuleiro::posicao_da_ordem(ordem), texto[ordem] - '0');
  }
  return desafio;
}

// ===========================================================================
// Persistencia: registro da partida (arquivo binario)
// ===========================================================================

// Aplica as regras do jogo a cada numero lido, rejeitando arquivos invalidos.
Tabuleiro montar_tabuleiro(const std::vector<int>& numeros) {
  if (numeros.size() < Tabuleiro::TOTAL_CELULAS) {
    throw ArquivoCorrompido("O registro tem menos numeros que o tabuleiro");
  }

  Tabuleiro tabuleiro;
  try {
    for (int ordem = 0; ordem < Tabuleiro::TOTAL_CELULAS; ordem++) {
      tabuleiro.preencher(Tabuleiro::posicao_da_ordem(ordem), numeros[ordem]);
    }
  } catch (const JogadaInvalida& erro) {
    throw ArquivoCorrompido(erro.what());
  }
  return tabuleiro;
}

// Formato do arquivo: [total de jogadas (int)][desafio][tabuleiro]...
// Cada tabuleiro ocupa TOTAL_CELULAS bytes: um digito ASCII por celula.
class RegistroDePartida {
 public:
  static bool existe();
  static RegistroDePartida criar(const Tabuleiro& desafio);
  static RegistroDePartida abrir();

  Tabuleiro ler_desafio();
  Tabuleiro ler_ultimo_tabuleiro();
  void registrar_jogada(const Tabuleiro& tabuleiro);

 private:
  static constexpr char NOME_DO_ARQUIVO[] = "partida.save";

  explicit RegistroDePartida(std::fstream arquivo)
      : arquivo_(std::move(arquivo)) {}

  Tabuleiro ler_tabuleiro(std::streamoff deslocamento,
                          std::ios::seekdir origem);
  int ler_total_de_jogadas();
  void escrever_total_de_jogadas(int total);
  void anexar_tabuleiro(const Tabuleiro& tabuleiro);
  void garantir_gravacao_bem_sucedida();

  std::fstream arquivo_;
};

bool RegistroDePartida::existe() {
  return std::ifstream(NOME_DO_ARQUIVO).good();
}

RegistroDePartida RegistroDePartida::criar(const Tabuleiro& desafio) {
  std::fstream arquivo(NOME_DO_ARQUIVO, std::ios::in | std::ios::out |
                                            std::ios::binary | std::ios::trunc);
  if (!arquivo) {
    throw ErroDeGravacao("Nao foi possivel salvar em " +
                         std::string(NOME_DO_ARQUIVO));
  }

  RegistroDePartida registro(std::move(arquivo));
  registro.escrever_total_de_jogadas(0);
  registro.anexar_tabuleiro(desafio);
  return registro;
}

RegistroDePartida RegistroDePartida::abrir() {
  std::fstream arquivo(NOME_DO_ARQUIVO,
                       std::ios::in | std::ios::out | std::ios::binary);
  if (!arquivo) {
    throw ArquivoInexistente(NOME_DO_ARQUIVO);
  }
  return RegistroDePartida(std::move(arquivo));
}

Tabuleiro RegistroDePartida::ler_desafio() {
  return ler_tabuleiro(sizeof(int), std::ios::beg);
}

Tabuleiro RegistroDePartida::ler_ultimo_tabuleiro() {
  return ler_tabuleiro(-Tabuleiro::TOTAL_CELULAS, std::ios::end);
}

Tabuleiro RegistroDePartida::ler_tabuleiro(std::streamoff deslocamento,
                                           std::ios::seekdir origem) {
  arquivo_.clear();
  arquivo_.seekg(deslocamento, origem);

  std::vector<int> numeros;
  char digito;
  while (numeros.size() < Tabuleiro::TOTAL_CELULAS && arquivo_.get(digito)) {
    numeros.push_back(digito - '0');
  }
  if (numeros.size() < Tabuleiro::TOTAL_CELULAS) {
    throw ArquivoCorrompido("O registro nao contem um tabuleiro completo");
  }
  return montar_tabuleiro(numeros);
}

void RegistroDePartida::registrar_jogada(const Tabuleiro& tabuleiro) {
  escrever_total_de_jogadas(ler_total_de_jogadas() + 1);
  anexar_tabuleiro(tabuleiro);
}

int RegistroDePartida::ler_total_de_jogadas() {
  int total = 0;
  arquivo_.seekg(0, std::ios::beg);
  arquivo_.read(reinterpret_cast<char*>(&total), sizeof(total));
  if (!arquivo_) {
    throw ArquivoCorrompido("O registro nao contem o total de jogadas");
  }
  return total;
}

void RegistroDePartida::escrever_total_de_jogadas(int total) {
  arquivo_.seekp(0, std::ios::beg);
  arquivo_.write(reinterpret_cast<const char*>(&total), sizeof(total));
  garantir_gravacao_bem_sucedida();
}

void RegistroDePartida::anexar_tabuleiro(const Tabuleiro& tabuleiro) {
  arquivo_.seekp(0, std::ios::end);
  for (int ordem = 0; ordem < Tabuleiro::TOTAL_CELULAS; ordem++) {
    const int numero = tabuleiro.numero_em(Tabuleiro::posicao_da_ordem(ordem));
    arquivo_.put(static_cast<char>('0' + numero));
  }
  garantir_gravacao_bem_sucedida();
}

void RegistroDePartida::garantir_gravacao_bem_sucedida() {
  arquivo_.flush();
  if (!arquivo_) {
    throw ErroDeGravacao("Falha ao gravar o registro da partida");
  }
}

// ===========================================================================
// Partida: junta desafio, tabuleiro e registro.
// Invariante: sem registro, o tabuleiro ainda e igual ao desafio.
// Toda operacao que pode falhar so altera o estado depois de dar certo.
// ===========================================================================

class Partida {
 public:
  const Tabuleiro& tabuleiro() const { return tabuleiro_; }

  void iniciar_nova(Nivel nivel);
  void continuar();
  void jogar(Jogada jogada);
  void apagar(Posicao posicao);
  std::optional<Jogada> dar_dica();
  void resolver_tudo();

 private:
  bool faz_parte_do_desafio(Posicao posicao) const;
  void aplicar(const Tabuleiro& proximo_tabuleiro);

  Tabuleiro desafio_;
  Tabuleiro tabuleiro_;
  std::optional<RegistroDePartida> registro_;
};

void Partida::iniciar_nova(Nivel nivel) {
  desafio_ = desafio_do_nivel(nivel);
  tabuleiro_ = desafio_;
  registro_.reset();
}

void Partida::continuar() {
  RegistroDePartida registro = RegistroDePartida::abrir();
  const Tabuleiro desafio = registro.ler_desafio();
  const Tabuleiro ultimo_tabuleiro = registro.ler_ultimo_tabuleiro();

  desafio_ = desafio;
  tabuleiro_ = ultimo_tabuleiro;
  registro_ = std::move(registro);
}

bool Partida::faz_parte_do_desafio(Posicao posicao) const {
  return Tabuleiro::existe(posicao) &&
         desafio_.numero_em(posicao) != Tabuleiro::CELULA_VAZIA;
}

void Partida::jogar(Jogada jogada) {
  if (faz_parte_do_desafio(jogada.posicao)) {
    throw JogadaInvalida("Esta celula faz parte do desafio e nao pode mudar");
  }

  Tabuleiro proximo_tabuleiro = tabuleiro_;
  proximo_tabuleiro.preencher(jogada.posicao, jogada.numero);
  aplicar(proximo_tabuleiro);
}

void Partida::apagar(Posicao posicao) {
  jogar({posicao, Tabuleiro::CELULA_VAZIA});
}

std::optional<Jogada> Partida::dar_dica() {
  Tabuleiro proximo_tabuleiro = tabuleiro_;
  const std::optional<Jogada> dica = proximo_tabuleiro.aplicar_dica();
  if (dica) {
    aplicar(proximo_tabuleiro);
  }
  return dica;
}

void Partida::resolver_tudo() {
  std::optional<Jogada> dica;
  do {
    dica = dar_dica();
  } while (dica.has_value());
}

void Partida::aplicar(const Tabuleiro& proximo_tabuleiro) {
  if (!registro_) {
    registro_ = RegistroDePartida::criar(desafio_);
  }
  registro_->registrar_jogada(proximo_tabuleiro);
  tabuleiro_ = proximo_tabuleiro;
}

// ===========================================================================
// Apresentacao: tudo o que escreve na tela
// ===========================================================================

// Na tela, linhas e colunas sao numeradas a partir de 1.
constexpr int DESLOCAMENTO_DA_TELA = 1;

void exibir_celula(std::ostream& saida, int numero) {
  if (numero == Tabuleiro::CELULA_VAZIA) {
    saida << ". ";
    return;
  }
  saida << numero << ' ';
}

void exibir_linha(std::ostream& saida, const Tabuleiro& tabuleiro, int linha) {
  saida << linha + DESLOCAMENTO_DA_TELA << " | ";
  for (int coluna = 0; coluna < Tabuleiro::TAMANHO; coluna++) {
    if (1==1) {
      saida << "| ";
    }
    exibir_celula(saida, tabuleiro.numero_em({linha, coluna}));
  }
  saida << "|\n";
}

void exibir_tabuleiro(std::ostream& saida, const Tabuleiro& tabuleiro) {
  constexpr char CABECALHO[] = "    1 2 3   4 5 6   7 8 9";
  constexpr char SEPARADOR[] = "  -------------------------";

  saida << '\n' << CABECALHO << '\n';
  for (int linha = 0; linha < Tabuleiro::TAMANHO; linha++) {
    if (linha % Tabuleiro::TAMANHO_BLOCO == 0) {
      saida << SEPARADOR << '\n';
    }
    exibir_linha(saida, tabuleiro, linha);
  }
  saida << SEPARADOR << '\n';
}

void exibir_andamento(std::ostream& saida, const Tabuleiro& tabuleiro) {
  if (tabuleiro.esta_completo()) {
    saida << "Parabens! Voce completou o Sudoku! "
          << "Escolha [5] para jogar de novo.\n";
    return;
  }
  saida << "Faltam " << tabuleiro.contar_celulas_vazias() << " celulas.\n";
}

void exibir_boas_vindas(std::ostream& saida) {
  saida << "Bem-vindo ao Sudoku!\n"
        << "Preencha a grade para que cada linha, cada coluna e cada bloco\n"
        << "3x3 tenha os numeros de 1 a 9, sem repetir. Os pontos (.) sao\n"
        << "celulas vazias. Linhas e colunas vao de 1 a 9.\n"
        << "O jogo salva sozinho e voltara de onde voce parou.\n";
}

void exibir_menu_principal(std::ostream& saida) {
  saida << "\nO que voce quer fazer?\n"
        << "[1] Colocar um numero\n"
        << "[2] Apagar um numero\n"
        << "[3] Pedir uma dica\n"
        << "[4] Resolver tudo\n"
        << "[5] Novo jogo\n"
        << "[0] Sair\n";
}

void exibir_menu_de_niveis(std::ostream& saida) {
  saida << "\nEscolha o nivel:\n"
        << "[1] Facil\n"
        << "[2] Medio\n"
        << "[3] Dificil\n"
        << "[0] Voltar\n";
}

// ===========================================================================
// Interface de texto: le a entrada, chama a Partida e mostra o resultado.
// O fluxo normal fica nos metodos; o fluxo excepcional, em um unico lugar.
// ===========================================================================

class InterfaceDeTexto {
 public:
  void executar();

 private:
  enum OpcaoPrincipal {
    SAIR = 0,
    COLOCAR_NUMERO = 1,
    APAGAR_NUMERO = 2,
    PEDIR_DICA = 3,
    RESOLVER_TUDO = 4,
    NOVO_JOGO = 5
  };

  enum OpcaoDeNivel { VOLTAR = 0, FACIL = 1, MEDIO = 2, DIFICIL = 3 };

  static constexpr int OPCAO_INVALIDA = -1;
  static constexpr char MSG_OPCAO_INVALIDA[] =
      "Opcao invalida! Digite um dos numeros do menu.";

  template <typename Acao>
  void executar_com_tratamento_de_erros(Acao acao);

  static void descartar_entrada_pendente();
  static int ler_opcao();
  static int ler_inteiro();
  static Posicao ler_posicao();
  static Jogada ler_jogada();

  void retomar_partida_anterior();
  void despachar(int opcao);
  void colocar_numero();
  void apagar_numero();
  void pedir_dica();
  void resolver_tudo();
  void iniciar_novo_jogo();

  Partida partida_;
};

void InterfaceDeTexto::executar() {
  partida_.iniciar_nova(Nivel::FACIL);
  exibir_boas_vindas(std::cout);
  executar_com_tratamento_de_erros([this] { retomar_partida_anterior(); });

  int opcao;
  do {
    exibir_tabuleiro(std::cout, partida_.tabuleiro());
    exibir_andamento(std::cout, partida_.tabuleiro());
    exibir_menu_principal(std::cout);

    opcao = ler_opcao();
    executar_com_tratamento_de_erros([&] { despachar(opcao); });
  } while (opcao != SAIR);
}

// Unico lugar onde os erros esperados sao tratados.
// Erros de programacao ou falta de memoria nao sao capturados (just crash).
template <typename Acao>
void InterfaceDeTexto::executar_com_tratamento_de_erros(Acao acao) {
  try {
    acao();
  } catch (const JogadaInvalida& erro) {
    std::cout << "Nao foi possivel: " << erro.what() << ".\n";
  } catch (const ArquivoInexistente& erro) {
    std::cout << "Nao foi possivel abrir o arquivo " << erro.what() << ".\n";
  } catch (const ArquivoCorrompido& erro) {
    std::cout << "O registro salvo esta danificado (" << erro.what()
              << "). Comecando com um jogo novo.\n";
  } catch (const ErroDeGravacao& erro) {
    std::cout << "Erro ao salvar: " << erro.what() << ".\n";
  }
}

void InterfaceDeTexto::retomar_partida_anterior() {
  if (!RegistroDePartida::existe()) {
    return;
  }
  partida_.continuar();
  std::cout << "\nPartida anterior retomada.\n";
}

void InterfaceDeTexto::despachar(int opcao) {
  switch (opcao) {
    case COLOCAR_NUMERO:
      colocar_numero();
      break;
    case APAGAR_NUMERO:
      apagar_numero();
      break;
    case PEDIR_DICA:
      pedir_dica();
      break;
    case RESOLVER_TUDO:
      resolver_tudo();
      break;
    case NOVO_JOGO:
      iniciar_novo_jogo();
      break;
    case SAIR:
      std::cout << "Ate a proxima!\n";
      break;
    default:
      std::cout << MSG_OPCAO_INVALIDA << '\n';
  }
}

void InterfaceDeTexto::colocar_numero() {
  std::cout << "Digite: linha coluna numero (exemplo: 3 5 7): ";
  partida_.jogar(ler_jogada());
}

void InterfaceDeTexto::apagar_numero() {
  std::cout << "Digite: linha coluna (exemplo: 3 5): ";
  partida_.apagar(ler_posicao());
}

void InterfaceDeTexto::pedir_dica() {
  const std::optional<Jogada> dica = partida_.dar_dica();
  if (!dica) {
    std::cout << "Nao ha dica agora. Algum numero seu pode estar errado.\n";
    return;
  }
  std::cout << "Dica: na linha " << dica->posicao.linha + DESLOCAMENTO_DA_TELA
            << ", coluna " << dica->posicao.coluna + DESLOCAMENTO_DA_TELA
            << " so cabe o numero " << dica->numero << ".\n";
}

void InterfaceDeTexto::resolver_tudo() {
  partida_.resolver_tudo();
  if (!partida_.tabuleiro().esta_completo()) {
    std::cout << "Nao consegui terminar. Algum numero seu pode estar errado:"
              << " apague-o ou comece um novo jogo.\n";
  }
}

void InterfaceDeTexto::iniciar_novo_jogo() {
  exibir_menu_de_niveis(std::cout);

  while (true) {
    switch (ler_opcao()) {
      case FACIL:
        partida_.iniciar_nova(Nivel::FACIL);
        return;
      case MEDIO:
        partida_.iniciar_nova(Nivel::MEDIO);
        return;
      case DIFICIL:
        partida_.iniciar_nova(Nivel::DIFICIL);
        return;
      case VOLTAR:
        return;
      default:
        std::cout << MSG_OPCAO_INVALIDA << '\n';
    }
  }
}

void InterfaceDeTexto::descartar_entrada_pendente() {
  std::cin.clear();
  std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

int InterfaceDeTexto::ler_opcao() {
  std::cout << "Opcao: ";

  int opcao = OPCAO_INVALIDA;
  if (std::cin >> opcao) {
    return opcao;
  }
  if (std::cin.eof()) {
    return SAIR;
  }
  descartar_entrada_pendente();
  return OPCAO_INVALIDA;
}

int InterfaceDeTexto::ler_inteiro() {
  int numero;
  if (!(std::cin >> numero)) {
    descartar_entrada_pendente();
    throw JogadaInvalida("Digite apenas numeros, separados por espaco");
  }
  return numero;
}

// Le linha e coluna como o jogador as ve (1 a 9).
Posicao InterfaceDeTexto::ler_posicao() {
  const int linha = ler_inteiro();
  const int coluna = ler_inteiro();

  const Posicao posicao{linha - DESLOCAMENTO_DA_TELA,
                        coluna - DESLOCAMENTO_DA_TELA};
  if (!Tabuleiro::existe(posicao)) {
    throw JogadaInvalida("Linha e coluna devem estar entre 1 e " +
                         std::to_string(Tabuleiro::TAMANHO));
  }
  return posicao;
}

Jogada InterfaceDeTexto::ler_jogada() {
  const Posicao posicao = ler_posicao();
  return {posicao, ler_inteiro()};
}

int main() {
  InterfaceDeTexto interface_de_texto;
  interface_de_texto.executar();
  return 0;
}
