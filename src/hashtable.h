/*
 * hashtable.h
 * -----------
 * Tabela hash com tratamento de colisoes por encadeamento separado
 * (separate chaining). Cada balde aponta para uma lista ligada de
 * usuarios. A tabela cresce automaticamente (rehash) quando o fator de
 * carga fica alto, mantendo as buscas proximas de O(1).
 */
#ifndef HASHTABLE_H
#define HASHTABLE_H

#include "user.h"
#include <stddef.h>

/* No de uma lista ligada dentro de um balde. */
typedef struct HashNode {
    Usuario          usuario;
    struct HashNode *next;
} HashNode;

typedef struct HashTable {
    HashNode **baldes;     /* vetor de ponteiros para listas */
    size_t     capacidade; /* numero de baldes */
    size_t     tamanho;    /* numero de usuarios armazenados */
    size_t     colisoes;   /* colisoes acumuladas (estatistica) */
} HashTable;

/* Cria uma tabela com capacidade inicial sugerida (arredondada). */
HashTable *ht_create(size_t capacidade_inicial);

/* Libera toda a memoria da tabela. */
void ht_free(HashTable *ht);

/*
 * Insere ou atualiza um usuario (a chave e o id).
 * Retorna 1 se inseriu um novo registro, 0 se atualizou um existente.
 */
int ht_insert(HashTable *ht, const Usuario *u);

/*
 * Procura um usuario pelo id.
 * Em "passos" (se nao for NULL) devolve quantos nos foram percorridos
 * na lista do balde, util para medir o custo da busca.
 * Retorna ponteiro para o usuario ou NULL se nao encontrado.
 */
const Usuario *ht_search(const HashTable *ht, uint64_t id, int *passos);

/* Fator de carga atual (tamanho / capacidade). */
double ht_fator_carga(const HashTable *ht);

#endif /* HASHTABLE_H */
