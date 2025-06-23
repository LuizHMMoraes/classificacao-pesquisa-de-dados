// =============================================================================
// bplus.c - Implementação da árvore B+
// =============================================================================

#include "bplus.h"

// Função para criar um novo nó
static BPlusNode* create_node(int is_leaf) {
    BPlusNode *node = (BPlusNode*)calloc(1, sizeof(BPlusNode));
    if (!node) return NULL;

    node->is_leaf = is_leaf;
    node->num_keys = 0;
    node->next = NULL;

    return node;
}

// Função para criar uma nova árvore B+
BPlusTree* bplus_create(const char *index_filename) {
    BPlusTree *tree = (BPlusTree*)malloc(sizeof(BPlusTree));
    if (!tree) return NULL;

    tree->root = create_node(1); // Raiz inicial é uma folha
    tree->total_nodes = 1;
    strncpy(tree->index_filename, index_filename, 255);
    tree->index_filename[255] = '\0';

    return tree;
}

// Função para destruir um nó recursivamente
static void destroy_node(BPlusNode *node) {
    if (!node) return;

    if (!node->is_leaf) {
        for (int i = 0; i <= node->num_keys; i++) {
            destroy_node(node->children[i]);
        }
    }

    free(node);
}

// Função para destruir a árvore
void bplus_destroy(BPlusTree *tree) {
    if (!tree) return;

    destroy_node(tree->root);
    free(tree);
}

// Função para buscar em uma folha
static long* search_leaf(BPlusNode *leaf, int key, int *count) {
    *count = 0;

    // Array dinâmico para armazenar resultados
    long *results = NULL;
    int capacity = 0;

    for (int i = 0; i < leaf->num_keys; i++) {
        if (leaf->keys[i] == key) {
            // Expande o array se necessário
            if (*count >= capacity) {
                capacity = capacity == 0 ? 2 : capacity * 2;
                results = (long*)realloc(results, capacity * sizeof(long));
            }
            results[*count] = leaf->file_positions[i];
            (*count)++;
        }
    }

    return results;
}

// Função de busca principal
long* bplus_search(BPlusTree *tree, int key, int *count) {
    if (!tree || !tree->root) {
        *count = 0;
        return NULL;
    }

    BPlusNode *current = tree->root;

    // Navega até a folha apropriada
    while (!current->is_leaf) {
        int i = 0;
        while (i < current->num_keys && key >= current->keys[i]) {
            i++;
        }
        current = current->children[i];
    }

    // Busca na folha
    return search_leaf(current, key, count);
}

// Função para busca por intervalo
long* bplus_range_search(BPlusTree *tree, int min_key, int max_key, int *count) {
    if (!tree || !tree->root || min_key > max_key) {
        *count = 0;
        return NULL;
    }

    *count = 0;
    long *results = NULL;
    int capacity = 0;

    // Encontra a primeira folha que pode conter min_key
    BPlusNode *current = tree->root;
    while (!current->is_leaf) {
        int i = 0;
        while (i < current->num_keys && min_key >= current->keys[i]) {
            i++;
        }
        current = current->children[i];
    }

    // Percorre as folhas coletando resultados no intervalo
    while (current) {
        for (int i = 0; i < current->num_keys; i++) {
            if (current->keys[i] >= min_key && current->keys[i] <= max_key) {
                // Expande o array se necessário
                if (*count >= capacity) {
                    capacity = capacity == 0 ? 10 : capacity * 2;
                    results = (long*)realloc(results, capacity * sizeof(long));
                }
                results[*count] = current->file_positions[i];
                (*count)++;
            } else if (current->keys[i] > max_key) {
                // Para de buscar se ultrapassou o intervalo
                return results;
            }
        }
        current = current->next;
    }

    return results;
}

// Função para inserir em uma folha
static int insert_in_leaf(BPlusNode *leaf, int key, long file_position) {
    int i = leaf->num_keys - 1;

    // Encontra a posição correta e desloca elementos
    while (i >= 0 && leaf->keys[i] > key) {
        leaf->keys[i + 1] = leaf->keys[i];
        leaf->file_positions[i + 1] = leaf->file_positions[i];
        i--;
    }

    // Insere a nova chave
    leaf->keys[i + 1] = key;
    leaf->file_positions[i + 1] = file_position;
    leaf->num_keys++;

    return 1;
}

// Função para dividir uma folha
static BPlusNode* split_leaf(BPlusNode *leaf, int key, long file_position) {
    BPlusNode *new_leaf = create_node(1);
    if (!new_leaf) return NULL;

    // Arrays temporários para ordenação
    int temp_keys[MAX_KEYS + 1];
    long temp_positions[MAX_KEYS + 1];

    // Copia chaves existentes
    for (int i = 0; i < MAX_KEYS; i++) {
        temp_keys[i] = leaf->keys[i];
        temp_positions[i] = leaf->file_positions[i];
    }

    // Insere nova chave na posição correta
    int pos = MAX_KEYS;
    while (pos > 0 && temp_keys[pos - 1] > key) {
        temp_keys[pos] = temp_keys[pos - 1];
        temp_positions[pos] = temp_positions[pos - 1];
        pos--;
    }
    temp_keys[pos] = key;
    temp_positions[pos] = file_position;

    // Divide as chaves
    int mid = (MAX_KEYS + 1) / 2;

    // Folha original fica com a primeira metade
    leaf->num_keys = mid;
    for (int i = 0; i < mid; i++) {
        leaf->keys[i] = temp_keys[i];
        leaf->file_positions[i] = temp_positions[i];
    }

    // Nova folha fica com a segunda metade
    new_leaf->num_keys = (MAX_KEYS + 1) - mid;
    for (int i = 0; i < new_leaf->num_keys; i++) {
        new_leaf->keys[i] = temp_keys[mid + i];
        new_leaf->file_positions[i] = temp_positions[mid + i];
    }

    // Atualiza ponteiros
    new_leaf->next = leaf->next;
    leaf->next = new_leaf;

    return new_leaf;
}

// Função para dividir nó interno
static int split_internal_node(BPlusNode *node, int key, BPlusNode *right_child, int *promoted_key, BPlusNode **new_node) {
    *new_node = create_node(0);
    if (!*new_node) return 0;

    // Arrays temporários
    int temp_keys[MAX_KEYS + 1];
    BPlusNode *temp_children[MAX_CHILDREN + 1];

    // Copia chaves e filhos existentes
    for (int i = 0; i < node->num_keys; i++) {
        temp_keys[i] = node->keys[i];
        temp_children[i] = node->children[i];
    }
    temp_children[node->num_keys] = node->children[node->num_keys];

    // Encontra posição para inserir nova chave
    int pos = node->num_keys;
    while (pos > 0 && temp_keys[pos - 1] > key) {
        temp_keys[pos] = temp_keys[pos - 1];
        temp_children[pos + 1] = temp_children[pos];
        pos--;
    }
    temp_keys[pos] = key;
    temp_children[pos + 1] = right_child;

    // Divide o nó
    int mid = MAX_KEYS / 2;
    *promoted_key = temp_keys[mid];

    // Nó original fica com a primeira metade
    node->num_keys = mid;
    for (int i = 0; i < mid; i++) {
        node->keys[i] = temp_keys[i];
        node->children[i] = temp_children[i];
    }
    node->children[mid] = temp_children[mid];

    // Novo nó fica com a segunda metade
    (*new_node)->num_keys = MAX_KEYS - mid;
    for (int i = 0; i < (*new_node)->num_keys; i++) {
        (*new_node)->keys[i] = temp_keys[mid + 1 + i];
        (*new_node)->children[i] = temp_children[mid + 1 + i];
    }
    (*new_node)->children[(*new_node)->num_keys] = temp_children[MAX_KEYS + 1];

    return 1;
}

// Função recursiva para inserção
static int insert_recursive(BPlusNode *node, int key, long file_position, int *promoted_key, BPlusNode **new_child) {
    *new_child = NULL;

    if (node->is_leaf) {
        // Inserção em folha
        if (node->num_keys < MAX_KEYS) {
            return insert_in_leaf(node, key, file_position);
        } else {
            // Divide a folha
            *new_child = split_leaf(node, key, file_position);
            if (!*new_child) return 0;
            *promoted_key = (*new_child)->keys[0];
            return 1;
        }
    } else {
        // Nó interno - encontra filho apropriado
        int i = 0;
        while (i < node->num_keys && key >= node->keys[i]) {
            i++;
        }

        int child_promoted_key;
        BPlusNode *child_new_node;

        // Recursão no filho
        if (!insert_recursive(node->children[i], key, file_position, &child_promoted_key, &child_new_node)) {
            return 0;
        }

        // Se não houve divisão no filho, inserção completa
        if (!child_new_node) {
            return 1;
        }

        // Houve divisão no filho - precisa inserir chave promovida
        if (node->num_keys < MAX_KEYS) {
            // Tem espaço no nó atual
            int pos = node->num_keys;
            while (pos > 0 && node->keys[pos - 1] > child_promoted_key) {
                node->keys[pos] = node->keys[pos - 1];
                node->children[pos + 1] = node->children[pos];
                pos--;
            }
            node->keys[pos] = child_promoted_key;
            node->children[pos + 1] = child_new_node;
            node->num_keys++;
            return 1;
        } else {
            // Não tem espaço - divide este nó também
            return split_internal_node(node, child_promoted_key, child_new_node, promoted_key, new_child);
        }
    }
}

// Função de inserção principal
int bplus_insert(BPlusTree *tree, int key, long file_position) {
    if (!tree || !tree->root) return 0;

    int promoted_key;
    BPlusNode *new_child;

    if (!insert_recursive(tree->root, key, file_position, &promoted_key, &new_child)) {
        return 0;
    }

    // Se houve divisão na raiz, cria nova raiz
    if (new_child) {
        BPlusNode *new_root = create_node(0);
        if (!new_root) return 0;

        new_root->keys[0] = promoted_key;
        new_root->children[0] = tree->root;
        new_root->children[1] = new_child;
        new_root->num_keys = 1;

        tree->root = new_root;
        tree->total_nodes++;
    }

    return 1;
}

// Função para salvar a árvore em arquivo
int bplus_save_to_file(BPlusTree *tree) {
    if (!tree) return 0;

    FILE *file = fopen(tree->index_filename, "wb");
    if (!file) return 0;

    // Salva metadados
    fwrite(&tree->total_nodes, sizeof(int), 1, file);

    // Para simplificar, salva apenas as folhas sequencialmente
    BPlusNode *current = tree->root;
    while (current && !current->is_leaf) {
        current = current->children[0];
    }

    while (current) {
        fwrite(&current->num_keys, sizeof(int), 1, file);
        fwrite(current->keys, sizeof(int), current->num_keys, file);
        fwrite(current->file_positions, sizeof(long), current->num_keys, file);
        current = current->next;
    }

    fclose(file);
    return 1;
}

// Função recursiva para imprimir árvore
static void print_node(BPlusNode *node, int level) {
    if (!node) return;

    // Indentação baseada no nível
    for (int i = 0; i < level; i++) printf("  ");

    if (node->is_leaf) {
        printf("FOLHA: ");
        for (int i = 0; i < node->num_keys; i++) {
            printf("[%d:%ld] ", node->keys[i], node->file_positions[i]);
        }
        printf("\n");
    } else {
        printf("INTERNO: ");
        for (int i = 0; i < node->num_keys; i++) {
            printf("[%d] ", node->keys[i]);
        }
        printf("\n");

        // Imprime filhos
        for (int i = 0; i <= node->num_keys; i++) {
            print_node(node->children[i], level + 1);
        }
    }
}

// Função para imprimir a árvore (debug)
void bplus_print_tree(BPlusTree *tree) {
    if (!tree || !tree->root) {
        printf("Árvore vazia\n");
        return;
    }

    printf("=== ÁRVORE B+ ===\n");
    printf("Total de nós: %d\n", tree->total_nodes);
    printf("Estrutura:\n");
    print_node(tree->root, 0);

    // Também mostra sequência de folhas
    printf("\nSequência de folhas:\n");
    BPlusNode *current = tree->root;
    while (current && !current->is_leaf) {
        current = current->children[0];
    }

    int leaf_count = 0;
    while (current) {
        printf("Folha %d: ", leaf_count++);
        for (int i = 0; i < current->num_keys; i++) {
            printf("[%d] ", current->keys[i]);
        }
        printf("\n");
        current = current->next;
    }
}
