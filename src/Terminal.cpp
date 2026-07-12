#include "Terminal.hpp"

#include "Syntax.hpp" // Definição completa de Theme e ThemePalette (resolve a 
                       // forward declaration feita em Terminal.hpp).

#include <unistd.h>
#include <sys/ioctl.h>
#include <cerrno>
#include <cstdio>

using namespace std;

// ---------------------------------------------------------------------------
// TerminalGuard
// ---------------------------------------------------------------------------

TerminalGuard::TerminalGuard() {
    rawModeActive_ = enableRawMode();
}

TerminalGuard::~TerminalGuard() {
    // Restaura o terminal para o estado original. Garantido de rodar mesmo 
    // durante o stack unwinding (exceções). Destrutores não devem lançar exceções.
    disableRawMode();
}

bool TerminalGuard::isRawModeActive() const noexcept {
    return rawModeActive_;
}

bool TerminalGuard::enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &originalTermios_) == -1) {
        fprintf(stderr, "TerminalGuard: tcgetattr failed (errno=%d)\n", errno);
        return false;
    }
    originalSaved_ = true;

    struct termios raw = originalTermios_;

    // Modo raw:
    // - Desabilita modo canônico e echo (ICANON, ECHO).
    // - ISIG intencionalmente mantido (Ctrl+C e Ctrl+Z disparam sinais normalmente).
    raw.c_lflag &= ~(static_cast<tcflag_t>(ICANON) | static_cast<tcflag_t>(ECHO));

    // Entrada: Desabilita controle de fluxo (IXON) e conversão automática CR -> NL (ICRNL)
    // para identificar a tecla Enter de forma confiável.
    raw.c_iflag &= ~(static_cast<tcflag_t>(IXON) | static_cast<tcflag_t>(ICRNL));

    // Saída: Desabilita conversão automática '\n' -> "\r\n" (OPOST).
    // O editor emitirá as quebras de linha "\r\n" explicitamente.
    raw.c_oflag &= ~(static_cast<tcflag_t>(OPOST));

    // Configuração de leitura bloqueante byte a byte (retorna assim que 1 byte chegar).
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        fprintf(stderr, "TerminalGuard: tcsetattr(raw) failed (errno=%d)\n", errno);
        return false;
    }

    return true;
}

void TerminalGuard::disableRawMode() {
    if (!originalSaved_) {
        return; // tcgetattr nunca teve sucesso; nada foi alterado.
    }
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &originalTermios_) == -1) {
        fprintf(stderr, "TerminalGuard: tcsetattr(restore) failed (errno=%d)\n", errno);
    }
    rawModeActive_ = false;
}

// ---------------------------------------------------------------------------
// Terminal
// ---------------------------------------------------------------------------

bool Terminal::readRawByte(char& outByte) {
    while (true) {
        ssize_t n = read(STDIN_FILENO, &outByte, 1);
        if (n == 1) {
            return true;
        }
        if (n == -1 && errno == EINTR) {
            continue; // Interrompido por sinal (ex: SIGWINCH), tenta novamente.
        }
        return false; // EOF ou erro irrecuperável.
    }
}

KeyEvent Terminal::readKeyEvent() {
    char c = '\0';
    if (!readRawByte(c)) {
        return KeyEvent{KeyType::Eof, '\0'};
    }

    // Enter: '\r' ou '\n' consolidados em um único evento lógico.
    if (c == '\r' || c == '\n') {
        return KeyEvent{KeyType::Enter, '\0'};
    }

    // Backspace: DEL (0x7F) ou BS clássico (0x08).
    if (c == 0x7F || c == 0x08) {
        return KeyEvent{KeyType::Backspace, '\0'};
    }

    if (c == '\t') {
        return KeyEvent{KeyType::Tab, '\0'};
    }

    // ESC isolado ou início de sequência de escape ANSI (ex: setas).
    if (c == '\033') {
        char seq0 = '\0';
        char seq1 = '\0';

        if (!readRawByte(seq0)) {
            return KeyEvent{KeyType::Escape, '\0'}; 
        }
        if (seq0 != '[') {
            return KeyEvent{KeyType::Escape, '\0'};
        }
        if (!readRawByte(seq1)) {
            return KeyEvent{KeyType::Escape, '\0'};
        }

        switch (seq1) {
            case 'A': return KeyEvent{KeyType::ArrowUp, '\0'};
            case 'B': return KeyEvent{KeyType::ArrowDown, '\0'};
            case 'C': return KeyEvent{KeyType::ArrowRight, '\0'};
            case 'D': return KeyEvent{KeyType::ArrowLeft, '\0'};
            default:  return KeyEvent{KeyType::Unknown, '\0'};
        }
    }

    // Ctrl+<letra>: mapeamento de bytes de controle (0x01 a 0x1A -> Ctrl+A a Ctrl+Z).
    if (c >= 0x01 && c <= 0x1A) {
        char letter = static_cast<char>('a' + (c - 1));
        return KeyEvent{KeyType::CtrlKey, letter};
    }

    // Qualquer outro caractere imprimível.
    return KeyEvent{KeyType::Char, c};
}

bool Terminal::getWindowSize(int& rows, int& cols) {
    struct winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return false;
    }
    rows = ws.ws_row;
    cols = ws.ws_col;
    return true;
}

void Terminal::writeRaw(const string& data) {
    const char* ptr = data.data();
    size_t remaining = data.size();

    while (remaining > 0) {
        ssize_t written = write(STDOUT_FILENO, ptr, remaining);
        if (written == -1) {
            if (errno == EINTR) {
                continue; // Retentativa segura em caso de interrupção por sinal.
            }
            fprintf(stderr, "Terminal::writeRaw: write failed (errno=%d)\n", errno);
            return;
        }
        ptr += written;
        remaining -= static_cast<size_t>(written);
    }
}

void Terminal::renderFrame(const string& frame) {
    // A string do quadro inteiro é submetida em uma única chamada de write() 
    // para garantir atomicidade e prevenir flicker. O chamador gerencia "\033[H" / "\033[K".
    writeRaw(frame);
}

void Terminal::clearScreenOnce() {
    writeRaw("\033[2J\033[H");
}

void Terminal::applyThemeColors(Theme theme) {
    writeRaw(ThemePalette::background(theme) + ThemePalette::foreground(theme));
}

void Terminal::resetAttributes() {
    writeRaw("\033[0m");
}