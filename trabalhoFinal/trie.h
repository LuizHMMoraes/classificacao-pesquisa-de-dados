// =============================================================================
// trie.h - Interface do índice TRIE
// =============================================================================
#ifndef TRIE_H
#define TRIE_H

#include "disaster.h"
#include <ctype.h>

#define ALPHABET_SIZE 37  // 26 letras + 10 dígitos + espaço (índice 36)
#define MAX_WORD_LENGTH 100

// Estrutura para armazenar lista de posições de arquivo
typedef struct FilePosList {
    long file_position;
    struct FilePosList *next;
} FilePosList;

// Nó do TRIE
typedef struct TrieNode {
    struct TrieNode *children[ALPHABET_SIZE];
    FilePosList *file_positions;  // Lista de posições onde a palavra aparece
    int is_end_of_word;
    int count;  // Número de ocorrências desta palavra
} TrieNode;

// Estrutura principal do TRIE
typedef struct {
    TrieNode *root;
    int total_words;
    char index_filename[256];
} Trie;

// Funções públicas do TRIE
Trie* trie_create(const char *index_filename);
void trie_destroy(Trie *trie);
int trie_insert(Trie *trie, const char *word, long file_position);
long* trie_search(Trie *trie, const char *word, int *count);
long* trie_prefix_search(Trie *trie, const char *prefix, int *count);
int trie_save_to_file(Trie *trie);
Trie* trie_load_from_file(const char *index_filename);
void trie_print_stats(Trie *trie);
void trie_print_words_with_prefix(Trie *trie, const char *prefix);

#endif
