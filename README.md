# Consulta de Usuários — Tabela Hash + Filtro de Bloom

Programa de terminal em **C** para armazenar e consultar muitos registros de
usuários com alta performance. Combina duas estruturas de dados clássicas:

- **Tabela hash** (encadeamento separado + rehash automático): busca por ID em
  tempo médio O(1).
- **Filtro de Bloom**: checagem probabilística de existência muito rápida. Antes
  de fazer a busca completa, o filtro responde "com certeza não existe" ou
  "provavelmente existe". Quando ele diz que não existe, a tabela hash nem é
  consultada.

## Estrutura do projeto

```
.
├── Makefile            # compilação e execução
├── README.md
├── data/
│   └── usuarios.txt    # arquivo de exemplo (20 usuários)
└── src/
    ├── user.h          # estrutura do usuário
    ├── bloom.h/.c      # filtro de Bloom
    ├── hashtable.h/.c  # tabela hash
    └── main.c          # menu interativo
```

## Como compilar e executar

Requisitos: `gcc` e `make`.

```bash
make            # compila -> gera bin/consulta
make run        # compila e executa carregando data/usuarios.txt
./bin/consulta  # executa sem carregar nada
./bin/consulta data/usuarios.txt   # carrega um arquivo no início
make clean      # remove os artefatos de build
```

A compilação usa `-Wall -Wextra -O2 -std=c11` e termina **sem erros nem avisos**.

## Formato do arquivo de entrada

Um usuário por linha, com os campos separados por ponto e vírgula (`;`):

```
id;nome;email;idade;cpf
```

Exemplo:

```
1;Ana Souza;ana.souza@email.com;28;123.456.789-00
2;Bruno Lima;bruno.lima@email.com;34;234.567.890-11
```

Regras:

- Linhas em branco são ignoradas.
- Linhas que começam com `#` são tratadas como comentário.
- `id` deve ser um número inteiro (é a chave de busca).
- Linhas com número de campos diferente de 5 são ignoradas.

## Menu

| Opção | Ação |
|-------|------|
| 1 | Carregar usuários de um arquivo |
| 2 | Cadastrar novo usuário manualmente |
| 3 | Consultar usuário por ID (busca direta na tabela hash) |
| 4 | Consultar usuário por ID (com filtro de Bloom antes) |
| 5 | Estatísticas de uso e desempenho |
| 0 | Sair |

## Estatísticas exibidas

- **Tabela hash:** registros armazenados, capacidade (baldes), fator de carga e
  colisões acumuladas.
- **Filtro de Bloom:** bits totais (`m`), número de funções hash (`k`), bits
  ligados e taxa estimada de falsos positivos.
- **Uso/desempenho:** total de consultas, encontradas, descartes feitos pelo
  Bloom, falsos positivos e o tempo da última busca em microssegundos.

## Detalhes de implementação

- **Hashing:** função `splitmix64` para espalhar os IDs inteiros entre os baldes.
- **Rehash:** quando o fator de carga passa de 0,75 a capacidade dobra e os nós
  são redistribuídos (sem realocar cada usuário).
- **Bloom (double hashing):** as `k` posições são derivadas de duas funções hash
  via `g_i = h1 + i·h2` (Kirsch & Mitzenmacher). `m` e `k` são dimensionados a
  partir do número esperado de itens e da taxa de falso positivo alvo.

## Referências
<li>
<strong>CORMEN, T. H. et al.</strong>
      <em>Introduction to Algorithms</em>. 3ª ed. MIT Press, 2009.
      <br><span style="color:var(--cinza);">Capítulo sobre Tabelas Hash — fundamento teórico de hashing e encadeamento.</span>
<li>
  <strong>BLOOM, B. H.</strong>
      "Space/Time Trade-offs in Hash Coding with Allowable Errors".
      <em>Communications of the ACM</em>, v. 13, n. 7, p. 422–426, 1970.
      <br><span style="color:var(--cinza);">Artigo original que propôs o filtro de Bloom.</span>
<li>
