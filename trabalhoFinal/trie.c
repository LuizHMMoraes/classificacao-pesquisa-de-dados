#include "trie.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define ALPHABET_SIZE 128
#define MAX_VALUES 1000

// Nó da Trie
typedef struct TrieNode {
    struct TrieNode *children[ALPHABET_SIZE];
    long *values;
    int value_count;
    int value_capacity;
    int is_end_of_word;
} TrieNode;

// Estrutura da Trie
struct Trie {
    TrieNode *root;
    char filename[256];
};

// Declarações das funções internas
TrieNode* trie_create_node(void);
void trie_destroy_node(TrieNode *node);
char* normalize_string(const char *str);
int char_to_index(char c);
int trie_search_prefix_internal(TrieNode *node, const char *prefix, int pos,
                               char **results, int *result_count, int max_results,
                               char *current_word, int word_pos);
int trie_save_node(FILE *file, TrieNode *node);
TrieNode* trie_load_node(FILE *file);

TrieNode* trie_create_node(void) {
    TrieNode *node = malloc(sizeof(TrieNode));
    if (!node) return NULL;

    for (int i = 0; i < ALPHABET_SIZE; i++) {
        node->children[i] = NULL;
    }

    node->values = malloc(MAX_VALUES * sizeof(long));
    if (!node->values) {
        free(node);
        return NULL;
    }

    node->value_count = 0;
    node->value_capacity = MAX_VALUES;
    node->is_end_of_word = 0;

    return node;
}

// Função para mapear caracteres
int char_to_index(char c) {
    // Mapear caracteres ASCII para índices
    if (c >= 0 && c < ALPHABET_SIZE) {
        return (unsigned char)c;
    }
    return -1; // Caractere inválido
}

// Normaliza strings para busca case-insensitive
char* normalize_string(const char *str) {
    if (!str) return NULL;

    int len = strlen(str);
    char *normalized = malloc(len + 1);
    if (!normalized) return NULL;

    for (int i = 0; i < len; i++) {
        normalized[i] = tolower(str[i]);
        // Substitui espaços por underscores para compatibilidade
        if (normalized[i] == ' ') {
            normalized[i] = '_';
        }
    }
    normalized[len] = '\0';

    return normalized;
}

Trie* trie_create(const char *filename) {
    Trie *trie = malloc(sizeof(Trie));
    if (!trie) return NULL;

    trie->root = trie_create_node();
    if (!trie->root) {
        free(trie);
        return NULL;
    }

    strncpy(trie->filename, filename ? filename : "trie.dat", sizeof(trie->filename) - 1);
    trie->filename[sizeof(trie->filename) - 1] = '\0';

    return trie;
}

void trie_destroy_node(TrieNode *node) {
    if (!node) return;

    for (int i = 0; i < ALPHABET_SIZE; i++) {
        trie_destroy_node(node->children[i]);
    }

    free(node->values);
    free(node);
}

void trie_destroy(Trie *trie) {
    if (!trie) return;

    trie_destroy_node(trie->root);
    free(trie);
}

int trie_insert(Trie *trie, const char *word, long value) {
    if (!trie || !word) return 0;

    // Normaliza string antes de inserir
    char *normalized_word = normalize_string(word);
    if (!normalized_word) return 0;

    TrieNode *current = trie->root;
    int len = strlen(normalized_word);

    for (int i = 0; i < len; i++) {
        int index = char_to_index(normalized_word[i]);
        if (index == -1) {
            free(normalized_word);
            return 0; // Caractere inválido
        }

        if (!current->children[index]) {
            current->children[index] = trie_create_node();
            if (!current->children[index]) {
                free(normalized_word);
                return 0;
            }
        }

        current = current->children[index];
    }

    current->is_end_of_word = 1;

    // Verifica duplicatas antes de adicionar
    for (int i = 0; i < current->value_count; i++) {
        if (current->values[i] == value) {
            free(normalized_word);
            return 1; // Valor já existe, mas não é erro
        }
    }

    // Adiciona o valor se houver espaço
    if (current->value_count < current->value_capacity) {
        current->values[current->value_count] = value;
        current->value_count++;
    } else {
        long *new_values = realloc(current->values,
                                  (current->value_capacity * 2) * sizeof(long));
        if (new_values) {
            current->values = new_values;
            current->values[current->value_count] = value;
            current->value_count++;
            current->value_capacity *= 2;
        }
    }

    free(normalized_word);
    return 1;
}

long* trie_search(Trie *trie, const char *word, int *count) {
    *count = 0;
    if (!trie || !word) return NULL;

    // Normaliza string antes de buscar
    char *normalized_word = normalize_string(word);
    if (!normalized_word) return NULL;

    TrieNode *current = trie->root;
    int len = strlen(normalized_word);

    for (int i = 0; i < len; i++) {
        int index = char_to_index(normalized_word[i]);
        if (index == -1) {
            free(normalized_word);
            return NULL;
        }

        if (!current->children[index]) {
            free(normalized_word);
            return NULL; // Palavra não encontrada
        }

        current = current->children[index];
    }

    free(normalized_word);

    if (current->is_end_of_word && current->value_count > 0) {
        long *result = malloc(current->value_count * sizeof(long));
        if (!result) return NULL;

        for (int i = 0; i < current->value_count; i++) {
            result[i] = current->values[i];
        }

        *count = current->value_count;
        return result;
    }

    return NULL;
}

// Função para busca por prefixo
int trie_search_prefix_internal(TrieNode *node, const char *prefix, int pos,
                               char **results, int *result_count, int max_results,
                               char *current_word, int word_pos) {
    if (!node || *result_count >= max_results) return 0;

    if (pos == strlen(prefix)) {
        // Chegou ao final do prefixo, coletar todas as palavras a partir daqui
        if (node->is_end_of_word) {
            current_word[word_pos] = '\0';
            results[*result_count] = malloc(strlen(current_word) + 1);
            if (results[*result_count]) {
                strcpy(results[*result_count], current_word);
                (*result_count)++;
            }
        }

        // Continuar explorando filhos
        for (int i = 0; i < ALPHABET_SIZE && *result_count < max_results; i++) {
            if (node->children[i]) {
                current_word[word_pos] = (char)i;
                trie_search_prefix_internal(node->children[i], prefix, pos,
                                          results, result_count, max_results,
                                          current_word, word_pos + 1);
            }
        }
        return 1;
    }

    // Ainda navegando pelo prefixo
    int index = char_to_index(prefix[pos]);
    if (index != -1 && node->children[index]) {
        current_word[word_pos] = prefix[pos];
        return trie_search_prefix_internal(node->children[index], prefix, pos + 1,
                                         results, result_count, max_results,
                                         current_word, word_pos + 1);
    }

    return 0;
}

// Nova função para busca por prefixo
char** trie_search_prefix(Trie *trie, const char *prefix, int *result_count, int max_results) {
    *result_count = 0;
    if (!trie || !prefix || max_results <= 0) return NULL;

    char *normalized_prefix = normalize_string(prefix);
    if (!normalized_prefix) return NULL;

    char **results = malloc(max_results * sizeof(char*));
    if (!results) {
        free(normalized_prefix);
        return NULL;
    }

    char current_word[256];
    trie_search_prefix_internal(trie->root, normalized_prefix, 0,
                              results, result_count, max_results,
                              current_word, 0);

    free(normalized_prefix);

    if (*result_count == 0) {
        free(results);
        return NULL;
    }

    return results;
}

// Melhorar salvamento para suportar ALPHABET_SIZE maior
int trie_save_node(FILE *file, TrieNode *node) {
    if (!node) {
        int null_marker = 0;
        if (fwrite(&null_marker, sizeof(int), 1, file) != 1) return 0;
        return 1;
    }

    int node_marker = 1;
    if (fwrite(&node_marker, sizeof(int), 1, file) != 1) return 0;
    if (fwrite(&node->is_end_of_word, sizeof(int), 1, file) != 1) return 0;
    if (fwrite(&node->value_count, sizeof(int), 1, file) != 1) return 0;

    if (node->value_count > 0) {
        if (fwrite(node->values, sizeof(long), node->value_count, file) != node->value_count) {
            return 0;
        }
    }

    // Salva informação sobre quais filhos existem
    unsigned char children_bitmap[ALPHABET_SIZE / 8 + 1];
    memset(children_bitmap, 0, sizeof(children_bitmap));

    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (node->children[i]) {
            children_bitmap[i / 8] |= (1 << (i % 8));
        }
    }

    if (fwrite(children_bitmap, sizeof(children_bitmap), 1, file) != 1) return 0;

    // Salva apenas os filhos que existem
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (node->children[i]) {
            if (!trie_save_node(file, node->children[i])) {
                return 0;
            }
        }
    }

    return 1;
}

int trie_save_to_file(Trie *trie) {
    if (!trie) return 0;

    FILE *file = fopen(trie->filename, "wb");
    if (!file) return 0;

    int success = trie_save_node(file, trie->root);
    fclose(file);

    return success;
}

// Melhorar carregamento
TrieNode* trie_load_node(FILE *file) {
    int node_marker;
    if (fread(&node_marker, sizeof(int), 1, file) != 1) {
        return NULL;
    }

    if (node_marker == 0) {
        return NULL; // Nó nulo
    }

    TrieNode *node = trie_create_node();
    if (!node) return NULL;

    if (fread(&node->is_end_of_word, sizeof(int), 1, file) != 1) {
        trie_destroy_node(node);
        return NULL;
    }

    if (fread(&node->value_count, sizeof(int), 1, file) != 1) {
        trie_destroy_node(node);
        return NULL;
    }

    if (node->value_count > 0) {
        if (node->value_count > node->value_capacity) {
            long *new_values = realloc(node->values, node->value_count * sizeof(long));
            if (!new_values) {
                trie_destroy_node(node);
                return NULL;
            }
            node->values = new_values;
            node->value_capacity = node->value_count;
        }

        if (fread(node->values, sizeof(long), node->value_count, file) != node->value_count) {
            trie_destroy_node(node);
            return NULL;
        }
    }

    // Carrega bitmap dos filhos
    unsigned char children_bitmap[ALPHABET_SIZE / 8 + 1];
    if (fread(children_bitmap, sizeof(children_bitmap), 1, file) != 1) {
        trie_destroy_node(node);
        return NULL;
    }

    // Carrega apenas os filhos que existem
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (children_bitmap[i / 8] & (1 << (i % 8))) {
            node->children[i] = trie_load_node(file);
            if (!node->children[i]) {
                trie_destroy_node(node);
                return NULL;
            }
        }
    }

    return node;
}

Trie* trie_load_from_file(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) return NULL;

    Trie *trie = malloc(sizeof(Trie));
    if (!trie) {
        fclose(file);
        return NULL;
    }

    strncpy(trie->filename, filename, sizeof(trie->filename) - 1);
    trie->filename[sizeof(trie->filename) - 1] = '\0';
    trie->root = trie_load_node(file);

    fclose(file);

    if (!trie->root) {
        free(trie);
        return NULL;
    }

    return trie;
}

// Adiciona função para estatísticas da Trie
void trie_print_statistics(Trie *trie) {
    if (!trie || !trie->root) return;

    int node_count = 0;
    int word_count = 0;
    int total_values = 0;

    trie_count_statistics(trie->root, &node_count, &word_count, &total_values);

    printf("=== TRIE STATISTICS ===\n");
    printf("Total nodes: %d\n", node_count);
    printf("Total words: %d\n", word_count);
    printf("Total values: %d\n", total_values);
    printf("Average values per word: %.2f\n",
           word_count > 0 ? (double)total_values / word_count : 0);
}

void trie_count_statistics(TrieNode *node, int *node_count, int *word_count, int *total_values) {
    if (!node) return;

    (*node_count)++;

    if (node->is_end_of_word) {
        (*word_count)++;
        *total_values += node->value_count;
    }

    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (node->children[i]) {
            trie_count_statistics(node->children[i], node_count, word_count, total_values);
        }
    }
}
