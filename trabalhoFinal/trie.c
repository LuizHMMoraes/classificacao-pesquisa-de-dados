// =============================================================================
// trie.c - Implementação do índice TRIE
// =============================================================================

#include "trie.h"

// Função para converter caractere para índice
static int char_to_index(char c) {
    c = tolower(c);
    if (c >= 'a' && c <= 'z') {
        return c - 'a';
    } else if (c >= '0' && c <= '9') {
        return c - '0' + 26;
    } else if (c == ' ') {
        return 36;
    }
    return -1; // Caractere inválido
}

// Função para converter índice para caractere
static char index_to_char(int index) {
    if (index >= 0 && index <= 25) {
        return 'a' + index;
    } else if (index >= 26 && index <= 35) {
        return '0' + (index - 26);
    } else if (index == 36) {
        return ' ';
    }
    return '?';
}

// Função para normalizar string (minúsculas, sem caracteres especiais)
static void normalize_string(const char *input, char *output) {
    int j = 0;
    for (int i = 0; input[i] != '\0' && j < MAX_WORD_LENGTH - 1; i++) {
        char c = tolower(input[i]);
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == ' ') {
            output[j++] = c;
        }
    }
    output[j] = '\0';
}

// Função para criar um novo nó do TRIE
static TrieNode* create_trie_node() {
    TrieNode *node = (TrieNode*)calloc(1, sizeof(TrieNode));
    if (!node) return NULL;

    node->is_end_of_word = 0;
    node->count = 0;
    node->file_positions = NULL;

    for (int i = 0; i < ALPHABET_SIZE; i++) {
        node->children[i] = NULL;
    }

    return node;
}

// Função para adicionar posição de arquivo à lista
static int add_file_position(TrieNode *node, long file_position) {
    FilePosList *new_pos = (FilePosList*)malloc(sizeof(FilePosList));
    if (!new_pos) return 0;

    new_pos->file_position = file_position;
    new_pos->next = node->file_positions;
    node->file_positions = new_pos;

    return 1;
}

// Função para criar uma nova estrutura TRIE
Trie* trie_create(const char *index_filename) {
    Trie *trie = (Trie*)malloc(sizeof(Trie));
    if (!trie) return NULL;

    trie->root = create_trie_node();
    if (!trie->root) {
        free(trie);
        return NULL;
    }

    trie->total_words = 0;
    strncpy(trie->index_filename, index_filename, 255);
    trie->index_filename[255] = '\0';

    return trie;
}

// Função para destruir lista de posições
static void destroy_file_positions(FilePosList *list) {
    while (list) {
        FilePosList *temp = list;
        list = list->next;
        free(temp);
    }
}

// Função para destruir nó do TRIE recursivamente
static void destroy_trie_node(TrieNode *node) {
    if (!node) return;

    for (int i = 0; i < ALPHABET_SIZE; i++) {
        destroy_trie_node(node->children[i]);
    }

    destroy_file_positions(node->file_positions);
    free(node);
}

// Função para destruir TRIE
void trie_destroy(Trie *trie) {
    if (!trie) return;

    destroy_trie_node(trie->root);
    free(trie);
}

// Função para inserir palavra no TRIE
int trie_insert(Trie *trie, const char *word, long file_position) {
    if (!trie || !word || strlen(word) == 0) return 0;

    char normalized_word[MAX_WORD_LENGTH];
    normalize_string(word, normalized_word);

    if (strlen(normalized_word) == 0) return 0;

    TrieNode *current = trie->root;

    // Navega ou cria o caminho para a palavra
    for (int i = 0; normalized_word[i] != '\0'; i++) {
        int index = char_to_index(normalized_word[i]);
        if (index == -1) continue; // Pula caracteres inválidos

        if (!current->children[index]) {
            current->children[index] = create_trie_node();
            if (!current->children[index]) return 0;
        }

        current = current->children[index];
    }

    // Marca o fim da palavra e adiciona posição do arquivo
    if (!current->is_end_of_word) {
        current->is_end_of_word = 1;
        trie->total_words++;
    }

    current->count++;
    return add_file_position(current, file_position);
}

// Função para buscar palavra no TRIE
long* trie_search(Trie *trie, const char *word, int *count) {
    *count = 0;
    if (!trie || !word) return NULL;

    char normalized_word[MAX_WORD_LENGTH];
    normalize_string(word, normalized_word);

    TrieNode *current = trie->root;

    // Navega até o nó da palavra
    for (int i = 0; normalized_word[i] != '\0'; i++) {
        int index = char_to_index(normalized_word[i]);
        if (index == -1 || !current->children[index]) {
            return NULL; // Palavra não encontrada
        }
        current = current->children[index];
    }

    // Verifica se é fim de palavra
    if (!current->is_end_of_word) {
        return NULL;
    }

    // Coleta todas as posições de arquivo
    FilePosList *pos_list = current->file_positions;
    *count = current->count;

    if (*count == 0) return NULL;

    long *results = (long*)malloc(*count * sizeof(long));
    if (!results) {
        *count = 0;
        return NULL;
    }

    int i = 0;
    while (pos_list && i < *count) {
        results[i] = pos_list->file_position;
        pos_list = pos_list->next;
        i++;
    }

    return results;
}

// Função auxiliar para busca por prefixo
static void collect_words_with_prefix(TrieNode *node, long **results, int *count, int *capacity) {
    if (!node) return;

    if (node->is_end_of_word) {
        FilePosList *pos_list = node->file_positions;
        while (pos_list) {
            // Expande array se necessário
            if (*count >= *capacity) {
                *capacity = (*capacity == 0) ? 10 : (*capacity * 2);
                *results = (long*)realloc(*results, *capacity * sizeof(long));
            }

            if (*results) {
                (*results)[*count] = pos_list->file_position;
                (*count)++;
            }

            pos_list = pos_list->next;
        }
    }

    // Recursivamente coleta de todos os filhos
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        collect_words_with_prefix(node->children[i], results, count, capacity);
    }
}

// Função para busca por prefixo
long* trie_prefix_search(Trie *trie, const char *prefix, int *count) {
    *count = 0;
    if (!trie || !prefix) return NULL;

    char normalized_prefix[MAX_WORD_LENGTH];
    normalize_string(prefix, normalized_prefix);

    TrieNode *current = trie->root;

    // Navega até o nó do prefixo
    for (int i = 0; normalized_prefix[i] != '\0'; i++) {
        int index = char_to_index(normalized_prefix[i]);
        if (index == -1 || !current->children[index]) {
            return NULL; // Prefixo não encontrado
        }
        current = current->children[index];
    }

    // Coleta todas as palavras que começam com o prefixo
    long *results = NULL;
    int capacity = 0;
    collect_words_with_prefix(current, &results, count, &capacity);

    return results;
}

// Função para imprimir estatísticas do TRIE
void trie_print_stats(Trie *trie) {
    if (!trie) return;

    printf("=== ESTATÍSTICAS DO TRIE ===\n");
    printf("Total de palavras únicas: %d\n", trie->total_words);
    printf("Arquivo de índice: %s\n", trie->index_filename);
    printf("Tamanho do alfabeto: %d caracteres\n", ALPHABET_SIZE);
}

// Função auxiliar para imprimir palavras com prefixo
static void print_words_recursive(TrieNode *node, char *prefix, int level) {
    if (!node) return;

    if (node->is_end_of_word) {
        prefix[level] = '\0';
        printf("'%s' (%d ocorrências)\n", prefix, node->count);
    }

    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (node->children[i]) {
            prefix[level] = index_to_char(i);
            print_words_recursive(node->children[i], prefix, level + 1);
        }
    }
}

// Função para imprimir palavras com determinado prefixo
void trie_print_words_with_prefix(Trie *trie, const char *prefix) {
    if (!trie || !prefix) return;

    char normalized_prefix[MAX_WORD_LENGTH];
    normalize_string(prefix, normalized_prefix);

    TrieNode *current = trie->root;

    // Navega até o nó do prefixo
    for (int i = 0; normalized_prefix[i] != '\0'; i++) {
        int index = char_to_index(normalized_prefix[i]);
        if (index == -1 || !current->children[index]) {
            printf("Nenhuma palavra encontrada com prefixo '%s'\n", prefix);
            return;
        }
        current = current->children[index];
    }

    printf("Palavras com prefixo '%s':\n", prefix);
    char word_buffer[MAX_WORD_LENGTH];
    strcpy(word_buffer, normalized_prefix);
    print_words_recursive(current, word_buffer, strlen(normalized_prefix));
}

// Função para salvar TRIE em arquivo (implementação básica)
int trie_save_to_file(Trie *trie) {
    if (!trie) return 0;

    FILE *file = fopen(trie->index_filename, "wb");
    if (!file) return 0;

    // Salva metadados
    fwrite(&trie->total_words, sizeof(int), 1, file);

    // Implementação completa da serialização seria mais complexa
    // Para o escopo do trabalho, consideramos que a construção é rápida

    fclose(file);
    return 1;
}

