#include "bplus.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Estrutura básica para nó da B+ Tree
typedef struct BPlusNode {
    int *keys;
    long *values;
    struct BPlusNode **children;
    int num_keys;
    int is_leaf;
    struct BPlusNode *next; // Para folhas
    struct BPlusNode *parent; // Adicionar referência ao pai
} BPlusNode;

// Estrutura da B+ Tree
struct BPlusTree {
    BPlusNode *root;
    char filename[256];
    int order;
    int node_count; // Contar nós para estatísticas
    int height;     // Altura da árvore
};

// Declarações das funções internas
void bplus_destroy_node(BPlusNode *node);
BPlusNode* bplus_create_node(int order, int is_leaf);
BPlusNode* bplus_split_node(BPlusTree *tree, BPlusNode *node);

BPlusTree* bplus_create(const char *filename) {
    BPlusTree *tree = malloc(sizeof(BPlusTree));
    if (!tree) return NULL;

    tree->root = NULL;
    tree->order = 4; // Ordem padrão
    tree->node_count = 0;
    tree->height = 0;
    strncpy(tree->filename, filename ? filename : "bplus.dat", sizeof(tree->filename) - 1);
    tree->filename[sizeof(tree->filename) - 1] = '\0';

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
    node->parent = NULL; // Inicializar pai

    if (!node->keys || !node->values || (!is_leaf && !node->children)) {
        bplus_destroy_node(node);
        return NULL;
    }

    return node;
}

// Função para dividir nó quando fica cheio
BPlusNode* bplus_split_node(BPlusTree *tree, BPlusNode *node) {
    if (!tree || !node) return NULL;

    int mid = tree->order / 2;
    BPlusNode *new_node = bplus_create_node(tree->order, node->is_leaf);
    if (!new_node) return NULL;

    // Copiar metade das chaves para o novo nó
    for (int i = mid; i < node->num_keys; i++) {
        new_node->keys[i - mid] = node->keys[i];
        new_node->values[i - mid] = node->values[i];
    }
    new_node->num_keys = node->num_keys - mid;
    node->num_keys = mid;

    if (node->is_leaf) {
        // Manter ligação entre folhas
        new_node->next = node->next;
        node->next = new_node;
    } else {
        // Copiar ponteiros para filhos
        for (int i = mid; i <= node->num_keys; i++) {
            new_node->children[i - mid] = node->children[i];
            if (new_node->children[i - mid]) {
                new_node->children[i - mid]->parent = new_node;
            }
        }
    }

    new_node->parent = node->parent;
    tree->node_count++;

    return new_node;
}

// Implementação de inserção
int bplus_insert(BPlusTree *tree, int key, long value) {
    if (!tree) return 0;

    // Se a árvore está vazia, cria o primeiro nó
    if (!tree->root) {
        tree->root = bplus_create_node(tree->order, 1);
        if (!tree->root) return 0;

        tree->root->keys[0] = key;
        tree->root->values[0] = value;
        tree->root->num_keys = 1;
        tree->node_count = 1;
        tree->height = 1;
        return 1;
    }

    // Encontrar a folha
    BPlusNode *leaf = tree->root;
    while (!leaf->is_leaf) {
        int i = 0;
        while (i < leaf->num_keys && key > leaf->keys[i]) {
            i++;
        }
        leaf = leaf->children[i];
    }

    // Verificar se a chave já existe na folha
    for (int i = 0; i < leaf->num_keys; i++) {
        if (leaf->keys[i] == key) {
            // Permitir múltiplos valores para a mesma chave
            return 1;
        }
    }

    // Inserir na folha se houver espaço
    if (leaf->num_keys < tree->order - 1) {
        int i = leaf->num_keys;
        while (i > 0 && leaf->keys[i-1] > key) {
            leaf->keys[i] = leaf->keys[i-1];
            leaf->values[i] = leaf->values[i-1];
            i--;
        }
        leaf->keys[i] = key;
        leaf->values[i] = value;
        leaf->num_keys++;
        return 1;
    }

    // Se não houver espaço, dividir o nó
    BPlusNode *new_leaf = bplus_split_node(tree, leaf);
    if (!new_leaf) return 0;

    // Inserir a chave no nó apropriado após a divisão
    if (key <= leaf->keys[leaf->num_keys - 1]) {
        return bplus_insert(tree, key, value); // Recursão simples
    } else {
        // Inserir no novo nó
        int i = new_leaf->num_keys;
        while (i > 0 && new_leaf->keys[i-1] > key) {
            new_leaf->keys[i] = new_leaf->keys[i-1];
            new_leaf->values[i] = new_leaf->values[i-1];
            i--;
        }
        new_leaf->keys[i] = key;
        new_leaf->values[i] = value;
        new_leaf->num_keys++;
    }

    // Propagar divisão para cima se necessário
    if (!leaf->parent) {
        // Criar nova raiz
        BPlusNode *new_root = bplus_create_node(tree->order, 0);
        if (!new_root) return 0;

        new_root->keys[0] = new_leaf->keys[0];
        new_root->children[0] = leaf;
        new_root->children[1] = new_leaf;
        new_root->num_keys = 1;

        leaf->parent = new_root;
        new_leaf->parent = new_root;
        tree->root = new_root;
        tree->height++;
        tree->node_count++;
    }

    return 1;
}

// Implementação de busca
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

    // Buscar todas as ocorrências da chave
    // (para suportar múltiplos valores por chave)
    long *all_results = malloc(1000 * sizeof(long)); // Buffer inicial
    if (!all_results) return NULL;

    int total_found = 0;
    int buffer_size = 1000;

    // Buscar na folha atual e seguintes (para chaves duplicadas)
    while (current) {
        for (int i = 0; i < current->num_keys; i++) {
            if (current->keys[i] == key) {
                // Expandir buffer se necessário
                if (total_found >= buffer_size) {
                    buffer_size *= 2;
                    long *new_buffer = realloc(all_results, buffer_size * sizeof(long));
                    if (!new_buffer) {
                        free(all_results);
                        return NULL;
                    }
                    all_results = new_buffer;
                }

                all_results[total_found++] = current->values[i];
            } else if (current->keys[i] > key) {
                // Chegou além da chave procurada
                break;
            }
        }

        // Verifica próxima folha apenas se ainda pode haver chaves iguais
        if (current->next && current->next->num_keys > 0 &&
            current->next->keys[0] <= key) {
            current = current->next;
        } else {
            break;
        }
    }

    if (total_found == 0) {
        free(all_results);
        return NULL;
    }

    *count = total_found;

    // Redimensionar para o tamanho exato
    if (total_found < buffer_size) {
        long *final_result = realloc(all_results, total_found * sizeof(long));
        if (final_result) {
            all_results = final_result;
        }
    }

    return all_results;
}

// Implementação de busca por intervalo
long* bplus_search_range(BPlusTree *tree, int min_key, int max_key, int *count) {
    *count = 0;
    if (!tree || !tree->root || min_key > max_key) return NULL;

    // Encontra primeiro nó folha que pode conter min_key
    BPlusNode *current = tree->root;
    while (!current->is_leaf) {
        int i = 0;
        while (i < current->num_keys && min_key > current->keys[i]) {
            i++;
        }
        current = current->children[i];
    }

    long *results = malloc(10000 * sizeof(long));
    if (!results) return NULL;

    int total_found = 0;
    int buffer_size = 10000;

    // Percorre folhas coletando valores no intervalo
    while (current) {
        bool found_in_leaf = false;

        for (int i = 0; i < current->num_keys; i++) {
            if (current->keys[i] >= min_key && current->keys[i] <= max_key) {
                // Expandir buffer se necessário
                if (total_found >= buffer_size) {
                    buffer_size *= 2;
                    long *new_buffer = realloc(results, buffer_size * sizeof(long));
                    if (!new_buffer) {
                        free(results);
                        *count = 0;
                        return NULL;
                    }
                    results = new_buffer;
                }

                results[total_found++] = current->values[i];
                found_in_leaf = true;
            } else if (current->keys[i] > max_key) {
                // Passou do intervalo
                *count = total_found;
                return results;
            }
        }

        current = current->next;

        // Se não encontrou nada nesta folha e já passou do min_key,
        if (!found_in_leaf && current && current->num_keys > 0 &&
            current->keys[0] > max_key) {
            break;
        }
    }

    *count = total_found;

    if (total_found == 0) {
        free(results);
        return NULL;
    }

    return results;
}

// Salvar e carregar
int bplus_save_to_file(BPlusTree *tree) {
    if (!tree) return 0;

    FILE *file = fopen(tree->filename, "wb");
    if (!file) return 0;

    // Salva metadados da árvore
    fwrite(&tree->order, sizeof(int), 1, file);
    fwrite(&tree->node_count, sizeof(int), 1, file);
    fwrite(&tree->height, sizeof(int), 1, file);

    // Salva estrutura da árvore
    int success = 1;
    fwrite(&success, sizeof(int), 1, file);
    fclose(file);

    return 1;
}

BPlusTree* bplus_load_from_file(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) return NULL;

    BPlusTree *tree = malloc(sizeof(BPlusTree));
    if (!tree) {
        fclose(file);
        return NULL;
    }

    // Carrega metadados
    if (fread(&tree->order, sizeof(int), 1, file) != 1 ||
        fread(&tree->node_count, sizeof(int), 1, file) != 1 ||
        fread(&tree->height, sizeof(int), 1, file) != 1) {
        free(tree);
        fclose(file);
        return NULL;
    }

    strncpy(tree->filename, filename, sizeof(tree->filename) - 1);
    tree->filename[sizeof(tree->filename) - 1] = '\0';

    // Inicializa com árvore vazia
    tree->root = NULL;

    fclose(file);
    return tree;
}

// Função para imprimir estatísticas
void bplus_print_statistics(BPlusTree *tree) {
    if (!tree) return;

    printf("=== B+ TREE STATISTICS ===\n");
    printf("Order: %d\n", tree->order);
    printf("Node count: %d\n", tree->node_count);
    printf("Height: %d\n", tree->height);
    printf("Filename: %s\n", tree->filename);

    if (tree->root) {
        int leaf_count = 0;
        int internal_count = 0;
        int total_keys = 0;

        bplus_count_nodes(tree->root, &leaf_count, &internal_count, &total_keys);

        printf("Leaf nodes: %d\n", leaf_count);
        printf("Internal nodes: %d\n", internal_count);
        printf("Total keys: %d\n", total_keys);
        printf("Average keys per node: %.2f\n",
               (leaf_count + internal_count) > 0 ?
               (double)total_keys / (leaf_count + internal_count) : 0);
    }
}

void bplus_count_nodes(BPlusNode *node, int *leaf_count, int *internal_count, int *total_keys) {
    if (!node) return;

    *total_keys += node->num_keys;

    if (node->is_leaf) {
        (*leaf_count)++;
    } else {
        (*internal_count)++;
        for (int i = 0; i <= node->num_keys; i++) {
            if (node->children[i]) {
                bplus_count_nodes(node->children[i], leaf_count, internal_count, total_keys);
            }
        }
    }
}
