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

/*
 * Faz o parse de uma linha "id;nome;email;idade;cpf" para um Usuario.
 * Retorna 1 em sucesso, 0 se a linha for invalida.
 */
static int parse_linha_usuario(char *linha, Usuario *u) {
    char *campos[5];
    int n = 0;
    char *p = linha;

    /* Divide manualmente em ';' (strtok colapsaria campos vazios). */
    campos[n++] = p;
    while (*p && n < 5) {
        if (*p == ';') {
            *p = '\0';
            campos[n++] = p + 1;
        }
        p++;
    }
    if (n != 5) return 0;

    /* id */
    char *fim = NULL;
    unsigned long long id = strtoull(campos[0], &fim, 10);
    if (fim == campos[0]) return 0;

    /* idade */
    char *fim2 = NULL;
    long idade = strtol(campos[3], &fim2, 10);
    if (fim2 == campos[3]) idade = 0;

    u->id = (uint64_t) id;
    copia_str(u->nome,  USER_NOME_MAX,  campos[1]);
    copia_str(u->email, USER_EMAIL_MAX, campos[2]);
    u->idade = (int) idade;
    copia_str(u->cpf,   USER_CPF_MAX,   campos[4]);
    return 1;
}

/* Imprime um cartao formatado com os dados de um usuario. */
static void imprime_usuario(const Usuario *u) {
    printf("  " C_GREEN "+-----------------------------------------------+\n" C_RESET);
    printf("  " C_GREEN "|" C_RESET " " C_BOLD "ID    " C_RESET ": %-37llu " C_GREEN "|\n" C_RESET, (unsigned long long) u->id);
    printf("  " C_GREEN "|" C_RESET " " C_BOLD "Nome  " C_RESET ": %-37.37s " C_GREEN "|\n" C_RESET, u->nome);
    printf("  " C_GREEN "|" C_RESET " " C_BOLD "Email " C_RESET ": %-37.37s " C_GREEN "|\n" C_RESET, u->email);
    printf("  " C_GREEN "|" C_RESET " " C_BOLD "Idade " C_RESET ": %-37d " C_GREEN "|\n" C_RESET, u->idade);
    printf("  " C_GREEN "|" C_RESET " " C_BOLD "CPF   " C_RESET ": %-37.37s " C_GREEN "|\n" C_RESET, u->cpf);
    printf("  " C_GREEN "+-----------------------------------------------+\n" C_RESET);
}

/* ----------------------------- Acoes -------------------------------- */

/* Opcao 1: carrega usuarios de um arquivo texto. */
static void acao_carregar_arquivo(App *app) {
    char caminho[512];
    printf(C_CYAN "Caminho do arquivo " C_DIM "(ex.: data/usuarios.txt)" C_RESET ": ");
    if (!ler_linha(caminho, sizeof(caminho)) || caminho[0] == '\0') {
        printf(C_YELLOW "Operacao cancelada.\n" C_RESET);
        return;
    }

    FILE *f = fopen(caminho, "r");
    if (!f) {
        printf(C_RED "Nao foi possivel abrir '%s'.\n" C_RESET, caminho);
        return;
    }

    char linha[1024];
    unsigned long lidos = 0, inseridos = 0, invalidos = 0;
    double t0 = agora_segundos();

    while (fgets(linha, sizeof(linha), f)) {
        remove_quebra_linha(linha);
        if (linha[0] == '\0' || linha[0] == '#') continue; /* pula vazio/comentario */
        lidos++;

        Usuario u;
        if (!parse_linha_usuario(linha, &u)) {
            invalidos++;
            continue;
        }
        if (ht_insert(app->ht, &u)) inseridos++;
        bloom_add(app->bloom, u.id);
    }
    double t1 = agora_segundos();
    fclose(f);

    printf(C_GREEN "\nArquivo processado em %.2f ms\n" C_RESET, (t1 - t0) * 1000.0);
    printf("  Linhas validas lidas : " C_BOLD "%lu\n" C_RESET, lidos);
    printf("  Novos registros      : " C_BOLD "%lu\n" C_RESET, inseridos);
    if (invalidos) {
        printf("  " C_YELLOW "Linhas ignoradas     : %lu\n" C_RESET, invalidos);
    }
    printf("  Total na tabela      : " C_BOLD "%zu\n" C_RESET, app->ht->tamanho);
}

/* Opcao 2: cadastra um usuario manualmente. */
static void acao_cadastrar(App *app) {
    Usuario u;
    char buf[256];

    printf(C_CYAN "ID (numero inteiro): " C_RESET);
    if (!ler_linha(buf, sizeof(buf)) || buf[0] == '\0') { printf(C_YELLOW "Cancelado.\n" C_RESET); return; }
    u.id = strtoull(buf, NULL, 10);

    printf(C_CYAN "Nome: " C_RESET);
    if (!ler_linha(buf, sizeof(buf))) return;
    copia_str(u.nome, USER_NOME_MAX, buf);

    printf(C_CYAN "Email: " C_RESET);
    if (!ler_linha(buf, sizeof(buf))) return;
    copia_str(u.email, USER_EMAIL_MAX, buf);

    printf(C_CYAN "Idade: " C_RESET);
    if (!ler_linha(buf, sizeof(buf))) return;
    u.idade = (int) strtol(buf, NULL, 10);

    printf(C_CYAN "CPF: " C_RESET);
    if (!ler_linha(buf, sizeof(buf))) return;
    copia_str(u.cpf, USER_CPF_MAX, buf);

    int novo = ht_insert(app->ht, &u);
    bloom_add(app->bloom, u.id);

    if (novo) {
        printf(C_GREEN "\nUsuario cadastrado com sucesso!\n" C_RESET);
    } else {
        printf(C_YELLOW "\nJa existia um usuario com este ID; os dados foram atualizados.\n" C_RESET);
    }
    imprime_usuario(&u);
}

/*
 * Realiza a busca por ID. Se "usar_bloom" for verdadeiro, consulta o
 * filtro de Bloom antes: se ele disser "ausente", evitamos a busca na
 * tabela hash por completo.
 */
static void buscar_por_id(App *app, uint64_t id, int usar_bloom) {
    app->consultas_total++;

    double t0 = agora_segundos();

    if (usar_bloom) {
        if (!bloom_maybe_contains(app->bloom, id)) {
            double t1 = agora_segundos();
            app->ultimo_tempo_us = (t1 - t0) * 1e6;
            app->bloom_descartes++;
            printf(C_BLUE "\n[Bloom] " C_RESET "ID %llu descartado instantaneamente "
                   "(com certeza nao existe).\n", (unsigned long long) id);
            printf(C_DIM "  Tempo: %.3f us | a tabela hash nem foi consultada.\n" C_RESET,
                   app->ultimo_tempo_us);
            return;
        }
    }

    int passos = 0;
    const Usuario *u = ht_search(app->ht, id, &passos);
    double t1 = agora_segundos();
    app->ultimo_tempo_us = (t1 - t0) * 1e6;

    if (u) {
        app->consultas_encontradas++;
        printf(C_GREEN "\nUsuario encontrado!" C_RESET);
        printf(C_DIM "  (%.3f us, %d passo(s) no balde)\n" C_RESET, app->ultimo_tempo_us, passos);
        imprime_usuario(u);
    } else {
        /* Se o Bloom disse "talvez" mas nao achamos, foi um falso positivo. */
        if (usar_bloom) app->bloom_falsos_positivos++;
        printf(C_RED "\nUsuario com ID %llu nao encontrado." C_RESET, (unsigned long long) id);
        printf(C_DIM "  (%.3f us)\n" C_RESET, app->ultimo_tempo_us);
        if (usar_bloom) {
            printf(C_DIM "  (o Bloom indicou 'talvez', mas era um falso positivo)\n" C_RESET);
        }
    }
}

/* Le um ID do usuario e dispara a busca. */
static void acao_consultar(App *app, int usar_bloom) {
    char buf[64];
    printf(C_CYAN "Digite o ID a consultar: " C_RESET);
    if (!ler_linha(buf, sizeof(buf)) || buf[0] == '\0') {
        printf(C_YELLOW "Cancelado.\n" C_RESET);
        return;
    }
    uint64_t id = strtoull(buf, NULL, 10);
    buscar_por_id(app, id, usar_bloom);
}

/* Opcao 5: painel de estatisticas de uso e desempenho. */
static void acao_estatisticas(App *app) {
    printf(C_BOLD C_CYAN "\n========== ESTATISTICAS ==========\n" C_RESET);

    printf(C_BOLD "\n-- Tabela hash --\n" C_RESET);
    printf("  Registros armazenados : " C_BOLD "%zu\n" C_RESET, app->ht->tamanho);
    printf("  Baldes (capacidade)   : %zu\n", app->ht->capacidade);
    printf("  Fator de carga        : %.3f\n", ht_fator_carga(app->ht));
    printf("  Colisoes acumuladas   : %zu\n", app->ht->colisoes);

    printf(C_BOLD "\n-- Filtro de Bloom --\n" C_RESET);
    printf("  Bits totais (m)       : %zu\n", app->bloom->num_bits);
    printf("  Funcoes hash (k)      : %d\n", app->bloom->num_hashes);
    printf("  Bits ligados          : %zu (%.1f%%)\n",
           app->bloom->bits_set,
           app->bloom->num_bits ? 100.0 * (double) app->bloom->bits_set / (double) app->bloom->num_bits : 0.0);
    printf("  Falso positivo (est.) : %.4f%%\n",
           bloom_taxa_falso_positivo(app->bloom, app->ht->tamanho) * 100.0);

    printf(C_BOLD "\n-- Uso / desempenho --\n" C_RESET);
    printf("  Consultas realizadas  : %lu\n", app->consultas_total);
    printf("  Encontradas           : %lu\n", app->consultas_encontradas);
    printf("  Descartes pelo Bloom  : %lu\n", app->bloom_descartes);
    printf("  Falsos positivos      : %lu\n", app->bloom_falsos_positivos);
    printf("  Tempo da ultima busca : %.3f us\n", app->ultimo_tempo_us);

    printf(C_BOLD C_CYAN "\n==================================\n" C_RESET);
}

/* ------------------------------ Menu -------------------------------- */

static void imprime_cabecalho(void) {
    printf(C_BOLD C_CYAN
        "\n"
        "  ____                       _ _          _____ ____  ___  \n"
        " / ___|___  _ __  ___ _   _ | | |_ __ _  | ____|  _ \\|__ \\ \n"
        "| |   / _ \\| '_ \\/ __| | | || | __/ _` | |  _| | | | | / / \n"
        "| |__| (_) | | | \\__ \\ |_| || | || (_| | | |___| |_| |/ /_ \n"
        " \\____\\___/|_| |_|___/\\__,_||_|\\__\\__,_| |_____|____/|____|\n"
        C_RESET
        C_DIM "  Consulta rapida de usuarios | Hash + Bloom\n" C_RESET);
}

static void imprime_menu(void) {
    printf(C_BOLD "\n-------------------- MENU --------------------\n" C_RESET);
    printf("  " C_CYAN "1" C_RESET " - Carregar usuarios de um arquivo\n");
    printf("  " C_CYAN "2" C_RESET " - Cadastrar novo usuario\n");
    printf("  " C_CYAN "3" C_RESET " - Consultar usuario por ID (hash direto)\n");
    printf("  " C_CYAN "4" C_RESET " - Consultar usuario por ID (com filtro de Bloom)\n");
    printf("  " C_CYAN "5" C_RESET " - Estatisticas de uso e desempenho\n");
    printf("  " C_CYAN "0" C_RESET " - Sair\n");
    printf(C_BOLD "----------------------------------------------\n" C_RESET);
    printf(C_YELLOW "Escolha uma opcao: " C_RESET);
}

int main(int argc, char **argv) {
    App app;
    app.ht = ht_create(CAP_INICIAL);
    app.bloom = bloom_create(BLOOM_N_ESPERADO, BLOOM_P);
    app.consultas_total = 0;
    app.consultas_encontradas = 0;
    app.bloom_descartes = 0;
    app.bloom_falsos_positivos = 0;
    app.ultimo_tempo_us = 0.0;

    if (!app.ht || !app.bloom) {
        fprintf(stderr, C_RED "Erro: falha ao alocar estruturas iniciais.\n" C_RESET);
        return EXIT_FAILURE;
    }

    imprime_cabecalho();

    /* Permite passar um arquivo direto na linha de comando: ./consulta data/usuarios.txt */
    if (argc > 1) {
        FILE *f = fopen(argv[1], "r");
        if (f) {
            char linha[1024];
            unsigned long inseridos = 0;
            while (fgets(linha, sizeof(linha), f)) {
                remove_quebra_linha(linha);
                if (linha[0] == '\0' || linha[0] == '#') continue;
                Usuario u;
                if (parse_linha_usuario(linha, &u)) {
                    if (ht_insert(app.ht, &u)) inseridos++;
                    bloom_add(app.bloom, u.id);
                }
            }
            fclose(f);
            printf(C_GREEN "\nCarregados %lu usuarios de '%s' automaticamente.\n" C_RESET,
                   inseridos, argv[1]);
        } else {
            printf(C_YELLOW "\nAviso: nao foi possivel abrir '%s'.\n" C_RESET, argv[1]);
        }
    }

    char buf[64];
    int rodando = 1;
    while (rodando) {
        imprime_menu();
        if (!ler_linha(buf, sizeof(buf))) break; /* EOF (ex.: Ctrl+D) */

        switch (buf[0]) {
            case '1': acao_carregar_arquivo(&app); break;
            case '2': acao_cadastrar(&app);        break;
            case '3': acao_consultar(&app, 0);     break;
            case '4': acao_consultar(&app, 1);     break;
            case '5': acao_estatisticas(&app);     break;
            case '0':
                rodando = 0;
                break;
            default:
                printf(C_RED "Opcao invalida. Tente novamente.\n" C_RESET);
                break;
        }
    }

    printf(C_CYAN "\nAte logo!\n" C_RESET);
    ht_free(app.ht);
    bloom_free(app.bloom);
    return EXIT_SUCCESS;
}
