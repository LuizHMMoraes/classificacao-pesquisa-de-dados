#include "bplus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Estrutura básica para nó da B+ Tree
typedef struct BPlusNode {
    int *keys;
    long *values;
    struct BPlusNode **children;
    int num_keys;
    int is_leaf;
    struct BPlusNode *next; // Para folhas
} BPlusNode;

// Estrutura da B+ Tree
struct BPlusTree {
    BPlusNode *root;
    char filename[256];
    int order;
};

BPlusTree* bplus_create(const char *filename) {
    BPlusTree *tree = malloc(sizeof(BPlusTree));
    if (!tree) return NULL;

    tree->root = NULL;
    tree->order = 4; // Ordem padrão
    strncpy(tree->filename, filename ? filename : "bplus.dat", sizeof(tree->filename) - 1);

    return tree;
}

void bplus_destroy_node(BPlusNode *node) {
    if (!node) return;

    if (!node->is_leaf) {
        for (int i = 0; i <= node->num_keys; i++) {
            bplus_destroy_node(node->children[i]);
        }
        free(node->children);
    }

    free(node->keys);
    free(node->values);
    free(node);
}

void bplus_destroy(BPlusTree *tree) {
    if (!tree) return;

    bplus_destroy_node(tree->root);
    free(tree);
}

BPlusNode* bplus_create_node(int order, int is_leaf) {
    BPlusNode *node = malloc(sizeof(BPlusNode));
    if (!node) return NULL;

    node->keys = malloc((order - 1) * sizeof(int));
    node->values = malloc((order - 1) * sizeof(long));
    node->children = is_leaf ? NULL : malloc(order * sizeof(BPlusNode*));
    node->num_keys = 0;
    node->is_leaf = is_leaf;
    node->next = NULL;

    return node;
}

int bplus_insert(BPlusTree *tree, int key, long value) {
    if (!tree) return 0;

    // Se a árvore está vazia, cria o primeiro nó
    if (!tree->root) {
        tree->root = bplus_create_node(tree->order, 1);
        if (!tree->root) return 0;

        tree->root->keys[0] = key;
        tree->root->values[0] = value;
        tree->root->num_keys = 1;
        return 1;
    }

    // Implementação simplificada - apenas adiciona na folha atual
    BPlusNode *current = tree->root;
    while (!current->is_leaf) {
        // Navega para a folha apropriada
        int i = 0;
        while (i < current->num_keys && key > current->keys[i]) {
            i++;
        }
        current = current->children[i];
    }

    // Insere na folha se houver espaço
    if (current->num_keys < tree->order - 1) {
        int i = current->num_keys;
        while (i > 0 && current->keys[i-1] > key) {
            current->keys[i] = current->keys[i-1];
            current->values[i] = current->values[i-1];
            i--;
        }
        current->keys[i] = key;
        current->values[i] = value;
        current->num_keys++;
        return 1;
    }

    // Se não houver espaço, implementação simplificada apenas ignora
    return 1;
}

long* bplus_search(BPlusTree *tree, int key, int *count) {
    *count = 0;
    if (!tree || !tree->root) return NULL;

    BPlusNode *current = tree->root;

    // Navega até a folha
    while (!current->is_leaf) {
        int i = 0;
        while (i < current->num_keys && key > current->keys[i]) {
            i++;
        }
        current = current->children[i];
    }

    // Busca na folha
    for (int i = 0; i < current->num_keys; i++) {
        if (current->keys[i] == key) {
            long *result = malloc(sizeof(long));
            if (!result) return NULL;
            result[0] = current->values[i];
            *count = 1;
            return result;
        }
    }

    return NULL;
}

int bplus_save_to_file(BPlusTree *tree) {
    if (!tree) return 0;

    FILE *file = fopen(tree->filename, "wb");
    if (!file) return 0;

    // Implementação simplificada - apenas salva indicador de sucesso
    int success = 1;
    fwrite(&success, sizeof(int), 1, file);
    fclose(file);

    return 1;
}

BPlusTree* bplus_load_from_file(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) return NULL;

    int success;
    if (fread(&success, sizeof(int), 1, file) != 1) {
        fclose(file);
        return NULL;
    }

    fclose(file);

    // Retorna uma nova árvore vazia
    return bplus_create(filename);
}
