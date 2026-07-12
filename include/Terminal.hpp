#ifndef TERMINAL_HPP
#define TERMINAL_HPP

#include <termios.h>
#include <string>

// Premissas de Design:
// - Alvo: Terminal POSIX/Linux com suporte a sequências ANSI/VT100.
// - Tamanho da tela: Consultado via ioctl(TIOCGWINSZ). Se falhar, o chamador providencia um valor padrão.
// - Sinais: O ISIG é mantido ATIVADO durante o raw mode (Ctrl+C/Ctrl+Z funcionam, conforme requisito "ISIG opcional").
// - Dependência: 'Theme' usa forward declaration para manter a inclusão unidirecional (Terminal -> Syntax).
enum class Theme;

// Eventos lógicos de entrada. Abstrai sequências ANSI (setas) e uniformiza 
// variações de teclas (Enter como \r ou \n, Backspace como 0x7F ou 0x08).
enum class KeyType {
    Char,       // Caractere imprimível padrão (ver KeyEvent::ch)
    ArrowUp,
    ArrowDown,
    ArrowLeft,
    ArrowRight,
    Backspace,
    Enter,
    Escape,     // Tecla ESC isolada ou sequência ANSI não reconhecida
    Tab,
    CtrlKey,    // Ctrl+<letra>; KeyEvent::ch guarda a letra em minúsculo
    Unknown,    // Sequência de escape válida, porém não suportada
    Eof         // Fim de arquivo (stdin fechado) ou erro de leitura
};

// Estrutura que representa um evento de teclado processado.
struct KeyEvent {
    KeyType type = KeyType::Unknown;
    char ch = '\0'; // Válido apenas quando type for Char ou CtrlKey
};

// Guarda RAII para o "raw mode" do terminal. 
// Garante a restauração segura das configurações originais na destruição, 
// mesmo durante o stack unwinding causado por exceções.
class TerminalGuard {
public:
    TerminalGuard();
    ~TerminalGuard();

    // Cópia e movimentação desabilitadas: exige dono único (singleton local)
    // para evitar restaurações múltiplas ou acidentais do estado do terminal.
    TerminalGuard(const TerminalGuard&) = delete;
    TerminalGuard& operator=(const TerminalGuard&) = delete;
    TerminalGuard(TerminalGuard&&) = delete;
    TerminalGuard& operator=(TerminalGuard&&) = delete;

    // Retorna true se o terminal foi configurado para raw mode com sucesso.
    bool isRawModeActive() const noexcept;

private:
    struct termios originalTermios_{};
    bool rawModeActive_ = false;
    bool originalSaved_ = false; 

    bool enableRawMode();
    void disableRawMode();
};

// Utilitário sem estado para abstrair o I/O de baixo nível do terminal.
// Encapsula o controle de sequências ANSI, tamanho de tela e renderização de frames.
class Terminal {
public:
    // Faz a leitura bloqueante e o parse de uma única tecla ou sequência ANSI.
    static KeyEvent readKeyEvent();

    // Consulta as dimensões da janela. Retorna false em caso de falha.
    static bool getWindowSize(int& rows, int& cols);

    // Renderiza um frame completo em uma única chamada de write() para evitar flicker.
    // O chamador deve incluir "\033[H" e "\033[K" na string; nunca emite "\033[2J".
    static void renderFrame(const std::string& frame);

    // Limpa a tela inteira. Uso restrito à inicialização e desligamento para evitar flicker.
    static void clearScreenOnce();

    // Aplica globalmente a cor de fundo e texto do tema.
    // Garante que sequências "\033[K" preencham a linha com a cor correta de fundo
    // sem vazar formatação não intencional durante redimensionamentos ou scroll.
    static void applyThemeColors(Theme theme);

    // Remove todos os atributos de estilo, restaurando o padrão do terminal.
    // Uso restrito ao desligamento (session boundaries).
    static void resetAttributes();

private:
    // Leitura crua de byte com retentativa automática em caso de interrupção (EINTR).
    static bool readRawByte(char& outByte);

    // Escrita atômica em STDOUT. Trata escritas parciais e sinais (EINTR) internamente.
    static void writeRaw(const std::string& data);
};

#endif // TERMINAL_HPP