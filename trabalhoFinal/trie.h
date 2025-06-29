
#ifndef TRIE_H
#define TRIE_H

// Estrutura opaca da Trie
typedef struct Trie Trie;

// Funções da Trie
Trie* trie_create(const char *filename);
void trie_destroy(Trie *trie);
int trie_insert(Trie *trie, const char *word, long value);
long* trie_search(Trie *trie, const char *word, int *count);
int trie_save_to_file(Trie *trie);
Trie* trie_load_from_file(const char *filename);

#endif
