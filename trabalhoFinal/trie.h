#ifndef TRIE_H
#define TRIE_H

typedef struct Trie Trie;

// Forward declaration para evitar erro de tipo desconhecido
typedef struct TrieNode TrieNode;

// Funções básicas da Trie
Trie* trie_create(const char *filename);
void trie_destroy(Trie *trie);
int trie_insert(Trie *trie, const char *word, long value);
long* trie_search(Trie *trie, const char *word, int *count);

char** trie_search_prefix(Trie *trie, const char *prefix, int *result_count, int max_results);
void trie_print_statistics(Trie *trie);

// Funções de persistência
int trie_save_to_file(Trie *trie);
Trie* trie_load_from_file(const char *filename);

// Declaração da função auxiliar interna
void trie_count_statistics(TrieNode *node, int *node_count, int *word_count, int *total_values);

#endif
