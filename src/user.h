/*
 * user.h
 * ------
 * Define a estrutura de dados de um usuario e os limites de tamanho
 * dos seus campos. Esta estrutura e compartilhada pela tabela hash,
 * pelo filtro de Bloom e pelo carregador de arquivos.
 */
#ifndef USER_H
#define USER_H

#include <stdint.h>

/* Tamanhos maximos dos campos de texto (incluindo o terminador '\0'). */
#define USER_NOME_MAX  100
#define USER_EMAIL_MAX 120
#define USER_CPF_MAX   20   /* "000.000.000-00" cabe com folga */

/*
 * Representa um unico registro de usuario.
 * O campo "id" e a chave usada tanto na tabela hash quanto no Bloom.
 */
typedef struct Usuario {
    uint64_t id;                  /* identificador unico (chave de busca) */
    char     nome[USER_NOME_MAX];
    char     email[USER_EMAIL_MAX];
    int      idade;
    char     cpf[USER_CPF_MAX];
} Usuario;

#endif /* USER_H */
