/*
 * hashtable.c
 * -----------
 * Implementacao da tabela hash com encadeamento separado e rehash
 * automatico. A funcao de hash usa o misturador splitmix64 para
 * espalhar bem os IDs inteiros entre os baldes.
 */
#include "hashtable.h"

#include <stdlib.h>
#include <string.h>

/* Fator de carga maximo antes de dobrar a capacidade. */
#define HT_FATOR_CARGA_MAX 0.75

/* Mistura de 64 bits (mesma ideia usada no filtro de Bloom). */
static uint64_t mix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

/* Indice do balde para uma chave dada uma capacidade. */
static size_t indice_balde(uint64_t id, size_t capacidade) {
    return (size_t) (mix64(id) % capacidade);
}

/* Arredonda a capacidade para um numero impar minimo razoavel. */
static size_t normaliza_capacidade(size_t c) {
    if (c < 16) c = 16;
    return c;
}

HashTable *ht_create(size_t capacidade_inicial) {
    HashTable *ht = malloc(sizeof(HashTable));
    if (!ht) return NULL;

    ht->capacidade = normaliza_capacidade(capacidade_inicial);
    ht->tamanho = 0;
    ht->colisoes = 0;
    ht->baldes = calloc(ht->capacidade, sizeof(HashNode *));
    if (!ht->baldes) {
        free(ht);
        return NULL;
    }
    return ht;
}

void ht_free(HashTable *ht) {
    if (!ht) return;
    for (size_t i = 0; i < ht->capacidade; i++) {
        HashNode *no = ht->baldes[i];
        while (no) {
            HashNode *prox = no->next;
            free(no);
            no = prox;
        }
    }
    free(ht->baldes);
    free(ht);
}

/*
 * Dobra a capacidade e redistribui todos os nos. Reaproveita os nos
 * existentes (apenas religa os ponteiros), sem realocar cada usuario.
 */
static int ht_rehash(HashTable *ht) {
    size_t nova_cap = ht->capacidade * 2;
    HashNode **novos = calloc(nova_cap, sizeof(HashNode *));
    if (!novos) return 0;

    for (size_t i = 0; i < ht->capacidade; i++) {
        HashNode *no = ht->baldes[i];
        while (no) {
            HashNode *prox = no->next;
            size_t idx = indice_balde(no->usuario.id, nova_cap);
            no->next = novos[idx];
            novos[idx] = no;
            no = prox;
        }
    }
    free(ht->baldes);
    ht->baldes = novos;
    ht->capacidade = nova_cap;
    return 1;
}

int ht_insert(HashTable *ht, const Usuario *u) {
    size_t idx = indice_balde(u->id, ht->capacidade);

    /* Se ja existe um usuario com este id, atualiza no lugar. */
    for (HashNode *no = ht->baldes[idx]; no != NULL; no = no->next) {
        if (no->usuario.id == u->id) {
            no->usuario = *u;
            return 0;
        }
    }

    HashNode *novo = malloc(sizeof(HashNode));
    if (!novo) return 0;
    novo->usuario = *u;

    /* Se o balde ja tinha algo, contamos como colisao. */
    if (ht->baldes[idx] != NULL) {
        ht->colisoes++;
    }
    novo->next = ht->baldes[idx];
    ht->baldes[idx] = novo;
    ht->tamanho++;

    if (ht_fator_carga(ht) > HT_FATOR_CARGA_MAX) {
        ht_rehash(ht);
    }
    return 1;
}

const Usuario *ht_search(const HashTable *ht, uint64_t id, int *passos) {
    size_t idx = indice_balde(id, ht->capacidade);
    int n = 0;
    for (HashNode *no = ht->baldes[idx]; no != NULL; no = no->next) {
        n++;
        if (no->usuario.id == id) {
            if (passos) *passos = n;
            return &no->usuario;
        }
    }
    if (passos) *passos = n;
    return NULL;
}

double ht_fator_carga(const HashTable *ht) {
    if (ht->capacidade == 0) return 0.0;
    return (double) ht->tamanho / (double) ht->capacidade;
}
