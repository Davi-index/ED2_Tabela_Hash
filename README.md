# Consulta Rápida de Usuários (C)

Programa de terminal em C para consultar grandes volumes de usuários usando
**tabela hash** (busca O(1) amortizada) e **filtro de Bloom** (checagem
probabilística de existência ultra rápida).

## Compilar

```bash
cd c-app
make
```

Requisitos: `gcc` (ou `clang`) e `make`. Compila com `-Wall -Wextra -O2`.

## Executar

```bash
./bin/usuarios
```

Ou já carregando um arquivo de entrada:

```bash
./bin/usuarios data/usuarios.csv
```

## Formato do arquivo de entrada

CSV simples separado por `;`, uma linha por usuário, sem cabeçalho:

```
ID;NOME;EMAIL;IDADE;CPF
1;Ana Souza;ana@example.com;28;123.456.789-00
2;Bruno Lima;bruno@example.com;34;987.654.321-00
```

Veja `data/usuarios.csv` para um exemplo pronto.

## Menu

1. Carregar arquivo de usuários
2. Cadastrar usuário manualmente
3. Consultar usuário por ID (hash)
4. Verificar existência por ID (filtro de Bloom + hash)
5. Estatísticas (registros, colisões, tempos de busca)
6. Listar primeiros N usuários
0. Sair

## Estrutura

- `src/usuario.{h,c}`   — struct de usuário e parser de CSV
- `src/hash.{h,c}`      — tabela hash com encadeamento separado
- `src/bloom.{h,c}`     — filtro de Bloom (k funções hash)
- `src/main.c`          — menu interativo e medição de tempo
- `Makefile`            — build simples
- `data/usuarios.csv`   — dados de exemplo
