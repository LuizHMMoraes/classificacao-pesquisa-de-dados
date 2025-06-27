// =============================================================================
// star_schema_indexes.h - Sistema de índices otimizado para esquema estrela
// =============================================================================
#ifndef STAR_SCHEMA_INDEXES_H
#define STAR_SCHEMA_INDEXES_H

#include "disaster_star_schema.h"

// =============================================================================
// ESTRUTURAS DE ÍNDICES HASH
// =============================================================================

#define HASH_TABLE_SIZE 1009  // Número primo para melhor distribuição

// Nó da tabela hash para índices
typedef struct HashNode {
    char key[100];
    int value;
    struct HashNode *next;
} HashNode;

// Tabela hash
typedef struct {
    HashNode *buckets[HASH_TABLE_SIZE];
    int size;
} HashTable;

// =============================================================================
// ESTRUTURAS DE ÍNDICES B-TREE SIMPLIFICADA
// =============================================================================

#define BTREE_ORDER 5  // Ordem da árvore B

// Nó da árvore B
typedef struct BTreeNode {
    int keys[BTREE_ORDER - 1];
    int values[BTREE_ORDER - 1];
    struct BTreeNode *children[BTREE_ORDER];
    int num_keys;
    int is_leaf;
} BTreeNode;

// Árvore B
typedef struct {
    BTreeNode *root;
    int height;
    int size;
} BTree;

// =============================================================================
// ESTRUTURA PRINCIPAL DE ÍNDICES
// =============================================================================

typedef struct {
    // Índices hash para busca rápida por strings
    HashTable *country_index;
    HashTable *disaster_type_index;
    HashTable *event_name_index;

    // Índices B-tree para busca por intervalos numéricos
    BTree *year_index;
    BTree *deaths_index;
    BTree *damage_index;

    // Bitmap indexes para consultas complexas
    unsigned char *year_bitmap[50];  // Para anos 1970-2020
    unsigned char *country_bitmap[200];  // Para até 200 países

    // Índices compostos para consultas multidimensionais
    HashTable *year_country_index;
    HashTable *disaster_country_index;

    // Referência ao data warehouse
    DataWarehouse *dw;

} IndexSystem;

// =============================================================================
// FUNÇÕES DE HASH TABLE
// =============================================================================

// Criar tabela hash
HashTable* hash_table_create();

// Destruir tabela hash
void hash_table_destroy(HashTable *ht);

// Inserir na tabela hash
int hash_table_insert(HashTable *ht, const char *key, int value);

// Buscar na tabela hash
int hash_table_search(HashTable *ht, const char *key);

// Remover da tabela hash
int hash_table_remove(HashTable *ht, const char *key);

// Função hash
unsigned int hash_function(const char *key);

// =============================================================================
// FUNÇÕES DE B-TREE
// =============================================================================

// Criar árvore B
BTree* btree_create();

// Destruir árvore B
void btree_destroy(BTree *bt);

// Inserir na árvore B
int btree_insert(BTree *bt, int key, int value);

// Buscar na árvore B
int btree_search(BTree *bt, int key);

// Buscar por intervalo na árvore B
int* btree_range_search(BTree *bt, int min_key, int max_key, int *result_count);

// =============================================================================
// FUNÇÕES DO SISTEMA DE ÍNDICES
// =============================================================================

// Criar sistema de índices
IndexSystem* index_system_create(DataWarehouse *dw);

// Destruir sistema de índices
void index_system_destroy(IndexSystem *idx);

// Construir todos os índices
int index_system_build_all(IndexSystem *idx);

// Reconstruir índices após inserção
int index_system_rebuild(IndexSystem *idx);

// Inserir entrada nos índices (chamada após inserção no DW)
int index_system_insert_entry(IndexSystem *idx, int fact_id);

// =============================================================================
// CONSULTAS OTIMIZADAS COM ÍNDICES
// =============================================================================

// Busca rápida por país
int* index_search_by_country(IndexSystem *idx, const char *country, int *result_count);

// Busca rápida por tipo de desastre
int* index_search_by_disaster_type(IndexSystem *idx, const char *disaster_type, int *result_count);

// Busca rápida por ano
int* index_search_by_year(IndexSystem *idx, int year, int *result_count);

// Busca por intervalo de anos
int* index_search_by_year_range(IndexSystem *idx, int start_year, int end_year, int *result_count);

// Busca por intervalo de danos
int* index_search_by_damage_range(IndexSystem *idx, long long min_damage, long long max_damage, int *result_count);

// Consulta combinada (país + ano)
int* index_search_country_year(IndexSystem *idx, const char *country, int year, int *result_count);

// Consulta combinada (tipo de desastre + país)
int* index_search_disaster_country(IndexSystem *idx, const char *disaster_type, const char *country, int *result_count);

// =============================================================================
// BITMAP OPERATIONS
// =============================================================================

// Inicializar bitmaps
int index_init_bitmaps(IndexSystem *idx);

// Definir bit no bitmap
void bitmap_set_bit(unsigned char *bitmap, int position);

// Limpar bit no bitmap
void bitmap_clear_bit(unsigned char *bitmap, int position);

// Verificar bit no bitmap
int bitmap_get_bit(unsigned char *bitmap, int position);

// Operação AND entre bitmaps
unsigned char* bitmap_and(unsigned char *bitmap1, unsigned char *bitmap2, int size);

// Operação OR entre bitmaps
unsigned char* bitmap_or(unsigned char *bitmap1, unsigned char *bitmap2, int size);

// Contar bits definidos no bitmap
int bitmap_count_bits(unsigned char *bitmap, int size);

// =============================================================================
// AGREGAÇÕES OTIMIZADAS
// =============================================================================

// Estrutura para resultados de agregação
typedef struct {
    int count;
    long long total_deaths;
    long long total_affected;
    long long total_damage;
    double avg_deaths;
    double avg_affected;
    double avg_damage;
} AggregationResult;

// Agregação por país usando índices
AggregationResult* index_aggregate_by_country(IndexSystem *idx, const char *country);

// Agregação por ano usando índices
AggregationResult* index_aggregate_by_year(IndexSystem *idx, int year);

// Agregação por tipo de desastre usando índices
AggregationResult* index_aggregate_by_disaster_type(IndexSystem *idx, const char *disaster_type);

// Agregação combinada usando múltiplos índices
AggregationResult* index_aggregate_multi_dimension(IndexSystem *idx, const char *country,
                                                 int year, const char *disaster_type);

// =============================================================================
// FUNÇÕES DE ANÁLISE E OTIMIZAÇÃO
// =============================================================================

// Analisar performance dos índices
void index_analyze_performance(IndexSystem *idx);

// Estatísticas dos índices
void index_print_statistics(IndexSystem *idx);

// Verificar integridade dos índices
int index_verify_integrity(IndexSystem *idx);

// Otimizar índices (reorganizar para melhor performance)
int index_optimize(IndexSystem *idx);

// =============================================================================
// CACHE SYSTEM
// =============================================================================

#define CACHE_SIZE 1000

// Entrada do cache
typedef struct CacheEntry {
    char query_key[200];
    int *results;
    int result_count;
    time_t timestamp;
    struct CacheEntry *next;
} CacheEntry;

// Sistema de cache
typedef struct {
    CacheEntry *entries[CACHE_SIZE];
    int hit_count;
    int miss_count;
    int max_age;  // Idade máxima em segundos
} CacheSystem;

// Criar sistema de cache
CacheSystem* cache_system_create(int max_age);

// Destruir sistema de cache
void cache_system_destroy(CacheSystem *cache);

// Buscar no cache
int* cache_search(CacheSystem *cache, const char *query_key, int *result_count);

// Inserir no cache
int cache_insert(CacheSystem *cache, const char *query_key, int *results, int result_count);

// Limpar cache expirado
void cache_cleanup_expired(CacheSystem *cache);

// Estatísticas do cache
void cache_print_statistics(CacheSystem *cache);

// =============================================================================
// SISTEMA COMPLETO COM CACHE E ÍNDICES
// =============================================================================

typedef struct {
    IndexSystem *indexes;
    CacheSystem *cache;
    DataWarehouse *dw;
} OptimizedDataWarehouse;

// Criar data warehouse otimizado
OptimizedDataWarehouse* optimized_dw_create();

// Destruir data warehouse otimizado
void optimized_dw_destroy(OptimizedDataWarehouse *odw);

// Consulta otimizada com cache e índices
int* optimized_query_by_country(OptimizedDataWarehouse *odw, const char *country, int *result_count);

// Consulta otimizada por múltiplas dimensões
AggregationResult* optimized_aggregate_query(OptimizedDataWarehouse *odw,
                                           const char *country, int year, const char *disaster_type);

#endif
