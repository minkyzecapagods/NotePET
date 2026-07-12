#ifndef SYNTAX_HPP
#define SYNTAX_HPP

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <memory>

using namespace std;

// Premissas de Design:
// ----------------------------------------------------------
// - Análise restrita por linha: Não suporta construtos multilinha (ex: comentários /* */).
// - Cores Truecolor: Usa ANSI 24 bits, delegando o fallback de cores para o terminal.
// - Indexação única: VariableIndex ignora duplicatas, comportando-se como um conjunto (set).
// - Léxico simplificado: Identifica variáveis via padrão básico alfanumérico e prefixo std:: como token único.
// - Conjunto fixo de sintaxe: Restrito a 9 palavras-chave (controle de fluxo) e 9 tipos de dados estritos. O resto é tratado como identificador comum.
// - Temas modulares: O sistema de cores globais (fundo/texto) é independente da colorização individual de tokens feita pelo aluno.

// ---------------------------------------------------------------------------
// Theme / ThemePalette (infraestrutura de temas)
// ---------------------------------------------------------------------------

// Define o tema global. Terminal e SyntaxHighlighter consomem a mesma
// ThemePalette para manter as cores de fundo e dos tokens sempre sincronizadas.
enum class Theme {
    Monokai,
    Dracula,
    Light
};

// Tabela de consulta: mapeia cada tema para sequências de escape ANSI SGR truecolor.
// Fonte única de verdade usada por Terminal e SyntaxHighlighter para evitar duplicação.
class ThemePalette {
public:
    static const string& background(Theme theme);
    static const string& foreground(Theme theme);
    static const string& keywordColor(Theme theme);
    static const string& typeColor(Theme theme);
    static const string& numberColor(Theme theme);
    static const string& stringColor(Theme theme);
    static const string& commentColor(Theme theme);
    static const string& successColor(Theme theme); // e.g. "Escopo: OK"
    static const string& errorColor(Theme theme);   // e.g. "Erro de Escopo"
    static const string& resetToThemeCode(Theme theme);
};

// ---------------------------------------------------------------------------
// Token (Parte da Missão 3.1)
// ---------------------------------------------------------------------------
// Token léxico gerado por SyntaxHighlighter::tokenize(), marcado com sua
// categoria sintática para determinar a cor do realce de sintaxe.
struct Token {
    string text;     // Texto bruto do token, exatamente como está na linha.
    string category; // Categoria: "keyword", "type", "identifier", "number", "string", "comment", "default".
};


// ---------------------------------------------------------------------------
// SyntaxHighlighter (Missão 3.1)
// ---------------------------------------------------------------------------
// Analisador léxico e colorizador rudimentar: divide uma única linha em Tokens
// e a renderiza com códigos ANSI por categoria baseando-se no tema ativo.
class SyntaxHighlighter {
public:
    SyntaxHighlighter();

    // Busca para palavras reservadas de controle de fluxo.
    bool isKeyword(const string& word) const;

    // Busca para nomes de tipos nativos/estruturais.
    bool isType(const string& word) const;

    // Retorna o código ANSI de uma categoria. Inclui as categorias de tokens
    // e as semânticas "success" e "error" usadas na barra de status do editor.
    const string& colorFor(const string& category) const;

    // Divide a linha em Tokens (identificadores, palavras-chave, tipos, literais,
    // comentários e "default"). A concatenação dos tokens reconstrói a linha exatamente.
    vector<Token> tokenize(const string& line) const;

    // Envoltório conveniente: tokeniza e insere os códigos de cor ANSI na linha,
    // deixando-a pronta para ser enviada diretamente ao Terminal::renderFrame().
    string highlightLine(const string& line) const;

    // Altera o esquema de cores ativo e reconstrói a tabela de cores interna.
    void setTheme(Theme theme);
    Theme theme() const noexcept;

private:
    unordered_set<string> keywords_; // Missão 3.1: control-flow
    unordered_set<string> types_;    // Item C: tipos estruturais/nativos
    unordered_map<string, string> tokenColors_;
    Theme theme_ = Theme::Monokai;

    void applyThemePalette();
};

// ---------------------------------------------------------------------------
// ScopeValidator (Missão 3.2)
// ---------------------------------------------------------------------------

// Validador de delimitadores baseado em stack<char>.
class ScopeValidator {
public:
    // Verifica se cada '(', '[', '{' em `text` é fechado na ordem correta por ')', ']', '}'.
    // `text` pode ser uma linha única ou um bloco inteiro concatenado.

    // Se o texto estiver desbalanceado e `mismatchDetail` não for nulo, o ponteiro recebe
    // uma descrição curta do erro (fechamento inesperado, par incompatível ou delimitador aberto e nunca fechado).
    static bool validate(const string& text, string* mismatchDetail = nullptr);

private:
    static bool isOpener(char c) noexcept;
    static bool isCloser(char c) noexcept;
    static char closingFor(char opener) noexcept;
};

// ---------------------------------------------------------------------------
// VariableIndex (Missão 3.3 + Missão 4.4)
// ---------------------------------------------------------------------------
 
// Árvore Binária de Busca (BST) para armazenar identificadores únicos de forma ordenada.
// O gerenciamento de memória é automático, com a posse dos nós controlada via std::unique_ptr.
class VariableIndex {
public:
    VariableIndex() = default;
    ~VariableIndex() = default; // A destruição em cascata é garantida pelos unique_ptr dos nós.
 
    // Cópia e movimentação desabilitadas intencionalmente para forçar o uso 
    // de uma única instância estável da árvore durante o ciclo de varredura.
    VariableIndex(const VariableIndex&) = delete;
    VariableIndex& operator=(const VariableIndex&) = delete;
    VariableIndex(VariableIndex&&) = delete;
    VariableIndex& operator=(VariableIndex&&) = delete;
 
    // Missão 3.3: inserção ordenada na BST. 
    // Inserções de nomes já existentes são ignoradas (comportamento de conjunto/set).
    void insert(const string& name);
 
    // In-order traversal
    // Retorna todos os identificadores armazenados em ordem lexicográfica (alfabética).
    vector<string> inOrder() const;
 
    // Missão 4.3: motor do autocomplete. 
    // Retorna identificadores que começam com o prefixo especificado, em ordem lexicográfica.
    // Um prefixo vazio retorna todos os elementos da árvore.
    vector<string> autocomplete(const string& prefix) const;
 
    bool empty() const noexcept;
    size_t size() const noexcept;
 
private:
    // Estrutura do nó. O uso de unique_ptr para os filhos esquerdo e direito
    // elimina a necessidade de chamadas manuais a `delete` e destrutores customizados.
    struct Node {
        string value;
        unique_ptr<Node> left;
        unique_ptr<Node> right;
        explicit Node(string v) : value(move(v)) {}
    };
 
    unique_ptr<Node> root_;
    size_t size_ = 0;
 
    // Função auxiliar recursiva de inserção. 
    // Recebe e retorna a propriedade da subárvore (via unique_ptr), exigindo o uso de std::move pelo chamador.
    static unique_ptr<Node> insertNode(unique_ptr<Node> node, const string& name,
                                             bool& inserted);
    static void inOrderCollect(const Node* node, vector<string>& out);
    static void collectWithPrefix(const Node* node, const string& prefix,
                                   std::vector<string>& out);
};

// ---------------------------------------------------------------------------
// IdentifierScanner 
// ---------------------------------------------------------------------------
// Scanner léxico pronto que extrai potenciais nomes de variáveis na linha e os 
// envia ao VariableIndex. Faz a ponte entre as tabelas de sintaxe e a árvore (BST).
class IdentifierScanner {
public:
    // Retorna os identificadores candidatos da linha em ordem, ignorando palavras-chave 
    // e tipos. Pode conter duplicatas, que são filtradas na inserção do VariableIndex.
    static vector<string> extractIdentifiers(const string& line,
                                                        const SyntaxHighlighter& highlighter);

    // Facilitador: escaneia a linha e insere diretamente todos os identificadores no índice.
    static void scanInto(const string& line, const SyntaxHighlighter& highlighter,
                            VariableIndex& index);
};

// ---------------------------------------------------------------------------
// TextStatistics (Missão 4.4)
// ---------------------------------------------------------------------------
// Resultado agregado de TextStatistics::compute() sobre o buffer: estatísticas
// de frequência calculadas via tabela hash e métricas descritivas básicas.
struct TextStatsResult {
    int uniqueWords = 0;
    double avgLength = 0.0;
    string longestWord = "";
    vector<pair<string, int>> top5Frequencies;
};

class TextStatistics {
public:
    static TextStatsResult compute(const vector<string>& lines);
};

#endif // SYNTAX_HPP