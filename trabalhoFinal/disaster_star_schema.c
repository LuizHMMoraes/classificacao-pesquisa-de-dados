// =============================================================================
// disaster_star_schema.c - Implementação do Data Warehouse com esquema estrela
// =============================================================================

#include "disaster_star_schema.h"

#define INITIAL_CAPACITY 1000
#define GROWTH_FACTOR 2

// =============================================================================
// FUNÇÕES AUXILIARES PRIVADAS
// =============================================================================

// Função para expandir array de dimensão tempo
static int expand_time_dimension(DataWarehouse *dw) {
    int new_capacity = dw->time_capacity * GROWTH_FACTOR;
    DimTime *new_array = (DimTime*)realloc(dw->dim_time, new_capacity * sizeof(DimTime));
    if (!new_array) return 0;

    dw->dim_time = new_array;
    dw->time_capacity = new_capacity;
    return 1;
}

// Função para expandir array de dimensão geografia
static int expand_geography_dimension(DataWarehouse *dw) {
    int new_capacity = dw->geography_capacity * GROWTH_FACTOR;
    DimGeography *new_array = (DimGeography*)realloc(dw->dim_geography, new_capacity * sizeof(DimGeography));
    if (!new_array) return 0;

    dw->dim_geography = new_array;
    dw->geography_capacity = new_capacity;
    return 1;
}

// Função para expandir array de dimensão tipo de desastre
static int expand_disaster_type_dimension(DataWarehouse *dw) {
    int new_capacity = dw->disaster_type_capacity * GROWTH_FACTOR;
    DimDisasterType *new_array = (DimDisasterType*)realloc(dw->dim_disaster_type, new_capacity * sizeof(DimDisasterType));
    if (!new_array) return 0;

    dw->dim_disaster_type = new_array;
    dw->disaster_type_capacity = new_capacity;
    return 1;
}

// Função para expandir array de dimensão evento
static int expand_event_dimension(DataWarehouse *dw) {
    int new_capacity = dw->event_capacity * GROWTH_FACTOR;
    DimEvent *new_array = (DimEvent*)realloc(dw->dim_event, new_capacity * sizeof(DimEvent));
    if (!new_array) return 0;

    dw->dim_event = new_array;
    dw->event_capacity = new_capacity;
    return 1;
}

// Função para expandir tabela fato
static int expand_fact_table(DataWarehouse *dw) {
    int new_capacity = dw->fact_capacity * GROWTH_FACTOR;
    DisasterFact *new_array = (DisasterFact*)realloc(dw->fact_table, new_capacity * sizeof(DisasterFact));
    if (!new_array) return 0;

    dw->fact_table = new_array;
    dw->fact_capacity = new_capacity;
    return 1;
}

// Função para formatar data como string
static void format_date_string(int year, int month, int day, char *date_str) {
    snprintf(date_str, 20, "%04d-%02d-%02d", year, month, day);
}

// =============================================================================
// FUNÇÕES PÚBLICAS
// =============================================================================

// Criar data warehouse
DataWarehouse* dw_create() {
    DataWarehouse *dw = (DataWarehouse*)malloc(sizeof(DataWarehouse));
    if (!dw) return NULL;

    // Inicializar arrays das dimensões
    dw->dim_time = (DimTime*)malloc(INITIAL_CAPACITY * sizeof(DimTime));
    dw->dim_geography = (DimGeography*)malloc(INITIAL_CAPACITY * sizeof(DimGeography));
    dw->dim_disaster_type = (DimDisasterType*)malloc(INITIAL_CAPACITY * sizeof(DimDisasterType));
    dw->dim_event = (DimEvent*)malloc(INITIAL_CAPACITY * sizeof(DimEvent));
    dw->fact_table = (DisasterFact*)malloc(INITIAL_CAPACITY * sizeof(DisasterFact));

    if (!dw->dim_time || !dw->dim_geography || !dw->dim_disaster_type ||
        !dw->dim_event || !dw->fact_table) {
        dw_destroy(dw);
        return NULL;
    }

    // Inicializar contadores e capacidades
    dw->time_count = 0;
    dw->geography_count = 0;
    dw->disaster_type_count = 0;
    dw->event_count = 0;
    dw->fact_count = 0;

    dw->time_capacity = INITIAL_CAPACITY;
    dw->geography_capacity = INITIAL_CAPACITY;
    dw->disaster_type_capacity = INITIAL_CAPACITY;
    dw->event_capacity = INITIAL_CAPACITY;
    dw->fact_capacity = INITIAL_CAPACITY;

    // Inicializar próximas chaves
    dw->next_time_key = 1;
    dw->next_geography_key = 1;
    dw->next_disaster_type_key = 1;
    dw->next_event_key = 1;
    dw->next_fact_id = 1;

    return dw;
}

// Destruir data warehouse
void dw_destroy(DataWarehouse *dw) {
    if (!dw) return;

    free(dw->dim_time);
    free(dw->dim_geography);
    free(dw->dim_disaster_type);
    free(dw->dim_event);
    free(dw->fact_table);
    free(dw);
}

// Inserir dimensão tempo
int dw_insert_time_dimension(DataWarehouse *dw, int start_year, int start_month, int start_day,
                           int end_year, int end_month, int end_day) {
    if (!dw) return -1;

    // Verificar se precisa expandir array
    if (dw->time_count >= dw->time_capacity) {
        if (!expand_time_dimension(dw)) return -1;
    }

    // Inserir nova dimensão tempo
    DimTime *dim = &dw->dim_time[dw->time_count];
    dim->time_key = dw->next_time_key++;
    dim->start_year = start_year;
    dim->start_month = start_month;
    dim->start_day = start_day;
    dim->end_year = end_year;
    dim->end_month = end_month;
    dim->end_day = end_day;

    format_date_string(start_year, start_month, start_day, dim->start_date_str);
    format_date_string(end_year, end_month, end_day, dim->end_date_str);

    dw->time_count++;
    return dim->time_key;
}

// Inserir dimensão geografia
int dw_insert_geography_dimension(DataWarehouse *dw, const char *country,
                                const char *subregion, const char *region) {
    if (!dw || !country) return -1;

    // Verificar se precisa expandir array
    if (dw->geography_count >= dw->geography_capacity) {
        if (!expand_geography_dimension(dw)) return -1;
    }

    // Inserir nova dimensão geografia
    DimGeography *dim = &dw->dim_geography[dw->geography_count];
    dim->geography_key = dw->next_geography_key++;
    strncpy(dim->country, country, 49);
    dim->country[49] = '\0';
    strncpy(dim->subregion, subregion ? subregion : "", 49);
    dim->subregion[49] = '\0';
    strncpy(dim->region, region ? region : "", 49);
    dim->region[49] = '\0';

    dw->geography_count++;
    return dim->geography_key;
}

// Inserir dimensão tipo de desastre
int dw_insert_disaster_type_dimension(DataWarehouse *dw, const char *disaster_group,
                                    const char *disaster_subgroup, const char *disaster_type,
                                    const char *disaster_subtype) {
    if (!dw || !disaster_type) return -1;

    // Verificar se precisa expandir array
    if (dw->disaster_type_count >= dw->disaster_type_capacity) {
        if (!expand_disaster_type_dimension(dw)) return -1;
    }

    // Inserir nova dimensão tipo de desastre
    DimDisasterType *dim = &dw->dim_disaster_type[dw->disaster_type_count];
    dim->disaster_type_key = dw->next_disaster_type_key++;
    strncpy(dim->disaster_group, disaster_group ? disaster_group : "", 49);
    dim->disaster_group[49] = '\0';
    strncpy(dim->disaster_subgroup, disaster_subgroup ? disaster_subgroup : "", 49);
    dim->disaster_subgroup[49] = '\0';
    strncpy(dim->disaster_type, disaster_type, 49);
    dim->disaster_type[49] = '\0';
    strncpy(dim->disaster_subtype, disaster_subtype ? disaster_subtype : "", 49);
    dim->disaster_subtype[49] = '\0';

    dw->disaster_type_count++;
    return dim->disaster_type_key;
}

// Inserir dimensão evento
int dw_insert_event_dimension(DataWarehouse *dw, const char *event_name,
                            const char *origin, const char *associated_types) {
    if (!dw) return -1;

    // Verificar se precisa expandir array
    if (dw->event_count >= dw->event_capacity) {
        if (!expand_event_dimension(dw)) return -1;
    }

    // Inserir nova dimensão evento
    DimEvent *dim = &dw->dim_event[dw->event_count];
    dim->event_key = dw->next_event_key++;
    strncpy(dim->event_name, event_name ? event_name : "", 99);
    dim->event_name[99] = '\0';
    strncpy(dim->origin, origin ? origin : "", 49);
    dim->origin[49] = '\0';
    strncpy(dim->associated_types, associated_types ? associated_types : "", 99);
    dim->associated_types[99] = '\0';

    dw->event_count++;
    return dim->event_key;
}

// Inserir fato
int dw_insert_fact(DataWarehouse *dw, int time_key, int geography_key,
                  int disaster_type_key, int event_key, int total_deaths,
                  long long total_affected, long long total_damage) {
    if (!dw) return -1;

    // Verificar se precisa expandir tabela fato
    if (dw->fact_count >= dw->fact_capacity) {
        if (!expand_fact_table(dw)) return -1;
    }

    // Inserir novo fato
    DisasterFact *fact = &dw->fact_table[dw->fact_count];
    fact->fact_id = dw->next_fact_id++;
    fact->time_key = time_key;
    fact->geography_key = geography_key;
    fact->disaster_type_key = disaster_type_key;
    fact->event_key = event_key;
    fact->total_deaths = total_deaths;
    fact->total_affected = total_affected;
    fact->total_damage = total_damage;

    dw->fact_count++;
    return fact->fact_id;
}

// Buscar chave de tempo
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

// Buscar chave de geografia
int dw_find_geography_key(DataWarehouse *dw, const char *country) {
    if (!dw || !country) return -1;

    for (int i = 0; i < dw->geography_count; i++) {
        if (strcmp(dw->dim_geography[i].country, country) == 0) {
            return dw->dim_geography[i].geography_key;
        }
    }
    return -1;
}

// Buscar chave de tipo de desastre
int dw_find_disaster_type_key(DataWarehouse *dw, const char *disaster_type) {
    if (!dw || !disaster_type) return -1;

    for (int i = 0; i < dw->disaster_type_count; i++) {
        if (strcmp(dw->dim_disaster_type[i].disaster_type, disaster_type) == 0) {
            return dw->dim_disaster_type[i].disaster_type_key;
        }
    }
    return -1;
}

// Buscar chave de evento
int dw_find_event_key(DataWarehouse *dw, const char *event_name) {
    if (!dw || !event_name) return -1;

    for (int i = 0; i < dw->event_count; i++) {
        if (strcmp(dw->dim_event[i].event_name, event_name) == 0) {
            return dw->dim_event[i].event_key;
        }
    }
    return -1;
}

// Converter dados originais para esquema estrela
int dw_convert_from_original(DataWarehouse *dw, OriginalDisaster *original) {
    if (!dw || !original) return 0;

    // Buscar ou criar dimensão tempo
    int time_key = dw_find_time_key(dw, original->start_year, original->start_month, original->start_day);
    if (time_key == -1) {
        time_key = dw_insert_time_dimension(dw, original->start_year, original->start_month, original->start_day,
                                          original->end_year, original->end_month, original->end_day);
    }

    // Buscar ou criar dimensão geografia
    int geography_key = dw_find_geography_key(dw, original->country);
    if (geography_key == -1) {
        geography_key = dw_insert_geography_dimension(dw, original->country, original->subregion, original->region);
    }

    // Buscar ou criar dimensão tipo de desastre
    int disaster_type_key = dw_find_disaster_type_key(dw, original->disaster_type);
    if (disaster_type_key == -1) {
        disaster_type_key = dw_insert_disaster_type_dimension(dw, original->disaster_group,
                                                            original->disaster_subgroup, original->disaster_type,
                                                            original->disaster_subtype);
    }

    // Buscar ou criar dimensão evento
    int event_key = dw_find_event_key(dw, original->event_name);
    if (event_key == -1) {
        event_key = dw_insert_event_dimension(dw, original->event_name, original->origin, original->associated_types);
    }

    // Inserir fato
    int fact_id = dw_insert_fact(dw, time_key, geography_key, disaster_type_key, event_key,
                                original->total_deaths, original->total_affected, original->total_damage);

    return fact_id > 0 ? 1 : 0;
}

// Consulta por ano
void dw_query_by_year(DataWarehouse *dw, int year) {
    if (!dw) return;

    printf("\n=== CONSULTA POR ANO: %d ===\n", year);

    int count = 0;
    long long total_damage = 0;
    long long total_affected = 0;
    int total_deaths = 0;

    for (int i = 0; i < dw->fact_count; i++) {
        DisasterFact *fact = &dw->fact_table[i];

        // Buscar dimensão tempo correspondente
        for (int j = 0; j < dw->time_count; j++) {
            if (dw->dim_time[j].time_key == fact->time_key && dw->dim_time[j].start_year == year) {
                count++;
                total_damage += fact->total_damage;
                total_affected += fact->total_affected;
                total_deaths += fact->total_deaths;
                break;
            }
        }
    }

    printf("Total de desastres: %d\n", count);
    printf("Total de mortes: %d\n", total_deaths);
    printf("Total de afetados: %lld\n", total_affected);
    printf("Danos totais: %lld milhares de US$\n", total_damage);
}

// Consulta por país
void dw_query_by_country(DataWarehouse *dw, const char *country) {
    if (!dw || !country) return;

    printf("\n=== CONSULTA POR PAÍS: %s ===\n", country);

    int geography_key = dw_find_geography_key(dw, country);
    if (geography_key == -1) {
        printf("País não encontrado\n");
        return;
    }

    int count = 0;
    long long total_damage = 0;
    long long total_affected = 0;
    int total_deaths = 0;

    for (int i = 0; i < dw->fact_count; i++) {
        DisasterFact *fact = &dw->fact_table[i];
        if (fact->geography_key == geography_key) {
            count++;
            total_damage += fact->total_damage;
            total_affected += fact->total_affected;
            total_deaths += fact->total_deaths;
        }
    }

    printf("Total de desastres: %d\n", count);
    printf("Total de mortes: %d\n", total_deaths);
    printf("Total de afetados: %lld\n", total_affected);
    printf("Danos totais: %lld milhares de US$\n", total_damage);
}

// Imprimir estatísticas
void dw_print_statistics(DataWarehouse *dw) {
    if (!dw) return;

    printf("\n=== ESTATÍSTICAS DO DATA WAREHOUSE ===\n");
    printf("Dimensão Tempo: %d registros\n", dw->time_count);
    printf("Dimensão Geografia: %d registros\n", dw->geography_count);
    printf("Dimensão Tipo de Desastre: %d registros\n", dw->disaster_type_count);
    printf("Dimensão Evento: %d registros\n", dw->event_count);
    printf("Tabela Fato: %d registros\n", dw->fact_count);

    // Calcular totais
    long long total_damage = 0;
    long long total_affected = 0;
    int total_deaths = 0;

    for (int i = 0; i < dw->fact_count; i++) {
        total_damage += dw->fact_table[i].total_damage;
        total_affected += dw->fact_table[i].total_affected;
        total_deaths += dw->fact_table[i].total_deaths;
    }

    printf("\nTOTAIS GERAIS:\n");
    printf("Total de mortes: %d\n", total_deaths);
    printf("Total de afetados: %lld\n", total_affected);
    printf("Danos totais: %lld milhares de US$\n", total_damage);
}

// Imprimir dados de amostra
void dw_print_sample_data(DataWarehouse *dw, int sample_size) {
    if (!dw) return;

    printf("\n=== AMOSTRA DE DADOS ===\n");

    int limit = (sample_size < dw->fact_count) ? sample_size : dw->fact_count;

    for (int i = 0; i < limit; i++) {
        DisasterFact *fact = &dw->fact_table[i];

        printf("\n--- Fato %d ---\n", fact->fact_id);

        // Buscar e exibir dimensões relacionadas
        for (int j = 0; j < dw->time_count; j++) {
            if (dw->dim_time[j].time_key == fact->time_key) {
                printf("Data: %s a %s\n", dw->dim_time[j].start_date_str, dw->dim_time[j].end_date_str);
                break;
            }
        }

        for (int j = 0; j < dw->geography_count; j++) {
            if (dw->dim_geography[j].geography_key == fact->geography_key) {
                printf("País: %s, Região: %s\n", dw->dim_geography[j].country, dw->dim_geography[j].region);
                break;
            }
        }

        for (int j = 0; j < dw->disaster_type_count; j++) {
            if (dw->dim_disaster_type[j].disaster_type_key == fact->disaster_type_key) {
                printf("Tipo: %s, Grupo: %s\n", dw->dim_disaster_type[j].disaster_type, dw->dim_disaster_type[j].disaster_group);
                break;
            }
        }

        printf("Mortes: %d, Afetados: %lld, Danos: %lld\n",
               fact->total_deaths, fact->total_affected, fact->total_damage);
    }
}
