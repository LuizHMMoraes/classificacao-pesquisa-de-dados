// =============================================================================
// star_schema_indexes.h - Header atualizado com ordenação e filtros de data
// =============================================================================
#ifndef STAR_SCHEMA_INDEXES_IMPROVED_H
#define STAR_SCHEMA_INDEXES_IMPROVED_H

#include "disaster_star_schema.h"
#include "bplus.h"
#include "trie.h"
#include <time.h>
#include <stdbool.h>

// =============================================================================
// ESTRUTURA PRINCIPAL DE ÍNDICES
// =============================================================================

typedef struct {
    // === ÍNDICES TRIE PARA STRINGS ===
    Trie *country_trie;           // Busca por país com suporte a prefixo
    Trie *disaster_type_trie;     // Busca por tipo de desastre
    Trie *region_trie;            // Busca por região
    Trie *subregion_trie;         // Busca por sub-região

    // === ÍNDICES B+ TREE PARA NÚMEROS ===
    BPlusTree *year_bplus;        // Índice por ano (consultas por intervalo)
    BPlusTree *deaths_bplus;      // Índice por número de mortes
    BPlusTree *affected_bplus;    // Índice por número de afetados
    BPlusTree *damage_bplus;      // Índice por valor de danos
    BPlusTree *month_bplus;       // Índice por mês
    BPlusTree *day_bplus;         // Índice por dia

    // === ÍNDICES COMPOSTOS ===
    Trie *year_country_trie;      // "2020_Brazil" -> fact_ids
    Trie *disaster_country_trie;  // "Earthquake_Japan" -> fact_ids
    Trie *year_disaster_trie;     // "2020_Flood" -> fact_ids

    // === BITMAP INDEXES ===
    unsigned char *year_bitmap[200];      // Para anos 1970-2020
    unsigned char *country_bitmap[250];  // Para até 200 países
    unsigned char *disaster_bitmap[100]; // Para tipos de desastre

    // === CONFIGURAÇÕES ===
    char index_base_path[256];
    bool indexes_loaded;
    time_t last_rebuild_time;

    // Referência ao data warehouse
    DataWarehouse *dw;

} IndexSystem;

// =============================================================================
// CONFIGURAÇÃO DE ÍNDICES
// =============================================================================

typedef struct {
    bool enable_trie_indexes;
    bool enable_bplus_indexes;
    bool enable_bitmap_indexes;
    bool enable_composite_indexes;
    bool auto_rebuild;
    int cache_size;
    int max_cache_age;
    char index_directory[256];
} IndexConfiguration;

// =============================================================================
// FUNÇÕES PRINCIPAIS
// =============================================================================

// Criar sistema de índices
IndexSystem* index_system_create(DataWarehouse *dw);
IndexSystem* index_system_create_with_config(DataWarehouse *dw, IndexConfiguration *config);

// Destruir sistema de índices
void index_system_destroy(IndexSystem *idx);

// Construir todos os índices
int index_system_build_all(IndexSystem *idx);

// Reconstruir após inserção
int index_system_rebuild(IndexSystem *idx);

// Inserir entrada nos índices
int index_system_insert_entry(IndexSystem *idx, int fact_id);

// Persistência
int index_system_save_all(IndexSystem *idx);
int index_system_load_all(IndexSystem *idx);

// =============================================================================
// CONSULTAS COM TRIE
// =============================================================================

// Busca por país com suporte a prefixo
int* index_search_by_country(IndexSystem *idx, const char *country, int *result_count);

// Autocompletar país
char** index_search_country_prefix(IndexSystem *idx, const char *prefix, int *result_count);

// Busca por tipo de desastre
int* index_search_by_disaster_type(IndexSystem *idx, const char *disaster_type, int *result_count);

// Autocompletar tipo de desastre
char** index_search_disaster_type_prefix(IndexSystem *idx, const char *prefix, int *result_count);

// =============================================================================
// CONSULTAS COM B+ TREE (MELHORADAS)
// =============================================================================

// Busca por ano
int* index_search_by_year(IndexSystem *idx, int year, int *result_count);

// Busca por intervalo de anos (OTIMIZADA)
int* index_search_by_year_range(IndexSystem *idx, int start_year, int end_year, int *result_count);
int* index_search_by_year_range_optimized(IndexSystem *idx, int start_year, int end_year, int *result_count);

// Busca por intervalo de danos
int* index_search_by_damage_range(IndexSystem *idx, long long min_damage, long long max_damage, int *result_count);

// Busca por intervalo de afetados
int* index_search_by_affected_range(IndexSystem *idx, long long min_affected, long long max_affected, int *result_count);

// Busca por intervalo de mortes
int* index_search_by_deaths_range(IndexSystem *idx, int min_deaths, int max_deaths, int *result_count);

// =============================================================================
// CONSULTAS COMPOSTAS (MELHORADAS)
// =============================================================================

// Consulta combinada (país + ano)
int* index_search_country_year(IndexSystem *idx, const char *country, int year, int *result_count);

// Consulta combinada (país + intervalo de anos) - NOVA
int* index_search_country_year_range(IndexSystem *idx, const char *country, int start_year, int end_year, int *result_count);

// Consulta combinada (tipo de desastre + país)
int* index_search_disaster_country(IndexSystem *idx, const char *disaster_type, const char *country, int *result_count);

// Consulta tripla
int* index_search_country_year_disaster(IndexSystem *idx, const char *country, int year, const char *disaster_type, int *result_count);

// =============================================================================
// FUNÇÕES DE ORDENAÇÃO (NOVAS)
// =============================================================================

// Ordenação de fatos por total de afetados
int* index_sort_facts_by_affected(IndexSystem *idx, int *fact_ids, int fact_count, bool descending, int *result_count);

// Ordenação de fatos por danos
int* index_sort_facts_by_damage(IndexSystem *idx, int *fact_ids, int fact_count, bool descending, int *result_count);

// Ordenação de fatos por mortes
int* index_sort_facts_by_deaths(IndexSystem *idx, int *fact_ids, int fact_count, bool descending, int *result_count);

// Ordenação de países por critério
int* index_get_sorted_countries_by_affected(IndexSystem *idx, int *country_ids, int country_count,
                                           bool descending, int *result_count);

// =============================================================================
// AGREGAÇÕES (MELHORADAS)
// =============================================================================

typedef struct {
    int count;
    long long total_deaths;
    long long total_affected;
    long long total_damage;
    double avg_deaths;
    double avg_affected;
    double avg_damage;
    long long max_deaths;
    long long max_affected;
    long long max_damage;
    long long min_deaths;
    long long min_affected;
    long long min_damage;
} AggregationResult;

// Agregação por país
AggregationResult* index_aggregate_by_country(IndexSystem *idx, const char *country);

// Agregação por ano
AggregationResult* index_aggregate_by_year(IndexSystem *idx, int year);

// Agregação por intervalo de anos (NOVA)
AggregationResult* index_aggregate_by_year_range(IndexSystem *idx, int start_year, int end_year);

// Agregação por tipo de desastre
AggregationResult* index_aggregate_by_disaster_type(IndexSystem *idx, const char *disaster_type);

// Agregação multidimensional
AggregationResult* index_aggregate_multi_dimension(IndexSystem *idx, const char *country,
                                                 int year, const char *disaster_type);

// =============================================================================
// BITMAP OPERATIONS
// =============================================================================

int index_init_bitmaps(IndexSystem *idx);
void bitmap_set_bit(unsigned char *bitmap, int position);
void bitmap_clear_bit(unsigned char *bitmap, int position);
int bitmap_get_bit(unsigned char *bitmap, int position);
unsigned char* bitmap_and(unsigned char *bitmap1, unsigned char *bitmap2, int size);
unsigned char* bitmap_or(unsigned char *bitmap1, unsigned char *bitmap2, int size);
int bitmap_count_bits(unsigned char *bitmap, int size);

// =============================================================================
// CACHE SYSTEM
// =============================================================================

#define CACHE_SIZE 1000
#define MAX_QUERY_KEY_SIZE 300

typedef struct CacheEntry {
    char query_key[MAX_QUERY_KEY_SIZE];
    int *results;
    int result_count;
    time_t timestamp;
    int access_count;
    struct CacheEntry *next;
    struct CacheEntry *lru_prev;
    struct CacheEntry *lru_next;
} CacheEntry;

typedef struct {
    CacheEntry *entries[CACHE_SIZE];
    CacheEntry *lru_head;
    CacheEntry *lru_tail;
    int hit_count;
    int miss_count;
    int current_size;
    int max_size;
    int max_age;
} CacheSystem;

// Funções do cache
CacheSystem* cache_system_create(int max_age);
void cache_system_destroy(CacheSystem *cache);
int* cache_search(CacheSystem *cache, const char *query_key, int *result_count);
int cache_insert(CacheSystem *cache, const char *query_key, int *results, int result_count);
void cache_cleanup_expired(CacheSystem *cache);
void cache_print_statistics(CacheSystem *cache);

// =============================================================================
// SISTEMA COMPLETO
// =============================================================================

typedef struct {
    IndexSystem *indexes;
    CacheSystem *cache;
    DataWarehouse *dw;
    IndexConfiguration *config;
    char version[32];
} OptimizedDataWarehouse;

// Criar data warehouse otimizado
OptimizedDataWarehouse* optimized_dw_create();
OptimizedDataWarehouse* optimized_dw_create_with_config(IndexConfiguration *config);

// Destruir data warehouse otimizado
void optimized_dw_destroy(OptimizedDataWarehouse *odw);

// Operações principais
OptimizedDataWarehouse* optimized_dw_load(const char *base_path);
int optimized_dw_save(OptimizedDataWarehouse *odw, const char *base_path);

// =============================================================================
// CONSULTAS OTIMIZADAS (MELHORADAS)
// =============================================================================

// Consultas básicas otimizadas
int* optimized_query_by_country(OptimizedDataWarehouse *odw, const char *country, int *result_count);
char** optimized_autocomplete_country(OptimizedDataWarehouse *odw, const char *prefix, int *result_count);

// Consultas por intervalo de anos (NOVAS)
int* optimized_query_by_country_and_year_range(OptimizedDataWarehouse *odw, const char *country,
                                               int start_year, int end_year, int *result_count);

// Agregação por intervalo de anos
AggregationResult* optimized_aggregate_by_year_range(OptimizedDataWarehouse *odw, int start_year, int end_year);

// Consulta completa com todos os filtros e ordenação (NOVA)
int* optimized_query_with_all_filters(OptimizedDataWarehouse *odw, const char *country,
                                     const char *disaster_type, int start_year, int end_year,
                                     int sort_type, bool descending, int *result_count);

// Agregação geral otimizada
AggregationResult* optimized_aggregate_query(OptimizedDataWarehouse *odw,
                                           const char *country, int year, const char *disaster_type);

// =============================================================================
// UTILITÁRIOS
// =============================================================================

// Configurações
IndexConfiguration* index_config_create_default();
IndexConfiguration* index_config_create_high_performance();
IndexConfiguration* index_config_create_low_memory();
void index_config_destroy(IndexConfiguration *config);

// Análise e estatísticas
void index_analyze_performance(IndexSystem *idx);
void index_print_statistics(IndexSystem *idx);
void index_print_performance_statistics(IndexSystem *idx);
int index_verify_integrity(IndexSystem *idx);
void optimized_dw_print_statistics(OptimizedDataWarehouse *odw);

// Benchmark e testes (NOVOS)
void index_benchmark_queries(IndexSystem *idx, int num_tests);

// =============================================================================
// CONSTANTES E ENUMS PARA ORDENAÇÃO
// =============================================================================

// Tipos de ordenação suportados
typedef enum {
    INDEX_SORT_BY_AFFECTED = 0,
    INDEX_SORT_BY_DAMAGE = 1,
    INDEX_SORT_BY_DEATHS = 2,
    INDEX_SORT_BY_COUNT = 3,
    INDEX_SORT_BY_YEAR = 4,
    INDEX_SORT_BY_COUNTRY = 5
} IndexSortType;

// Ordem de classificação
typedef enum {
    INDEX_SORT_ASC = 0,
    INDEX_SORT_DESC = 1
} IndexSortOrder;

// =============================================================================
// MACROS PARA PERFORMANCE
// =============================================================================

// Macro para timing de consultas
#define INDEX_TIME_QUERY(query_name, query_code) do { \
    clock_t _start = clock(); \
    query_code; \
    clock_t _end = clock(); \
    double _time = ((double)(_end - _start)) / CLOCKS_PER_SEC; \
    printf("Query %s took %.6f seconds\n", query_name, _time); \
} while(0)

// Macro para verificação de resultados
#define INDEX_VALIDATE_RESULT(result, count, operation) do { \
    if (!(result) || (count) <= 0) { \
        printf("Warning: %s returned no results\n", operation); \
    } else { \
        printf("Success: %s returned %d results\n", operation, count); \
    } \
} while(0)

// =============================================================================
// ESTRUTURAS AUXILIARES PARA ORDENAÇÃO
// =============================================================================

// Estrutura para manter dados de ordenação de países
typedef struct {
    char country[50];
    long long total_affected;
    long long total_damage;
    int total_deaths;
    int disaster_count;
    int original_index;
} CountrySortData;

// Estrutura para manter dados de ordenação de fatos
typedef struct {
    int fact_id;
    long long sort_value;
    int year;
    char country[50];
    char disaster_type[50];
} FactSortData;

// =============================================================================
// FUNÇÕES DE COMPARAÇÃO PARA QSORT
// =============================================================================

// Comparadores para países
int compare_country_by_affected_desc(const void *a, const void *b);
int compare_country_by_damage_desc(const void *a, const void *b);
int compare_country_by_deaths_desc(const void *a, const void *b);
int compare_country_by_count_desc(const void *a, const void *b);
int compare_country_by_name_asc(const void *a, const void *b);

// Comparadores para fatos
int compare_fact_by_affected_desc(const void *a, const void *b);
int compare_fact_by_damage_desc(const void *a, const void *b);
int compare_fact_by_deaths_desc(const void *a, const void *b);
int compare_fact_by_year_desc(const void *a, const void *b);

// =============================================================================
// FUNÇÕES DE VALIDAÇÃO E DEBUG
// =============================================================================

// Validação de integridade dos índices
int index_validate_bplus_trees(IndexSystem *idx);
int index_validate_tries(IndexSystem *idx);
int index_validate_bitmaps(IndexSystem *idx);

// Debug e logging
void index_debug_print_query_plan(const char *query_description);
void index_debug_print_result_summary(int *results, int count, const char *operation);

// Estatísticas detalhadas
typedef struct {
    int total_queries;
    int cache_hits;
    int cache_misses;
    double avg_query_time;
    double total_query_time;
    int bplus_queries;
    int trie_queries;
    int bitmap_queries;
    int linear_queries;
} IndexPerformanceStats;

IndexPerformanceStats* index_get_performance_stats(IndexSystem *idx);
void index_reset_performance_stats(IndexSystem *idx);
void index_print_detailed_performance_stats(IndexPerformanceStats *stats);

// =============================================================================
// FUNÇÕES DE OTIMIZAÇÃO AUTOMÁTICA
// =============================================================================

// Análise automática e sugestões de otimização
typedef struct {
    bool recommend_rebuild_bplus;
    bool recommend_rebuild_tries;
    bool recommend_increase_cache;
    bool recommend_bitmap_optimization;
    char suggestions[1024];
} OptimizationSuggestions;

OptimizationSuggestions* index_analyze_and_suggest_optimizations(IndexSystem *idx);
int index_apply_automatic_optimizations(IndexSystem *idx, OptimizationSuggestions *suggestions);

// =============================================================================
// COMPATIBILIDADE E VERSIONAMENTO
// =============================================================================

#define INDEX_SYSTEM_VERSION_MAJOR 2
#define INDEX_SYSTEM_VERSION_MINOR 0
#define INDEX_SYSTEM_VERSION_PATCH 0

typedef struct {
    int major;
    int minor;
    int patch;
    char build_date[32];
    char features[256];
} IndexSystemVersion;

IndexSystemVersion* index_get_version_info();
int index_check_compatibility(IndexSystemVersion *version);

#endif // STAR_SCHEMA_INDEXES_IMPROVED_H
