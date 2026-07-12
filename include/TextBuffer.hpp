#ifndef TEXTBUFFER_HPP
#define TEXTBUFFER_HPP

#include <cstddef>
#include <memory>
#include <stack>
#include <string>
#include <vector>

// Premissas de Design:
// - Linhas são armazenadas sem o caractere de quebra de linha ('\n').
// - A busca binária (Missão 1.3) exige um buffer ordenado e faz correspondência exata de linha.
// - Buscas e ordenações operam no mesmo std::vector<std::string> usado na edição em tempo real.
// - RawLineStore (Missão 1.1) é uma estrutura didática isolada para demonstrar alocação dinâmica. 
//   É usada apenas no carregamento em lote e não sincroniza com o buffer ativo depois.
// - Histórico de Undo/Redo (Missão 4.1) rastreia apenas inserções e remoções de texto. 
//   Operações de ordenação em lote são ignoradas pelo histórico.

// ---------------------------------------------------------------------------
// Cursor
// ---------------------------------------------------------------------------

// Rastreia a posição atual via índices (linha, coluna). O uso de iteradores STL
// é evitado no estado persistente, pois eles seriam invalidados a cada realocação
// ou modificação das strings e vetores durante as edições.
struct Cursor {
    std::size_t row = 0;
    std::size_t col = 0;
};

class TextBuffer; // forward declaration; Command precisa da referência

// ---------------------------------------------------------------------------
// Command (Suporte para Missão 4.1)
// ---------------------------------------------------------------------------

// Interface base para registrar edições individuais reversíveis na pilha de histórico.
class Command {
public:
    virtual ~Command() = default;
    virtual void undo(TextBuffer& buffer) = 0;
    virtual void redo(TextBuffer& buffer) = 0;
};

// ---------------------------------------------------------------------------
// RawLineStore (Suporte para Missão 1.1)
// ---------------------------------------------------------------------------

// Estrutura didática para gerenciamento manual de array dinâmico no heap usando 
// std::unique_ptr. Usada estritamente durante o carregamento inicial de arquivos.
class RawLineStore {
public:
    RawLineStore();

    // Anexa uma linha, dobrando a capacidade do array subjacente se necessário.
    void append(const std::string& line);

    // Acesso com verificação de limites (lança std::out_of_range se inválido).
    const std::string& at(std::size_t index) const;

    std::size_t size() const noexcept;
    std::size_t capacity() const noexcept;

private:
    std::unique_ptr<std::string[]> data_;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;

    void grow();
};

// ---------------------------------------------------------------------------
// Declarações antecipadas dos comandos concretos (definidos em TextBuffer.cpp)
// que precisam de acesso (friend) ao estado privado do TextBuffer.
// ---------------------------------------------------------------------------
class InsertCharCommand;
class DeleteCharCommand;
class MergeLineCommand;
class SplitLineCommand;
class DeleteLineCommand;

// ---------------------------------------------------------------------------
// TextBuffer
// ---------------------------------------------------------------------------

class TextBuffer {
public:
    TextBuffer();

    // -- Missão 2.1: Operações base de TAD por linha -------------------------
    void insertLine();                       // Divide a linha atual no cursor (Enter)
    void deleteLine(std::size_t lineIndex);   // Remove uma linha inteira por índice
    const std::string& getLine(std::size_t lineIndex) const;
    std::size_t lineCount() const noexcept;

    // -- Missão 2.2: Operações de caractere via iteradores -------------------
    void insertChar(char c);
    void deleteChar(); // Backspace: remove o caractere antes do cursor
    void moveCursorLeft();
    void moveCursorRight();
    void moveCursorUp();
    void moveCursorDown();
    Cursor getCursor() const noexcept;

    // -- Missão 1.2 / 1.3: Buscas ---------------------------------------------
    long linearSearch(const std::string& needle) const;
    long binarySearch(const std::string& needle) const;

    // -- Missão 1.4 / 4.2: Ordenação ------------------------------------------
    void insertionSort();
    void mergeSort();

    // -- Missão 4.1: Undo/Redo ------------------------------------------------
    void undo();
    void redo();

    // -- Infraestrutura de carregamento e acesso de leitura -------------------
    void loadLines(const std::vector<std::string>& lines);
    const std::vector<std::string>& lines() const noexcept;

private:
    std::vector<std::string> lines_;
    Cursor cursor_;

    std::stack<std::unique_ptr<Command>> undoStack_;
    std::stack<std::unique_ptr<Command>> redoStack_;

    // Permite que os comandos apliquem ou revertam edições acessando o estado interno
    friend class InsertCharCommand;
    friend class DeleteCharCommand;
    friend class MergeLineCommand;
    friend class SplitLineCommand;
    friend class DeleteLineCommand;

    // -- Missão 2.1: Métodos de suporte (mutação de linhas) -------------------
    void rawSplitLineAt(std::size_t row, std::size_t col);
    void rawMergeLineIntoPrevious(std::size_t row);
    void rawInsertLineAt(std::size_t index, const std::string& content);
    std::string rawRemoveLineAt(std::size_t index);

    // -- Missão 2.2: Métodos de suporte (mutação de caracteres) ---------------
    std::size_t rawInsertCharAt(std::size_t row, std::size_t col, char c); // Retorna a nova coluna
    char rawDeleteCharAt(std::size_t row, std::size_t col);                 // Retorna o char removido

    // -- Missão 4.1: Suporte a histórico --------------------------------------
    void pushCommand(std::unique_ptr<Command> command);

    // -- Missão 4.2: Suporte ao Merge Sort ------------------------------------
    static void mergeSortRange(std::vector<std::string>& data,
                                std::size_t left, std::size_t right,
                                std::vector<std::string>& scratch);
    static void mergeRanges(std::vector<std::string>& data,
                             std::size_t left, std::size_t mid, std::size_t right,
                             std::vector<std::string>& scratch);
};

#endif // TEXTBUFFER_HPP