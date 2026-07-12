# Guia Didático e de Correção: Missões do Editor de Texto

Este documento descreve o raciocínio esperado para cada missão. Não contém o código de solução. Use o material para orientar alunos e avaliar entregas.

### [MISSÃO 1.1] RawLineStore: Array Dinâmico Manual no Heap

**Enunciado detalhado:**
Implemente uma estrutura de array expansível sem usar `std::vector`, gerenciando um bloco contíguo de `std::string` alocado no heap via `std::unique_ptr<string[]>`. A estrutura precisa suportar `append()` (adicionar ao final), `at()` (acesso com verificação de limites) e relatar corretamente `size()`/`capacity()`.

**Passo a passo do raciocínio:**
* O aluno precisa entender que unique_ptr gere um array no heap e chama delete automaticamente.
* A função append precisa verificar se size é igual a capacity. Se os valores forem iguais, o array deve crescer antes da inserção.
* Crescer o array exige alocar um bloco maior, mover os elementos existentes e descartar o bloco antigo.
* Dobrar a capacidade a cada realocação garante custo amortizado constante por inserção.
* A função at precisa validar o índice explicitamente e lançar std::out_of_range se o valor for inválido.

**Descrição da Solução Ideal:**
* O método append tem complexidade amortizada constante.
* A operação de crescimento move os elementos com std::move para evitar cópias de std::string.
* O método at opera em tempo constante e lança exceção para índice fora do intervalo permitido.
* A capacidade inicial é um valor fixo. As realocações dobram essa capacidade.

### [MISSÃO 1.2] linearSearch: Busca Sequencial de Substring

**Enunciado detalhado:**
Implemente uma busca que percorre todas as linhas do buffer, em ordem, e retorna o índice da **primeira** linha que contém a substring buscada (`needle`) em qualquer posição. Se nenhuma linha contiver, retornar `-1`.

**Passo a passo do raciocínio:**
* A busca é sequencial, exigindo a inspeção de cada linha na ordem do buffer.
* Para verificar se uma string contém outra, usa std::string::find, que retorna std::string::npos quando não há ocorrência.
* A busca é interrompida na primeira ocorrência.
* Casos de limite incluem buffer vazio ou needle vazia.

**Descrição da Solução Ideal:**
* Complexidade linear no pior caso.
* O tipo de retorno é long para representar valores negativos sem transbordo de size_t.
* A função não assume ordenação do buffer.

### [MISSÃO 1.3] binarySearch: Busca Binária

**Enunciado detalhado:**
Implemente busca binária sobre o vetor de linhas, assumindo que ele já está ordenado lexicograficamente. Diferente da busca linear, aqui a correspondência é **exata**: a linha inteira precisa ser igual à string buscada, não apenas contê-la.

**Passo a passo do raciocínio:**
* A busca binária exige dados ordenados. A função não verifica a ordenação.
* O cálculo do ponto médio soma o limite inferior à metade da diferença entre os limites para evitar transbordo.
* A linha do meio é comparada com o alvo. A busca continua na metade direita se a linha do meio for menor, e na esquerda caso contrário.
* Se o buffer estiver vazio, a função retorna um valor negativo sem entrar no ciclo.

**Descrição da Solução Ideal:**
* Complexidade logarítmica estrita.
* Uso de índices long para os limites evita transbordo se o limite superior for decrementado abaixo de zero.

### [MISSÃO 1.4] insertionSort: Ordenação por Inserção In Place

**Enunciado detalhado:**
Ordene as linhas do buffer em ordem lexicográfica crescente, usando o algoritmo de ordenação por inserção, operando diretamente sobre o vetor (sem alocar uma estrutura auxiliar completa).

**Passo a passo do raciocínio:**
* O algoritmo divide o vetor numa região ordenada à esquerda e uma não processada à direita. O primeiro elemento da região não processada é inserido na posição correta na região ordenada.
* O percurso começa no índice 1. O valor atual é guardado antes dos deslocamentos.
* O deslocamento para a direita para assim que encontra um elemento menor, igual, ou atinge o início do vetor.
* Vetores com 0 ou 1 elemento não acionam o ciclo externo.

**Descrição da Solução Ideal:**
* Complexidade quadrática no pior caso e linear no melhor caso.
* Ordenação ocorre no próprio vetor usando apenas uma variável temporária.
* O algoritmo mantém a estabilidade dos elementos.

### [MISSÃO 2.1] TextBuffer: Operações de Linha

**Enunciado detalhado:**
Implemente as operações fundamentais de manipulação de linhas sobre o `std::vector<std::string>`: dividir uma linha em duas no ponto do cursor (Enter), mesclar uma linha com a anterior (Backspace no início de linha), inserir uma linha em um índice arbitrário, remover uma linha por índice, e os acessores `getLine`/`lineCount`.

**Passo a passo do raciocínio:**
* Na divisão de linha, o texto a partir da coluna de corte forma uma nova linha. O texto anterior permanece na original. Usa vector::insert com iteradores.
* Na fusão, o conteúdo atual vai para o final da linha anterior, e a linha redundante é removida com vector::erase.
* Inserir e remover por índice usam vector::insert e vector::erase.
* Os métodos públicos atualizam a posição do cursor. Se o buffer ficar vazio, uma linha vazia é reinserida. Índices inválidos são ignorados.

**Descrição da Solução Ideal:**
* Uso de iteradores de std::vector e std::string para cortes e inserções.
* O buffer sempre contém pelo menos uma linha.
* O cursor é recolocado de forma previsível após as operações.

### [MISSÃO 2.2] TextBuffer: Operações de Caractere e Cursor

**Enunciado detalhado:**
Implemente inserção e remoção de um único caractere na posição do cursor, e a movimentação do cursor (esquerda, direita, cima, baixo), usando iteradores da STL para localizar e manipular a posição exata dentro da `std::string` da linha.

**Passo a passo do raciocínio:**
* Para inserir caractere, o iterador localiza a posição e string::insert é usado. A nova posição do cursor baseia no iterador retornado.
* Para remover caractere, a posição é localizada via iterador. O caractere é apagado com string::erase.
* Movimento para a esquerda na coluna 0 leva o cursor ao final da linha anterior. Movimento para a direita no final da linha leva ao início da próxima linha.
* Ao mudar de linha verticalmente, a coluna do cursor é ajustada ao tamanho da nova linha.

**Descrição da Solução Ideal:**
* A navegação nas strings ocorre via iteradores.
* O limite de coluna na movimentação vertical usa std::min.

### [MISSÃO 3.1] SyntaxHighlighter: Classificação de Tokens

**Enunciado detalhado:**
Preencha as tabelas internas de palavras-chave (`keywords_`) e tipos (`types_`) do `SyntaxHighlighter`, que serão consultadas pelo tokenizador (infraestrutura já pronta) para categorizar cada palavra reconhecida da linha como `"keyword"`, `"type"` ou `"identifier"`.

**Passo a passo do raciocínio:**
* O aluno separa palavras de controlo de fluxo de nomes de tipos.
* Usa std::unordered_set para verificação de pertinência em tempo constante.

**Descrição da Solução Ideal:**
* Verificação feita via unordered_set::find.
* Tabelas preenchidas apenas uma vez no construtor.

### [MISSÃO 3.2] ScopeValidator: Emparelhamento de Delimitadores

**Enunciado detalhado:**
Implemente a validação de que todo delimitador de abertura (`(`, `[`, `{`) em um texto tem um fechamento correspondente e na ordem correta, usando `std::stack<char>`. Em caso de erro, preencher uma mensagem descritiva do tipo de problema encontrado.

**Passo a passo do raciocínio:**
* Percorrer o texto e empilhar delimitadores de abertura. Delimitadores de fechamento são comparados com o topo da pilha.
* Os erros incluem fechamento inesperado, par incompatível e delimitador não fechado.
* Caracteres que não são delimitadores são ignorados.

**Descrição da Solução Ideal:**
* Uso de std::stack com complexidade linear na varredura.
* Mensagens de erro cobrem as três falhas possíveis.

### [MISSÃO 3.3] VariableIndex: Inserção Recursiva na Árvore

**Enunciado detalhado:**
Implemente a inserção ordenada de um nome de identificador em uma Árvore Binária de Busca, usando `std::unique_ptr<Node>` para posse dos filhos, com uma função auxiliar recursiva (`insertNode`) que recebe e devolve a posse da subárvore. Inserções de nomes já existentes devem ser ignoradas (comportamento de conjunto).

**Passo a passo do raciocínio:**
* A função recebe e devolve a posse da subárvore usando unique_ptr passado por valor com std::move.
* Se o nó for nulo, um novo nó é criado e retornado.
* Se o nome for menor, a inserção continua na esquerda. Se for maior, na direita.
* Um parâmetro booleano avisa o chamador se a inserção ocorreu.

**Descrição da Solução Ideal:**
* Duplicatas são ignoradas.
* Gestão de memória usa unique_ptr sem alocação ou libertação explícitas.

### [MISSÃO 4.1] Undo Redo: Duas Pilhas e Padrão Command

**Enunciado detalhado:**
Implemente o mecanismo de desfazer/refazer usando duas pilhas de `std::unique_ptr<Command>` (`undoStack_`, `redoStack_`). Cada edição do usuário deve ser registrada como um `Command` reversível; `undo()` deve reverter a última edição e movê-la para a pilha de redo; `redo()` deve reaplicar a última edição desfeita e devolvê-la para a pilha de undo.

**Passo a passo do raciocínio:**
* Toda nova edição empilha um comando.
* Empilhar um novo comando esvazia a pilha de redo para evitar futuros alternativos inconsistentes.
* O undo remove o comando do topo, reverte a ação, e empilha na outra estrutura.

**Descrição da Solução Ideal:**
* A limpeza da pilha de redo é completa após nova edição.
* Cada comando gere apenas a sua própria operação usando polimorfismo.

### [MISSÃO 4.2] mergeSort: Ordenação Eficiente

**Enunciado detalhado:**
Implementar merge sort sobre o vetor de linhas, usando um vetor auxiliar (`scratch`) para a etapa de fusão, com as funções recursivas `mergeSortRange` (divide) e `mergeRanges` (combina).

**Passo a passo do raciocínio:**
* O vetor é dividido recursivamente até atingir subintervalos unitários.
* O vetor auxiliar é alocado uma única vez no início e reutilizado.
* A fusão usa dois ponteiros para copiar o menor elemento para o vetor auxiliar.
* Os elementos no vetor auxiliar são copiados de volta para o vetor original.

**Descrição da Solução Ideal:**
* Complexidade log-linear em todos os casos.
* O vetor auxiliar ocupa espaço linear e é alocado apenas uma vez.

### [MISSÃO 4.3] Autocomplete: Busca por Prefixo

**Enunciado detalhado:**
Implemente `collectWithPrefix`, uma travessia da BST que retorna, em ordem lexicográfica, todos os identificadores armazenados que começam com um prefixo dado. Prefixo vazio deve retornar todos os elementos.

**Passo a passo do raciocínio:**
* A busca verifica se o prefixo existe.
* Se o nó possui o prefixo, a varredura ocorre em ambos os lados e inclui o próprio nó.
* Se o valor é menor que o prefixo, a busca ocorre na direita.
* Se o valor é maior e não possui o prefixo, a busca ocorre na esquerda.

**Descrição da Solução Ideal:**
* A travessia respeita a ordem da árvore e gera sugestões ordenadas.
* A poda de subárvores melhora a eficiência.

### [MISSÃO 4.4] Estatísticas de Texto: Tabela Hash

**Enunciado detalhado:**
Implemente `TextStatistics::compute`, que recebe todas as linhas do buffer e calcula: número de palavras únicas, comprimento médio das palavras, a palavra mais longa, e as 5 palavras mais frequentes (com contagem), usando `std::unordered_map` para contagem eficiente de frequência.

**Passo a passo do raciocínio:**
* O sistema converte palavras para minúsculas e ignora pontuações.
* A contagem de palavras é armazenada num mapa hash.
* O total de palavras e o tamanho máximo são atualizados durante o ciclo.
* O topo exige ordenar as palavras por frequência e por ordem alfabética em caso de empate.

**Descrição da Solução Ideal:**
* Contagem executa em tempo linear.
* A ordenação para o topo usa critério determinístico de desempate.