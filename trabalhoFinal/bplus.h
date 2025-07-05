#ifndef BPLUS_H
#define BPLUS_H

// Estrutura opaca da B+ Tree
typedef struct BPlusTree BPlusTree;

// Forward declaration para evitar erro de tipo desconhecido
typedef struct BPlusNode BPlusNode;

// Funções básicas da B+ Tree
BPlusTree* bplus_create(const char *filename);
void bplus_destroy(BPlusTree *tree);
int bplus_insert(BPlusTree *tree, int key, long value);
long* bplus_search(BPlusTree *tree, int key, int *count);

// Nova função implementada: busca por intervalo
long* bplus_search_range(BPlusTree *tree, int min_key, int max_key, int *count);

// Função alternativa de busca por intervalo (implementação mais simples)
long* bplus_search_range_simple(BPlusTree *tree, int min_key, int max_key, int *count);

// Funções de estatísticas
void bplus_print_statistics(BPlusTree *tree);

// Funções de persistência
int bplus_save_to_file(BPlusTree *tree);
BPlusTree* bplus_load_from_file(const char *filename);

// Declaração da função auxiliar interna
void bplus_count_nodes(BPlusNode *node, int *leaf_count, int *internal_count, int *total_keys);

#endif
