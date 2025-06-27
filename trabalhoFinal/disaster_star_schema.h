// =============================================================================
// disaster_star_schema.h - Estruturas de dados para esquema estrela
// =============================================================================
#ifndef DISASTER_STAR_SCHEMA_H
#define DISASTER_STAR_SCHEMA_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// =============================================================================
// TABELA FATO (centro da estrela)
// =============================================================================
typedef struct {
    int fact_id;
    int time_key;               // FK para dimensão tempo
    int geography_key;          // FK para dimensão geografia
    int disaster_type_key;      // FK para dimensão tipo desastre
    int event_key;              // FK para dimensão evento

    // MÉTRICAS/FATOS
    int total_deaths;
    long long total_affected;
    long long total_damage;
} DisasterFact;

// =============================================================================
// DIMENSÕES
// =============================================================================

// Dimensão Tempo
typedef struct {
    int time_key;
    int start_year, start_month, start_day;
    int end_year, end_month, end_day;
    char start_date_str[20];
    char end_date_str[20];
} DimTime;

// Dimensão Geografia
typedef struct {
    int geography_key;
    char country[50];
    char subregion[50];
    char region[50];
} DimGeography;

// Dimensão Tipo de Desastre
typedef struct {
    int disaster_type_key;
    char disaster_group[50];
    char disaster_subgroup[50];
    char disaster_type[50];
    char disaster_subtype[50];
} DimDisasterType;

// Dimensão Evento
typedef struct {
    int event_key;
    char event_name[100];
    char origin[50];
    char associated_types[100];
} DimEvent;

// =============================================================================
// ESTRUTURAS DE ÍNDICES PARA CADA DIMENSÃO
// =============================================================================

// Índices para dimensão tempo
typedef struct {
    int time_key;
    int year;
    int month;
    int day;
} TimeIndex;

// Índices para dimensão geografia
typedef struct {
    int geography_key;
    char country[50];
    char region[50];
} GeographyIndex;

// Índices para dimensão tipo de desastre
typedef struct {
    int disaster_type_key;
    char disaster_type[50];
    char disaster_group[50];
} DisasterTypeIndex;

// Índices para dimensão evento
typedef struct {
    int event_key;
    char event_name[100];
} EventIndex;

// =============================================================================
// ESTRUTURA PRINCIPAL DO DATA WAREHOUSE
// =============================================================================
typedef struct {
    // Tabelas dimensão
    DimTime *dim_time;
    DimGeography *dim_geography;
    DimDisasterType *dim_disaster_type;
    DimEvent *dim_event;

    // Tabela fato
    DisasterFact *fact_table;

    // Contadores
    int time_count;
    int geography_count;
    int disaster_type_count;
    int event_count;
    int fact_count;

    // Capacidades dos arrays
    int time_capacity;
    int geography_capacity;
    int disaster_type_capacity;
    int event_capacity;
    int fact_capacity;

    // Próximas chaves primárias
    int next_time_key;
    int next_geography_key;
    int next_disaster_type_key;
    int next_event_key;
    int next_fact_id;

} DataWarehouse;

// =============================================================================
// FUNÇÕES PÚBLICAS
// =============================================================================

// Criação e destruição do data warehouse
DataWarehouse* dw_create();
void dw_destroy(DataWarehouse *dw);

// Funções para inserir dimensões
int dw_insert_time_dimension(DataWarehouse *dw, int start_year, int start_month, int start_day,
                           int end_year, int end_month, int end_day);
int dw_insert_geography_dimension(DataWarehouse *dw, const char *country,
                                const char *subregion, const char *region);
int dw_insert_disaster_type_dimension(DataWarehouse *dw, const char *disaster_group,
                                    const char *disaster_subgroup, const char *disaster_type,
                                    const char *disaster_subtype);
int dw_insert_event_dimension(DataWarehouse *dw, const char *event_name,
                            const char *origin, const char *associated_types);

// Função para inserir fato
int dw_insert_fact(DataWarehouse *dw, int time_key, int geography_key,
                  int disaster_type_key, int event_key, int total_deaths,
                  long long total_affected, long long total_damage);

// Funções de busca nas dimensões
int dw_find_time_key(DataWarehouse *dw, int year, int month, int day);
int dw_find_geography_key(DataWarehouse *dw, const char *country);
int dw_find_disaster_type_key(DataWarehouse *dw, const char *disaster_type);
int dw_find_event_key(DataWarehouse *dw, const char *event_name);

// Funções de consulta OLAP
void dw_query_by_year(DataWarehouse *dw, int year);
void dw_query_by_country(DataWarehouse *dw, const char *country);
void dw_query_by_disaster_type(DataWarehouse *dw, const char *disaster_type);
void dw_query_summary_by_year_country(DataWarehouse *dw, int year, const char *country);

// Funções de agregação
long long dw_total_damage_by_year(DataWarehouse *dw, int year);
long long dw_total_affected_by_country(DataWarehouse *dw, const char *country);
int dw_total_deaths_by_disaster_type(DataWarehouse *dw, const char *disaster_type);

// Funções de persistência
int dw_save_to_files(DataWarehouse *dw, const char *base_filename);
DataWarehouse* dw_load_from_files(const char *base_filename);

// Funções de depuração
void dw_print_statistics(DataWarehouse *dw);
void dw_print_sample_data(DataWarehouse *dw, int sample_size);

// Estrutura original para conversão
typedef struct {
    char disaster_group[50];
    char disaster_subgroup[50];
    char disaster_type[50];
    char disaster_subtype[50];
    char event_name[100];
    char country[50];
    char subregion[50];
    char region[50];
    char origin[50];
    char associated_types[100];
    int start_year;
    int start_month;
    int start_day;
    int end_year;
    int end_month;
    int end_day;
    int total_deaths;
    long long total_affected;
    long long total_damage;
} OriginalDisaster;

// Função para converter dados originais para esquema estrela
int dw_convert_from_original(DataWarehouse *dw, OriginalDisaster *original);

#endif
