// =============================================================================
// star_schema_indexes.c - Implementação completa do sistema de índices
// =============================================================================

#include "star_schema_indexes.h"
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <limits.h>
#include <ctype.h>

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
    if (!cache || !query_key || !result_count) return NULL;

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

                // Verificar se há resultados antes de copiar
                if (entry->result_count > 0 && entry->results) {
                    int *results = malloc(entry->result_count * sizeof(int));
                    if (results) {
                        memcpy(results, entry->results, entry->result_count * sizeof(int));
                        return results;
                    }
                }

                *result_count = 0;
                return NULL;
            } else {
                // Entrada expirada - marcar como expirada
                entry->result_count = 0;
            }
        }
        entry = entry->next;
    }

    cache->miss_count++;
    *result_count = 0;
    return NULL;
}

int cache_insert(CacheSystem *cache, const char *query_key, int *results, int result_count) {
    if (!cache || !query_key || !results || result_count <= 0) return 0;

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

    // Criar Tries reais ao invés de calloc
    if (config->enable_trie_indexes) {
        idx->country_trie = trie_create("country_index.dat");
        idx->disaster_type_trie = trie_create("disaster_type_index.dat");
        idx->region_trie = trie_create("region_index.dat");
        idx->subregion_trie = trie_create("subregion_index.dat");
        idx->year_country_trie = trie_create("year_country_index.dat");
        idx->disaster_country_trie = trie_create("disaster_country_index.dat");
        idx->year_disaster_trie = trie_create("year_disaster_index.dat");
    }

    // Criar B+ Trees reais ao invés de calloc
    if (config->enable_bplus_indexes) {
        idx->year_bplus = bplus_create("year_index.dat");
        idx->deaths_bplus = bplus_create("deaths_index.dat");
        idx->affected_bplus = bplus_create("affected_index.dat");
        idx->damage_bplus = bplus_create("damage_index.dat");
        idx->month_bplus = bplus_create("month_index.dat");
        idx->day_bplus = bplus_create("day_index.dat");
    }

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

    // Liberar Tries reais
    if (idx->country_trie) trie_destroy(idx->country_trie);
    if (idx->disaster_type_trie) trie_destroy(idx->disaster_type_trie);
    if (idx->region_trie) trie_destroy(idx->region_trie);
    if (idx->subregion_trie) trie_destroy(idx->subregion_trie);
    if (idx->year_country_trie) trie_destroy(idx->year_country_trie);
    if (idx->disaster_country_trie) trie_destroy(idx->disaster_country_trie);
    if (idx->year_disaster_trie) trie_destroy(idx->year_disaster_trie);

    // Liberar B+ Trees reais
    if (idx->year_bplus) bplus_destroy(idx->year_bplus);
    if (idx->deaths_bplus) bplus_destroy(idx->deaths_bplus);
    if (idx->affected_bplus) bplus_destroy(idx->affected_bplus);
    if (idx->damage_bplus) bplus_destroy(idx->damage_bplus);
    if (idx->month_bplus) bplus_destroy(idx->month_bplus);
    if (idx->day_bplus) bplus_destroy(idx->day_bplus);

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
        if (!index_system_insert_entry(idx, i)) {
            printf("Warning: Failed to index fact %d\n", i);
        }
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
    if (!idx || !idx->dw || fact_id >= idx->dw->fact_count || fact_id < 0) return 0;

    DisasterFact *fact = &idx->dw->fact_table[fact_id];

    // Encontrar dimensões relacionadas
    DimTime *time_dim = NULL;
    DimGeography *geo_dim = NULL;
    DimDisasterType *type_dim = NULL;

    // Buscar dimensão tempo
    for (int i = 0; i < idx->dw->time_count; i++) {
        if (idx->dw->dim_time[i].time_key == fact->time_key) {
            time_dim = &idx->dw->dim_time[i];
            break;
        }
    }

    // Buscar dimensão geografia
    for (int i = 0; i < idx->dw->geography_count; i++) {
        if (idx->dw->dim_geography[i].geography_key == fact->geography_key) {
            geo_dim = &idx->dw->dim_geography[i];
            break;
        }
    }

    // Buscar dimensão tipo de desastre
    for (int i = 0; i < idx->dw->disaster_type_count; i++) {
        if (idx->dw->dim_disaster_type[i].disaster_type_key == fact->disaster_type_key) {
            type_dim = &idx->dw->dim_disaster_type[i];
            break;
        }
    }

    // Inserir nos índices Trie reais
    if (geo_dim && idx->country_trie) {
        trie_insert(idx->country_trie, geo_dim->country, fact_id);
        if (idx->region_trie) {
            trie_insert(idx->region_trie, geo_dim->region, fact_id);
        }
        if (idx->subregion_trie) {
            trie_insert(idx->subregion_trie, geo_dim->subregion, fact_id);
        }
    }

    if (type_dim && idx->disaster_type_trie) {
        trie_insert(idx->disaster_type_trie, type_dim->disaster_type, fact_id);
    }

    // Inserir nos índices B+ Tree reais
    if (time_dim && idx->year_bplus) {
        bplus_insert(idx->year_bplus, time_dim->start_year, fact_id);
        if (idx->month_bplus) {
            bplus_insert(idx->month_bplus, time_dim->start_month, fact_id);
        }
        if (idx->day_bplus) {
            bplus_insert(idx->day_bplus, time_dim->start_day, fact_id);
        }
    }

    if (idx->deaths_bplus) {
        bplus_insert(idx->deaths_bplus, fact->total_deaths, fact_id);
    }
    if (idx->affected_bplus) {
        // Para valores muito grandes, usar hash ou dividir por 1000
        bplus_insert(idx->affected_bplus, (int)(fact->total_affected / 1000), fact_id);
    }
    if (idx->damage_bplus) {
        bplus_insert(idx->damage_bplus, (int)(fact->total_damage / 1000), fact_id);
    }

    // Inserir nos índices compostos
    if (geo_dim && time_dim && idx->year_country_trie) {
        char composite_key[100];
        snprintf(composite_key, sizeof(composite_key), "%d_%s",
                time_dim->start_year, geo_dim->country);
        trie_insert(idx->year_country_trie, composite_key, fact_id);
    }

    if (geo_dim && type_dim && idx->disaster_country_trie) {
        char composite_key[100];
        snprintf(composite_key, sizeof(composite_key), "%s_%s",
                type_dim->disaster_type, geo_dim->country);
        trie_insert(idx->disaster_country_trie, composite_key, fact_id);
    }

    if (time_dim && type_dim && idx->year_disaster_trie) {
        char composite_key[100];
        snprintf(composite_key, sizeof(composite_key), "%d_%s",
                time_dim->start_year, type_dim->disaster_type);
        trie_insert(idx->year_disaster_trie, composite_key, fact_id);
    }

    // Atualizar bitmaps
    if (time_dim && time_dim->start_year >= 1970 && time_dim->start_year < 2170) {
        int year_index = time_dim->start_year - 1970;
        if (year_index >= 0 && year_index < 200 && idx->year_bitmap[year_index]) {
            bitmap_set_bit(idx->year_bitmap[year_index], fact_id);
        }
    }

    return 1;
}

int index_system_save_all(IndexSystem *idx) {
    if (!idx) return 0;

    printf("Saving indexes to %s\n", idx->index_base_path);

    // Salvar Tries
    if (idx->country_trie) trie_save_to_file(idx->country_trie);
    if (idx->disaster_type_trie) trie_save_to_file(idx->disaster_type_trie);
    if (idx->region_trie) trie_save_to_file(idx->region_trie);
    if (idx->subregion_trie) trie_save_to_file(idx->subregion_trie);
    if (idx->year_country_trie) trie_save_to_file(idx->year_country_trie);
    if (idx->disaster_country_trie) trie_save_to_file(idx->disaster_country_trie);
    if (idx->year_disaster_trie) trie_save_to_file(idx->year_disaster_trie);

    // Salvar B+ Trees
    if (idx->year_bplus) bplus_save_to_file(idx->year_bplus);
    if (idx->deaths_bplus) bplus_save_to_file(idx->deaths_bplus);
    if (idx->affected_bplus) bplus_save_to_file(idx->affected_bplus);
    if (idx->damage_bplus) bplus_save_to_file(idx->damage_bplus);
    if (idx->month_bplus) bplus_save_to_file(idx->month_bplus);
    if (idx->day_bplus) bplus_save_to_file(idx->day_bplus);

    return 1;
}

int index_system_load_all(IndexSystem *idx) {
    if (!idx) return 0;

    printf("Loading indexes from %s\n", idx->index_base_path);
    // Para esta implementação, apenas reconstrói os índices
    return index_system_build_all(idx);
}

// =============================================================================
// CONSULTAS SIMPLES
// =============================================================================

int* index_search_by_country(IndexSystem *idx, const char *country, int *result_count) {
    if (!idx || !idx->dw || !country || !result_count) return NULL;

    *result_count = 0;

    // Tentar usar índice Trie primeiro
    if (idx->country_trie) {
        long *trie_results = trie_search(idx->country_trie, country, result_count);
        if (trie_results && *result_count > 0) {
            // Converter long* para int*
            int *results = malloc(*result_count * sizeof(int));
            if (results) {
                for (int i = 0; i < *result_count; i++) {
                    results[i] = (int)trie_results[i];
                }
            }
            free(trie_results);
            return results;
        }
    }

    // Fallback para busca linear
    int *results = malloc(idx->dw->fact_count * sizeof(int));
    if (!results) return NULL;

    for (int i = 0; i < idx->dw->fact_count; i++) {
        DisasterFact *fact = &idx->dw->fact_table[i];

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

    // Coletar países únicos que começam com o prefixo
    char **results = malloc(idx->dw->geography_count * sizeof(char*));
    if (!results) return NULL;

    int prefix_len = strlen(prefix);

    // Converter prefixo para minúsculo para comparação case-insensitive
    char prefix_lower[50];
    strncpy(prefix_lower, prefix, sizeof(prefix_lower) - 1);
    prefix_lower[sizeof(prefix_lower) - 1] = '\0';
    for (int i = 0; prefix_lower[i]; i++) {
        prefix_lower[i] = tolower(prefix_lower[i]);
    }

    // Buscar em todas as dimensões geografia
    for (int i = 0; i < idx->dw->geography_count; i++) {
        char country_lower[50];
        strncpy(country_lower, idx->dw->dim_geography[i].country, sizeof(country_lower) - 1);
        country_lower[sizeof(country_lower) - 1] = '\0';

        // Converter para minúsculo
        for (int j = 0; country_lower[j]; j++) {
            country_lower[j] = tolower(country_lower[j]);
        }

        // Verificar se começa com o prefixo
        if (strncmp(country_lower, prefix_lower, prefix_len) == 0) {
            // Verificar se já não foi adicionado (evitar duplicatas)
            bool already_added = false;
            for (int k = 0; k < *result_count; k++) {
                if (strcmp(results[k], idx->dw->dim_geography[i].country) == 0) {
                    already_added = true;
                    break;
                }
            }

            if (!already_added) {
                results[*result_count] = malloc(strlen(idx->dw->dim_geography[i].country) + 1);
                if (results[*result_count]) {
                    strcpy(results[*result_count], idx->dw->dim_geography[i].country);
                    (*result_count)++;
                }
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

    // Tentar usar índice Trie primeiro
    if (idx->disaster_type_trie) {
        long *trie_results = trie_search(idx->disaster_type_trie, disaster_type, result_count);
        if (trie_results && *result_count > 0) {
            int *results = malloc(*result_count * sizeof(int));
            if (results) {
                for (int i = 0; i < *result_count; i++) {
                    results[i] = (int)trie_results[i];
                }
            }
            free(trie_results);
            return results;
        }
    }

    // Fallback para busca linear
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

    // Tentar usar índice B+ Tree primeiro
    if (idx->year_bplus) {
        long *bplus_results = bplus_search(idx->year_bplus, year, result_count);
        if (bplus_results && *result_count > 0) {
            // Converter long* para int*
            int *results = malloc(*result_count * sizeof(int));
            if (results) {
                for (int i = 0; i < *result_count; i++) {
                    results[i] = (int)bplus_results[i];
                }
            }
            free(bplus_results);
            return results;
        }
    }

    // Fallback para busca linear
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
    if (!idx || !idx->dw || !result_count || start_year > end_year) return NULL;

    *result_count = 0;

    // Usar bitmaps se disponíveis para consultas de intervalo
    if (start_year >= 1970 && end_year < 2170) {
        int start_index = start_year - 1970;
        int end_index = end_year - 1970;

        if (start_index >= 0 && end_index < 200) {
            // Usar operações de bitmap para encontrar união
            unsigned char *result_bitmap = NULL;

            for (int year_idx = start_index; year_idx <= end_index; year_idx++) {
                if (idx->year_bitmap[year_idx]) {
                    if (!result_bitmap) {
                        result_bitmap = malloc(1000); // 8000 bits
                        memcpy(result_bitmap, idx->year_bitmap[year_idx], 1000);
                    } else {
                        unsigned char *temp = bitmap_or(result_bitmap, idx->year_bitmap[year_idx], 1000);
                        if (temp) {
                            free(result_bitmap);
                            result_bitmap = temp;
                        }
                    }
                }
            }

            if (result_bitmap) {
                // Contar bits setados e criar array de resultados
                int bit_count = bitmap_count_bits(result_bitmap, 1000);
                if (bit_count > 0) {
                    int *results = malloc(bit_count * sizeof(int));
                    if (results) {
                        int result_idx = 0;
                        for (int i = 0; i < 8000 && result_idx < bit_count; i++) {
                            if (bitmap_get_bit(result_bitmap, i)) {
                                results[result_idx++] = i;
                            }
                        }
                        *result_count = result_idx;
                        free(result_bitmap);
                        return results;
                    }
                }
                free(result_bitmap);
            }
        }
    }

    // Fallback para busca linear
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

    // Tentar usar índice composto primeiro
    if (idx->year_country_trie) {
        char composite_key[100];
        snprintf(composite_key, sizeof(composite_key), "%d_%s", year, country);

        long *composite_results = trie_search(idx->year_country_trie, composite_key, result_count);
        if (composite_results && *result_count > 0) {
            int *results = malloc(*result_count * sizeof(int));
            if (results) {
                for (int i = 0; i < *result_count; i++) {
                    results[i] = (int)composite_results[i];
                }
            }
            free(composite_results);
            return results;
        }
    }

    // Fallback: intersecção de resultados individuais
    int country_count = 0, year_count = 0;
    int *country_results = index_search_by_country(idx, country, &country_count);
    int *year_results = index_search_by_year(idx, year, &year_count);

    if (!country_results || !year_results) {
        free(country_results);
        free(year_results);
        return NULL;
    }

    // Intersecção dos resultados
    int *results = malloc((country_count < year_count ? country_count : year_count) * sizeof(int));
    if (!results) {
        free(country_results);
        free(year_results);
        return NULL;
    }

    for (int i = 0; i < country_count; i++) {
        for (int j = 0; j < year_count; j++) {
            if (country_results[i] == year_results[j]) {
                results[(*result_count)++] = country_results[i];
                break;
            }
        }
    }

    free(country_results);
    free(year_results);

    if (*result_count == 0) {
        free(results);
        return NULL;
    }

    return results;
}

int* index_search_disaster_country(IndexSystem *idx, const char *disaster_type, const char *country, int *result_count) {
    if (!idx || !idx->dw || !disaster_type || !country || !result_count) return NULL;

    *result_count = 0;

    // Tentar usar índice composto primeiro
    if (idx->disaster_country_trie) {
        char composite_key[100];
        snprintf(composite_key, sizeof(composite_key), "%s_%s", disaster_type, country);

        long *composite_results = trie_search(idx->disaster_country_trie, composite_key, result_count);
        if (composite_results && *result_count > 0) {
            int *results = malloc(*result_count * sizeof(int));
            if (results) {
                for (int i = 0; i < *result_count; i++) {
                    results[i] = (int)composite_results[i];
                }
            }
            free(composite_results);
            return results;
        }
    }

    // Fallback para busca linear
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

    printf("Composite Indexes:\n");
    printf("  Year-Country Trie: %s\n", idx->year_country_trie ? "Initialized" : "NULL");
    printf("  Disaster-Country Trie: %s\n", idx->disaster_country_trie ? "Initialized" : "NULL");
    printf("  Year-Disaster Trie: %s\n", idx->year_disaster_trie ? "Initialized" : "NULL");
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
    int invalid_refs = 0;
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
            invalid_refs++;
            if (invalid_refs <= 5) { // Mostrar apenas os primeiros 5 erros
                printf("ERROR: Fact %d has invalid foreign keys (time:%s, geo:%s, type:%s)\n",
                       i, time_found ? "OK" : "MISSING",
                       geo_found ? "OK" : "MISSING",
                       type_found ? "OK" : "MISSING");
            }
        }
    }

    if (invalid_refs > 0) {
        printf("ERROR: Found %d facts with invalid foreign key references\n", invalid_refs);
        return 0;
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

// =============================================================================
// IMPLEMENTAÇÕES ESSENCIAIS ADICIONAIS - CONSULTAS POR INTERVALO DE ANOS OTIMIZADAS
// =============================================================================

int* index_search_by_year_range_optimized(IndexSystem *idx, int start_year, int end_year, int *result_count) {
    if (!idx || !idx->dw || !result_count || start_year > end_year) return NULL;

    *result_count = 0;

    // Usar B+ Tree para busca por intervalo se disponível
    if (idx->year_bplus) {
        long *range_results = bplus_search_range(idx->year_bplus, start_year, end_year, result_count);
        if (range_results && *result_count > 0) {
            int *results = malloc(*result_count * sizeof(int));
            if (results) {
                for (int i = 0; i < *result_count; i++) {
                    results[i] = (int)range_results[i];
                }
            }
            free(range_results);
            return results;
        }
    }

    // Fallback para busca por intervalo usando bitmap ou linear
    return index_search_by_year_range(idx, start_year, end_year, result_count);
}

int* index_search_country_year_range(IndexSystem *idx, const char *country, int start_year, int end_year, int *result_count) {
    if (!idx || !idx->dw || !country || !result_count || start_year > end_year) return NULL;

    *result_count = 0;

    // Buscar todos os anos no intervalo para o país
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

        // Verificar intervalo de anos
        for (int j = 0; j < idx->dw->time_count; j++) {
            if (idx->dw->dim_time[j].time_key == fact->time_key) {
                if (idx->dw->dim_time[j].start_year >= start_year &&
                    idx->dw->dim_time[j].start_year <= end_year) {
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

// =============================================================================
// FUNÇÕES DE ORDENAÇÃO COM B+ TREE
// =============================================================================

int* index_sort_facts_by_affected(IndexSystem *idx, int *fact_ids, int fact_count, bool descending, int *result_count) {
    if (!idx || !idx->dw || !fact_ids || fact_count <= 0 || !result_count) return NULL;

    *result_count = 0;

    // Criar array de estruturas para ordenação
    FactSortData *sort_data = malloc(fact_count * sizeof(FactSortData));
    if (!sort_data) return NULL;

    // Preencher dados para ordenação
    for (int i = 0; i < fact_count; i++) {
        int fact_id = fact_ids[i];
        if (fact_id >= 0 && fact_id < idx->dw->fact_count) {
            sort_data[i].fact_id = fact_id;
            sort_data[i].sort_value = idx->dw->fact_table[fact_id].total_affected;

            // Preencher dados auxiliares
            DimTime *time_dim = NULL;
            DimGeography *geo_dim = NULL;

            for (int j = 0; j < idx->dw->time_count; j++) {
                if (idx->dw->dim_time[j].time_key == idx->dw->fact_table[fact_id].time_key) {
                    time_dim = &idx->dw->dim_time[j];
                    break;
                }
            }

            for (int j = 0; j < idx->dw->geography_count; j++) {
                if (idx->dw->dim_geography[j].geography_key == idx->dw->fact_table[fact_id].geography_key) {
                    geo_dim = &idx->dw->dim_geography[j];
                    break;
                }
            }

            sort_data[i].year = time_dim ? time_dim->start_year : 0;
            strncpy(sort_data[i].country, geo_dim ? geo_dim->country : "Unknown",
                   sizeof(sort_data[i].country) - 1);
            sort_data[i].country[sizeof(sort_data[i].country) - 1] = '\0';
        }
    }

    // Ordenar usando qsort
    qsort(sort_data, fact_count, sizeof(FactSortData), compare_fact_by_affected_desc);

    // Criar array de resultados
    int *results = malloc(fact_count * sizeof(int));
    if (!results) {
        free(sort_data);
        return NULL;
    }

    for (int i = 0; i < fact_count; i++) {
        results[i] = sort_data[i].fact_id;
    }

    *result_count = fact_count;
    free(sort_data);
    return results;
}

int* index_sort_facts_by_damage(IndexSystem *idx, int *fact_ids, int fact_count, bool descending, int *result_count) {
    if (!idx || !idx->dw || !fact_ids || fact_count <= 0 || !result_count) return NULL;

    *result_count = 0;

    FactSortData *sort_data = malloc(fact_count * sizeof(FactSortData));
    if (!sort_data) return NULL;

    for (int i = 0; i < fact_count; i++) {
        int fact_id = fact_ids[i];
        if (fact_id >= 0 && fact_id < idx->dw->fact_count) {
            sort_data[i].fact_id = fact_id;
            sort_data[i].sort_value = idx->dw->fact_table[fact_id].total_damage;
        }
    }

    qsort(sort_data, fact_count, sizeof(FactSortData), compare_fact_by_damage_desc);

    int *results = malloc(fact_count * sizeof(int));
    if (!results) {
        free(sort_data);
        return NULL;
    }

    for (int i = 0; i < fact_count; i++) {
        results[i] = sort_data[i].fact_id;
    }

    *result_count = fact_count;
    free(sort_data);
    return results;
}

int* index_sort_facts_by_deaths(IndexSystem *idx, int *fact_ids, int fact_count, bool descending, int *result_count) {
    if (!idx || !idx->dw || !fact_ids || fact_count <= 0 || !result_count) return NULL;

    *result_count = 0;

    FactSortData *sort_data = malloc(fact_count * sizeof(FactSortData));
    if (!sort_data) return NULL;

    for (int i = 0; i < fact_count; i++) {
        int fact_id = fact_ids[i];
        if (fact_id >= 0 && fact_id < idx->dw->fact_count) {
            sort_data[i].fact_id = fact_id;
            sort_data[i].sort_value = idx->dw->fact_table[fact_id].total_deaths;
        }
    }

    qsort(sort_data, fact_count, sizeof(FactSortData), compare_fact_by_deaths_desc);

    int *results = malloc(fact_count * sizeof(int));
    if (!results) {
        free(sort_data);
        return NULL;
    }

    for (int i = 0; i < fact_count; i++) {
        results[i] = sort_data[i].fact_id;
    }

    *result_count = fact_count;
    free(sort_data);
    return results;
}

int* index_get_sorted_countries_by_affected(IndexSystem *idx, int *country_ids, int country_count,
                                           bool descending, int *result_count) {
    if (!idx || !idx->dw || !country_ids || country_count <= 0 || !result_count) return NULL;

    *result_count = 0;

    // Calcular totais por país
    CountrySortData *sort_data = malloc(country_count * sizeof(CountrySortData));
    if (!sort_data) return NULL;

    for (int i = 0; i < country_count; i++) {
        int country_id = country_ids[i];
        if (country_id >= 0 && country_id < idx->dw->geography_count) {
            strncpy(sort_data[i].country, idx->dw->dim_geography[country_id].country,
                   sizeof(sort_data[i].country) - 1);
            sort_data[i].country[sizeof(sort_data[i].country) - 1] = '\0';
            sort_data[i].total_affected = 0;
            sort_data[i].total_damage = 0;
            sort_data[i].total_deaths = 0;
            sort_data[i].disaster_count = 0;
            sort_data[i].original_index = i;

            // Somar todos os fatos deste país
            for (int j = 0; j < idx->dw->fact_count; j++) {
                if (idx->dw->fact_table[j].geography_key == idx->dw->dim_geography[country_id].geography_key) {
                    sort_data[i].total_affected += idx->dw->fact_table[j].total_affected;
                    sort_data[i].total_damage += idx->dw->fact_table[j].total_damage;
                    sort_data[i].total_deaths += idx->dw->fact_table[j].total_deaths;
                    sort_data[i].disaster_count++;
                }
            }
        }
    }

    qsort(sort_data, country_count, sizeof(CountrySortData), compare_country_by_affected_desc);

    int *results = malloc(country_count * sizeof(int));
    if (!results) {
        free(sort_data);
        return NULL;
    }

    for (int i = 0; i < country_count; i++) {
        results[i] = sort_data[i].original_index;
    }

    *result_count = country_count;
    free(sort_data);
    return results;
}

// =============================================================================
// AGREGAÇÃO POR INTERVALO DE ANOS
// =============================================================================

AggregationResult* index_aggregate_by_year_range(IndexSystem *idx, int start_year, int end_year) {
    if (!idx || !idx->dw || start_year > end_year) return NULL;

    AggregationResult *result = malloc(sizeof(AggregationResult));
    if (!result) return NULL;

    memset(result, 0, sizeof(AggregationResult));
    result->min_deaths = LLONG_MAX;
    result->min_affected = LLONG_MAX;
    result->min_damage = LLONG_MAX;

    for (int i = 0; i < idx->dw->fact_count; i++) {
        DisasterFact *fact = &idx->dw->fact_table[i];

        // Verificar se pertence ao intervalo de anos
        for (int j = 0; j < idx->dw->time_count; j++) {
            if (idx->dw->dim_time[j].time_key == fact->time_key) {
                if (idx->dw->dim_time[j].start_year >= start_year &&
                    idx->dw->dim_time[j].start_year <= end_year) {
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

// =============================================================================
// CONSULTAS OTIMIZADAS ADICIONAIS
// =============================================================================

int* optimized_query_by_country_and_year_range(OptimizedDataWarehouse *odw, const char *country,
                                               int start_year, int end_year, int *result_count) {
    if (!odw || !country || !result_count || start_year > end_year) return NULL;

    // Verificar cache primeiro
    char cache_key[256];
    snprintf(cache_key, sizeof(cache_key), "country_year_range:%s:%d:%d", country, start_year, end_year);

    int *cached_results = cache_search(odw->cache, cache_key, result_count);
    if (cached_results) {
        return cached_results;
    }

    // Buscar usando índices
    int *results = index_search_country_year_range(odw->indexes, country, start_year, end_year, result_count);

    // Inserir no cache se encontrou resultados
    if (results && *result_count > 0) {
        cache_insert(odw->cache, cache_key, results, *result_count);
    }

    return results;
}

AggregationResult* optimized_aggregate_by_year_range(OptimizedDataWarehouse *odw, int start_year, int end_year) {
    if (!odw || start_year > end_year) return NULL;

    return index_aggregate_by_year_range(odw->indexes, start_year, end_year);
}

int* optimized_query_with_all_filters(OptimizedDataWarehouse *odw, const char *country,
                                     const char *disaster_type, int start_year, int end_year,
                                     int sort_type, bool descending, int *result_count) {
    if (!odw || !result_count) return NULL;

    *result_count = 0;

    // Buscar com filtros básicos
    int *filtered_results = NULL;
    int filtered_count = 0;

    // Aplicar filtros em sequência
    if (country && strlen(country) > 0) {
        if (start_year > 0 && end_year > 0) {
            filtered_results = index_search_country_year_range(odw->indexes, country, start_year, end_year, &filtered_count);
        } else {
            filtered_results = index_search_by_country(odw->indexes, country, &filtered_count);
        }
    } else if (start_year > 0 && end_year > 0) {
        filtered_results = index_search_by_year_range(odw->indexes, start_year, end_year, &filtered_count);
    } else {
        // Sem filtros, retornar todos os fatos
        filtered_results = malloc(odw->dw->fact_count * sizeof(int));
        if (filtered_results) {
            for (int i = 0; i < odw->dw->fact_count; i++) {
                filtered_results[i] = i;
            }
            filtered_count = odw->dw->fact_count;
        }
    }

    if (!filtered_results || filtered_count == 0) {
        return NULL;
    }

    // Aplicar filtro de tipo de desastre se especificado
    if (disaster_type && strlen(disaster_type) > 0) {
        int *disaster_filtered = malloc(filtered_count * sizeof(int));
        int disaster_count = 0;

        for (int i = 0; i < filtered_count; i++) {
            int fact_id = filtered_results[i];
            DisasterFact *fact = &odw->dw->fact_table[fact_id];

            for (int j = 0; j < odw->dw->disaster_type_count; j++) {
                if (odw->dw->dim_disaster_type[j].disaster_type_key == fact->disaster_type_key) {
                    if (strcmp(odw->dw->dim_disaster_type[j].disaster_type, disaster_type) == 0) {
                        disaster_filtered[disaster_count++] = fact_id;
                    }
                    break;
                }
            }
        }

        free(filtered_results);
        filtered_results = disaster_filtered;
        filtered_count = disaster_count;
    }

    if (filtered_count == 0) {
        free(filtered_results);
        return NULL;
    }

    // Aplicar ordenação se especificada
    int *sorted_results = NULL;
    int sorted_count = 0;

    switch (sort_type) {
        case INDEX_SORT_BY_AFFECTED:
            sorted_results = index_sort_facts_by_affected(odw->indexes, filtered_results, filtered_count, descending, &sorted_count);
            break;
        case INDEX_SORT_BY_DAMAGE:
            sorted_results = index_sort_facts_by_damage(odw->indexes, filtered_results, filtered_count, descending, &sorted_count);
            break;
        case INDEX_SORT_BY_DEATHS:
            sorted_results = index_sort_facts_by_deaths(odw->indexes, filtered_results, filtered_count, descending, &sorted_count);
            break;
        default:
            sorted_results = filtered_results;
            sorted_count = filtered_count;
            filtered_results = NULL; // Evitar double free
            break;
    }

    free(filtered_results);

    *result_count = sorted_count;
    return sorted_results;
}

// =============================================================================
// FUNÇÕES DE COMPARAÇÃO PARA QSORT
// =============================================================================

int compare_country_by_affected_desc(const void *a, const void *b) {
    CountrySortData *country_a = (CountrySortData *)a;
    CountrySortData *country_b = (CountrySortData *)b;
    if (country_b->total_affected > country_a->total_affected) return 1;
    if (country_b->total_affected < country_a->total_affected) return -1;
    return 0;
}

int compare_country_by_damage_desc(const void *a, const void *b) {
    CountrySortData *country_a = (CountrySortData *)a;
    CountrySortData *country_b = (CountrySortData *)b;
    if (country_b->total_damage > country_a->total_damage) return 1;
    if (country_b->total_damage < country_a->total_damage) return -1;
    return 0;
}

int compare_country_by_deaths_desc(const void *a, const void *b) {
    CountrySortData *country_a = (CountrySortData *)a;
    CountrySortData *country_b = (CountrySortData *)b;
    return country_b->total_deaths - country_a->total_deaths;
}

int compare_country_by_count_desc(const void *a, const void *b) {
    CountrySortData *country_a = (CountrySortData *)a;
    CountrySortData *country_b = (CountrySortData *)b;
    return country_b->disaster_count - country_a->disaster_count;
}

int compare_country_by_name_asc(const void *a, const void *b) {
    CountrySortData *country_a = (CountrySortData *)a;
    CountrySortData *country_b = (CountrySortData *)b;
    return strcmp(country_a->country, country_b->country);
}

int compare_fact_by_affected_desc(const void *a, const void *b) {
    FactSortData *fact_a = (FactSortData *)a;
    FactSortData *fact_b = (FactSortData *)b;
    if (fact_b->sort_value > fact_a->sort_value) return 1;
    if (fact_b->sort_value < fact_a->sort_value) return -1;
    return 0;
}

int compare_fact_by_damage_desc(const void *a, const void *b) {
    FactSortData *fact_a = (FactSortData *)a;
    FactSortData *fact_b = (FactSortData *)b;
    if (fact_b->sort_value > fact_a->sort_value) return 1;
    if (fact_b->sort_value < fact_a->sort_value) return -1;
    return 0;
}

int compare_fact_by_deaths_desc(const void *a, const void *b) {
    FactSortData *fact_a = (FactSortData *)a;
    FactSortData *fact_b = (FactSortData *)b;
    if (fact_b->sort_value > fact_a->sort_value) return 1;
    if (fact_b->sort_value < fact_a->sort_value) return -1;
    return 0;
}

int compare_fact_by_year_desc(const void *a, const void *b) {
    FactSortData *fact_a = (FactSortData *)a;
    FactSortData *fact_b = (FactSortData *)b;
    return fact_b->year - fact_a->year;
}
