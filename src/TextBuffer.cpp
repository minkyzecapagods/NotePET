#include "TextBuffer.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <utility>

using namespace std;

// Detalhe de implementação restrito a este arquivo (file-scope),
// não faz parte do contrato público da classe.
namespace {
constexpr size_t kInitialCapacity = 4;
} // namespace

// =============================================================================
// Construtor TextBuffer
// =============================================================================

TextBuffer::TextBuffer() {
    lines_.emplace_back(); // Inicia com uma linha vazia, cursor em (0, 0)
}

// =============================================================================
// [MISSÃO 1.1 - INÍCIO: Armazenamento de linhas expansível alocado no heap com unique_ptr]
// =============================================================================

RawLineStore::RawLineStore()
    : data_(make_unique<string[]>(kInitialCapacity)),
      size_(0),
      capacity_(kInitialCapacity) {}

void RawLineStore::append(const string& line) {
    if (size_ == capacity_) {
        grow();
    }
    data_[size_] = line;
    ++size_;
}

void RawLineStore::grow() {
    // Realocação de array dinâmico: dobra a capacidade e 
    // transfere a posse dos elementos automaticamente via std::unique_ptr.
    size_t newCapacity = (capacity_ == 0) ? kInitialCapacity : capacity_ * 2;
    unique_ptr<string[]> newData = make_unique<string[]>(newCapacity);
    for (size_t i = 0; i < size_; ++i) {
        newData[i] = move(data_[i]);
    }
    data_ = move(newData);
    capacity_ = newCapacity;
}

const string& RawLineStore::at(size_t index) const {
    if (index >= size_) {
        throw out_of_range("RawLineStore::at: index out of range");
    }
    return data_[index];
}

size_t RawLineStore::size() const noexcept {
    return size_;
}

size_t RawLineStore::capacity() const noexcept {
    return capacity_;
}

// =============================================================================
// [MISSÃO 1.1 - FIM]
// =============================================================================

// =============================================================================
// [MISSÃO 1.2 - INÍCIO: Pesquisa sequencial (linear) de substring nas linhas]
// =============================================================================

long TextBuffer::linearSearch(const string& needle) const {
    for (size_t i = 0; i < lines_.size(); ++i) {
        if (lines_[i].find(needle) != string::npos) {
            return static_cast<long>(i);
        }
    }
    return -1;
}

// =============================================================================
// [MISSÃO 1.2 - FIM]
// =============================================================================

// =============================================================================
// [MISSÃO 1.3 - INÍCIO: Pesquisa binária por correspondência exata em linhas ordenadas]
// =============================================================================

long TextBuffer::binarySearch(const string& needle) const {
    // Pré-requisito: o vetor lines_ já deve estar lexicograficamente ordenado.
    // Esta função assume a ordenação e não faz verificações preventivas.
    if (lines_.empty()) {
        return -1;
    }

    long low = 0;
    long high = static_cast<long>(lines_.size()) - 1;

    while (low <= high) {
        long mid = low + (high - low) / 2;
        const string& candidate = lines_[static_cast<size_t>(mid)];

        if (candidate == needle) {
            return mid;
        }
        if (candidate < needle) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    return -1;
}

// =============================================================================
// [MISSÃO 1.3 - FIM]
// =============================================================================

// =============================================================================
// [MISSÃO 1.4 - INÍCIO: Base de ordenação por inserção (Insertion sort) para ordenação alfabética]
// =============================================================================

void TextBuffer::insertionSort() {
    for (size_t i = 1; i < lines_.size(); ++i) {
        string key = lines_[i];
        size_t j = i;
        while (j > 0 && lines_[j - 1] > key) {
            lines_[j] = lines_[j - 1];
            --j;
        }
        lines_[j] = key;
    }
}

// =============================================================================
// [MISSÃO 1.4 - FIM]
// =============================================================================

// =============================================================================
// [MISSÃO 4.1 - INÍCIO: Histórico de anular/refazer (Undo/Redo) usando duas pilhas de comandos]
// =============================================================================

// Cada Command armazena apenas o estado estritamente necessário para desfazer/refazer
// uma edição específica. A execução delega para os mutadores brutos (raw*) para
// não engatilhar novas inserções no histórico acidentalmente.

class InsertCharCommand : public Command {
public:
    InsertCharCommand(Cursor before, Cursor after, char inserted)
        : before_(before), after_(after), inserted_(inserted) {}

    void undo(TextBuffer& buffer) override {
        buffer.rawDeleteCharAt(before_.row, before_.col);
        buffer.cursor_ = before_;
    }

    void redo(TextBuffer& buffer) override {
        buffer.rawInsertCharAt(before_.row, before_.col, inserted_);
        buffer.cursor_ = after_;
    }

private:
    Cursor before_;
    Cursor after_;
    char inserted_;
};

class DeleteCharCommand : public Command {
public:
    DeleteCharCommand(Cursor before, Cursor after, char removed)
        : before_(before), after_(after), removed_(removed) {}

    void undo(TextBuffer& buffer) override {
        buffer.rawInsertCharAt(after_.row, after_.col, removed_);
        buffer.cursor_ = before_;
    }

    void redo(TextBuffer& buffer) override {
        buffer.rawDeleteCharAt(after_.row, after_.col);
        buffer.cursor_ = after_;
    }

private:
    Cursor before_;
    Cursor after_;
    char removed_;
};

class MergeLineCommand : public Command {
public:
    MergeLineCommand(Cursor before, Cursor after, size_t splitColumn)
        : before_(before), after_(after), splitColumn_(splitColumn) {}

    void undo(TextBuffer& buffer) override {
        buffer.rawSplitLineAt(after_.row, splitColumn_);
        buffer.cursor_ = before_;
    }

    void redo(TextBuffer& buffer) override {
        buffer.rawMergeLineIntoPrevious(before_.row);
        buffer.cursor_ = after_;
    }

private:
    Cursor before_;
    Cursor after_;
    size_t splitColumn_;
};

class SplitLineCommand : public Command {
public:
    SplitLineCommand(Cursor before, Cursor after)
        : before_(before), after_(after) {}

    void undo(TextBuffer& buffer) override {
        buffer.rawMergeLineIntoPrevious(after_.row);
        buffer.cursor_ = before_;
    }

    void redo(TextBuffer& buffer) override {
        buffer.rawSplitLineAt(before_.row, before_.col);
        buffer.cursor_ = after_;
    }

private:
    Cursor before_;
    Cursor after_;
};

class DeleteLineCommand : public Command {
public:
    DeleteLineCommand(Cursor before, Cursor after, size_t index, string content)
        : before_(before), after_(after), index_(index), content_(move(content)) {}

    void undo(TextBuffer& buffer) override {
        buffer.rawInsertLineAt(index_, content_);
        buffer.cursor_ = before_;
    }

    void redo(TextBuffer& buffer) override {
        buffer.rawRemoveLineAt(index_);
        buffer.cursor_ = after_;
    }

private:
    Cursor before_;
    Cursor after_;
    size_t index_;
    string content_;
};

void TextBuffer::pushCommand(unique_ptr<Command> command) {
    undoStack_.push(move(command));
    // Uma nova edição do usuário invalida a linha do tempo anterior.
    // Todo o histórico de "refazer" obsoleto deve ser descartado.
    while (!redoStack_.empty()) {
        redoStack_.pop();
    }
}

void TextBuffer::undo() {
    if (undoStack_.empty()) {
        return;
    }
    unique_ptr<Command> command = move(undoStack_.top());
    undoStack_.pop();
    command->undo(*this);
    redoStack_.push(move(command));
}

void TextBuffer::redo() {
    if (redoStack_.empty()) {
        return;
    }
    unique_ptr<Command> command = move(redoStack_.top());
    redoStack_.pop();
    command->redo(*this);
    undoStack_.push(move(command));
}

// =============================================================================
// [MISSÃO 4.1 - FIM]
// =============================================================================

// =============================================================================
// [MISSÃO 2.1 - INÍCIO: TAD TextBuffer (operações ao nível da linha sobre vector)]
// =============================================================================

void TextBuffer::rawSplitLineAt(size_t row, size_t col) {
    auto lineIt = lines_.begin() + static_cast<ptrdiff_t>(row);
    auto splitPoint = lineIt->begin() + static_cast<ptrdiff_t>(col);

    string tail(splitPoint, lineIt->end());
    lineIt->erase(splitPoint, lineIt->end());
    lines_.insert(lineIt + 1, tail);
}

void TextBuffer::rawMergeLineIntoPrevious(size_t row) {
    // Mescla a linha atual no fim da linha anterior e em seguida a remove.
    auto currentIt = lines_.begin() + static_cast<ptrdiff_t>(row);
    auto prevIt = prev(currentIt);
    prevIt->insert(prevIt->end(), currentIt->begin(), currentIt->end());
    lines_.erase(currentIt);
}

void TextBuffer::rawInsertLineAt(size_t index, const string& content) {
    lines_.insert(lines_.begin() + static_cast<ptrdiff_t>(index), content);
}

string TextBuffer::rawRemoveLineAt(size_t index) {
    auto it = lines_.begin() + static_cast<ptrdiff_t>(index);
    string removed = *it;
    lines_.erase(it);
    return removed;
}

void TextBuffer::insertLine() {
    if (lines_.empty()) {
        lines_.emplace_back();
        cursor_ = Cursor{0, 0};
    }

    Cursor before = cursor_;
    rawSplitLineAt(cursor_.row, cursor_.col);
    cursor_.row += 1;
    cursor_.col = 0;

    pushCommand(make_unique<SplitLineCommand>(before, cursor_));
}

void TextBuffer::deleteLine(size_t lineIndex) {
    if (lineIndex >= lines_.size()) {
        return; // Índice inválido, operação ignorada silenciosamente.
    }

    Cursor before = cursor_;
    string removed = rawRemoveLineAt(lineIndex);

    if (lines_.empty()) {
        lines_.emplace_back();
    }
    if (cursor_.row >= lines_.size()) {
        cursor_.row = lines_.size() - 1;
    }
    cursor_.col = min(cursor_.col, lines_[cursor_.row].size());

    pushCommand(make_unique<DeleteLineCommand>(before, cursor_, lineIndex, move(removed)));
}

const string& TextBuffer::getLine(size_t lineIndex) const {
    return lines_.at(lineIndex);
}

size_t TextBuffer::lineCount() const noexcept {
    return lines_.size();
}

// =============================================================================
// [MISSÃO 2.1 - FIM]
// =============================================================================

// =============================================================================
// [MISSÃO 2.2 - INÍCIO: Operações de cursor ao nível do carácter usando iteradores da STL]
// =============================================================================

size_t TextBuffer::rawInsertCharAt(size_t row, size_t col, char c) {
    auto lineIt = lines_.begin() + static_cast<ptrdiff_t>(row);
    auto pos = lineIt->begin() + static_cast<ptrdiff_t>(col);
    auto insertedIt = lineIt->insert(pos, c);
    
    // A nova coluna é calculada via iteradores (distance) e não deslocamento manual
    // para assegurar coerência absoluta com a STL.
    return static_cast<size_t>(distance(lineIt->begin(), insertedIt)) + 1;
}

char TextBuffer::rawDeleteCharAt(size_t row, size_t col) {
    auto lineIt = lines_.begin() + static_cast<ptrdiff_t>(row);
    auto pos = lineIt->begin() + static_cast<ptrdiff_t>(col);
    char removed = *pos;
    lineIt->erase(pos);
    return removed;
}

void TextBuffer::insertChar(char c) {
    if (lines_.empty()) {
        lines_.emplace_back();
        cursor_ = Cursor{0, 0};
    }

    Cursor before = cursor_;
    size_t newCol = rawInsertCharAt(cursor_.row, cursor_.col, c);
    cursor_.col = newCol;

    pushCommand(make_unique<InsertCharCommand>(before, cursor_, c));
}

void TextBuffer::deleteChar() {
    if (cursor_.col > 0) {
        // Caso básico: deleta o caractere na mesma linha, à esquerda.
        Cursor before = cursor_;
        char removed = rawDeleteCharAt(cursor_.row, cursor_.col - 1);
        cursor_.col -= 1;

        pushCommand(make_unique<DeleteCharCommand>(before, cursor_, removed));
        return;
    }

    if (cursor_.row > 0) {
        // Início de linha: Backspace remove a quebra de linha implícita,
        // concatenando esta linha à linha anterior.
        Cursor before = cursor_;
        size_t prevLen = lines_[cursor_.row - 1].size();
        rawMergeLineIntoPrevious(cursor_.row);
        cursor_.row -= 1;
        cursor_.col = prevLen;

        pushCommand(make_unique<MergeLineCommand>(before, cursor_, prevLen));
        return;
    }

    // Cursor em (0, 0): nada a fazer. Ignorado silenciosamente.
}

void TextBuffer::moveCursorLeft() {
    auto lineIt = lines_.begin() + static_cast<ptrdiff_t>(cursor_.row);
    auto pos = lineIt->begin() + static_cast<ptrdiff_t>(cursor_.col);

    if (pos != lineIt->begin()) {
        --pos;
        cursor_.col = static_cast<size_t>(distance(lineIt->begin(), pos));
    } else if (lineIt != lines_.begin()) {
        auto prevLineIt = prev(lineIt);
        cursor_.row -= 1;
        cursor_.col = prevLineIt->size();
    }
}

void TextBuffer::moveCursorRight() {
    auto lineIt = lines_.begin() + static_cast<ptrdiff_t>(cursor_.row);
    auto pos = lineIt->begin() + static_cast<ptrdiff_t>(cursor_.col);

    if (pos != lineIt->end()) {
        ++pos;
        cursor_.col = static_cast<size_t>(distance(lineIt->begin(), pos));
    } else if (next(lineIt) != lines_.end()) {
        cursor_.row += 1;
        cursor_.col = 0;
    }
}

void TextBuffer::moveCursorUp() {
    if (cursor_.row == 0) {
        return;
    }
    auto lineIt = lines_.begin() + static_cast<ptrdiff_t>(cursor_.row);
    auto prevLineIt = prev(lineIt);
    cursor_.row -= 1;
    cursor_.col = min(cursor_.col, prevLineIt->size());
}

void TextBuffer::moveCursorDown() {
    auto lineIt = lines_.begin() + static_cast<ptrdiff_t>(cursor_.row);
    if (next(lineIt) == lines_.end()) {
        return;
    }
    auto nextLineIt = next(lineIt);
    cursor_.row += 1;
    cursor_.col = min(cursor_.col, nextLineIt->size());
}

Cursor TextBuffer::getCursor() const noexcept {
    return cursor_;
}

// =============================================================================
// [MISSÃO 2.2 - FIM]
// =============================================================================


// =============================================================================
// [MISSÃO 4.2 - INÍCIO: Ordenação por fusão (Merge sort) para ordenação alfabética]
// =============================================================================

void TextBuffer::mergeSort() {
    if (lines_.size() < 2) {
        return;
    }
    vector<string> scratch(lines_.size());
    mergeSortRange(lines_, 0, lines_.size() - 1, scratch);
}

void TextBuffer::mergeSortRange(vector<string>& data,
                                 size_t left, size_t right,
                                 vector<string>& scratch) {
    if (left >= right) {
        return;
    }
    size_t mid = left + (right - left) / 2;
    mergeSortRange(data, left, mid, scratch);
    mergeSortRange(data, mid + 1, right, scratch);
    mergeRanges(data, left, mid, right, scratch);
}

void TextBuffer::mergeRanges(vector<string>& data,
                              size_t left, size_t mid, size_t right,
                              vector<string>& scratch) {
    size_t i = left;
    size_t j = mid + 1;
    size_t k = left;

    while (i <= mid && j <= right) {
        if (data[i] <= data[j]) {
            scratch[k++] = data[i++];
        } else {
            scratch[k++] = data[j++];
        }
    }
    while (i <= mid) {
        scratch[k++] = data[i++];
    }
    while (j <= right) {
        scratch[k++] = data[j++];
    }
    for (size_t x = left; x <= right; ++x) {
        data[x] = scratch[x];
    }
}

// =============================================================================
// [MISSÃO 4.2 - FIM]
// =============================================================================

// =============================================================================
// Helpers estáticos de infraestrutura e carregamento de arquivos
// =============================================================================

void TextBuffer::loadLines(const vector<string>& lines) {
    // Utiliza a estrutura base do RawLineStore (Missão 1.1) durante o processo didático
    // de ingestão de dados, e em seguida transfere o conteúdo para o std::vector principal.
    RawLineStore rawStore;
    for (const auto& line : lines) {
        rawStore.append(line);
    }

    lines_.clear();
    lines_.reserve(rawStore.size());
    for (size_t i = 0; i < rawStore.size(); ++i) {
        lines_.push_back(rawStore.at(i));
    }
    if (lines_.empty()) {
        lines_.emplace_back();
    }

    cursor_ = Cursor{0, 0};
    while (!undoStack_.empty()) {
        undoStack_.pop();
    }
    while (!redoStack_.empty()) {
        redoStack_.pop();
    }
}

const vector<string>& TextBuffer::lines() const noexcept {
    return lines_;
}

/* STUBS_ALUNO

Substituições literais para o esqueleto do aluno. Para cada tag abaixo,
troque INTEIRAMENTE o conteúdo entre "// [MISSÃO ... - INÍCIO ...]" e
"// [MISSÃO ... - FIM]" no arquivo TextBuffer.cpp pelo bloco correspondente.
As assinaturas em TextBuffer.hpp NÃO mudam.

--------------------------------------------------------------------------
[MISSÃO 1.1] — mantém buffer fixo, sem realocação (sem lógica de "grow").
NOTA: o membro privado data_ continua com o MESMO TIPO declarado no header
(unique_ptr<string[]>); a diferença é que ele é alocado UMA ÚNICA
VEZ, com capacidade fixa, e append() ignora silenciosamente qualquer linha
além dessa capacidade — ou seja, comportamento "array fixo", sem realocar.
--------------------------------------------------------------------------

RawLineStore::RawLineStore()
    : data_(make_unique<string[]>(kInitialCapacity)),
      size_(0),
      capacity_(kInitialCapacity) {}

void RawLineStore::append(const string& line) {
    if (size_ == capacity_) {
        return; // Capacidade fixa atingida: novas linhas são descartadas.
    }
    data_[size_] = line;
    ++size_;
}

void RawLineStore::grow() {
    // No-op proposital: esta versão nunca realoca o buffer.
}

const string& RawLineStore::at(size_t index) const {
    if (index >= size_) {
        throw out_of_range("RawLineStore::at: index out of range");
    }
    return data_[index];
}

size_t RawLineStore::size() const noexcept {
    return size_;
}

size_t RawLineStore::capacity() const noexcept {
    return capacity_;
}

--------------------------------------------------------------------------
[MISSÃO 1.2]

long TextBuffer::linearSearch(const string& needle) const {
    (void)needle;
    return -1;
}

--------------------------------------------------------------------------
[MISSÃO 1.3]

long TextBuffer::binarySearch(const string& needle) const {
    (void)needle;
    return -1;
}

--------------------------------------------------------------------------
[MISSÃO 1.4] — no-op: as linhas permanecem na ordem original.

void TextBuffer::insertionSort() {
    // Intencionalmente vazio: a ordenação ainda não foi implementada.
}

--------------------------------------------------------------------------
[MISSÃO 2.1] — TAD no-op: digitar/editar ainda não altera o buffer.

void TextBuffer::rawSplitLineAt(size_t row, size_t col) {
    (void)row;
    (void)col;
}

void TextBuffer::rawMergeLineIntoPrevious(size_t row) {
    (void)row;
}

void TextBuffer::rawInsertLineAt(size_t index, const string& content) {
    (void)index;
    (void)content;
}

string TextBuffer::rawRemoveLineAt(size_t index) {
    (void)index;
    return string();
}

void TextBuffer::insertLine() {
    // Intencionalmente vazio: nenhuma linha nova é criada.
}

void TextBuffer::deleteLine(size_t lineIndex) {
    (void)lineIndex;
    // Intencionalmente vazio: nenhuma linha é removida.
}

const string& TextBuffer::getLine(size_t lineIndex) const {
    (void)lineIndex;
    static const string kEmpty;
    return kEmpty;
}

size_t TextBuffer::lineCount() const noexcept {
    return 0;
}

--------------------------------------------------------------------------
[MISSÃO 2.2] — cursor "parado": movimentação e edição de caracteres não
produzem efeito observável.

size_t TextBuffer::rawInsertCharAt(size_t row, size_t col, char c) {
    (void)row;
    (void)c;
    return col; // devolve a mesma coluna: nada muda.
}

char TextBuffer::rawDeleteCharAt(size_t row, size_t col) {
    (void)row;
    (void)col;
    return '\0';
}

void TextBuffer::insertChar(char c) {
    (void)c;
    // Intencionalmente vazio: o caractere não é inserido.
}

void TextBuffer::deleteChar() {
    // Intencionalmente vazio: nenhum caractere é removido.
}

void TextBuffer::moveCursorLeft() {}
void TextBuffer::moveCursorRight() {}
void TextBuffer::moveCursorUp() {}
void TextBuffer::moveCursorDown() {}

Cursor TextBuffer::getCursor() const noexcept {
    return Cursor{0, 0};
}

--------------------------------------------------------------------------
[MISSÃO 4.1] — undo()/redo() como no-op (histórico nunca é usado).

class InsertCharCommand : public Command {
public:
    void undo(TextBuffer&) override {}
    void redo(TextBuffer&) override {}
};

void TextBuffer::pushCommand(unique_ptr<Command> command) {
    (void)command; // Descarta o comando: nenhum histórico é mantido.
}

void TextBuffer::undo() {
    // Intencionalmente vazio.
}

void TextBuffer::redo() {
    // Intencionalmente vazio.
}

NOTA: como pushCommand() descarta o comando recebido, as classes
DeleteCharCommand, MergeLineCommand, SplitLineCommand e DeleteLineCommand
tornam-se código morto nesta versão; podem ser removidas do arquivo do
aluno sem qualquer efeito observável.

--------------------------------------------------------------------------
[MISSÃO 4.2] — reaproveita a Missão 1.4 em vez de implementar Merge Sort.

void TextBuffer::mergeSort() {
    insertionSort();
}

void TextBuffer::mergeSortRange(vector<string>& data,
                                 size_t left, size_t right,
                                 vector<string>& scratch) {
    (void)data;
    (void)left;
    (void)right;
    (void)scratch;
}

void TextBuffer::mergeRanges(vector<string>& data,
                              size_t left, size_t mid, size_t right,
                              vector<string>& scratch) {
    (void)data;
    (void)left;
    (void)mid;
    (void)right;
    (void)scratch;
}

*/