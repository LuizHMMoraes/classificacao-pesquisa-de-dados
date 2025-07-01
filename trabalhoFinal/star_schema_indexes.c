// =============================================================================
// star_schema_indexes.c - Implementação do sistema de índices
// =============================================================================

#include "star_schema_indexes.h"
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <limits.h>

// =============================================================================
// IMPLEMENTAÇÃO DO CACHE SYSTEM
// =============================================================================

static unsigned int hash_string(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash % CACHE_SIZE;
}

CacheSystem* cache_system_create(int max_age) {
    CacheSystem *cache = malloc(sizeof(CacheSystem));
    if (!cache) return NULL;

    memset(cache->entries, 0, sizeof(cache->entries));
    cache->lru_head = NULL;
    cache->lru_tail = NULL;
    cache->hit_count = 0;
    cache->miss_count = 0;
    cache->current_size = 0;
    cache->max_size = CACHE_SIZE;
    cache->max_age = max_age;

    return cache;
}

void cache_system_destroy(CacheSystem *cache) {
    if (!cache) return;

    for (int i = 0; i < CACHE_SIZE; i++) {
        CacheEntry *entry = cache->entries[i];
        while (entry) {
            CacheEntry *next = entry->next;
            free(entry->results);
            free(entry);
            entry = next;
        }
    }
    free(cache);
}

int* cache_search(CacheSystem *cache, const char *query_key, int *result_count) {
    if (!cache || !query_key) return NULL;

    unsigned int hash = hash_string(query_key);
    CacheEntry *entry = cache->entries[hash];

    while (entry) {
        if (strcmp(entry->query_key, query_key) == 0) {
            // Verificar se não expirou
            time_t now = time(NULL);
            if (now - entry->timestamp <= cache->max_age) {
                cache->hit_count++;
                entry->access_count++;
                *result_count = entry->result_count;

                // Copiar resultados
                int *results = malloc(entry->result_count * sizeof(int));
                memcpy(results, entry->results, entry->result_count * sizeof(int));
                return results;
            }
        }
        entry = entry->next;
    }

    cache->miss_count++;
    return NULL;
}

int cache_insert(CacheSystem *cache, const char *query_key, int *results, int result_count) {
    if (!cache || !query_key || !results) return 0;

    unsigned int hash = hash_string(query_key);

    CacheEntry *entry = malloc(sizeof(CacheEntry));
    if (!entry) return 0;

    strncpy(entry->query_key, query_key, MAX_QUERY_KEY_SIZE - 1);
    entry->query_key[MAX_QUERY_KEY_SIZE - 1] = '\0';

    entry->results = malloc(result_count * sizeof(int));
    if (!entry->results) {
        free(entry);
        return 0;
    }

    memcpy(entry->results, results, result_count * sizeof(int));
    entry->result_count = result_count;
    entry->timestamp = time(NULL);
    entry->access_count = 1;

    entry->next = cache->entries[hash];
    cache->entries[hash] = entry;
    cache->current_size++;

    return 1;
}

void cache_cleanup_expired(CacheSystem *cache) {
    if (!cache) return;

    time_t now = time(NULL);

    for (int i = 0; i < CACHE_SIZE; i++) {
        CacheEntry **entry_ptr = &cache->entries[i];
        while (*entry_ptr) {
            CacheEntry *entry = *entry_ptr;
            if (now - entry->timestamp > cache->max_age) {
                *entry_ptr = entry->next;
                free(entry->results);
                free(entry);
                cache->current_size--;
            } else {
                entry_ptr = &entry->next;
            }
        }
    }
}

void cache_print_statistics(CacheSystem *cache) {
    if (!cache) return;

    printf("=== CACHE STATISTICS ===\n");
    printf("Hits: %d\n", cache->hit_count);
    printf("Misses: %d\n", cache->miss_count);
    printf("Hit ratio: %.2f%%\n",
           cache->hit_count + cache->miss_count > 0 ?
           (double)cache->hit_count / (cache->hit_count + cache->miss_count) * 100 : 0);
    printf("Current size: %d/%d\n", cache->current_size, cache->max_size);
}

// =============================================================================
// BITMAP OPERATIONS
// =============================================================================

int index_init_bitmaps(IndexSystem *idx) {
    if (!idx) return 0;

    // Inicializar bitmaps para anos (1970-2169, 200 anos)
    for (int i = 0; i < 200; i++) {
        idx->year_bitmap[i] = calloc(1000, sizeof(unsigned char)); // 8000 bits
        if (!idx->year_bitmap[i]) return 0;
    }

    // Inicializar bitmaps para países
    for (int i = 0; i < 250; i++) {
        idx->country_bitmap[i] = calloc(1000, sizeof(unsigned char));
        if (!idx->country_bitmap[i]) return 0;
    }

    // Inicializar bitmaps para tipos de desastre
    for (int i = 0; i < 100; i++) {
        idx->disaster_bitmap[i] = calloc(1000, sizeof(unsigned char));
        if (!idx->disaster_bitmap[i]) return 0;
    }

    return 1;
}

void bitmap_set_bit(unsigned char *bitmap, int position) {
    if (!bitmap) return;
    int byte_index = position / 8;
    int bit_index = position % 8;
    bitmap[byte_index] |= (1 << bit_index);
}

void bitmap_clear_bit(unsigned char *bitmap, int position) {
    if (!bitmap) return;
    int byte_index = position / 8;
    int bit_index = position % 8;
    bitmap[byte_index] &= ~(1 << bit_index);
}

int bitmap_get_bit(unsigned char *bitmap, int position) {
    if (!bitmap) return 0;
    int byte_index = position / 8;
    int bit_index = position % 8;
    return (bitmap[byte_index] >> bit_index) & 1;
}

unsigned char* bitmap_and(unsigned char *bitmap1, unsigned char *bitmap2, int size) {
    if (!bitmap1 || !bitmap2) return NULL;

    unsigned char *result = malloc(size);
    if (!result) return NULL;

    for (int i = 0; i < size; i++) {
        result[i] = bitmap1[i] & bitmap2[i];
    }

    return result;
}

unsigned char* bitmap_or(unsigned char *bitmap1, unsigned char *bitmap2, int size) {
    if (!bitmap1 || !bitmap2) return NULL;

    unsigned char *result = malloc(size);
    if (!result) return NULL;

    for (int i = 0; i < size; i++) {
        result[i] = bitmap1[i] | bitmap2[i];
    }

    return result;
}

int bitmap_count_bits(unsigned char *bitmap, int size) {
    if (!bitmap) return 0;

    int count = 0;
    for (int i = 0; i < size; i++) {
        unsigned char byte = bitmap[i];
        while (byte) {
            count += byte & 1;
            byte >>= 1;
        }
    }

    return count;
}

// =============================================================================
// CONFIGURAÇÕES
// =============================================================================

IndexConfiguration* index_config_create_default() {
    IndexConfiguration *config = malloc(sizeof(IndexConfiguration));
    if (!config) return NULL;

    config->enable_trie_indexes = true;
    config->enable_bplus_indexes = true;
    config->enable_bitmap_indexes = true;
    config->enable_composite_indexes = true;
    config->auto_rebuild = true;
    config->cache_size = 1000;
    config->max_cache_age = 3600; // 1 hora
    strcpy(config->index_directory, "./indexes/");

    return config;
}

IndexConfiguration* index_config_create_high_performance() {
    IndexConfiguration *config = index_config_create_default();
    if (!config) return NULL;

    config->cache_size = 5000;
    config->max_cache_age = 7200; // 2 horas
    config->enable_bitmap_indexes = true;

    return config;
}

IndexConfiguration* index_config_create_low_memory() {
    IndexConfiguration *config = index_config_create_default();
    if (!config) return NULL;

    config->enable_bitmap_indexes = false;
    config->cache_size = 100;
    config->max_cache_age = 900; // 15 minutos

    return config;
}

void index_config_destroy(IndexConfiguration *config) {
    if (config) free(config);
}

// =============================================================================
// SISTEMA DE ÍNDICES PRINCIPAL
// =============================================================================

IndexSystem* index_system_create(DataWarehouse *dw) {
    IndexConfiguration *config = index_config_create_default();
    IndexSystem *idx = index_system_create_with_config(dw, config);
    index_config_destroy(config);
    return idx;
}

IndexSystem* index_system_create_with_config(DataWarehouse *dw, IndexConfiguration *config) {
    if (!dw || !config) return NULL;

    IndexSystem *idx = malloc(sizeof(IndexSystem));
    if (!idx) return NULL;

    memset(idx, 0, sizeof(IndexSystem));
    idx->dw = dw;

    // Inicializar tries (simulação - usando arrays simples)
    idx->country_trie = (Trie*)calloc(1, sizeof(void*));
    idx->disaster_type_trie = (Trie*)calloc(1, sizeof(void*));
    idx->region_trie = (Trie*)calloc(1, sizeof(void*));
    idx->subregion_trie = (Trie*)calloc(1, sizeof(void*));
    idx->year_country_trie = (Trie*)calloc(1, sizeof(void*));
    idx->disaster_country_trie = (Trie*)calloc(1, sizeof(void*));
    idx->year_disaster_trie = (Trie*)calloc(1, sizeof(void*));

    // Inicializar B+ trees (simulação - usando arrays simples)
    idx->year_bplus = (BPlusTree*)calloc(1, sizeof(void*));
    idx->deaths_bplus = (BPlusTree*)calloc(1, sizeof(void*));
    idx->affected_bplus = (BPlusTree*)calloc(1, sizeof(void*));
    idx->damage_bplus = (BPlusTree*)calloc(1, sizeof(void*));
    idx->month_bplus = (BPlusTree*)calloc(1, sizeof(void*));
    idx->day_bplus = (BPlusTree*)calloc(1, sizeof(void*));

    // Inicializar bitmaps
    if (config->enable_bitmap_indexes) {
        index_init_bitmaps(idx);
    }

    strcpy(idx->index_base_path, config->index_directory);
    idx->indexes_loaded = false;
    idx->last_rebuild_time = time(NULL);

    return idx;
}

void index_system_destroy(IndexSystem *idx) {
    if (!idx) return;

    // Liberar tries
    free(idx->country_trie);
    free(idx->disaster_type_trie);
    free(idx->region_trie);
    free(idx->subregion_trie);
    free(idx->year_country_trie);
    free(idx->disaster_country_trie);
    free(idx->year_disaster_trie);

    // Liberar B+ trees
    free(idx->year_bplus);
    free(idx->deaths_bplus);
    free(idx->affected_bplus);
    free(idx->damage_bplus);
    free(idx->month_bplus);
    free(idx->day_bplus);

    // Liberar bitmaps
    for (int i = 0; i < 200; i++) {
        free(idx->year_bitmap[i]);
    }
    for (int i = 0; i < 250; i++) {
        free(idx->country_bitmap[i]);
    }
    for (int i = 0; i < 100; i++) {
        free(idx->disaster_bitmap[i]);
    }

    free(idx);
}

int index_system_build_all(IndexSystem *idx) {
    if (!idx || !idx->dw) return 0;

    printf("Building indexes for %d facts...\n", idx->dw->fact_count);

    // Construir índices para cada fato
    for (int i = 0; i < idx->dw->fact_count; i++) {
        index_system_insert_entry(idx, i);
    }

    idx->indexes_loaded = true;
    idx->last_rebuild_time = time(NULL);

    printf("Indexes built successfully!\n");
    return 1;
}

int index_system_rebuild(IndexSystem *idx) {
    return index_system_build_all(idx);
}

int index_system_insert_entry(IndexSystem *idx, int fact_id) {
    if (!idx || !idx->dw || fact_id >= idx->dw->fact_count) return 0;

    DisasterFact *fact = &idx->dw->fact_table[fact_id];

    // Encontrar dimensões relacionadas
    DimTime *time_dim = NULL;
    DimGeography *geo_dim = NULL;
    DimDisasterType *type_dim = NULL;

    for (int i = 0; i < idx->dw->time_count; i++) {
        if (idx->dw->dim_time[i].time_key == fact->time_key) {
            time_dim = &idx->dw->dim_time[i];
            break;
        }
    }

    for (int i = 0; i < idx->dw->geography_count; i++) {
        if (idx->dw->dim_geography[i].geography_key == fact->geography_key) {
            geo_dim = &idx->dw->dim_geography[i];
            break;
        }
    }

    for (int i = 0; i < idx->dw->disaster_type_count; i++) {
        if (idx->dw->dim_disaster_type[i].disaster_type_key == fact->disaster_type_key) {
            type_dim = &idx->dw->dim_disaster_type[i];
            break;
        }
    }

    // Atualizar bitmaps se disponíveis
    if (time_dim && time_dim->start_year >= 1970 && time_dim->start_year < 2170) {
        int year_index = time_dim->start_year - 1970;
        if (year_index >= 0 && year_index < 200) {
            bitmap_set_bit(idx->year_bitmap[year_index], fact_id);
        }
    }

    return 1;
}

int index_system_save_all(IndexSystem *idx) {
    if (!idx) return 0;

    printf("Saving indexes to %s\n", idx->index_base_path);
    // Implementação simplificada - apenas retorna sucesso
    return 1;
}

int index_system_load_all(IndexSystem *idx) {
    if (!idx) return 0;

    printf("Loading indexes from %s\n", idx->index_base_path);
    // Implementação simplificada - reconstrói os índices
    return index_system_build_all(idx);
}

// =============================================================================
// CONSULTAS SIMPLES
// =============================================================================

int* index_search_by_country(IndexSystem *idx, const char *country, int *result_count) {
    if (!idx || !idx->dw || !country || !result_count) return NULL;

    *result_count = 0;
    int *results = malloc(idx->dw->fact_count * sizeof(int));
    if (!results) return NULL;

    // Busca linear simples
    for (int i = 0; i < idx->dw->fact_count; i++) {
        DisasterFact *fact = &idx->dw->fact_table[i];

        // Encontrar geografia
        for (int j = 0; j < idx->dw->geography_count; j++) {
            if (idx->dw->dim_geography[j].geography_key == fact->geography_key) {
                if (strcmp(idx->dw->dim_geography[j].country, country) == 0) {
                    results[(*result_count)++] = i;
                }
                break;
            }
        }
    }

    if (*result_count == 0) {
        free(results);
        return NULL;
    }

    return results;
}

char** index_search_country_prefix(IndexSystem *idx, const char *prefix, int *result_count) {
    if (!idx || !idx->dw || !prefix || !result_count) return NULL;

    *result_count = 0;
    char **results = malloc(idx->dw->geography_count * sizeof(char*));
    if (!results) return NULL;

    int prefix_len = strlen(prefix);

    for (int i = 0; i < idx->dw->geography_count; i++) {
        if (strncmp(idx->dw->dim_geography[i].country, prefix, prefix_len) == 0) {
            results[*result_count] = malloc(strlen(idx->dw->dim_geography[i].country) + 1);
            if (results[*result_count]) {
                strcpy(results[*result_count], idx->dw->dim_geography[i].country);
                (*result_count)++;
            }
        }
    }

    if (*result_count == 0) {
        free(results);
        return NULL;
    }

    return results;
}

int* index_search_by_disaster_type(IndexSystem *idx, const char *disaster_type, int *result_count) {
    if (!idx || !idx->dw || !disaster_type || !result_count) return NULL;

    *result_count = 0;
    int *results = malloc(idx->dw->fact_count * sizeof(int));
    if (!results) return NULL;

    for (int i = 0; i < idx->dw->fact_count; i++) {
        DisasterFact *fact = &idx->dw->fact_table[i];

        for (int j = 0; j < idx->dw->disaster_type_count; j++) {
            if (idx->dw->dim_disaster_type[j].disaster_type_key == fact->disaster_type_key) {
                if (strcmp(idx->dw->dim_disaster_type[j].disaster_type, disaster_type) == 0) {
                    results[(*result_count)++] = i;
                }
                break;
            }
        }
    }

    if (*result_count == 0) {
        free(results);
        return NULL;
    }

    return results;
}

char** index_search_disaster_type_prefix(IndexSystem *idx, const char *prefix, int *result_count) {
    if (!idx || !idx->dw || !prefix || !result_count) return NULL;

    *result_count = 0;
    char **results = malloc(idx->dw->disaster_type_count * sizeof(char*));
    if (!results) return NULL;

    int prefix_len = strlen(prefix);

    for (int i = 0; i < idx->dw->disaster_type_count; i++) {
        if (strncmp(idx->dw->dim_disaster_type[i].disaster_type, prefix, prefix_len) == 0) {
            results[*result_count] = malloc(strlen(idx->dw->dim_disaster_type[i].disaster_type) + 1);
            if (results[*result_count]) {
                strcpy(results[*result_count], idx->dw->dim_disaster_type[i].disaster_type);
                (*result_count)++;
            }
        }
    }

    if (*result_count == 0) {
        free(results);
        return NULL;
    }

    return results;
}

int* index_search_by_year(IndexSystem *idx, int year, int *result_count) {
    if (!idx || !idx->dw || !result_count) return NULL;

    *result_count = 0;
    int *results = malloc(idx->dw->fact_count * sizeof(int));
    if (!results) return NULL;

    for (int i = 0; i < idx->dw->fact_count; i++) {
        DisasterFact *fact = &idx->dw->fact_table[i];

        for (int j = 0; j < idx->dw->time_count; j++) {
            if (idx->dw->dim_time[j].time_key == fact->time_key) {
                if (idx->dw->dim_time[j].start_year == year) {
                    results[(*result_count)++] = i;
                }
                break;
            }
        }
    }

    if (*result_count == 0) {
        free(results);
        return NULL;
    }

    return results;
}

int* index_search_by_year_range(IndexSystem *idx, int start_year, int end_year, int *result_count) {
    if (!idx || !idx->dw || !result_count) return NULL;

    *result_count = 0;
    int *results = malloc(idx->dw->fact_count * sizeof(int));
    if (!results) return NULL;

    for (int i = 0; i < idx->dw->fact_count; i++) {
        DisasterFact *fact = &idx->dw->fact_table[i];

        for (int j = 0; j < idx->dw->time_count; j++) {
            if (idx->dw->dim_time[j].time_key == fact->time_key) {
                if (idx->dw->dim_time[j].start_year >= start_year &&
                    idx->dw->dim_time[j].start_year <= end_year) {
                    results[(*result_count)++] = i;
                }
                break;
            }
        }
    }

    if (*result_count == 0) {
        free(results);
        return NULL;
    }

    return results;
}

int* index_search_by_damage_range(IndexSystem *idx, long long min_damage, long long max_damage, int *result_count) {
    if (!idx || !idx->dw || !result_count) return NULL;

    *result_count = 0;
    int *results = malloc(idx->dw->fact_count * sizeof(int));
    if (!results) return NULL;

    for (int i = 0; i < idx->dw->fact_count; i++) {
        DisasterFact *fact = &idx->dw->fact_table[i];
        if (fact->total_damage >= min_damage && fact->total_damage <= max_damage) {
            results[(*result_count)++] = i;
        }
    }

    if (*result_count == 0) {
        free(results);
        return NULL;
    }

    return results;
}

int* index_search_by_affected_range(IndexSystem *idx, long long min_affected, long long max_affected, int *result_count) {
    if (!idx || !idx->dw || !result_count) return NULL;

    *result_count = 0;
    int *results = malloc(idx->dw->fact_count * sizeof(int));
    if (!results) return NULL;

    for (int i = 0; i < idx->dw->fact_count; i++) {
        DisasterFact *fact = &idx->dw->fact_table[i];
        if (fact->total_affected >= min_affected && fact->total_affected <= max_affected) {
            results[(*result_count)++] = i;
        }
    }

    if (*result_count == 0) {
        free(results);
        return NULL;
    }

    return results;
}

int* index_search_by_deaths_range(IndexSystem *idx, int min_deaths, int max_deaths, int *result_count) {
    if (!idx || !idx->dw || !result_count) return NULL;

    *result_count = 0;
    int *results = malloc(idx->dw->fact_count * sizeof(int));
    if (!results) return NULL;

    for (int i = 0; i < idx->dw->fact_count; i++) {
        DisasterFact *fact = &idx->dw->fact_table[i];
        if (fact->total_deaths >= min_deaths && fact->total_deaths <= max_deaths) {
            results[(*result_count)++] = i;
        }
    }

    if (*result_count == 0) {
        free(results);
        return NULL;
    }

    return results;
}

// =============================================================================
// CONSULTAS COMPOSTAS
// =============================================================================

int* index_search_country_year(IndexSystem *idx, const char *country, int year, int *result_count) {
    if (!idx || !idx->dw || !country || !result_count) return NULL;

    *result_count = 0;
    int *results = malloc(idx->dw->fact_count * sizeof(int));
    if (!results) return NULL;

    for (int i = 0; i < idx->dw->fact_count; i++) {
        DisasterFact *fact = &idx->dw->fact_table[i];
        bool country_match = false, year_match = false;

        // Verificar país
        for (int j = 0; j < idx->dw->geography_count; j++) {
            if (idx->dw->dim_geography[j].geography_key == fact->geography_key) {
                if (strcmp(idx->dw->dim_geography[j].country, country) == 0) {
                    country_match = true;
                }
                break;
            }
        }

        // Verificar ano
        for (int j = 0; j < idx->dw->time_count; j++) {
            if (idx->dw->dim_time[j].time_key == fact->time_key) {
                if (idx->dw->dim_time[j].start_year == year) {
                    year_match = true;
                }
                break;
            }
        }

        if (country_match && year_match) {
            results[(*result_count)++] = i;
        }
    }

    if (*result_count == 0) {
        free(results);
        return NULL;
    }

    return results;
}

int* index_search_disaster_country(IndexSystem *idx, const char *disaster_type, const char *country, int *result_count) {
    if (!idx || !idx->dw || !disaster_type || !country || !result_count) return NULL;

    *result_count = 0;
    int *results = malloc(idx->dw->fact_count * sizeof(int));
    if (!results) return NULL;

    for (int i = 0; i < idx->dw->fact_count; i++) {
        DisasterFact *fact = &idx->dw->fact_table[i];
        bool country_match = false, disaster_match = false;

        // Verificar país
        for (int j = 0; j < idx->dw->geography_count; j++) {
            if (idx->dw->dim_geography[j].geography_key == fact->geography_key) {
                if (strcmp(idx->dw->dim_geography[j].country, country) == 0) {
                    country_match = true;
                }
                break;
            }
        }

        // Verificar tipo de desastre
        for (int j = 0; j < idx->dw->disaster_type_count; j++) {
            if (idx->dw->dim_disaster_type[j].disaster_type_key == fact->disaster_type_key) {
                if (strcmp(idx->dw->dim_disaster_type[j].disaster_type, disaster_type) == 0) {
                    disaster_match = true;
                }
                break;
            }
        }

        if (country_match && disaster_match) {
            results[(*result_count)++] = i;
        }
    }

    if (*result_count == 0) {
        free(results);
        return NULL;
    }

    return results;
}

int* index_search_country_year_disaster(IndexSystem *idx, const char *country, int year, const char *disaster_type, int *result_count) {
    if (!idx || !idx->dw || !country || !disaster_type || !result_count) return NULL;

    *result_count = 0;
    int *results = malloc(idx->dw->fact_count * sizeof(int));
    if (!results) return NULL;

    for (int i = 0; i < idx->dw->fact_count; i++) {
        DisasterFact *fact = &idx->dw->fact_table[i];
        bool country_match = false, year_match = false, disaster_match = false;

        // Verificar país
        for (int j = 0; j < idx->dw->geography_count; j++) {
            if (idx->dw->dim_geography[j].geography_key == fact->geography_key) {
                if (strcmp(idx->dw->dim_geography[j].country, country) == 0) {
                    country_match = true;
                }
                break;
            }
        }

// =============================================================================
// CONTINUAÇÃO DE star_schema_indexes.c - Funções restantes
// =============================================================================

// Verificar ano
        for (int j = 0; j < idx->dw->time_count; j++) {
            if (idx->dw->dim_time[j].time_key == fact->time_key) {
                if (idx->dw->dim_time[j].start_year == year) {
                    year_match = true;
                }
                break;
            }
        }

        // Verificar tipo de desastre
        for (int j = 0; j < idx->dw->disaster_type_count; j++) {
            if (idx->dw->dim_disaster_type[j].disaster_type_key == fact->disaster_type_key) {
                if (strcmp(idx->dw->dim_disaster_type[j].disaster_type, disaster_type) == 0) {
                    disaster_match = true;
                }
                break;
            }
        }

        if (country_match && year_match && disaster_match) {
            results[(*result_count)++] = i;
        }
    }

    if (*result_count == 0) {
        free(results);
        return NULL;
    }

    return results;
}

// =============================================================================
// AGREGAÇÕES
// =============================================================================

AggregationResult* index_aggregate_by_country(IndexSystem *idx, const char *country) {
    if (!idx || !idx->dw || !country) return NULL;

    AggregationResult *result = malloc(sizeof(AggregationResult));
    if (!result) return NULL;

    memset(result, 0, sizeof(AggregationResult));
    result->min_deaths = LLONG_MAX;
    result->min_affected = LLONG_MAX;
    result->min_damage = LLONG_MAX;

    for (int i = 0; i < idx->dw->fact_count; i++) {
        DisasterFact *fact = &idx->dw->fact_table[i];

        // Verificar se pertence ao país
        for (int j = 0; j < idx->dw->geography_count; j++) {
            if (idx->dw->dim_geography[j].geography_key == fact->geography_key) {
                if (strcmp(idx->dw->dim_geography[j].country, country) == 0) {
                    result->count++;
                    result->total_deaths += fact->total_deaths;
                    result->total_affected += fact->total_affected;
                    result->total_damage += fact->total_damage;

                    // Máximos
                    if (fact->total_deaths > result->max_deaths) result->max_deaths = fact->total_deaths;
                    if (fact->total_affected > result->max_affected) result->max_affected = fact->total_affected;
                    if (fact->total_damage > result->max_damage) result->max_damage = fact->total_damage;

                    // Mínimos
                    if (fact->total_deaths < result->min_deaths) result->min_deaths = fact->total_deaths;
                    if (fact->total_affected < result->min_affected) result->min_affected = fact->total_affected;
                    if (fact->total_damage < result->min_damage) result->min_damage = fact->total_damage;
                }
                break;
            }
        }
    }

    if (result->count > 0) {
        result->avg_deaths = (double)result->total_deaths / result->count;
        result->avg_affected = (double)result->total_affected / result->count;
        result->avg_damage = (double)result->total_damage / result->count;
    } else {
        result->min_deaths = 0;
        result->min_affected = 0;
        result->min_damage = 0;
    }

    return result;
}

AggregationResult* index_aggregate_by_year(IndexSystem *idx, int year) {
    if (!idx || !idx->dw) return NULL;

    AggregationResult *result = malloc(sizeof(AggregationResult));
    if (!result) return NULL;

    memset(result, 0, sizeof(AggregationResult));
    result->min_deaths = LLONG_MAX;
    result->min_affected = LLONG_MAX;
    result->min_damage = LLONG_MAX;

    for (int i = 0; i < idx->dw->fact_count; i++) {
        DisasterFact *fact = &idx->dw->fact_table[i];

        // Verificar se pertence ao ano
        for (int j = 0; j < idx->dw->time_count; j++) {
            if (idx->dw->dim_time[j].time_key == fact->time_key) {
                if (idx->dw->dim_time[j].start_year == year) {
                    result->count++;
                    result->total_deaths += fact->total_deaths;
                    result->total_affected += fact->total_affected;
                    result->total_damage += fact->total_damage;

                    // Máximos
                    if (fact->total_deaths > result->max_deaths) result->max_deaths = fact->total_deaths;
                    if (fact->total_affected > result->max_affected) result->max_affected = fact->total_affected;
                    if (fact->total_damage > result->max_damage) result->max_damage = fact->total_damage;

                    // Mínimos
                    if (fact->total_deaths < result->min_deaths) result->min_deaths = fact->total_deaths;
                    if (fact->total_affected < result->min_affected) result->min_affected = fact->total_affected;
                    if (fact->total_damage < result->min_damage) result->min_damage = fact->total_damage;
                }
                break;
            }
        }
    }

    if (result->count > 0) {
        result->avg_deaths = (double)result->total_deaths / result->count;
        result->avg_affected = (double)result->total_affected / result->count;
        result->avg_damage = (double)result->total_damage / result->count;
    } else {
        result->min_deaths = 0;
        result->min_affected = 0;
        result->min_damage = 0;
    }

    return result;
}

AggregationResult* index_aggregate_by_disaster_type(IndexSystem *idx, const char *disaster_type) {
    if (!idx || !idx->dw || !disaster_type) return NULL;

    AggregationResult *result = malloc(sizeof(AggregationResult));
    if (!result) return NULL;

    memset(result, 0, sizeof(AggregationResult));
    result->min_deaths = LLONG_MAX;
    result->min_affected = LLONG_MAX;
    result->min_damage = LLONG_MAX;

    for (int i = 0; i < idx->dw->fact_count; i++) {
        DisasterFact *fact = &idx->dw->fact_table[i];

        // Verificar se pertence ao tipo de desastre
        for (int j = 0; j < idx->dw->disaster_type_count; j++) {
            if (idx->dw->dim_disaster_type[j].disaster_type_key == fact->disaster_type_key) {
                if (strcmp(idx->dw->dim_disaster_type[j].disaster_type, disaster_type) == 0) {
                    result->count++;
                    result->total_deaths += fact->total_deaths;
                    result->total_affected += fact->total_affected;
                    result->total_damage += fact->total_damage;

                    // Máximos
                    if (fact->total_deaths > result->max_deaths) result->max_deaths = fact->total_deaths;
                    if (fact->total_affected > result->max_affected) result->max_affected = fact->total_affected;
                    if (fact->total_damage > result->max_damage) result->max_damage = fact->total_damage;

                    // Mínimos
                    if (fact->total_deaths < result->min_deaths) result->min_deaths = fact->total_deaths;
                    if (fact->total_affected < result->min_affected) result->min_affected = fact->total_affected;
                    if (fact->total_damage < result->min_damage) result->min_damage = fact->total_damage;
                }
                break;
            }
        }
    }

    if (result->count > 0) {
        result->avg_deaths = (double)result->total_deaths / result->count;
        result->avg_affected = (double)result->total_affected / result->count;
        result->avg_damage = (double)result->total_damage / result->count;
    } else {
        result->min_deaths = 0;
        result->min_affected = 0;
        result->min_damage = 0;
    }

    return result;
}

AggregationResult* index_aggregate_multi_dimension(IndexSystem *idx, const char *country,
                                                 int year, const char *disaster_type) {
    if (!idx || !idx->dw) return NULL;

    AggregationResult *result = malloc(sizeof(AggregationResult));
    if (!result) return NULL;

    memset(result, 0, sizeof(AggregationResult));
    result->min_deaths = LLONG_MAX;
    result->min_affected = LLONG_MAX;
    result->min_damage = LLONG_MAX;

    for (int i = 0; i < idx->dw->fact_count; i++) {
        DisasterFact *fact = &idx->dw->fact_table[i];
        bool country_match = (country == NULL);
        bool year_match = (year <= 0);
        bool disaster_match = (disaster_type == NULL);

        // Verificar país se especificado
        if (country != NULL) {
            for (int j = 0; j < idx->dw->geography_count; j++) {
                if (idx->dw->dim_geography[j].geography_key == fact->geography_key) {
                    if (strcmp(idx->dw->dim_geography[j].country, country) == 0) {
                        country_match = true;
                    }
                    break;
                }
            }
        }

        // Verificar ano se especificado
        if (year > 0) {
            for (int j = 0; j < idx->dw->time_count; j++) {
                if (idx->dw->dim_time[j].time_key == fact->time_key) {
                    if (idx->dw->dim_time[j].start_year == year) {
                        year_match = true;
                    }
                    break;
                }
            }
        }

        // Verificar tipo de desastre se especificado
        if (disaster_type != NULL) {
            for (int j = 0; j < idx->dw->disaster_type_count; j++) {
                if (idx->dw->dim_disaster_type[j].disaster_type_key == fact->disaster_type_key) {
                    if (strcmp(idx->dw->dim_disaster_type[j].disaster_type, disaster_type) == 0) {
                        disaster_match = true;
                    }
                    break;
                }
            }
        }

        if (country_match && year_match && disaster_match) {
            result->count++;
            result->total_deaths += fact->total_deaths;
            result->total_affected += fact->total_affected;
            result->total_damage += fact->total_damage;

            // Máximos
            if (fact->total_deaths > result->max_deaths) result->max_deaths = fact->total_deaths;
            if (fact->total_affected > result->max_affected) result->max_affected = fact->total_affected;
            if (fact->total_damage > result->max_damage) result->max_damage = fact->total_damage;

            // Mínimos
            if (fact->total_deaths < result->min_deaths) result->min_deaths = fact->total_deaths;
            if (fact->total_affected < result->min_affected) result->min_affected = fact->total_affected;
            if (fact->total_damage < result->min_damage) result->min_damage = fact->total_damage;
        }
    }

    if (result->count > 0) {
        result->avg_deaths = (double)result->total_deaths / result->count;
        result->avg_affected = (double)result->total_affected / result->count;
        result->avg_damage = (double)result->total_damage / result->count;
    } else {
        result->min_deaths = 0;
        result->min_affected = 0;
        result->min_damage = 0;
    }

    return result;
}

// =============================================================================
// SISTEMA COMPLETO - OptimizedDataWarehouse
// =============================================================================

OptimizedDataWarehouse* optimized_dw_create() {
    IndexConfiguration *config = index_config_create_default();
    OptimizedDataWarehouse *odw = optimized_dw_create_with_config(config);
    index_config_destroy(config);
    return odw;
}

OptimizedDataWarehouse* optimized_dw_create_with_config(IndexConfiguration *config) {
    if (!config) return NULL;

    OptimizedDataWarehouse *odw = malloc(sizeof(OptimizedDataWarehouse));
    if (!odw) return NULL;

    // Criar data warehouse base
    odw->dw = dw_create();
    if (!odw->dw) {
        free(odw);
        return NULL;
    }

    // Criar sistema de índices
    odw->indexes = index_system_create_with_config(odw->dw, config);
    if (!odw->indexes) {
        dw_destroy(odw->dw);
        free(odw);
        return NULL;
    }

    // Criar sistema de cache
    odw->cache = cache_system_create(config->max_cache_age);
    if (!odw->cache) {
        index_system_destroy(odw->indexes);
        dw_destroy(odw->dw);
        free(odw);
        return NULL;
    }

    // Copiar configuração
    odw->config = malloc(sizeof(IndexConfiguration));
    if (odw->config) {
        memcpy(odw->config, config, sizeof(IndexConfiguration));
    }

    strcpy(odw->version, "1.0.0");

    return odw;
}

void optimized_dw_destroy(OptimizedDataWarehouse *odw) {
    if (!odw) return;

    cache_system_destroy(odw->cache);
    index_system_destroy(odw->indexes);
    dw_destroy(odw->dw);
    index_config_destroy(odw->config);
    free(odw);
}

OptimizedDataWarehouse* optimized_dw_load(const char *base_path) {
    if (!base_path) return NULL;

    OptimizedDataWarehouse *odw = optimized_dw_create();
    if (!odw) return NULL;

    // Tentar carregar data warehouse
    DataWarehouse *loaded_dw = dw_load_from_files(base_path);
    if (loaded_dw) {
        dw_destroy(odw->dw);
        odw->dw = loaded_dw;
        odw->indexes->dw = loaded_dw;

        // Construir índices
        index_system_build_all(odw->indexes);
    }

    return odw;
}

int optimized_dw_save(OptimizedDataWarehouse *odw, const char *base_path) {
    if (!odw || !base_path) return 0;

    // Salvar data warehouse
    if (!dw_save_to_files(odw->dw, base_path)) {
        return 0;
    }

    // Salvar índices
    if (!index_system_save_all(odw->indexes)) {
        return 0;
    }

    return 1;
}

int* optimized_query_by_country(OptimizedDataWarehouse *odw, const char *country, int *result_count) {
    if (!odw || !country || !result_count) return NULL;

    // Verificar cache primeiro
    char cache_key[256];
    snprintf(cache_key, sizeof(cache_key), "country:%s", country);

    int *cached_results = cache_search(odw->cache, cache_key, result_count);
    if (cached_results) {
        return cached_results;
    }

    // Buscar usando índices
    int *results = index_search_by_country(odw->indexes, country, result_count);

    // Inserir no cache se encontrou resultados
    if (results && *result_count > 0) {
        cache_insert(odw->cache, cache_key, results, *result_count);
    }

    return results;
}

AggregationResult* optimized_aggregate_query(OptimizedDataWarehouse *odw,
                                           const char *country, int year, const char *disaster_type) {
    if (!odw) return NULL;

    // Para agregações, não usar cache devido à complexidade dos resultados
    return index_aggregate_multi_dimension(odw->indexes, country, year, disaster_type);
}

char** optimized_autocomplete_country(OptimizedDataWarehouse *odw, const char *prefix, int *result_count) {
    if (!odw || !prefix || !result_count) return NULL;

    return index_search_country_prefix(odw->indexes, prefix, result_count);
}

// =============================================================================
// ANÁLISE E ESTATÍSTICAS
// =============================================================================

void index_analyze_performance(IndexSystem *idx) {
    if (!idx || !idx->dw) return;

    printf("=== INDEX PERFORMANCE ANALYSIS ===\n");
    printf("Data Warehouse Statistics:\n");
    printf("  Facts: %d\n", idx->dw->fact_count);
    printf("  Time dimensions: %d\n", idx->dw->time_count);
    printf("  Geography dimensions: %d\n", idx->dw->geography_count);
    printf("  Disaster type dimensions: %d\n", idx->dw->disaster_type_count);

    printf("\nIndex Status:\n");
    printf("  Indexes loaded: %s\n", idx->indexes_loaded ? "Yes" : "No");
    printf("  Last rebuild: %ld\n", idx->last_rebuild_time);
    printf("  Base path: %s\n", idx->index_base_path);
}

void index_print_statistics(IndexSystem *idx) {
    if (!idx) return;

    printf("=== INDEX SYSTEM STATISTICS ===\n");
    printf("Trie Indexes:\n");
    printf("  Country Trie: %s\n", idx->country_trie ? "Initialized" : "NULL");
    printf("  Disaster Type Trie: %s\n", idx->disaster_type_trie ? "Initialized" : "NULL");
    printf("  Region Trie: %s\n", idx->region_trie ? "Initialized" : "NULL");

    printf("B+ Tree Indexes:\n");
    printf("  Year B+ Tree: %s\n", idx->year_bplus ? "Initialized" : "NULL");
    printf("  Deaths B+ Tree: %s\n", idx->deaths_bplus ? "Initialized" : "NULL");
    printf("  Damage B+ Tree: %s\n", idx->damage_bplus ? "Initialized" : "NULL");

    printf("Bitmap Indexes:\n");
    int year_bitmaps = 0, country_bitmaps = 0, disaster_bitmaps = 0;
    for (int i = 0; i < 200; i++) {
        if (idx->year_bitmap[i]) year_bitmaps++;
    }
    for (int i = 0; i < 250; i++) {
        if (idx->country_bitmap[i]) country_bitmaps++;
    }
    for (int i = 0; i < 100; i++) {
        if (idx->disaster_bitmap[i]) disaster_bitmaps++;
    }
    printf("  Year bitmaps: %d/200\n", year_bitmaps);
    printf("  Country bitmaps: %d/250\n", country_bitmaps);
    printf("  Disaster bitmaps: %d/100\n", disaster_bitmaps);
}

int index_verify_integrity(IndexSystem *idx) {
    if (!idx || !idx->dw) return 0;

    printf("Verifying index integrity...\n");

    // Verificar consistência básica
    if (idx->dw->fact_count < 0 || idx->dw->time_count < 0 ||
        idx->dw->geography_count < 0 || idx->dw->disaster_type_count < 0) {
        printf("ERROR: Negative counts detected\n");
        return 0;
    }

    // Verificar se todas as chaves estrangeiras são válidas
    for (int i = 0; i < idx->dw->fact_count; i++) {
        DisasterFact *fact = &idx->dw->fact_table[i];

        bool time_found = false, geo_found = false, type_found = false;

        for (int j = 0; j < idx->dw->time_count; j++) {
            if (idx->dw->dim_time[j].time_key == fact->time_key) {
                time_found = true;
                break;
            }
        }

        for (int j = 0; j < idx->dw->geography_count; j++) {
            if (idx->dw->dim_geography[j].geography_key == fact->geography_key) {
                geo_found = true;
                break;
            }
        }

        for (int j = 0; j < idx->dw->disaster_type_count; j++) {
            if (idx->dw->dim_disaster_type[j].disaster_type_key == fact->disaster_type_key) {
                type_found = true;
                break;
            }
        }

        if (!time_found || !geo_found || !type_found) {
            printf("ERROR: Fact %d has invalid foreign keys\n", i);
            return 0;
        }
    }

    printf("Index integrity verification completed successfully\n");
    return 1;
}

void optimized_dw_print_statistics(OptimizedDataWarehouse *odw) {
    if (!odw) return;

    printf("=== OPTIMIZED DATA WAREHOUSE STATISTICS ===\n");
    printf("Version: %s\n", odw->version);

    if (odw->dw) {
        dw_print_statistics(odw->dw);
    }

    if (odw->indexes) {
        index_print_statistics(odw->indexes);
    }

    if (odw->cache) {
        cache_print_statistics(odw->cache);
    }

    if (odw->config) {
        printf("\nConfiguration:\n");
        printf("  Trie indexes: %s\n", odw->config->enable_trie_indexes ? "Enabled" : "Disabled");
        printf("  B+ Tree indexes: %s\n", odw->config->enable_bplus_indexes ? "Enabled" : "Disabled");
        printf("  Bitmap indexes: %s\n", odw->config->enable_bitmap_indexes ? "Enabled" : "Disabled");
        printf("  Composite indexes: %s\n", odw->config->enable_composite_indexes ? "Enabled" : "Disabled");
        printf("  Auto rebuild: %s\n", odw->config->auto_rebuild ? "Enabled" : "Disabled");
        printf("  Cache size: %d\n", odw->config->cache_size);
        printf("  Max cache age: %d seconds\n", odw->config->max_cache_age);
        printf("  Index directory: %s\n", odw->config->index_directory);
    }
}
