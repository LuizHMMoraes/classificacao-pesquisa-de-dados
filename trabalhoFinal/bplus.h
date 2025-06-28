// =============================================================================
// bplus.h - Header da B+ Tree
// =============================================================================
#ifndef BPLUS_H
#define BPLUS_H

// Estrutura opaca da B+ Tree
typedef struct BPlusTree BPlusTree;

// Funções da B+ Tree
BPlusTree* bplus_create(const char *filename);
void bplus_destroy(BPlusTree *tree);
int bplus_insert(BPlusTree *tree, int key, long value);
long* bplus_search(BPlusTree *tree, int key, int *count);
int bplus_save_to_file(BPlusTree *tree);
BPlusTree* bplus_load_from_file(const char *filename);

#endif
