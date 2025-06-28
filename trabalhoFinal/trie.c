// =============================================================================
// trie.c - Implementação da Trie
// =============================================================================

#include "trie.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define ALPHABET_SIZE 26
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

TrieNode* trie_create_node() {
    TrieNode *node = malloc(sizeof(TrieNode));
    if (!node) return NULL;

    for (int i = 0; i < ALPHABET_SIZE; i++) {
        node->children[i] = NULL;
    }

    node->values = malloc(MAX_VALUES * sizeof(long));
    node->value_count = 0;
    node->value_capacity = MAX_VALUES;
    node->is_end_of_word = 0;

    return node;
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

int char_to_index(char c) {
    if (c >= 'a' && c <= 'z') {
        return c - 'a';
    } else if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    return -1; // Caractere inválido
}

int trie_insert(Trie *trie, const char *word, long value) {
    if (!trie || !word) return 0;

    TrieNode *current = trie->root;
    int len = strlen(word);

    for (int i = 0; i < len; i++) {
        int index = char_to_index(word[i]);
        if (index == -1) continue; // Ignora caracteres não alfabéticos

        if (!current->children[index]) {
            current->children[index] = trie_create_node();
            if (!current->children[index]) return 0;
        }

        current = current->children[index];
    }

    current->is_end_of_word = 1;

    // Adiciona o valor se houver espaço
    if (current->value_count < current->value_capacity) {
        current->values[current->value_count] = value;
        current->value_count++;
    }

    return 1;
}

long* trie_search(Trie *trie, const char *word, int *count) {
    *count = 0;
    if (!trie || !word) return NULL;

    TrieNode *current = trie->root;
    int len = strlen(word);

    for (int i = 0; i < len; i++) {
        int index = char_to_index(word[i]);
        if (index == -1) continue; // Ignora caracteres não alfabéticos

        if (!current->children[index]) {
            return NULL; // Palavra não encontrada
        }

        current = current->children[index];
    }

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

int trie_save_node(FILE *file, TrieNode *node) {
    if (!node) {
        int null_marker = 0;
        fwrite(&null_marker, sizeof(int), 1, file);
        return 1;
    }

    int node_marker = 1;
    fwrite(&node_marker, sizeof(int), 1, file);
    fwrite(&node->is_end_of_word, sizeof(int), 1, file);
    fwrite(&node->value_count, sizeof(int), 1, file);

    if (node->value_count > 0) {
        fwrite(node->values, sizeof(long), node->value_count, file);
    }

    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (!trie_save_node(file, node->children[i])) {
            return 0;
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

    fread(&node->is_end_of_word, sizeof(int), 1, file);
    fread(&node->value_count, sizeof(int), 1, file);

    if (node->value_count > 0) {
        fread(node->values, sizeof(long), node->value_count, file);
    }

    for (int i = 0; i < ALPHABET_SIZE; i++) {
        node->children[i] = trie_load_node(file);
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
    trie->root = trie_load_node(file);

    fclose(file);

    if (!trie->root) {
        free(trie);
        return NULL;
    }

    return trie;
}
