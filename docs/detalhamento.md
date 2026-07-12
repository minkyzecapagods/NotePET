# Guia de Arquitetura e Manutenção - NotePET

## 1. Visão Geral da Arquitetura

O projeto é um editor de texto modal, no estilo Neovim, que roda **inteiramente em um terminal POSIX/Linux**, sem nenhuma biblioteca de UI (nada de ncurses, nada de terceiros). Isso é possível porque terminais compatíveis com VT100/ANSI expõem duas superfícies de controle que o projeto explora integralmente:

1. **`<termios.h>` - modo "raw" do terminal.** Por padrão, o terminal roda em modo *canônico*: o kernel armazena a entrada em um buffer de linha, só a repassa ao processo quando o usuário pressiona Enter, e ecoa cada tecla na tela automaticamente. Isso é inútil para um editor, que precisa reagir a **cada tecla individual** (inclusive setas, Ctrl+algo, Backspace) assim que ela chega. `TerminalGuard` reconfigura os atributos do terminal (`ICANON`, `ECHO`, `IXON`, `ICRNL`, `OPOST`) para entregar os bytes crus, um a um, ao programa.

2. **Sequências de escape ANSI/VT100.** Tudo o que aparece na tela , posicionamento do cursor, cores, limpeza de linha, cores de fundo truecolor , é obtido escrevendo strings de escape especiais (iniciadas por `\033[`) diretamente em `STDOUT`. O projeto não "desenha" nada; ele **imprime texto com códigos de controle embutidos** e o próprio terminal interpreta esses códigos.

A combinação dessas duas superfícies é suficiente para reproduzir grande parte do comportamento de um editor modal real, e criar problemas relacionados a:
- Parsing manual de sequências de bytes (setas de teclado chegam como 3 bytes: `ESC [ A/B/C/D`);
- Renderização atômica de frame único (para evitar *flicker*, todo o quadro é montado em uma `std::string` e despachado em **uma única chamada de `write()`**);
- Separação de responsabilidades entre "motor de texto" (TextBuffer), "motor de sintaxe" (Syntax) e "camada de I/O bruta" (Terminal) , um exemplo natural de arquitetura em camadas para aula.

---

## 2. Descrição Arquivo por Arquivo

### 2.1 `Terminal.hpp` / `Terminal.cpp`

**Responsabilidade:** abstrair toda a interação de baixo nível com o terminal , nada aqui sabe o que é um "buffer de texto" ou uma "linguagem".

#### `TerminalGuard` - RAII para o Raw Mode
Este é o ponto mais rico para discutir RAII (*Resource Acquisition Is Initialization*) em aula:
- O **construtor** chama `enableRawMode()`, que salva a configuração original do terminal (`tcgetattr`) e aplica a configuração raw (`tcsetattr`).
- O **destrutor** chama `disableRawMode()`, restaurando a configuração original.
- Como o destrutor de `TerminalGuard` roda automaticamente ao sair de escopo , **inclusive durante o *stack unwinding* de uma exceção** , o terminal do usuário nunca fica "preso" em modo raw mesmo se o programa crashar de forma inesperada. É o mesmo princípio de `std::lock_guard` ou `std::unique_ptr`, aplicado a um recurso de sistema operacional em vez de memória.
- Cópia e movimentação foram deletadas: só pode existir **uma** instância controlando o terminal por vez (não faria sentido restaurar o terminal duas vezes, nem transferir "posse" de um recurso que é global ao processo).
- `originalSaved_`, se `tcgetattr` falhar (terminal não suportado, por exemplo, saída redirecionada para um arquivo), o destrutor **não deve** tentar restaurar um `termios` que nunca foi validamente salvo.

#### Leitura de teclado: `readRawByte` e `readKeyEvent`
- `readRawByte` faz um `read()` bloqueante de 1 byte, mas com retentativa em caso de `EINTR` (a leitura foi interrompida por um sinal, como `SIGWINCH` no resize de janela). Isso é uma prática de robustez padrão em sistemas POSIX que vale a pena destacar: **um `read()` interrompido por sinal não é um erro de verdade**, é preciso tentar de novo.
- `readKeyEvent` interpreta a sequência de bytes crus e a converte em um `KeyEvent` semântico (`KeyType`). O caso mais interessante é o parsing de setas: elas chegam como 3 bytes (`\033`, `[`, e uma letra `A/B/C/D`), então a função precisa ler byte a byte de forma condicional, tratando timeouts implícitos (se o segundo/terceiro byte nunca chegar, o evento vira `Escape`).
- Enter chega como `\r` **ou** `\n` dependendo do terminal, e Backspace chega como `0x7F` (DEL) **ou** `0x08` (BS) , o código unifica ambos, de forma que o resto do editor nunca precisa se preocupar com essa variação.

#### Renderização: `renderFrame`, `writeRaw`, `clearScreenOnce`
- `writeRaw` também trata escrita parcial (`write()` pode escrever menos bytes do que o pedido) e `EINTR`, seguindo o mesmo padrão de robustez da leitura.
- `renderFrame` recebe uma string **já pronta** contendo o quadro inteiro (posicionamento de cursor, cores, texto, tudo) e a escreve de uma vez. Isso é o que garante ausência de flicker: se o editor fizesse múltiplas chamadas de `write()` para cada linha, o terminal poderia repintar a tela em passos visíveis.
- `clearScreenOnce` (`\033[2J\033[H`) é deliberadamente restrito à inicialização e ao encerramento , o comentário no header já avisa que **nunca** deve ser chamado dentro do loop principal, pois isso geraria flicker constante a cada frame.

---

### 2.2 `TextBuffer.hpp` / `TextBuffer.cpp`

Este é o "motor de dados" do editor , nenhuma linha aqui sabe que existe um terminal.

#### `RawLineStore` - array dinâmico manual isolado
É uma estrutura **puramente didática** (Missão 1.1): reimplementa manualmente o que `std::vector` já faz, usando `std::unique_ptr<string[]>` para simular um array bruto no heap com dobra de capacidade. O comentário no header é explícito: essa estrutura **não sincroniza** com o buffer principal depois do carregamento , ela é usada só uma vez, dentro de `loadLines()`, como uma "esteira" de ingestão de dados, e depois seu conteúdo é copiado para o `std::vector<std::string> lines_` real, que é a estrutura viva usada por todo o resto do editor.

Vale destacar em aula que `grow()` segue a estratégia clássica de **dobrar a capacidade** ao atingi-la, o que é o motivo pelo qual `push_back`/`append` amortizado é O(1) no `std::vector` real da STL , este código explicita manualmente o mecanismo que normalmente fica escondido.

#### `std::vector<std::string> lines_` - o TAD ativo
Todas as operações de edição em tempo real (digitar, apagar, mover cursor, buscar, ordenar) atuam sobre este vetor, e **não** sobre `RawLineStore`. Isso é proposital: a Missão 1.1 ensina alocação manual como exercício isolado, mas a aplicação real usa a estrutura de dados robusta e testada da STL , uma boa oportunidade para discutir quando vale a pena reimplementar uma estrutura (aprendizado) versus quando usar a implementação de biblioteca (produção).

#### Operações "raw" vs. operações "públicas"
Repare no padrão consistente do arquivo: para cada operação editável existe um par de métodos:
- `rawInsertCharAt` / `rawDeleteCharAt` / `rawSplitLineAt` / `rawMergeLineIntoPrevious` / `rawInsertLineAt` / `rawRemoveLineAt` , mutadores **de baixo nível**, que só manipulam o `std::vector`/`std::string` e retornam informação suficiente para desfazer a operação (novo índice, caractere removido, etc.). Eles **não** tocam a pilha de undo.
- `insertChar` / `deleteChar` / `insertLine` / `deleteLine` , as versões **públicas**, que chamam a versão `raw*` correspondente e, além disso, criam e empilham um `Command` no histórico.

Essa separação existe exatamente para permitir que `undo()`/`redo()` reexecutem a operação "raw" sem gerar **recursivamente** uma nova entrada no histórico (o que causaria um loop infinito de comandos se undo empilhasse outro undo).

#### Padrão Command , Undo/Redo (Missão 4.1)
`Command` é uma interface abstrata com `undo(TextBuffer&)` e `redo(TextBuffer&)`. Cada operação editável tem sua classe concreta (`InsertCharCommand`, `DeleteCharCommand`, `MergeLineCommand`, `SplitLineCommand`, `DeleteLineCommand`), todas declaradas como `friend` de `TextBuffer` para poderem acessar `cursor_` e os métodos `raw*` privados.

Cada comando armazena o **mínimo de estado necessário** para reverter/reaplicar a edição , por exemplo, `InsertCharCommand` guarda o cursor antes e depois da inserção, e o caractere inserido; `undo()` simplesmente apaga esse caractere na posição `before_` e restaura o cursor.

O mecanismo de duas pilhas é padrão de mercado:
- `undoStack_` acumula comandos aplicados.
- `redoStack_` acumula comandos desfeitos, prontos para reaplicação.
- **Ponto crítico de design (`pushCommand`):** toda nova edição do usuário **limpa completamente** a `redoStack_`. Isso evita a inconsistência clássica de "linha do tempo bifurcada" , se o usuário desfaz 3 ações e depois digita algo novo, as ações "refazíveis" anteriores deixam de fazer sentido (o estado ao qual elas se referiam não existe mais) e são descartadas.

Observação didática importante: o comentário no `TextBuffer.hpp` deixa explícito que **operações de ordenação em lote são ignoradas pelo histórico** , `insertionSort()`/`mergeSort()` não empilham `Command`. Isso é uma decisão de escopo (não uma omissão acidental) que vale a pena justificar em aula: desfazer uma reordenação completa exigiria armazenar o vetor inteiro antes/depois, o que foge do modelo "comando granular" adotado para os demais casos.

#### Buscas e ordenação (Missões 1.2, 1.3, 1.4, 4.2)
Todas operam diretamente sobre `lines_`, tratando cada `std::string` da linha como o "elemento" a ser buscado/comparado. É importante frisar em aula que **busca binária e busca linear têm contratos diferentes**: `linearSearch` faz correspondência por substring (`std::string::find`) em qualquer ordem, enquanto `binarySearch` exige que o vetor já esteja ordenado e faz correspondência **exata** de linha inteira , o comentário no header já avisa que a função não faz nenhuma verificação preventiva de pré-condição (não é papel dela detectar se o buffer está desordenado; isso é responsabilidade de quem a chama).

---

### 2.3 `Syntax.hpp` / `Syntax.cpp`

Este módulo concentra toda a "inteligência léxica" do editor: tokenização, cores, validação de escopo, indexação de identificadores e estatísticas.

#### `ThemePalette` - fonte única de verdade de cores
Cada tema (`Monokai`, `Dracula`, `Light`) é mapeado para códigos SGR truecolor (`\033[38;2;R;G;Bm` para foreground, `\033[48;2;R;G;Bm` para background) construídos uma única vez em variáveis `static const string` locais a cada função , um uso elegante de *lazy initialization* que evita reconstrução de string a cada chamada. `Terminal` e `SyntaxHighlighter` consultam a **mesma** tabela, o que garante que a cor de fundo do terminal e as cores de tokens nunca fiquem dessincronizadas entre si.

#### `SyntaxHighlighter::tokenize` - o analisador léxico
Percorre a linha caractere a caractere, com regras em ordem de prioridade: espaços em branco → comentário de linha (`//`, consome o resto da linha) → literais de string/char (com suporte a escape `\`) → identificadores/palavras-chave/tipos → literais numéricos → qualquer outro caractere (tratado como token de pontuação/operador, categoria `"default"`).

Um detalhe elegante para destacar em aula: o tratamento especial de `std::algo` como **token único**. Depois de reconhecer a palavra `std`, o tokenizador verifica se ela é seguida por `::` e mais um identificador, e nesse caso concatena tudo (`std::vector`, por exemplo) em um só `Token`, em vez de gerar três tokens separados (`std`, `::`, `vector`). Isso evita que o `::` apareça destacado com a cor de "pontuação" no meio de um nome qualificado.

A garantia de invariante mencionada no header , *"a concatenação dos tokens reconstrói a linha exatamente"* , é importante: nenhum caractere da linha original é descartado ou transformado durante a tokenização, apenas categorizado. Isso é o que permite que `highlightLine` insira cores sem alterar o conteúdo textual.

#### `ScopeValidator` - pilha de delimitadores
Implementação clássica de "parênteses balanceados" usando `std::stack<char>`: abre → empilha; fecha → compara com o topo da pilha e desempilha se combinar, ou reporta erro específico (fechamento inesperado / par incompatível / delimitador nunca fechado). É uma das aplicações mais canônicas de pilha em Estruturas de Dados, e a mensagem de erro detalhada (`mismatchDetail`) é o que alimenta a barra de status ("Erro de Escopo: ...") em `main.cpp`.

**Atenção:** este validador roda sobre o **buffer inteiro concatenado**, não sobre tokens , veja a seção de Bugs Conhecidos (Bug 3) para a implicação disso.

#### `VariableIndex` - Árvore Binária de Busca com `unique_ptr`
BST clássica onde a posse dos nós é modelada com `std::unique_ptr<Node>` recursivo , quando um nó é destruído, seus filhos são destruídos em cascata automaticamente (sem `delete` manual, sem vazamento, sem *use-after-free*). A classe desabilita cópia e movimentação deliberadamente (comentário no header: força uma única instância estável durante o ciclo de varredura), o que é coerente com o fato de que ela é **reconstruída inteira a cada frame** em `main.cpp` (veja Decisões de Design, item sobre autocomplete).

`insertNode` é um exemplo didático de função recursiva que "recebe e devolve a posse" via `unique_ptr` , pode abrir discussão para entender que a assinatura é `unique_ptr<Node> insertNode(unique_ptr<Node> node, ...)` (por valor, com `std::move` do lado do chamador) e não por referência porque cada chamada recursiva pode **substituir** o ponteiro do nível acima (por exemplo, quando o nó é nulo e um novo nó precisa ser criado no lugar).

`collectWithPrefix` (Missão 4.3) faz uma poda inteligente da busca: como a árvore é ordenada alfabeticamente e não por "tem o prefixo ou não", ela não pode simplesmente ir para a esquerda/direita como uma busca binária normal. Em vez disso, quando o nó atual **tem** o prefixo, ela varre **ambos** os lados (porque pode haver mais elementos com o mesmo prefixo tanto antes quanto depois dele na ordenação lexicográfica); quando **não tem**, aí sim decide um único lado com base na comparação lexicográfica.

#### `TextStatistics::compute` - tabela hash
Usa `std::unordered_map<std::string, int>` para contar frequência de palavras em O(1) médio por atualização, ao custo de percorrer o texto uma vez (O(n) no total de caracteres). A ordenação final do "Top 5" usa um comparador customizado com critério de desempate explícito (frequência decrescente, depois ordem alfabética) , um bom exemplo de `std::sort` com functor lambda de dois critérios.

---

### 2.4 `main.cpp`

Este arquivo orquestra os três módulos anteriores em um loop de eventos de editor modal: **ler estado → renderizar → ler tecla → mutar estado → repetir**.

#### Loop principal e cálculo de viewport
A cada iteração:
1. Consulta o tamanho do terminal (`Terminal::getWindowSize`), com fallback documentado (`rows=24, cols=80`) se a chamada `ioctl` falhar.
2. Calcula a largura dinâmica da calha lateral (`gutterWidth`) com base no número de dígitos da última linha do buffer , a calha "cresce" conforme o arquivo cresce, para que o número da última linha sempre caiba.
3. Calcula quantas linhas de texto cabem na tela (`textAreaHeight`), descontando as linhas reservadas para status (`kStatusRows = 2`) e, se ativo, o painel de estatísticas (`kStatsBoxRows = 13`, altura **fixa** propositalmente, para que a aritmética de scroll não precise recalcular dinamicamente quando o painel abre/fecha).
4. Ajusta `topLine` (scroll vertical): se o cursor saiu da área visível por cima ou por baixo, `topLine` é deslocado para trazê-lo de volta , a lógica clássica de "scroll follows cursor" de qualquer editor de texto.

#### Reconstrução do índice de autocomplete a cada frame
```cpp
VariableIndex variables;
for (size_t i = 0; i < buffer.lineCount(); ++i) {
    IdentifierScanner::scanInto(buffer.getLine(i), highlighter, variables);
}
```
A árvore inteira é **descartada e reconstruída do zero em toda iteração do loop**. Isso é uma decisão didática explícita (documentada no comentário de topo do arquivo) , o custo é aceitável para arquivos de tamanho didático, mas é um ótimo gancho para discussão de complexidade em aula: quanto maior o arquivo, mais caro fica repetir isso a cada tecla pressionada.

#### Barra de status dinâmica
A "Barra de Status 1" mostra modo, nome do arquivo, posição do cursor e o resultado do `ScopeValidator` (que roda sobre o buffer inteiro concatenado a cada frame). A "Barra de Status 2" é contextual , muda de conteúdo conforme o modo (`SaveAs`, `Search`, mensagem transitória, ou sugestões de autocomplete).

#### Dispatch de atalhos (modo Normal)
O `switch` sobre `KeyType::CtrlKey` mapeia cada `Ctrl+<letra>` para uma ação: `u`/`r` (undo/redo), `f` (busca), `s` (salvar), `o` (ordenar via merge sort), `t` (ciclar tema), `w` (alternar painel de estatísticas). Modos especiais (`Search`, `SaveAs`) capturam a entrada de teclado **antes** do dispatch normal, redirecionando `Char`/`Backspace`/`Enter`/`Escape` para a lógica específica daquele modo , um padrão simples de máquina de estados finita para modos de interação.

---

## 3. Decisões de Implementação (Design Rationale)

Estas são escolhas de design deliberadas, com trade-offs que valem a pena discutir com os alunos.

- **`Cursor` como `struct` de índices (`row`, `col`), nunca iteradores STL persistentes.** Iteradores de `std::vector`/`std::string` são invalidados por qualquer realocação (inserção/remoção de linha, crescimento de string). Índices numéricos são estáveis (embora precisem ser revalidados contra os limites atuais a cada uso) , uma boa oportunidade para discutir invalidação de iteradores.

- **Ignorar chaves/comentários multilinha (`/* */`) por simplicidade didática.** Tanto o `SyntaxHighlighter` (léxico por linha) quanto o `ScopeValidator` (ingenuamente concatenado) tratam cada linha de forma isolada ou como texto bruto sequencial, sem manter estado de "dentro de um comentário de bloco" entre linhas. Isso é uma simplificação de escopo intencional , suportar `/* */` multilinha exigiria manter um estado de "modo" entre chamadas de tokenização, o que ainda pode ser implementado no futuro.

- **Reconstrução do `VariableIndex` a cada frame.** Já discutido acima , trade-off consciente entre simplicidade de implementação (sem necessidade de invalidação incremental) e custo computacional (aceitável em escala didática, mas não em arquivos grandes).

- **`RawLineStore` isolado e "descartável".** Reforça a separação entre "exercício de alocação manual" (Missão 1.1) e "estrutura de dados de produção usada pelo editor" (`std::vector` em `TextBuffer`).

- **ISIG mantido ativo no raw mode.** Diferente de muitos tutoriais de "terminal raw mode" que desabilitam `ISIG` (fazendo Ctrl+C parar de gerar `SIGINT`), este projeto mantém `ISIG` ativo de propósito , Ctrl+C/Ctrl+Z continuam funcionando normalmente. Isso é chamado de "ISIG opcional" no comentário do header, sugerindo que desabilitar sinais é uma escolha de design válida, mas não a adotada aqui.

- **Syntax highlighting restrito a arquivos `.cpp`.** `hasCppExtension()` é checada antes de aplicar `highlighter.highlightLine()`; para outros arquivos, a linha é impressa crua. Isso evita que o léxico C++ (que reconhece palavras-chave como `if`/`for`/`int`) produza destaque sem sentido em arquivos de texto genérico.

- **Ordenação (Missões 1.4/4.2) fora do histórico de undo.** Já discutido , decisão de escopo documentada explicitamente no header do `TextBuffer`.

---

## 4. Bugs Conhecidos e Diagnóstico

**Bug 1: O início da primeira linha não troca de cor de fundo**
A execução da limpeza de ecrã acontece antes da aplicação de temas de cores. A limpeza retém os valores do emulador original. A correção exige aplicar as cores do tema antes da limpeza inicial.

**Bug 2: O tema termina abruptamente após a barra de estado**
O uso de códigos de reinício para encerrar formatações também apaga atributos de fundo. A execução subsequente de preenchimento de linha atua com as cores antigas. A correção requer reaplicar a cor de fundo desejada antes do preenchimento e adicionar espaços em branco até ao limite do ecrã.

**Bug 3: Falsos positivos no erro de escopo em ficheiros grandes**
A função de validação acede a linhas brutas sem identificar contextos léxicos definidos pelo analisador. Parênteses e delimitações textuais dentro de comentários ou cadeias de caracteres perturbam a pilha inteira. A correção requer o uso do validador em contexto léxico, ignorando símbolos que estejam classificados como texto livre.