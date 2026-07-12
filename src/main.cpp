// - Ctrl+F (Busca): Usa linearSearch (Missão 1.2, substring). O Enter busca a próxima ocorrência (com wraparound), Esc sai do modo.
// - Ctrl+O (Ordenar): Aciona mergeSort (Missão 4.2). O insertionSort (Missão 1.4) segue disponível na API.
// - Navegação: Emulação de "goto linha" via repetição de moveCursorUp/Down para não quebrar a API restrita do TextBuffer.
// - Autocompletar: O VariableIndex é reconstruído a cada quadro (decisão didática aceitável, pois o custo é desprezível nestes cenários).
// - Resize da Tela: Avaliado de forma síncrona (blocking read). O redimensionamento é aplicado visualmente na próxima tecla.
// - Teclas especiais: Tab emula 2 espaços. A tecla Delete ("\033[3~") é descartada via fallback de unknown.
// - I/O de Arquivos: Leitura tolera CRLF, gravação força LF ('\n').
// - Gutter e UI: Calha lateral com largura dinâmica adaptada ao número de linhas do buffer.
// - Ctrl+S (Save As): Se não houver nome de arquivo no arranque, entra em modo interativo de criação.
// - Syntax Highlighting: Aplicado estritamente em arquivos com extensão ".cpp".
// - Ctrl+T (Temas): Alterna ciclicamente entre temas, repintando fundo e tokens na transição.
// - Ctrl+W (Estatísticas): Exibe painel inferior (Missão 4.4) fixado em 13 linhas de altura para estabilizar o cálculo de viewport.

#include "Terminal.hpp"
#include "TextBuffer.hpp"
#include "Syntax.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

namespace {

// Linhas reservadas na base da tela (Status, Mensagens, Autocompletar).
constexpr int kStatusRows = 2;

// Altura fixa da janela de estatísticas (Ctrl+W) para garantir que a aritmética de scroll/cursor seja constante.
constexpr int kStatsBoxRows = 13;

// Estados de interação da interface.
enum class EditorMode { Normal, Search, SaveAs };

// -----------------------------------------------------------------------
// Funções de I/O de Arquivos
// -----------------------------------------------------------------------

// Lê o arquivo para `outLines`, limpando eventuais resíduos de CRLF.
bool tryLoadFile(const string& path, vector<string>& outLines) {
    ifstream input(path);
    if (!input.is_open()) {
        return false;
    }

    string line;
    while (getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        outLines.push_back(line);
    }
    return true;
}

// Grava o buffer no disco forçando terminação nativa LF ('\n').
bool trySaveFile(const string& path, const vector<string>& lines) {
    ofstream output(path, ios::trunc);
    if (!output.is_open()) {
        return false;
    }
    for (const string& line : lines) {
        output << line << '\n';
    }
    return output.good();
}

// -----------------------------------------------------------------------
// Utilitários de Cursor, Autocompletar e Busca
// -----------------------------------------------------------------------

// Move fisicamente o cursor do buffer para `targetRow` respeitando limites.
void moveCursorToRow(TextBuffer& buffer, size_t targetRow) {
    Cursor cursor = buffer.getCursor();

    while (cursor.row < targetRow) {
        buffer.moveCursorDown();
        Cursor next = buffer.getCursor();
        if (next.row == cursor.row) {
            break; 
        }
        cursor = next;
    }
    while (cursor.row > targetRow) {
        buffer.moveCursorUp();
        Cursor next = buffer.getCursor();
        if (next.row == cursor.row) {
            break; 
        }
        cursor = next;
    }
}

bool isIdentifierBodyChar(char c) noexcept {
    return isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

// Isola o prefixo do identificador colado ao cursor atual para a busca no VariableIndex.
string currentWordPrefix(const TextBuffer& buffer) {
    Cursor cursor = buffer.getCursor();
    if (cursor.row >= buffer.lineCount()) {
        return {};
    }

    const string& line = buffer.getLine(cursor.row);
    const size_t col = (cursor.col <= line.size()) ? cursor.col : line.size();

    size_t start = col;
    while (start > 0 && isIdentifierBodyChar(line[start - 1])) {
        --start;
    }
    return line.substr(start, col - start);
}

// Busca iterativa por substring com wraparound para a função de "Próxima Ocorrência" (Ctrl+F).
long findNextOccurrence(const TextBuffer& buffer, const string& needle, long startRow) {
    if (needle.empty()) {
        return -1;
    }
    const size_t n = buffer.lineCount();
    if (n == 0) {
        return -1;
    }
    const size_t start = (startRow < 0) ? 0 : static_cast<size_t>(startRow) % n;
    for (size_t offset = 0; offset < n; ++offset) {
        const size_t idx = (start + offset) % n;
        if (buffer.getLine(idx).find(needle) != string::npos) {
            return static_cast<long>(idx);
        }
    }
    return -1;
}

// Guarda de segurança que impede "SaveAs" em arquivos com nomes invisíveis/vazios.
bool isBlank(const string& s) {
    for (char c : s) {
        if (isspace(static_cast<unsigned char>(c)) == 0) {
            return false;
        }
    }
    return true;
}

// Verifica se a extensão do arquivo exige colorização de sintaxe.
bool hasCppExtension(const string& path) {
    static const string kExt = ".cpp";
    if (path.size() < kExt.size()) {
        return false;
    }
    return path.compare(path.size() - kExt.size(), kExt.size(), kExt) == 0;
}

// Ciclo de seleção de cores do Ctrl+T.
Theme nextTheme(Theme current) {
    switch (current) {
        case Theme::Monokai: return Theme::Dracula;
        case Theme::Dracula: return Theme::Light;
        case Theme::Light:   return Theme::Monokai;
    }
    return Theme::Monokai;
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 2) {
        cerr << "Uso: " << argv[0] << " [caminho-do-arquivo]\n";
        return EXIT_FAILURE;
    }

    // Passou a ser mutável para suportar atualizações no modo "Save As".
    string filePath = (argc == 2) ? string(argv[1]) : string();

    // Carregamento prévio, facilitando exibição de eventuais erros em STDERR antes do Raw Mode.
    TextBuffer buffer;
    if (!filePath.empty()) {
        vector<string> initialLines;
        if (tryLoadFile(filePath, initialLines) && !initialLines.empty()) {
            buffer.loadLines(initialLines);
        }
    }

    TerminalGuard guard;
    if (!guard.isRawModeActive()) {
        cerr << "Erro: nao foi possivel colocar o terminal em modo raw.\n";
        return EXIT_FAILURE;
    }

    Terminal::clearScreenOnce(); 

    SyntaxHighlighter highlighter;
    Terminal::applyThemeColors(highlighter.theme()); // Aplica cor de fundo base globalmente.

    EditorMode mode = EditorMode::Normal;
    string searchQuery;
    long lastFoundRow = -1; 
    string saveAsBuffer; 
    string transientMessage;
    bool showStats = false; 
    size_t topLine = 0; 
    bool running = true;

    while (running) {
        // -- 1) Dimensões do Terminal (com fallback documentado)
        int rows = 24;
        int cols = 80;
        Terminal::getWindowSize(rows, cols); 
        (void)cols; 

        // -- 2) Largura dinâmica da Calha Lateral (Gutter)
        const int gutterWidth =
            static_cast<int>(to_string(buffer.lineCount()).size());
        const int gutterPrefixWidth = gutterWidth + 3; // N dígitos + " │ "

        const int statsRows = showStats ? kStatsBoxRows : 0;
        const int reservedRows = kStatusRows + statsRows;
        const int textAreaHeight = (rows > reservedRows) ? (rows - reservedRows) : 1;

        // -- 3) Controle de Viewport (Scroll Vertical)
        const Cursor cursor = buffer.getCursor();
        if (cursor.row < topLine) {
            topLine = cursor.row;
        } else if (cursor.row >= topLine + static_cast<size_t>(textAreaHeight)) {
            topLine = cursor.row - static_cast<size_t>(textAreaHeight) + 1;
        }

        // -- 4) Atualização do Índice de Autocompletar
        VariableIndex variables;
        for (size_t i = 0; i < buffer.lineCount(); ++i) {
            IdentifierScanner::scanInto(buffer.getLine(i), highlighter, variables);
        }

        const bool syntaxActive = hasCppExtension(filePath); 
        const string& gutterColor = highlighter.colorFor("comment"); 
        const string& resetColor = highlighter.colorFor("default");

        // -- 5) Renderização da Área de Edição
        string frame = "\033[H";
        for (int screenRow = 0; screenRow < textAreaHeight; ++screenRow) {
            const size_t bufferRow = topLine + static_cast<size_t>(screenRow);
            if (bufferRow < buffer.lineCount()) {
                const string numStr = to_string(bufferRow + 1);
                const string padding(
                    static_cast<size_t>(gutterWidth) > numStr.size()
                        ? static_cast<size_t>(gutterWidth) - numStr.size()
                        : 0,
                    ' ');
                frame += gutterColor + padding + numStr + " │ " + resetColor;
                frame += syntaxActive ? highlighter.highlightLine(buffer.getLine(bufferRow))
                                      : buffer.getLine(bufferRow);
            } else {
                frame += gutterColor + string(static_cast<size_t>(gutterWidth), ' ') +
                         " │ " + resetColor;
                frame += "~"; // Estilo vi: indicativo de fora-de-limites.
            }
            frame += "\033[K\r\n";
        }

        // -- 5b) Renderização do Painel de Estatísticas
        if (showStats) {
            const TextStatsResult stats = TextStatistics::compute(buffer.lines());

            vector<string> boxLines;
            boxLines.push_back("╭──────────────────────");
            boxLines.push_back("│  Estatísticas do texto");
            boxLines.push_back("├──────────────────────");
            boxLines.push_back("│  Palavras únicas: " + to_string(stats.uniqueWords));
            {
                char avgBuf[64];
                snprintf(avgBuf, sizeof(avgBuf), "%.2f", stats.avgLength);
                boxLines.push_back(string("│  Comprimento médio: ") + avgBuf);
            }
            boxLines.push_back("│  Maior palavra: " +
                                (stats.longestWord.empty() ? string("-") : stats.longestWord));
            boxLines.push_back("│  Top 5 mais frequentes:");
            for (int i = 0; i < 5; ++i) {
                if (static_cast<size_t>(i) < stats.top5Frequencies.size()) {
                    const auto& entry = stats.top5Frequencies[static_cast<size_t>(i)];
                    boxLines.push_back("│    " + to_string(i + 1) + ". " + entry.first +
                                        " (" + to_string(entry.second) + ")");
                } else {
                    boxLines.push_back("│");
                }
            }
            boxLines.push_back("╰──────────────────────");

            for (const string& boxLine : boxLines) {
                frame += boxLine;
                frame += "\033[K\r\n";
            }
        }

        // -- 6) Renderização: Barra de Status 1
        string wholeBuffer;
        for (size_t i = 0; i < buffer.lineCount(); ++i) {
            wholeBuffer += buffer.getLine(i);
            wholeBuffer += '\n';
        }
        string scopeDetail;
        const bool scopeOk = ScopeValidator::validate(wholeBuffer, &scopeDetail);

        string modeLabel = "-- EDIT --";
        if (mode == EditorMode::Search) {
            modeLabel = "-- BUSCA --";
        } else if (mode == EditorMode::SaveAs) {
            modeLabel = "-- SALVAR COMO --";
        }

        string statusLine1 = modeLabel + " | ";
        statusLine1 += filePath.empty() ? "[Novo Arquivo]" : filePath;
        statusLine1 += " | Ln " + to_string(cursor.row + 1) +
                        ", Col " + to_string(cursor.col + 1) + " | ";
        if (scopeOk) {
            statusLine1 += "\033[32mEscopo: OK\033[0m";
        } else {
            statusLine1 += "\033[31mErro de Escopo: " + scopeDetail + "\033[0m";
        }
        frame += resetColor + statusLine1;
        frame += "\033[K\r\n";

        // -- 7) Renderização: Barra de Status 2 (Prompts e Notificações)
        string statusLine2;
        if (mode == EditorMode::SaveAs) {
            statusLine2 = "Salvar como (Enter=confirmar / Esc=cancelar): " + saveAsBuffer;
        } else if (mode == EditorMode::Search) {
            statusLine2 = "Buscar (Enter=proxima / Esc=cancelar): " + searchQuery;
        } else if (!transientMessage.empty()) {
            statusLine2 = transientMessage;
            transientMessage.clear();
        } else {
            const string prefix = currentWordPrefix(buffer);
            if (!prefix.empty()) {
                const vector<string> suggestions = variables.autocomplete(prefix);
                const bool onlyExactMatch = (suggestions.size() == 1 && suggestions.front() == prefix);
                if (!suggestions.empty() && !onlyExactMatch) {
                    statusLine2 = "Sugestoes: ";
                    for (size_t i = 0; i < suggestions.size(); ++i) {
                        if (i > 0) {
                            statusLine2 += ", ";
                        }
                        statusLine2 += suggestions[i];
                    }
                }
            }
        }
        frame += statusLine2;
        frame += "\033[K";

        // -- 8) Posicionamento dinâmico do cursor na tela
        if (mode == EditorMode::SaveAs) {
            const int screenRow = rows;
            const int screenCol = static_cast<int>(statusLine2.size()) + 1;
            frame += "\033[" + to_string(screenRow) + ";" + to_string(screenCol) + "H";
        } else if (mode == EditorMode::Search) {
            const int screenRow = rows;
            const int screenCol = static_cast<int>(statusLine2.size()) + 1;
            frame += "\033[" + to_string(screenRow) + ";" + to_string(screenCol) + "H";
        } else {
            const int screenRow = static_cast<int>(cursor.row - topLine) + 1;
            const int screenCol = gutterPrefixWidth + static_cast<int>(cursor.col) + 1;
            frame += "\033[" + to_string(screenRow) + ";" + to_string(screenCol) + "H";
        }

        Terminal::renderFrame(frame);

        // -- 9) Tratamento de Entrada e Loop de Eventos
        const KeyEvent event = Terminal::readKeyEvent();

        if (event.type == KeyType::Eof ||
            (event.type == KeyType::CtrlKey && event.ch == 'q')) {
            running = false;
            continue;
        }

        if (mode == EditorMode::SaveAs) {
            switch (event.type) {
                case KeyType::Char:
                    saveAsBuffer += event.ch;
                    break;
                case KeyType::Backspace:
                    if (!saveAsBuffer.empty()) {
                        saveAsBuffer.pop_back();
                    }
                    break;
                case KeyType::Enter: {
                    if (isBlank(saveAsBuffer)) {
                        transientMessage = "Aviso: nome de arquivo invalido. Salvamento cancelado.";
                    } else if (trySaveFile(saveAsBuffer, buffer.lines())) {
                        filePath = saveAsBuffer;
                        transientMessage = "Arquivo salvo como " + filePath;
                    } else {
                        transientMessage = "Erro ao salvar o arquivo!";
                    }
                    mode = EditorMode::Normal;
                    saveAsBuffer.clear();
                    break;
                }
                case KeyType::Escape:
                    mode = EditorMode::Normal;
                    saveAsBuffer.clear();
                    break;
                default:
                    break; 
            }
            continue;
        }

        if (mode == EditorMode::Search) {
            switch (event.type) {
                case KeyType::Char:
                    searchQuery += event.ch;
                    lastFoundRow = -1; 
                    break;
                case KeyType::Backspace:
                    if (!searchQuery.empty()) {
                        searchQuery.pop_back();
                    }
                    lastFoundRow = -1; 
                    break;
                case KeyType::Enter: {
                    const long searchFrom = (lastFoundRow < 0) ? 0 : (lastFoundRow + 1);
                    const long found = findNextOccurrence(buffer, searchQuery, searchFrom);
                    if (found >= 0) {
                        moveCursorToRow(buffer, static_cast<size_t>(found));
                        lastFoundRow = found;
                        transientMessage = "Encontrado na linha " + to_string(found + 1);
                    } else {
                        transientMessage = "Nao encontrado: " + searchQuery;
                    }
                    break;
                }
                case KeyType::Escape:
                    mode = EditorMode::Normal;
                    searchQuery.clear();
                    lastFoundRow = -1;
                    break;
                default:
                    break; 
            }
            continue;
        }

        // -- Modo de Edição Padrão --
        switch (event.type) {
            case KeyType::ArrowUp:
                buffer.moveCursorUp();
                break;
            case KeyType::ArrowDown:
                buffer.moveCursorDown();
                break;
            case KeyType::ArrowLeft:
                buffer.moveCursorLeft();
                break;
            case KeyType::ArrowRight:
                buffer.moveCursorRight();
                break;
            case KeyType::Enter:
                buffer.insertLine();
                break;
            case KeyType::Backspace:
                buffer.deleteChar();
                break;
            case KeyType::Tab:
                buffer.insertChar(' ');
                buffer.insertChar(' ');
                break;
            case KeyType::CtrlKey:
                switch (event.ch) {
                    case 'u':
                        buffer.undo();
                        break;
                    case 'r':
                        buffer.redo();
                        break;
                    case 'f':
                        mode = EditorMode::Search;
                        searchQuery.clear();
                        lastFoundRow = -1;
                        break;
                    case 's':
                        if (filePath.empty()) {
                            mode = EditorMode::SaveAs;
                            saveAsBuffer.clear();
                        } else if (trySaveFile(filePath, buffer.lines())) {
                            transientMessage = "Arquivo salvo!";
                        } else {
                            transientMessage = "Erro ao salvar o arquivo!";
                        }
                        break;
                    case 'o':
                        buffer.mergeSort();
                        break;
                    case 't': {
                        const Theme chosen = nextTheme(highlighter.theme());
                        highlighter.setTheme(chosen);
                        Terminal::applyThemeColors(chosen);
                        break;
                    }
                    case 'w':
                        showStats = !showStats;
                        break;
                    default:
                        break; 
                }
                break;
            case KeyType::Char:
                buffer.insertChar(event.ch);
                break;
            case KeyType::Escape:
            case KeyType::Unknown:
            default:
                break; 
        }
    }

    Terminal::resetAttributes();  
    Terminal::clearScreenOnce();  
    return EXIT_SUCCESS;
}