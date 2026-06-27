/*
 * bloom.h
 * -------
 * Filtro de Bloom: estrutura probabilistica que responde, de forma muito
 * rapida, "este ID definitivamente NAO existe" ou "este ID PROVAVELMENTE
 * existe". Nunca produz falsos negativos, mas pode produzir falsos
 * positivos (daí a confirmacao posterior na tabela hash).
 */
#ifndef BLOOM_H
#define BLOOM_H

#include <stdint.h>
#include <stddef.h>

typedef struct BloomFilter {
    uint8_t *bits;       /* vetor de bits (1 bit por posicao) */
    size_t   num_bits;   /* quantidade total de bits (m) */
    int      num_hashes; /* numero de funcoes hash (k) */
    size_t   bits_set;   /* quantos bits estao ligados (para estatistica) */
} BloomFilter;

/*
 * Cria um filtro dimensionado para "n" itens esperados com uma taxa de
 * falsos positivos alvo "p" (ex.: 0.01 = 1%). Calcula m e k otimos.
 * Retorna NULL em caso de falha de alocacao.
 */
BloomFilter *bloom_create(size_t n_esperado, double p_falso_positivo);

/* Libera toda a memoria do filtro. */
void bloom_free(BloomFilter *bf);

/* Insere uma chave (ID) no filtro. */
void bloom_add(BloomFilter *bf, uint64_t key);

/*
 * Verifica a possivel presenca de uma chave.
 * Retorna 0  -> com certeza NAO esta no conjunto.
 * Retorna 1  -> provavelmente esta (pode ser falso positivo).
 */
int bloom_maybe_contains(const BloomFilter *bf, uint64_t key);

/* Probabilidade teorica de falso positivo dado o estado atual de preenchimento. */
double bloom_taxa_falso_positivo(const BloomFilter *bf, size_t itens_inseridos);

#endif /* BLOOM_H */
