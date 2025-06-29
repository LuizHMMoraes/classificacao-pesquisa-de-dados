// =============================================================================
// disaster.h - Estruturas de dados principais
// =============================================================================
#ifndef DISASTER_H
#define DISASTER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Estrutura para armazenar um registro de desastre
typedef struct {
    char disaster_group[50];
    char disaster_subgroup[50];
    char disaster_type[50];
    char disaster_subtype[50];
    char country[50];
    char subregion[50];
    char region[50];
    int start_year;
    int start_month;
    int start_day;
    int end_year;
    int end_month;
    int end_day;
    int total_deaths;
    long long total_affected;
    long long total_damage;
} Disaster;

// Estrutura para índice (chave + posição no arquivo)
typedef struct {
    int key;           // Chave de busca (ano, danos, etc.)
    long file_pos;     // Posição no arquivo binário
} IndexEntry;

#endif
