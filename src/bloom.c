/*
 * bloom.c
 * -------
 * Implementacao do filtro de Bloom usando "double hashing": derivamos as
 * k posicoes a partir de apenas duas funcoes hash, conforme a tecnica de
 * Kirsch & Mitzenmacher (g_i = h1 + i*h2). Isso e rapido e mantem boa
 * distribuicao.
 */
#include "bloom.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/*
 * Duas variacoes do hash splitmix64, um misturador de 64 bits rapido e
 * de boa qualidade. Usamos sementes diferentes para gerar h1 e h2.
 */
static uint64_t splitmix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

static uint64_t hash1(uint64_t key) { return splitmix64(key); }
static uint64_t hash2(uint64_t key) { return splitmix64(key ^ 0xD1B54A32D192ED03ULL); }

BloomFilter *bloom_create(size_t n_esperado, double p_falso_positivo) {
    if (n_esperado == 0) n_esperado = 1;
    if (p_falso_positivo <= 0.0 || p_falso_positivo >= 1.0) p_falso_positivo = 0.01;

    /* m = -(n * ln p) / (ln 2)^2  -> numero otimo de bits */
    double m = -((double) n_esperado * log(p_falso_positivo)) / (log(2.0) * log(2.0));
    size_t num_bits = (size_t) (m + 0.5);
    if (num_bits < 8) num_bits = 8;

    /* k = (m / n) * ln 2  -> numero otimo de funcoes hash */
    int num_hashes = (int) (((double) num_bits / (double) n_esperado) * log(2.0) + 0.5);
    if (num_hashes < 1) num_hashes = 1;

    BloomFilter *bf = malloc(sizeof(BloomFilter));
    if (!bf) return NULL;

    size_t num_bytes = (num_bits + 7) / 8;
    bf->bits = calloc(num_bytes, 1);
    if (!bf->bits) {
        free(bf);
        return NULL;
    }
    bf->num_bits = num_bits;
    bf->num_hashes = num_hashes;
    bf->bits_set = 0;
    return bf;
}

void bloom_free(BloomFilter *bf) {
    if (!bf) return;
    free(bf->bits);
    free(bf);
}

void bloom_add(BloomFilter *bf, uint64_t key) {
    uint64_t h1 = hash1(key);
    uint64_t h2 = hash2(key);
    for (int i = 0; i < bf->num_hashes; i++) {
        size_t pos = (size_t) ((h1 + (uint64_t) i * h2) % bf->num_bits);
        uint8_t mask = (uint8_t) (1u << (pos % 8));
        if (!(bf->bits[pos / 8] & mask)) {
            bf->bits[pos / 8] |= mask;
            bf->bits_set++;
        }
    }
}

int bloom_maybe_contains(const BloomFilter *bf, uint64_t key) {
    uint64_t h1 = hash1(key);
    uint64_t h2 = hash2(key);
    for (int i = 0; i < bf->num_hashes; i++) {
        size_t pos = (size_t) ((h1 + (uint64_t) i * h2) % bf->num_bits);
        uint8_t mask = (uint8_t) (1u << (pos % 8));
        if (!(bf->bits[pos / 8] & mask)) {
            return 0; /* algum bit zerado => com certeza ausente */
        }
    }
    return 1; /* todos os bits ligados => provavelmente presente */
}

double bloom_taxa_falso_positivo(const BloomFilter *bf, size_t itens_inseridos) {
    /* p ~= (1 - e^(-k*n/m))^k */
    double k = (double) bf->num_hashes;
    double n = (double) itens_inseridos;
    double m = (double) bf->num_bits;
    double base = 1.0 - exp(-k * n / m);
    return pow(base, k);
}
