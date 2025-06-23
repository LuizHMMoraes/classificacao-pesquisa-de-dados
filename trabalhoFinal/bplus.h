// =============================================================================
// bplus.h - Interface da árvore B+
// =============================================================================
#ifndef BPLUS_H
#define BPLUS_H

#include "disaster.h"

#define BPLUS_ORDER 5  // Ordem da árvore B+ (máximo de chaves por nó)
#define MAX_KEYS (BPLUS_ORDER - 1)
#define MAX_CHILDREN BPLUS_ORDER

// Estrutura de um nó da árvore B+
typedef struct BPlusNode {
    int is_leaf;                           // 1 se é folha, 0 se é nó interno
    int num_keys;                          // Número atual de chaves
    int keys[MAX_KEYS];                    // Array de chaves
    long file_positions[MAX_KEYS];         // Posições no arquivo (apenas em folhas)
    struct BPlusNode *children[MAX_CHILDREN];  // Ponteiros para filhos (apenas em nós internos)
    struct BPlusNode *next;                // Ponteiro para próxima folha (apenas em folhas)
} BPlusNode;

// Estrutura principal da árvore B+
typedef struct {
    BPlusNode *root;
    int total_nodes;
    char index_filename[256];
} BPlusTree;

// Funções públicas da árvore B+
BPlusTree* bplus_create(const char *index_filename);
void bplus_destroy(BPlusTree *tree);
int bplus_insert(BPlusTree *tree, int key, long file_position);
long* bplus_search(BPlusTree *tree, int key, int *count);
long* bplus_range_search(BPlusTree *tree, int min_key, int max_key, int *count);
int bplus_save_to_file(BPlusTree *tree);
BPlusTree* bplus_load_from_file(const char *index_filename);
void bplus_print_tree(BPlusTree *tree);

#endif
