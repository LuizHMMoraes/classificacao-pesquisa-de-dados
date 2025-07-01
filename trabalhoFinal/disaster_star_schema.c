// =============================================================================
// disaster_star_schema.c - Implementação das funções do esquema estrela
// =============================================================================

#include "disaster_star_schema.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// =============================================================================
// FUNÇÕES DE CRIAÇÃO E DESTRUIÇÃO
// =============================================================================

DataWarehouse* dw_create() {
    DataWarehouse *dw = malloc(sizeof(DataWarehouse));
    if (!dw) return NULL;

    // Inicializa capacidades
    dw->time_capacity = 50000;
    dw->geography_capacity = 500;
    dw->disaster_type_capacity = 100;
    dw->fact_capacity = 50000;

    // Aloca memória para as tabelas
    dw->dim_time = malloc(dw->time_capacity * sizeof(DimTime));
    dw->dim_geography = malloc(dw->geography_capacity * sizeof(DimGeography));
    dw->dim_disaster_type = malloc(dw->disaster_type_capacity * sizeof(DimDisasterType));
    dw->fact_table = malloc(dw->fact_capacity * sizeof(DisasterFact));

    if (!dw->dim_time || !dw->dim_geography || !dw->dim_disaster_type ||
        !dw->fact_table) {
        dw_destroy(dw);
        return NULL;
    }

    // Inicializa contadores
    dw->time_count = 0;
    dw->geography_count = 0;
    dw->disaster_type_count = 0;
    dw->fact_count = 0;

    // Inicializa próximas chaves
    dw->next_time_key = 1;
    dw->next_geography_key = 1;
    dw->next_disaster_type_key = 1;
    dw->next_fact_id = 1;

    return dw;
}

void dw_destroy(DataWarehouse *dw) {
    if (!dw) return;

    free(dw->dim_time);
    free(dw->dim_geography);
    free(dw->dim_disaster_type);
    free(dw->fact_table);
    free(dw);
}

// =============================================================================
// FUNÇÕES DE INSERÇÃO NAS DIMENSÕES
// =============================================================================

int dw_insert_time_dimension(DataWarehouse *dw, int start_year, int start_month, int start_day,
                            int end_year, int end_month, int end_day) {
    if (!dw || dw->time_count >= dw->time_capacity) return -1;

    DimTime *time_dim = &dw->dim_time[dw->time_count];
    time_dim->time_key = dw->next_time_key++;
    time_dim->start_year = start_year;
    time_dim->start_month = start_month;
    time_dim->start_day = start_day;
    time_dim->end_year = end_year;
    time_dim->end_month = end_month;
    time_dim->end_day = end_day;

    snprintf(time_dim->start_date_str, sizeof(time_dim->start_date_str),
             "%04d-%02d-%02d", start_year, start_month, start_day);
    snprintf(time_dim->end_date_str, sizeof(time_dim->end_date_str),
             "%04d-%02d-%02d", end_year, end_month, end_day);

    dw->time_count++;
    return time_dim->time_key;
}

int dw_insert_geography_dimension(DataWarehouse *dw, const char *country,
                                 const char *subregion, const char *region) {
    if (!dw || dw->geography_count >= dw->geography_capacity) return -1;

    DimGeography *geo_dim = &dw->dim_geography[dw->geography_count];
    geo_dim->geography_key = dw->next_geography_key++;
    strncpy(geo_dim->country, country ? country : "", sizeof(geo_dim->country) - 1);
    strncpy(geo_dim->subregion, subregion ? subregion : "", sizeof(geo_dim->subregion) - 1);
    strncpy(geo_dim->region, region ? region : "", sizeof(geo_dim->region) - 1);

    dw->geography_count++;
    return geo_dim->geography_key;
}

int dw_insert_disaster_type_dimension(DataWarehouse *dw, const char *disaster_group,
                                     const char *disaster_subgroup, const char *disaster_type,
                                     const char *disaster_subtype) {
    if (!dw || dw->disaster_type_count >= dw->disaster_type_capacity) return -1;

    DimDisasterType *type_dim = &dw->dim_disaster_type[dw->disaster_type_count];
    type_dim->disaster_type_key = dw->next_disaster_type_key++;
    strncpy(type_dim->disaster_group, disaster_group ? disaster_group : "", sizeof(type_dim->disaster_group) - 1);
    strncpy(type_dim->disaster_subgroup, disaster_subgroup ? disaster_subgroup : "", sizeof(type_dim->disaster_subgroup) - 1);
    strncpy(type_dim->disaster_type, disaster_type ? disaster_type : "", sizeof(type_dim->disaster_type) - 1);
    strncpy(type_dim->disaster_subtype, disaster_subtype ? disaster_subtype : "", sizeof(type_dim->disaster_subtype) - 1);

    dw->disaster_type_count++;
    return type_dim->disaster_type_key;
}

// =============================================================================
// FUNÇÃO DE INSERÇÃO DE FATOS
// =============================================================================

int dw_insert_fact(DataWarehouse *dw, int time_key, int geography_key,
                   int disaster_type_key, int total_deaths,
                   long long total_affected, long long total_damage) {
    if (!dw || dw->fact_count >= dw->fact_capacity) return -1;

    DisasterFact *fact = &dw->fact_table[dw->fact_count];
    fact->fact_id = dw->next_fact_id++;
    fact->time_key = time_key;
    fact->geography_key = geography_key;
    fact->disaster_type_key = disaster_type_key;
    fact->total_deaths = total_deaths;
    fact->total_affected = total_affected;
    fact->total_damage = total_damage;

    dw->fact_count++;
    return fact->fact_id;
}

// =============================================================================
// FUNÇÕES DE BUSCA NAS DIMENSÕES
// =============================================================================

int dw_find_time_key(DataWarehouse *dw, int year, int month, int day) {
    if (!dw) return -1;

    for (int i = 0; i < dw->time_count; i++) {
        if (dw->dim_time[i].start_year == year &&
            dw->dim_time[i].start_month == month &&
            dw->dim_time[i].start_day == day) {
            return dw->dim_time[i].time_key;
        }
    }
    return -1;
}

int dw_find_geography_key(DataWarehouse *dw, const char *country) {
    if (!dw || !country) return -1;

    for (int i = 0; i < dw->geography_count; i++) {
        if (strcmp(dw->dim_geography[i].country, country) == 0) {
            return dw->dim_geography[i].geography_key;
        }
    }
    return -1;
}

int dw_find_disaster_type_key(DataWarehouse *dw, const char *disaster_type) {
    if (!dw || !disaster_type) return -1;

    for (int i = 0; i < dw->disaster_type_count; i++) {
        if (strcmp(dw->dim_disaster_type[i].disaster_type, disaster_type) == 0) {
            return dw->dim_disaster_type[i].disaster_type_key;
        }
    }
    return -1;
}

// =============================================================================
// FUNÇÕES DE CONSULTA OLAP
// =============================================================================

void dw_query_by_year(DataWarehouse *dw, int year) {
    if (!dw) return;

    printf("Consultando desastres para o ano %d...\n", year);

    int count = 0;
    int total_deaths = 0;
    long long total_affected = 0;
    long long total_damage = 0;

    for (int i = 0; i < dw->fact_count; i++) {
        DisasterFact *fact = &dw->fact_table[i];

        // Busca dimensão tempo
        for (int j = 0; j < dw->time_count; j++) {
            if (dw->dim_time[j].time_key == fact->time_key &&
                dw->dim_time[j].start_year == year) {
                count++;
                total_deaths += fact->total_deaths;
                total_affected += fact->total_affected;
                total_damage += fact->total_damage;
                break;
            }
        }
    }

    printf("Encontrados %d desastres em %d\n", count, year);
    printf("Total de mortes: %d\n", total_deaths);
    printf("Total de afetados: %lld\n", total_affected);
    printf("Total de danos: %lld mil US$\n", total_damage);
}

void dw_query_by_country(DataWarehouse *dw, const char *country) {
    if (!dw || !country) return;

    printf("Consultando desastres para %s...\n", country);

    int count = 0;
    int total_deaths = 0;
    long long total_affected = 0;
    long long total_damage = 0;

    for (int i = 0; i < dw->fact_count; i++) {
        DisasterFact *fact = &dw->fact_table[i];

        // Busca dimensão geografia
        for (int j = 0; j < dw->geography_count; j++) {
            if (dw->dim_geography[j].geography_key == fact->geography_key &&
                strcmp(dw->dim_geography[j].country, country) == 0) {
                count++;
                total_deaths += fact->total_deaths;
                total_affected += fact->total_affected;
                total_damage += fact->total_damage;
                break;
            }
        }
    }

    printf("Encontrados %d desastres em %s\n", count, country);
    printf("Total de mortes: %d\n", total_deaths);
    printf("Total de afetados: %lld\n", total_affected);
    printf("Total de danos: %lld mil US$\n", total_damage);
}

void dw_query_by_disaster_type(DataWarehouse *dw, const char *disaster_type) {
    if (!dw || !disaster_type) return;

    printf("Consultando desastres do tipo %s...\n", disaster_type);

    int count = 0;
    int total_deaths = 0;
    long long total_affected = 0;
    long long total_damage = 0;

    for (int i = 0; i < dw->fact_count; i++) {
        DisasterFact *fact = &dw->fact_table[i];

        // Busca dimensão tipo de desastre
        for (int j = 0; j < dw->disaster_type_count; j++) {
            if (dw->dim_disaster_type[j].disaster_type_key == fact->disaster_type_key &&
                strcmp(dw->dim_disaster_type[j].disaster_type, disaster_type) == 0) {
                count++;
                total_deaths += fact->total_deaths;
                total_affected += fact->total_affected;
                total_damage += fact->total_damage;
                break;
            }
        }
    }

    printf("Encontrados %d desastres do tipo %s\n", count, disaster_type);
    printf("Total de mortes: %d\n", total_deaths);
    printf("Total de afetados: %lld\n", total_affected);
    printf("Total de danos: %lld mil US$\n", total_damage);
}

void dw_query_summary_by_year_country(DataWarehouse *dw, int year, const char *country) {
    if (!dw || !country) return;

    printf("Consultando desastres em %s durante %d...\n", country, year);

    int count = 0;
    int total_deaths = 0;
    long long total_affected = 0;
    long long total_damage = 0;

    for (int i = 0; i < dw->fact_count; i++) {
        DisasterFact *fact = &dw->fact_table[i];

        // Busca dimensão tempo
        DimTime *time_dim = NULL;
        for (int j = 0; j < dw->time_count; j++) {
            if (dw->dim_time[j].time_key == fact->time_key) {
                time_dim = &dw->dim_time[j];
                break;
            }
        }

        // Busca dimensão geografia
        DimGeography *geo_dim = NULL;
        for (int j = 0; j < dw->geography_count; j++) {
            if (dw->dim_geography[j].geography_key == fact->geography_key) {
                geo_dim = &dw->dim_geography[j];
                break;
            }
        }

        if (time_dim && geo_dim &&
            time_dim->start_year == year &&
            strcmp(geo_dim->country, country) == 0) {
            count++;
            total_deaths += fact->total_deaths;
            total_affected += fact->total_affected;
            total_damage += fact->total_damage;
        }
    }

    printf("Encontrados %d desastres em %s durante %d\n", count, country, year);
    printf("Total de mortes: %d\n", total_deaths);
    printf("Total de afetados: %lld\n", total_affected);
    printf("Total de danos: %lld mil US$\n", total_damage);
}

// =============================================================================
// FUNÇÕES DE AGREGAÇÃO
// =============================================================================

long long dw_total_damage_by_year(DataWarehouse *dw, int year) {
    if (!dw) return 0;

    long long total_damage = 0;

    for (int i = 0; i < dw->fact_count; i++) {
        DisasterFact *fact = &dw->fact_table[i];

        // Busca dimensão tempo
        for (int j = 0; j < dw->time_count; j++) {
            if (dw->dim_time[j].time_key == fact->time_key &&
                dw->dim_time[j].start_year == year) {
                total_damage += fact->total_damage;
                break;
            }
        }
    }

    return total_damage;
}

long long dw_total_affected_by_country(DataWarehouse *dw, const char *country) {
    if (!dw || !country) return 0;

    long long total_affected = 0;

    for (int i = 0; i < dw->fact_count; i++) {
        DisasterFact *fact = &dw->fact_table[i];

        // Busca dimensão geografia
        for (int j = 0; j < dw->geography_count; j++) {
            if (dw->dim_geography[j].geography_key == fact->geography_key &&
                strcmp(dw->dim_geography[j].country, country) == 0) {
                total_affected += fact->total_affected;
                break;
            }
        }
    }

    return total_affected;
}

int dw_total_deaths_by_disaster_type(DataWarehouse *dw, const char *disaster_type) {
    if (!dw || !disaster_type) return 0;

    int total_deaths = 0;

    for (int i = 0; i < dw->fact_count; i++) {
        DisasterFact *fact = &dw->fact_table[i];

        // Busca dimensão tipo de desastre
        for (int j = 0; j < dw->disaster_type_count; j++) {
            if (dw->dim_disaster_type[j].disaster_type_key == fact->disaster_type_key &&
                strcmp(dw->dim_disaster_type[j].disaster_type, disaster_type) == 0) {
                total_deaths += fact->total_deaths;
                break;
            }
        }
    }

    return total_deaths;
}

// =============================================================================
// FUNÇÕES DE PERSISTÊNCIA
// =============================================================================

int dw_save_to_files(DataWarehouse *dw, const char *base_filename) {
    if (!dw || !base_filename) return 0;

    char filename[256];
    FILE *file;

    // Salva dimensão tempo
    snprintf(filename, sizeof(filename), "%s_time.dat", base_filename);
    file = fopen(filename, "wb");
    if (!file) return 0;
    fwrite(&dw->time_count, sizeof(int), 1, file);
    fwrite(dw->dim_time, sizeof(DimTime), dw->time_count, file);
    fclose(file);

    // Salva dimensão geografia
    snprintf(filename, sizeof(filename), "%s_geography.dat", base_filename);
    file = fopen(filename, "wb");
    if (!file) return 0;
    fwrite(&dw->geography_count, sizeof(int), 1, file);
    fwrite(dw->dim_geography, sizeof(DimGeography), dw->geography_count, file);
    fclose(file);

    // Salva dimensão tipo de desastre
    snprintf(filename, sizeof(filename), "%s_disaster_type.dat", base_filename);
    file = fopen(filename, "wb");
    if (!file) return 0;
    fwrite(&dw->disaster_type_count, sizeof(int), 1, file);
    fwrite(dw->dim_disaster_type, sizeof(DimDisasterType), dw->disaster_type_count, file);
    fclose(file);

    // Salva tabela fato
    snprintf(filename, sizeof(filename), "%s_fact.dat", base_filename);
    file = fopen(filename, "wb");
    if (!file) return 0;
    fwrite(&dw->fact_count, sizeof(int), 1, file);
    fwrite(dw->fact_table, sizeof(DisasterFact), dw->fact_count, file);
    fclose(file);

    printf("Data warehouse salvo com sucesso!\n");
    return 1;
}

DataWarehouse* dw_load_from_files(const char *base_filename) {
    if (!base_filename) return NULL;

    DataWarehouse *dw = dw_create();
    if (!dw) return NULL;

    char filename[256];
    FILE *file;

    // Carrega dimensão tempo
    snprintf(filename, sizeof(filename), "%s_time.dat", base_filename);
    file = fopen(filename, "rb");
    if (!file) {
        dw_destroy(dw);
        return NULL;
    }
    fread(&dw->time_count, sizeof(int), 1, file);
    fread(dw->dim_time, sizeof(DimTime), dw->time_count, file);
    fclose(file);

    // Carrega dimensão geografia
    snprintf(filename, sizeof(filename), "%s_geography.dat", base_filename);
    file = fopen(filename, "rb");
    if (!file) {
        dw_destroy(dw);
        return NULL;
    }
    fread(&dw->geography_count, sizeof(int), 1, file);
    fread(dw->dim_geography, sizeof(DimGeography), dw->geography_count, file);
    fclose(file);

    // Carrega dimensão tipo de desastre
    snprintf(filename, sizeof(filename), "%s_disaster_type.dat", base_filename);
    file = fopen(filename, "rb");
    if (!file) {
        dw_destroy(dw);
        return NULL;
    }
    fread(&dw->disaster_type_count, sizeof(int), 1, file);
    fread(dw->dim_disaster_type, sizeof(DimDisasterType), dw->disaster_type_count, file);
    fclose(file);

    // Carrega tabela fato
    snprintf(filename, sizeof(filename), "%s_fact.dat", base_filename);
    file = fopen(filename, "rb");
    if (!file) {
        dw_destroy(dw);
        return NULL;
    }
    fread(&dw->fact_count, sizeof(int), 1, file);
    fread(dw->fact_table, sizeof(DisasterFact), dw->fact_count, file);
    fclose(file);

    return dw;
}

// =============================================================================
// FUNÇÕES DE DEPURAÇÃO
// =============================================================================

void dw_print_statistics(DataWarehouse *dw) {
    if (!dw) return;

    printf("=== ESTATÍSTICAS DO DATA WAREHOUSE ===\n");
    printf("Dimensões:\n");
    printf("  - Tempo: %d registros\n", dw->time_count);
    printf("  - Geografia: %d registros\n", dw->geography_count);
    printf("  - Tipo de Desastre: %d registros\n", dw->disaster_type_count);
    printf("Fatos: %d registros\n", dw->fact_count);
    printf("========================================\n");
}

void dw_print_sample_data(DataWarehouse *dw, int sample_size) {
    if (!dw) return;

    printf("=== AMOSTRA DOS DADOS ===\n");
    int limit = (sample_size < dw->fact_count) ? sample_size : dw->fact_count;

    for (int i = 0; i < limit; i++) {
        DisasterFact *fact = &dw->fact_table[i];
        printf("Fato %d: Mortes=%d, Afetados=%lld, Danos=%lld\n",
               fact->fact_id, fact->total_deaths, fact->total_affected, fact->total_damage);
    }
    printf("=========================\n");
}

// =============================================================================
// FUNÇÃO DE CONVERSÃO DE DADOS ORIGINAIS
// =============================================================================

// Se não existir, adicione esta implementação:
int dw_convert_from_original(DataWarehouse *dw, OriginalDisaster *original) {
    if (!dw || !original) return 0;

    // 1. Inserir/Encontrar dimensão tempo
    int time_key = dw_find_time_key(dw, original->start_year, original->start_month, original->start_day);
    if (time_key == -1) {
        time_key = dw_insert_time_dimension(dw,
            original->start_year, original->start_month, original->start_day,
            original->end_year, original->end_month, original->end_day);
        if (time_key == -1) return 0;
    }

    // 2. Inserir/Encontrar dimensão geografia
    int geography_key = dw_find_geography_key(dw, original->country);
    if (geography_key == -1) {
        geography_key = dw_insert_geography_dimension(dw,
            original->country, original->subregion, original->region);
        if (geography_key == -1) return 0;
    }

    // 3. Inserir/Encontrar dimensão tipo de desastre
    int disaster_type_key = dw_find_disaster_type_key(dw, original->disaster_type);
    if (disaster_type_key == -1) {
        disaster_type_key = dw_insert_disaster_type_dimension(dw,
            original->disaster_group, original->disaster_subgroup,
            original->disaster_type, original->disaster_subtype);
        if (disaster_type_key == -1) return 0;
    }

    // 4. Inserir fato
    int fact_id = dw_insert_fact(dw, time_key, geography_key, disaster_type_key,
        original->total_deaths, original->total_affected, original->total_damage);

    return (fact_id != -1) ? 1 : 0;
}
