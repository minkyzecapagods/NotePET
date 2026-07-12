#include "Syntax.hpp"

#include <algorithm>
#include <cctype>
#include <stack>
#include <utility>

using namespace std;

// Decisão de Design: Estes predicados léxicos e auxiliares de cor são 
// primitivas internas em escopo de arquivo (file-scope). Não são expostos 
// no contrato público, pois são detalhes exclusivos de implementação.
namespace {

bool isIdentifierStart(char c) noexcept {
    return isalpha(static_cast<unsigned char>(c)) != 0 || c == '_';
}

bool isIdentifierChar(char c) noexcept {
    return isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

bool isDigitChar(char c) noexcept {
    return isdigit(static_cast<unsigned char>(c)) != 0;
}

// Representação RGB usada internamente para construir strings TrueColor SGR.
struct RGB {
    int r;
    int g;
    int b;
};

string sgrForeground(RGB c) {
    return "\033[38;2;" + to_string(c.r) + ";" + to_string(c.g) + ";" +
           to_string(c.b) + "m";
}

string sgrBackground(RGB c) {
    return "\033[48;2;" + to_string(c.r) + ";" + to_string(c.g) + ";" +
           to_string(c.b) + "m";
}

} // namespace

// =============================================================================
// ThemePalette (Infraestrutura de temas)
// =============================================================================

// Decisão de Design: Cada função constrói e retorna referências const para 
// strings estáticas. Isso garante que a string SGR seja montada apenas na 
// primeira chamada, economizando alocações. O "return" padrão ao final 
// previne alertas do compilador (-Wswitch).

const string& ThemePalette::background(Theme theme) {
    static const string kMonokai = sgrBackground({39, 40, 34});   // #272822
    static const string kDracula = sgrBackground({40, 42, 54});   // #282A36
    static const string kLight = sgrBackground({250, 250, 250}); // #FAFAFA
    switch (theme) {
        case Theme::Monokai: return kMonokai;
        case Theme::Dracula: return kDracula;
        case Theme::Light:   return kLight;
    }
    return kMonokai;
}

const string& ThemePalette::foreground(Theme theme) {
    static const string kMonokai = sgrForeground({248, 248, 242}); // #F8F8F2
    static const string kDracula = sgrForeground({248, 248, 242}); // #F8F8F2
    static const string kLight = sgrForeground({56, 58, 66});     // #383A42
    switch (theme) {
        case Theme::Monokai: return kMonokai;
        case Theme::Dracula: return kDracula;
        case Theme::Light:   return kLight;
    }
    return kMonokai;
}

const string& ThemePalette::keywordColor(Theme theme) {
    static const string kMonokai = sgrForeground({249, 38, 114}); // rosa #F92672
    static const string kDracula = sgrForeground({255, 85, 85});  // vermelho #FF5555
    static const string kLight = sgrForeground({0, 92, 197});    // azul escuro #005CC5
    switch (theme) {
        case Theme::Monokai: return kMonokai;
        case Theme::Dracula: return kDracula;
        case Theme::Light:   return kLight;
    }
    return kMonokai;
}

const string& ThemePalette::typeColor(Theme theme) {
    static const string kMonokai = sgrForeground({102, 217, 239}); // ciano #66D9EF
    static const string kDracula = sgrForeground({59, 130, 246});  // azul #3B82F6
    static const string kLight = sgrForeground({111, 66, 193});   // roxo #6F42C1
    switch (theme) {
        case Theme::Monokai: return kMonokai;
        case Theme::Dracula: return kDracula;
        case Theme::Light:   return kLight;
    }
    return kMonokai;
}

const string& ThemePalette::numberColor(Theme theme) {
    static const string kMonokai = sgrForeground({174, 129, 255}); // #AE81FF
    static const string kDracula = sgrForeground({189, 147, 249}); // #BD93F9
    static const string kLight = sgrForeground({168, 101, 0});    // #A86500
    switch (theme) {
        case Theme::Monokai: return kMonokai;
        case Theme::Dracula: return kDracula;
        case Theme::Light:   return kLight;
    }
    return kMonokai;
}

const string& ThemePalette::stringColor(Theme theme) {
    static const string kMonokai = sgrForeground({230, 219, 116}); // #E6DB74
    static const string kDracula = sgrForeground({241, 250, 140}); // #F1FA8C
    static const string kLight = sgrForeground({34, 134, 58});    // #22863A
    switch (theme) {
        case Theme::Monokai: return kMonokai;
        case Theme::Dracula: return kDracula;
        case Theme::Light:   return kLight;
    }
    return kMonokai;
}

const string& ThemePalette::commentColor(Theme theme) {
    static const string kMonokai = sgrForeground({117, 113, 94}); // #75715E
    static const string kDracula = sgrForeground({98, 114, 164}); // #6272A4
    static const string kLight = sgrForeground({106, 115, 125}); // #6A737D
    switch (theme) {
        case Theme::Monokai: return kMonokai;
        case Theme::Dracula: return kDracula;
        case Theme::Light:   return kLight;
    }
    return kMonokai;
}

const string& ThemePalette::successColor(Theme theme) {
    static const string kMonokai = sgrForeground({166, 226, 46}); // #A6E22E
    static const string kDracula = sgrForeground({80, 250, 123}); // #50FA7B
    static const string kLight = sgrForeground({26, 127, 55});   // #1A7F37
    switch (theme) {
        case Theme::Monokai: return kMonokai;
        case Theme::Dracula: return kDracula;
        case Theme::Light:   return kLight;
    }
    return kMonokai;
}

const string& ThemePalette::errorColor(Theme theme) {
    static const string kMonokai = sgrForeground({249, 38, 114}); // #F92672
    static const string kDracula = sgrForeground({255, 85, 85});  // #FF5555
    static const string kLight = sgrForeground({215, 58, 73});   // #D73A49
    switch (theme) {
        case Theme::Monokai: return kMonokai;
        case Theme::Dracula: return kDracula;
        case Theme::Light:   return kLight;
    }
    return kMonokai;
}

const string& ThemePalette::resetToThemeCode(Theme theme) {
    static const string kMonokai =
        "\033[0m" + background(Theme::Monokai) + foreground(Theme::Monokai);
    static const string kDracula =
        "\033[0m" + background(Theme::Dracula) + foreground(Theme::Dracula);
    static const string kLight =
        "\033[0m" + background(Theme::Light) + foreground(Theme::Light);
    switch (theme) {
        case Theme::Monokai: return kMonokai;
        case Theme::Dracula: return kDracula;
        case Theme::Light:   return kLight;
    }
    return kMonokai;
}

// =============================================================================
// SyntaxHighlighter
// =============================================================================

// =============================================================================
// [MISSÃO 3.1 - INÍCIO: Conjunto de palavras-chave reservadas e tabela de cores ANSI por categoria]
// =============================================================================

SyntaxHighlighter::SyntaxHighlighter() {
    keywords_ = {
        "if", "else", "while", "for", "return", "switch", "case", "break", "continue"
    };

    types_ = {
        "int", "char", "bool", "float", "double", "void", "size_t",
        "string", "vector"
    };

    // =============================================================================
    // [MISSÃO 3.1 - FIM]
    // =============================================================================

    applyThemePalette(); // Monta a tabela tokenColors_ com base no tema atual.
}

// -----------------------------------------------------------------------------
// Helpers da infraestrutura base de tokenização. Se o aluno não tiver
// preenchido os contêineres na Missão 3.1, a tokenização continua funcionando
// graciosamente com fallback "uncolored".
// -----------------------------------------------------------------------------

bool SyntaxHighlighter::isKeyword(const string& word) const {
    return keywords_.find(word) != keywords_.end();
}

bool SyntaxHighlighter::isType(const string& word) const {
    return types_.find(word) != types_.end();
}

const string& SyntaxHighlighter::colorFor(const string& category) const {
    static const string kNoColor; // Fallback seguro.

    auto it = tokenColors_.find(category);
    if (it != tokenColors_.end()) {
        return it->second;
    }
    auto defaultIt = tokenColors_.find("default");
    if (defaultIt != tokenColors_.end()) {
        return defaultIt->second;
    }
    return kNoColor;
}

vector<Token> SyntaxHighlighter::tokenize(const string& line) const {
    vector<Token> tokens;
    size_t i = 0;
    const size_t n = line.size();

    while (i < n) {
        const char c = line[i];

        // Espaços em branco: preservados sem formatação de cor.
        if (isspace(static_cast<unsigned char>(c)) != 0) {
            const size_t start = i;
            while (i < n && isspace(static_cast<unsigned char>(line[i])) != 0) {
                ++i;
            }
            tokens.push_back(Token{line.substr(start, i - start), "default"});
            continue;
        }

        // Comentários de linha "//": consomem o restante da string.
        if (c == '/' && i + 1 < n && line[i + 1] == '/') {
            tokens.push_back(Token{line.substr(i), "comment"});
            break;
        }

        // Literais de String e Char (suportam escape por contra-barra).
        if (c == '"') {
            const size_t start = i;
            ++i;
            while (i < n && line[i] != '"') {
                i += (line[i] == '\\' && i + 1 < n) ? 2 : 1;
            }
            if (i < n) ++i;
            tokens.push_back(Token{line.substr(start, i - start), "string"});
            continue;
        }

        if (c == '\'') {
            const size_t start = i;
            ++i;
            while (i < n && line[i] != '\'') {
                i += (line[i] == '\\' && i + 1 < n) ? 2 : 1;
            }
            if (i < n) ++i;
            tokens.push_back(Token{line.substr(start, i - start), "string"});
            continue;
        }

        // Identificadores, palavras-chave e nomes de tipos.
        if (isIdentifierStart(c)) {
            const size_t start = i;
            while (i < n && isIdentifierChar(line[i])) {
                ++i;
            }
            string word = line.substr(start, i - start);

            // Regra especial: Consolidar "" e o próximo nome como token único.
            while (word == "std" && i + 1 < n && line[i] == ':' && line[i + 1] == ':') {
                const size_t next = i + 2;
                if (next >= n || !isIdentifierStart(line[next])) {
                    break;
                }
                size_t identEnd = next;
                while (identEnd < n && isIdentifierChar(line[identEnd])) {
                    ++identEnd;
                }
                word += "::";
                word += line.substr(next, identEnd - next);
                i = identEnd;
            }

            string category;
            if (isType(word)) category = "type";
            else if (isKeyword(word)) category = "keyword";
            else category = "identifier";
            
            tokens.push_back(Token{move(word), category});
            continue;
        }

        // Literais Numéricos.
        if (isDigitChar(c)) {
            const size_t start = i;
            while (i < n && (isDigitChar(line[i]) || line[i] == '.')) {
                ++i;
            }
            tokens.push_back(Token{line.substr(start, i - start), "number"});
            continue;
        }

        // Outros caracteres (pontuação, operadores): tokenizados unicamente.
        tokens.push_back(Token{string(1, c), "default"});
        ++i;
    }

    return tokens;
}

string SyntaxHighlighter::highlightLine(const string& line) const {
    string result;
    result.reserve(line.size() * 2); // Pré-aloca espaço extra para códigos de escape.

    const string& resetCode = colorFor("default");

    for (const Token& token : tokenize(line)) {
        if (token.category == "default") {
            result += token.text; 
        } else {
            result += colorFor(token.category);
            result += token.text;
            result += resetCode;
        }
    }

    return result;
}

// -----------------------------------------------------------------------------
// Gestão de temas: Lógica estrita de apresentação visual (independe de missões)
// -----------------------------------------------------------------------------

void SyntaxHighlighter::setTheme(Theme theme) {
    theme_ = theme;
    applyThemePalette();
}

Theme SyntaxHighlighter::theme() const noexcept {
    return theme_;
}

void SyntaxHighlighter::applyThemePalette() {
    tokenColors_ = {
        {"keyword",    ThemePalette::keywordColor(theme_)},
        {"type",       ThemePalette::typeColor(theme_)},
        {"identifier", ThemePalette::foreground(theme_)},
        {"number",     ThemePalette::numberColor(theme_)},
        {"string",     ThemePalette::stringColor(theme_)},
        {"comment",    ThemePalette::commentColor(theme_)},
        {"success",    ThemePalette::successColor(theme_)},
        {"error",      ThemePalette::errorColor(theme_)},
        {"default",    ThemePalette::resetToThemeCode(theme_)},
    };
}

// =============================================================================
// ScopeValidator
// =============================================================================

bool ScopeValidator::isOpener(char c) noexcept {
    return c == '(' || c == '[' || c == '{';
}

bool ScopeValidator::isCloser(char c) noexcept {
    return c == ')' || c == ']' || c == '}';
}

char ScopeValidator::closingFor(char opener) noexcept {
    switch (opener) {
        case '(': return ')';
        case '[': return ']';
        case '{': return '}';
        default:  return '\0';
    }
}

// =============================================================================
// [MISSÃO 3.2 - INÍCIO: Validação de emparelhamento de chaves/parênteses usando stack<char>]
// =============================================================================

bool ScopeValidator::validate(const string& text, string* mismatchDetail) {
    stack<char> openers;

    for (char c : text) {
        if (isOpener(c)) {
            openers.push(c);
            continue;
        }
        if (!isCloser(c)) {
            continue; 
        }

        if (openers.empty()) {
            if (mismatchDetail != nullptr) {
                *mismatchDetail = string("fechamento inesperado '") + c +
                                   "' sem abertura correspondente";
            }
            return false;
        }

        const char expected = closingFor(openers.top());
        if (c != expected) {
            if (mismatchDetail != nullptr) {
                *mismatchDetail = string("'") + openers.top() +
                                   "' foi fechado com '" + c + "', mas esperava-se '" +
                                   expected + "'";
            }
            return false;
        }
        openers.pop();
    }

    if (!openers.empty()) {
        if (mismatchDetail != nullptr) {
            *mismatchDetail = string("'") + openers.top() + "' nunca foi fechado";
        }
        return false;
    }

    return true;
}

// =============================================================================
// [MISSÃO 3.2 - FIM]
// =============================================================================

// =============================================================================
// VariableIndex
// =============================================================================

// O destrutor gerado implicitamente para o VariableIndex é suficiente porque
// unique_ptr<Node> gerencia automaticamente a destruição em cascata 
// (os nós deletam seus próprios filhos). Nenhuma ação manual é necessária.

// =============================================================================
// [MISSÃO 3.3 - INÍCIO: Inserção ordenada na BST (comparação de strings)]
// =============================================================================

unique_ptr<VariableIndex::Node> VariableIndex::insertNode(unique_ptr<Node> node,
                                                                 const string& name,
                                                                 bool& inserted) {
    if (!node) {
        inserted = true;
        return make_unique<Node>(name);
    }
    if (name < node->value) {
        node->left = insertNode(move(node->left), name, inserted);
    } else if (node->value < name) {
        node->right = insertNode(move(node->right), name, inserted);
    } else {
        inserted = false; // Comportamento de conjunto (set): ignora duplicatas.
    }
    return node;
}

void VariableIndex::insert(const string& name) {
    bool inserted = false;
    root_ = insertNode(move(root_), name, inserted);
    if (inserted) {
        ++size_;
    }
}

// =============================================================================
// [MISSÃO 3.3 - FIM]
// =============================================================================

vector<string> VariableIndex::inOrder() const {
    vector<string> out;
    out.reserve(size_);
    inOrderCollect(root_.get(), out);
    return out;
}

void VariableIndex::inOrderCollect(const Node* node, vector<string>& out) {
    if (node == nullptr) {
        return;
    }
    inOrderCollect(node->left.get(), out);
    out.push_back(node->value);
    inOrderCollect(node->right.get(), out);
}

// =============================================================================
// [MISSÃO 4.3 - INÍCIO: Motor de autocompletar por prefixo sobre a BST]
// =============================================================================

void VariableIndex::collectWithPrefix(const Node* node, const string& prefix,
                                       vector<string>& out) {
    if (node == nullptr) {
        return;
    }

    // Verifica se node->value começa com `prefix` (equivalente C++ < 20 para starts_with).
    const bool startsWithPrefix = node->value.rfind(prefix, 0) == 0;

    if (startsWithPrefix) {
        // Devido à ordenação alfabética completa, strings com este prefixo podem
        // estar à esquerda e à direita. A busca in-order mantém a resposta ordenada.
        collectWithPrefix(node->left.get(), prefix, out);
        out.push_back(node->value);
        collectWithPrefix(node->right.get(), prefix, out);
        return;
    }

    if (node->value < prefix) {
        collectWithPrefix(node->right.get(), prefix, out);
    } else {
        collectWithPrefix(node->left.get(), prefix, out);
    }
}

vector<string> VariableIndex::autocomplete(const string& prefix) const {
    vector<string> out;
    collectWithPrefix(root_.get(), prefix, out);
    return out;
}

// =============================================================================
// [MISSÃO 4.3 - FIM]
// =============================================================================

bool VariableIndex::empty() const noexcept {
    return root_ == nullptr;
}

size_t VariableIndex::size() const noexcept {
    return size_;
}

// =============================================================================
// IdentifierScanner
// =============================================================================

vector<string> IdentifierScanner::extractIdentifiers(const string& line,
                                                                 const SyntaxHighlighter& highlighter) {
    vector<string> result;
    size_t i = 0;
    const size_t n = line.size();

    while (i < n) {
        if (isIdentifierStart(line[i])) {
            const size_t start = i;
            while (i < n && isIdentifierChar(line[i])) {
                ++i;
            }
            string word = line.substr(start, i - start);
            if (!highlighter.isKeyword(word) && !highlighter.isType(word)) {
                result.push_back(move(word));
            }
            continue;
        }
        ++i;
    }

    return result;
}

void IdentifierScanner::scanInto(const string& line, const SyntaxHighlighter& highlighter,
                                  VariableIndex& index) {
    for (const string& name : extractIdentifiers(line, highlighter)) {
        index.insert(name);
    }
}

// =============================================================================
// [MISSÃO 4.4 - INÍCIO: Motor de Estatísticas Textuais com Tabelas Hash]
// =============================================================================

TextStatsResult TextStatistics::compute(const vector<string>& lines) {
    TextStatsResult result;

    // Uso de unordered_map garante maior desempenho na atualização das frequências.
    unordered_map<string, int> frequency;

    size_t totalWords = 0;
    size_t totalLength = 0;

    for (const string& line : lines) {
        size_t i = 0;
        const size_t n = line.size();

        while (i < n) {
            // Ignora caracteres não alfanuméricos (atuam como delimitadores).
            if (isalnum(static_cast<unsigned char>(line[i])) == 0) {
                ++i;
                continue;
            }

            string word;
            while (i < n && isalnum(static_cast<unsigned char>(line[i])) != 0) {
                // Normaliza o texto computado para letras minúsculas.
                word += static_cast<char>(tolower(static_cast<unsigned char>(line[i])));
                ++i;
            }

            ++frequency[word]; 
            ++totalWords;
            totalLength += word.size();

            if (word.size() > result.longestWord.size()) {
                result.longestWord = word;
            }
        }
    }

    result.uniqueWords = static_cast<int>(frequency.size());

    result.avgLength = (totalWords > 0)
        ? static_cast<double>(totalLength) / static_cast<double>(totalWords)
        : 0.0;

    // Transferência do hash para vetor e ordenação final.
    // Critério 1: Frequência decrescente. Critério 2: Ordem alfabética.
    vector<pair<string, int>> entries(frequency.begin(), frequency.end());
    sort(entries.begin(), entries.end(),
              [](const pair<string, int>& a, const pair<string, int>& b) {
                  if (a.second != b.second) {
                      return a.second > b.second;
                  }
                  return a.first < b.first; 
              });

    const size_t topCount = min<size_t>(5, entries.size());
    result.top5Frequencies.assign(entries.begin(),
                                   entries.begin() + static_cast<ptrdiff_t>(topCount));

    return result;
}

// =============================================================================
// [MISSÃO 4.4 - FIM]
// =============================================================================

/* STUBS_ALUNO

Substituições literais para o esqueleto do aluno. Para cada tag abaixo,
troque INTEIRAMENTE o conteúdo entre "// [MISSÃO ... - INÍCIO ...]" e
"// [MISSÃO ... - FIM]" no arquivo Syntax.cpp pelo bloco correspondente.
As assinaturas em Syntax.hpp NÃO mudam.

--------------------------------------------------------------------------
[MISSÃO 3.1] — tabelas vazias: nenhuma palavra-chave nem tipo é registrado.
NOTA: isKeyword() e isType() (infraestrutura não tagueada, já prontas)
continuam funcionando sem alteração: com keywords_/types_ vazios, ambas
sempre retornam false, então tokenize() classifica toda palavra como
"identifier" — ou seja, colorFor() sempre cai no fallback de "default"
para as categorias "keyword"/"type", e o editor roda normalmente, só que
sem destacar nenhuma palavra reservada. O sistema de temas
(applyThemePalette(), Item D) e setTheme()/theme() SEGUEM funcionando à
parte, pois são infraestrutura de apresentação, não desta missão: mesmo
com as tabelas vazias, a cor de fundo e a cor padrão de texto do tema
ativo continuam sendo aplicadas normalmente.
--------------------------------------------------------------------------

SyntaxHighlighter::SyntaxHighlighter() {
    // Intencionalmente vazio: nenhuma palavra-chave nem tipo é registrado
    // nesta versão.
    applyThemePalette();
}

--------------------------------------------------------------------------
[MISSÃO 3.2] — validação sempre "passa" (ignora o conteúdo de `text`).

bool ScopeValidator::validate(const string& text, string* mismatchDetail) {
    (void)text;
    (void)mismatchDetail;
    return true; // Validação de escopo ainda não implementada.
}

--------------------------------------------------------------------------
[MISSÃO 3.3] — inserção vira no-op: a árvore nunca cresce.
NOTA: como insert() deixa de chamar insertNode(), esse helper privado não
precisa ser definido nesta versão (apenas continua declarado em
Syntax.hpp) — em C++ isso é válido desde que a função nunca seja
referenciada. Pode ser omitido do Syntax.cpp do aluno sem qualquer efeito
observável.

void VariableIndex::insert(const string& name) {
    (void)name; // Intencionalmente vazio: nenhuma variável é indexada.
}

--------------------------------------------------------------------------
[MISSÃO 4.3] — autocomplete sempre retorna uma lista vazia.
NOTA: pela mesma razão da Missão 3.3, collectWithPrefix() não precisa ser
definido nesta versão (apenas declarado em Syntax.hpp) e pode ser omitido
do Syntax.cpp do aluno sem qualquer efeito observável.

vector<string> VariableIndex::autocomplete(const string& prefix) const {
    (void)prefix;
    return {}; // Intencionalmente vazio: autocomplete ainda não implementado.
}

--------------------------------------------------------------------------
[MISSÃO 4.4] — retorna um TextStatsResult zerado (struct default), sem
calcular estatística alguma.

TextStatsResult TextStatistics::compute(const vector<string>& lines) {
    (void)lines;
    return TextStatsResult{}; // Todos os campos em seus valores padrão (zerados).
}

*/