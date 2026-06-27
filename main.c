/*
 * main.c
 * ------
 * Programa de terminal para consultar muitos registros de usuarios com
 * alta performance. Combina:
 *
 *   - Tabela hash (busca O(1) media por ID)
 *   - Filtro de Bloom (descarta IDs inexistentes quase instantaneamente,
 *     antes de tocar na tabela hash)
 *
 * Oferece um menu interativo para carregar arquivos, cadastrar, consultar
 * e ver estatisticas de uso/desempenho.
 *
 * Formato do arquivo de entrada (uma linha por usuario, campos separados
 * por ';'):   id;nome;email;idade;cpf
 * Linhas em branco e linhas iniciadas por '#' sao ignoradas.
 */

/* Necessario para clock_gettime/CLOCK_MONOTONIC com -std=c11. */
#define _POSIX_C_SOURCE 199309L

#include "user.h"
#include "hashtable.h"
#include "bloom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/* ---- Codigos ANSI para deixar a saida do terminal mais agradavel ---- */
#define C_RESET  "\x1b[0m"
#define C_BOLD   "\x1b[1m"
#define C_DIM    "\x1b[2m"
#define C_CYAN   "\x1b[36m"
#define C_GREEN  "\x1b[32m"
#define C_YELLOW "\x1b[33m"
#define C_RED    "\x1b[31m"
#define C_BLUE   "\x1b[34m"

/* Capacidade inicial da tabela e estimativa de itens para o Bloom. */
#define CAP_INICIAL        1024
#define BLOOM_N_ESPERADO   100000
#define BLOOM_P            0.01

/* ----------------------- Estado / estatisticas ----------------------- */
typedef struct {
    HashTable   *ht;
    BloomFilter *bloom;
    /* Contadores de uso para o painel de estatisticas. */
    unsigned long consultas_total;
    unsigned long consultas_encontradas;
    unsigned long bloom_descartes;       /* vezes que o Bloom evitou a busca */
    unsigned long bloom_falsos_positivos;/* Bloom disse "talvez" mas nao existia */
    double        ultimo_tempo_us;       /* tempo da ultima busca, em microssegundos */
} App;

/* ------------------------- Funcoes utilitarias ------------------------ */

/* Tempo monotonico em segundos com alta resolucao. */
static double agora_segundos(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double) ts.tv_sec + (double) ts.tv_nsec / 1e9;
}

/* Copia "src" em "dst" respeitando o tamanho e garantindo o '\0' final. */
static void copia_str(char *dst, size_t tam, const char *src) {
    if (tam == 0) return;
    size_t i = 0;
    for (; i < tam - 1 && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

/* Remove '\n'/'\r' do final de uma string. */
static void remove_quebra_linha(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[--n] = '\0';
    }
}

/* Le uma linha inteira do stdin de forma segura. Retorna 0 em EOF. */
static int ler_linha(char *buf, size_t tam) {
    if (!fgets(buf, (int) tam, stdin)) return 0;
    remove_quebra_linha(buf);
    return 1;
}
