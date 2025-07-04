#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>
#include <ctype.h>

//Headers específicos do projeto
#include "disaster.h"
#include "disaster_star_schema.h"
#include "bplus.h"
#include "trie.h"
#include "star_schema_indexes.h"

#define SCREEN_WIDTH 1600
#define SCREEN_HEIGHT 1000
#define MAX_DISASTERS 30000
#define MAX_COUNTRIES 250
#define MAX_DISASTER_TYPES 50
#define MAX_VISIBLE_RECORDS 20

// Cores personalizadas
#define BACKGROUND_COLOR (Color){245, 245, 250, 255}
#define PANEL_COLOR (Color){255, 255, 255, 255}
#define BORDER_COLOR (Color){200, 200, 210, 255}
#define PRIMARY_COLOR (Color){52, 152, 219, 255}
#define SECONDARY_COLOR (Color){46, 204, 113, 255}
#define ACCENT_COLOR (Color){231, 76, 60, 255}
#define TEXT_COLOR (Color){52, 73, 94, 255}
#define SLIDER_COLOR (Color){100, 149, 237, 255}

// Estrutura para armazenar dados de desastre
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
} DisasterRecord;

// Estrutura para estatísticas por país com ordenação
typedef struct {
    char country[50];
    long long total_affected;
    int disaster_count;
    long long total_damage;
    int total_deaths;
} CountryStats;

// Enum para tipos de ordenação
typedef enum {
    SORT_BY_AFFECTED,
    SORT_BY_DAMAGE,
    SORT_BY_DEATHS,
    SORT_BY_COUNT,
    SORT_BY_COUNTRY_NAME
} SortType;

// Enum para ordem de classificação
typedef enum {
    SORT_ORDER_DESC,
    SORT_ORDER_ASC
} SortOrder;

typedef struct {
    // Dados
    DisasterRecord *disasters;
    int disaster_count;
    DisasterRecord *filtered_disasters;
    int filtered_count;

    // Listas únicas
    char countries[MAX_COUNTRIES][50];
    int country_count;
    char disaster_types[MAX_DISASTER_TYPES][50];
    int disaster_type_count;

    // Filtros
    int selected_country;
    int selected_disaster_type;
    int start_year;
    int end_year;
    int min_year;  // Ano mínimo dos dados
    int max_year;  // Ano máximo dos dados

    // Controles de slider duplo
    bool start_year_slider_active;
    bool end_year_slider_active;

    //Campo de texto para países
    char country_input[50];
    bool country_input_active;

    // Sistema de índices
    OptimizedDataWarehouse *optimized_dw;
    bool use_optimized_queries;

    // Estados da interface
    bool country_dropout_open;
    bool type_dropdown_open;
    int scroll_offset;
    int table_scroll_y;

    // Ordenação
    SortType current_sort_type;
    SortOrder current_sort_order;
    BPlusTree *sort_bplus_affected;
    BPlusTree *sort_bplus_damage;
    BPlusTree *sort_bplus_deaths;

    // Estatísticas calculadas
    long long total_affected_filtered;
    int total_deaths_filtered;
    long long total_damage_filtered;

    // Stats por país para gráfico (com ordenação)
    CountryStats country_stats[MAX_COUNTRIES];
    int country_stats_count;

} DisasterGUI;

// =============================================================================
// FUNÇÕES DE ORDENAÇÃO COM B+ TREE
// =============================================================================

// Comparadores para qsort como fallback
int compare_country_stats_affected_desc(const void *a, const void *b) {
    CountryStats *stats_a = (CountryStats *)a;
    CountryStats *stats_b = (CountryStats *)b;
    if (stats_b->total_affected > stats_a->total_affected) return 1;
    if (stats_b->total_affected < stats_a->total_affected) return -1;
    return 0;
}

int compare_country_stats_damage_desc(const void *a, const void *b) {
    CountryStats *stats_a = (CountryStats *)a;
    CountryStats *stats_b = (CountryStats *)b;
    if (stats_b->total_damage > stats_a->total_damage) return 1;
    if (stats_b->total_damage < stats_a->total_damage) return -1;
    return 0;
}

int compare_country_stats_deaths_desc(const void *a, const void *b) {
    CountryStats *stats_a = (CountryStats *)a;
    CountryStats *stats_b = (CountryStats *)b;
    if (stats_b->total_deaths > stats_a->total_deaths) return 1;
    if (stats_b->total_deaths < stats_a->total_deaths) return -1;
    return 0;
}

int compare_country_stats_count_desc(const void *a, const void *b) {
    CountryStats *stats_a = (CountryStats *)a;
    CountryStats *stats_b = (CountryStats *)b;
    return stats_b->disaster_count - stats_a->disaster_count;
}

int compare_country_stats_name_asc(const void *a, const void *b) {
    CountryStats *stats_a = (CountryStats *)a;
    CountryStats *stats_b = (CountryStats *)b;
    return strcmp(stats_a->country, stats_b->country);
}

// Inicializar B+ Trees para ordenação
void InitializeSortingTrees(DisasterGUI *gui) {
    if (!gui) return;

    gui->sort_bplus_affected = bplus_create("sort_affected.dat");
    gui->sort_bplus_damage = bplus_create("sort_damage.dat");
    gui->sort_bplus_deaths = bplus_create("sort_deaths.dat");

    gui->current_sort_type = SORT_BY_AFFECTED;
    gui->current_sort_order = SORT_ORDER_DESC;
}

// Limpar B+ Trees de ordenação
void CleanupSortingTrees(DisasterGUI *gui) {
    if (!gui) return;

    if (gui->sort_bplus_affected) bplus_destroy(gui->sort_bplus_affected);
    if (gui->sort_bplus_damage) bplus_destroy(gui->sort_bplus_damage);
    if (gui->sort_bplus_deaths) bplus_destroy(gui->sort_bplus_deaths);
}

// Construir índices de ordenação para países
void BuildCountrySortingIndexes(DisasterGUI *gui) {
    if (!gui || gui->country_stats_count == 0) return;

    // Limpar árvores existentes
    if (gui->sort_bplus_affected) bplus_destroy(gui->sort_bplus_affected);
    if (gui->sort_bplus_damage) bplus_destroy(gui->sort_bplus_damage);
    if (gui->sort_bplus_deaths) bplus_destroy(gui->sort_bplus_deaths);

    // Recriar árvores
    gui->sort_bplus_affected = bplus_create("sort_affected.dat");
    gui->sort_bplus_damage = bplus_create("sort_damage.dat");
    gui->sort_bplus_deaths = bplus_create("sort_deaths.dat");

    // Inserir dados nas B+ Trees
    for (int i = 0; i < gui->country_stats_count; i++) {
        CountryStats *stats = &gui->country_stats[i];

        // Para evitar problemas com valores muito grandes, dividir por 1000
        if (gui->sort_bplus_affected) {
            bplus_insert(gui->sort_bplus_affected, (int)(stats->total_affected / 1000), i);
        }
        if (gui->sort_bplus_damage) {
            bplus_insert(gui->sort_bplus_damage, (int)(stats->total_damage / 1000), i);
        }
        if (gui->sort_bplus_deaths) {
            bplus_insert(gui->sort_bplus_deaths, stats->total_deaths, i);
        }
    }
}

// Ordenar países usando B+ Tree
void SortCountryStats(DisasterGUI *gui, SortType sort_type, SortOrder sort_order) {
    if (!gui || gui->country_stats_count == 0) return;

    gui->current_sort_type = sort_type;
    gui->current_sort_order = sort_order;

    // Usar qsort como método principal (mais confiável)
    switch (sort_type) {
        case SORT_BY_AFFECTED:
            qsort(gui->country_stats, gui->country_stats_count, sizeof(CountryStats),
                  sort_order == SORT_ORDER_DESC ? compare_country_stats_affected_desc : compare_country_stats_affected_desc);
            break;
        case SORT_BY_DAMAGE:
            qsort(gui->country_stats, gui->country_stats_count, sizeof(CountryStats),
                  compare_country_stats_damage_desc);
            break;
        case SORT_BY_DEATHS:
            qsort(gui->country_stats, gui->country_stats_count, sizeof(CountryStats),
                  compare_country_stats_deaths_desc);
            break;
        case SORT_BY_COUNT:
            qsort(gui->country_stats, gui->country_stats_count, sizeof(CountryStats),
                  compare_country_stats_count_desc);
            break;
        case SORT_BY_COUNTRY_NAME:
            qsort(gui->country_stats, gui->country_stats_count, sizeof(CountryStats),
                  compare_country_stats_name_asc);
            break;
    }

    // Após ordenação com qsort, reconstruir os índices B+ Tree para próximas consultas
    BuildCountrySortingIndexes(gui);
}

// Comparadores para ordenação de tabela
int compare_disasters_by_year_desc(const void *a, const void *b) {
    DisasterRecord *rec_a = (DisasterRecord *)a;
    DisasterRecord *rec_b = (DisasterRecord *)b;
    return rec_b->start_year - rec_a->start_year;
}

int compare_disasters_by_affected_desc(const void *a, const void *b) {
    DisasterRecord *rec_a = (DisasterRecord *)a;
    DisasterRecord *rec_b = (DisasterRecord *)b;
    if (rec_b->total_affected > rec_a->total_affected) return 1;
    if (rec_b->total_affected < rec_a->total_affected) return -1;
    return 0;
}

int compare_disasters_by_damage_desc(const void *a, const void *b) {
    DisasterRecord *rec_a = (DisasterRecord *)a;
    DisasterRecord *rec_b = (DisasterRecord *)b;
    if (rec_b->total_damage > rec_a->total_damage) return 1;
    if (rec_b->total_damage < rec_a->total_damage) return -1;
    return 0;
}

int compare_disasters_by_deaths_desc(const void *a, const void *b) {
    DisasterRecord *rec_a = (DisasterRecord *)a;
    DisasterRecord *rec_b = (DisasterRecord *)b;
    return rec_b->total_deaths - rec_a->total_deaths;
}

int compare_disasters_by_country_asc(const void *a, const void *b) {
    DisasterRecord *rec_a = (DisasterRecord *)a;
    DisasterRecord *rec_b = (DisasterRecord *)b;
    return strcmp(rec_a->country, rec_b->country);
}

// Ordenar tabela de desastres
void SortDisasterTable(DisasterGUI *gui, SortType sort_type) {
    if (!gui || !gui->filtered_disasters || gui->filtered_count == 0) return;

    switch (sort_type) {
        case SORT_BY_AFFECTED:
            qsort(gui->filtered_disasters, gui->filtered_count, sizeof(DisasterRecord),
                  compare_disasters_by_affected_desc);
            break;
        case SORT_BY_DAMAGE:
            qsort(gui->filtered_disasters, gui->filtered_count, sizeof(DisasterRecord),
                  compare_disasters_by_damage_desc);
            break;
        case SORT_BY_DEATHS:
            qsort(gui->filtered_disasters, gui->filtered_count, sizeof(DisasterRecord),
                  compare_disasters_by_deaths_desc);
            break;
        case SORT_BY_COUNT: // Usar ano como proxy
            qsort(gui->filtered_disasters, gui->filtered_count, sizeof(DisasterRecord),
                  compare_disasters_by_year_desc);
            break;
        case SORT_BY_COUNTRY_NAME:
            qsort(gui->filtered_disasters, gui->filtered_count, sizeof(DisasterRecord),
                  compare_disasters_by_country_asc);
            break;
    }
}

// =============================================================================
// CONTROLES DE SLIDER DUPLO
// =============================================================================

// Desenhar slider duplo para anos
bool DrawDoubleSlider(Rectangle bounds, int min_val, int max_val, int *start_val, int *end_val,
                     bool *start_active, bool *end_active) {
    bool value_changed = false;
    Vector2 mouse_pos = GetMousePosition();

    // Calcular posições dos sliders
    float slider_width = bounds.width - 40;
    float start_ratio = (float)(*start_val - min_val) / (max_val - min_val);
    float end_ratio = (float)(*end_val - min_val) / (max_val - min_val);

    float start_x = bounds.x + 20 + start_ratio * slider_width;
    float end_x = bounds.x + 20 + end_ratio * slider_width;

    // Retângulos dos sliders
    Rectangle start_slider = {start_x - 8, bounds.y + 10, 16, 20};
    Rectangle end_slider = {end_x - 8, bounds.y + 10, 16, 20};

    // Desenhar trilha do slider
    DrawRectangle(bounds.x + 20, bounds.y + 18, slider_width, 4, (Color){200, 200, 200, 255});

    // Desenhar área selecionada
    float selected_start = bounds.x + 20 + start_ratio * slider_width;
    float selected_end = bounds.x + 20 + end_ratio * slider_width;
    DrawRectangle(selected_start, bounds.y + 18, selected_end - selected_start, 4, SLIDER_COLOR);

    // Detectar cliques e arrastar
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (CheckCollisionPointRec(mouse_pos, start_slider)) {
            *start_active = true;
            *end_active = false;
        } else if (CheckCollisionPointRec(mouse_pos, end_slider)) {
            *end_active = true;
            *start_active = false;
        } else {
            *start_active = false;
            *end_active = false;
        }
    }

    // Arrastar sliders
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        if (*start_active) {
            float new_ratio = (mouse_pos.x - bounds.x - 20) / slider_width;
            new_ratio = fmaxf(0.0f, fminf(1.0f, new_ratio));
            int new_val = min_val + (int)(new_ratio * (max_val - min_val));
            if (new_val <= *end_val && new_val != *start_val) {
                *start_val = new_val;
                value_changed = true;
            }
        } else if (*end_active) {
            float new_ratio = (mouse_pos.x - bounds.x - 20) / slider_width;
            new_ratio = fmaxf(0.0f, fminf(1.0f, new_ratio));
            int new_val = min_val + (int)(new_ratio * (max_val - min_val));
            if (new_val >= *start_val && new_val != *end_val) {
                *end_val = new_val;
                value_changed = true;
            }
        }
    }

    // Desenhar sliders
    Color start_color = *start_active ? PRIMARY_COLOR : SLIDER_COLOR;
    Color end_color = *end_active ? PRIMARY_COLOR : SLIDER_COLOR;

    DrawRectangleRec(start_slider, start_color);
    DrawRectangleRec(end_slider, end_color);

    // Desenhar bordas
    DrawRectangleLinesEx(start_slider, 2, WHITE);
    DrawRectangleLinesEx(end_slider, 2, WHITE);

    return value_changed;
}

// =============================================================================
// IMPLEMENTAÇÕES DAS FUNÇÕES EXISTENTES (com modificações)
// =============================================================================

// Inicializar estrutura GUI
DisasterGUI* InitializeGUI() {
    DisasterGUI *gui = malloc(sizeof(DisasterGUI));
    if (!gui) return NULL;

    memset(gui, 0, sizeof(DisasterGUI));

    // Inicializar sistema de ordenação
    InitializeSortingTrees(gui);

    return gui;
}

// Limpar recursos da GUI
void CleanupGUI(DisasterGUI *gui) {
    if (!gui) return;

    if (gui->disasters) free(gui->disasters);
    if (gui->filtered_disasters) free(gui->filtered_disasters);

    // Limpar árvores de ordenação
    CleanupSortingTrees(gui);

    free(gui);
}

// Função para inicializar sistema otimizado
int InitializeOptimizedSystem(DisasterGUI *gui, DataWarehouse *dw) {
    if (!gui || !dw) return 0;

    printf("Inicializando sistema de índices otimizado...\n");

    // Criar configuração otimizada
    IndexConfiguration *config = index_config_create_high_performance();
    if (!config) {
        printf("Erro ao criar configuração de índices\n");
        return 0;
    }

    // Verificar se há dados suficientes
    if (dw->fact_count == 0) {
        printf("Nenhum dado disponível para indexar\n");
        index_config_destroy(config);
        return 0;
    }

    // Criar data warehouse otimizado
    gui->optimized_dw = optimized_dw_create_with_config(config);
    if (!gui->optimized_dw) {
        printf("Erro ao criar data warehouse otimizado\n");
        index_config_destroy(config);
        return 0;
    }

    // Associar data warehouse original corretamente
    gui->optimized_dw->dw = dw;
    gui->optimized_dw->indexes->dw = dw;

    // Construir todos os índices
    printf("Construindo índices para %d registros...\n", dw->fact_count);
    clock_t start_time = clock();

    if (index_system_build_all(gui->optimized_dw->indexes) != 1) {
        printf("Erro ao construir índices\n");
        optimized_dw_destroy(gui->optimized_dw);
        gui->optimized_dw = NULL;
        index_config_destroy(config);
        return 0;
    }

    clock_t end_time = clock();
    double build_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;

    printf("Sistema de índices inicializado com sucesso!\n");
    printf("Tempo de construção: %.3f segundos\n", build_time);
    printf("Índices criados para %d registros\n", dw->fact_count);

    // Verificar integridade dos índices
    if (!index_verify_integrity(gui->optimized_dw->indexes)) {
        printf("Problemas de integridade detectados nos índices\n");
    }

    // Imprimir estatísticas dos índices
    index_print_statistics(gui->optimized_dw->indexes);

    gui->use_optimized_queries = true;
    index_config_destroy(config);

    return 1;
}

// Função melhorada para aplicar filtros (com filtro de ano usando B+ Tree)
void ApplyFilters(DisasterGUI *gui) {
    if (!gui || !gui->disasters) return;

    clock_t start_time = clock();

    gui->filtered_count = 0;
    gui->total_affected_filtered = 0;
    gui->total_deaths_filtered = 0;
    gui->total_damage_filtered = 0;

    // Usar índices otimizados quando disponível e apropriado
    if (gui->use_optimized_queries && gui->optimized_dw &&
        gui->optimized_dw->indexes && strlen(gui->country_input) > 0) {

        printf("Usando consulta otimizada para país: '%s'\n", gui->country_input);

        int *result_ids = NULL;
        int result_count = 0;

        // Busca otimizada por país
        result_ids = optimized_query_by_country(gui->optimized_dw,
                                               gui->country_input,
                                               &result_count);

        if (result_ids && result_count > 0) {
            printf("Consulta otimizada retornou %d resultados\n", result_count);

            // Aplicar filtros adicionais aos resultados otimizados
            for (int i = 0; i < result_count && gui->filtered_count < MAX_DISASTERS; i++) {
                int fact_id = result_ids[i];

                // Verificar bounds do array
                if (fact_id >= 0 && fact_id < gui->disaster_count) {
                    DisasterRecord *record = &gui->disasters[fact_id];
                    bool include = true;

                    // Filtro por tipo de desastre
                    if (gui->selected_disaster_type > 0 &&
                        gui->selected_disaster_type < gui->disaster_type_count) {
                        if (strcmp(record->disaster_type,
                                 gui->disaster_types[gui->selected_disaster_type]) != 0) {
                            include = false;
                        }
                    }

                    // Filtro por ano usando slider duplo
                    if (record->start_year < gui->start_year ||
                        record->start_year > gui->end_year) {
                        include = false;
                    }

                    if (include) {
                        gui->filtered_disasters[gui->filtered_count] = *record;
                        gui->filtered_count++;
                        gui->total_affected_filtered += record->total_affected;
                        gui->total_deaths_filtered += record->total_deaths;
                        gui->total_damage_filtered += record->total_damage;
                    }
                }
            }

            free(result_ids);

            clock_t end_time = clock();
            double query_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
            printf("Consulta otimizada executada em %.4f segundos\n", query_time);
        } else {
            printf("Consulta otimizada não retornou resultados, usando busca convencional\n");
            gui->use_optimized_queries = false; // Fallback temporário
        }
    }

    // Consulta convencional (fallback ou quando não há índices)
    if (!gui->use_optimized_queries || gui->filtered_count == 0) {
        printf("Usando busca convencional\n");

        for (int i = 0; i < gui->disaster_count; i++) {
            DisasterRecord *record = &gui->disasters[i];
            bool include = true;

            // Filtro por país usando input de texto (busca parcial)
            if (strlen(gui->country_input) > 0) {
                char country_lower[50], input_lower[50];

                // Verificar bounds antes de copiar
                strncpy(country_lower, record->country, sizeof(country_lower) - 1);
                country_lower[sizeof(country_lower) - 1] = '\0';

                strncpy(input_lower, gui->country_input, sizeof(input_lower) - 1);
                input_lower[sizeof(input_lower) - 1] = '\0';

                // Converter para minúsculo para busca case-insensitive
                for (int j = 0; country_lower[j]; j++) {
                    country_lower[j] = tolower(country_lower[j]);
                }
                for (int j = 0; input_lower[j]; j++) {
                    input_lower[j] = tolower(input_lower[j]);
                }

                if (strstr(country_lower, input_lower) == NULL) {
                    include = false;
                }
            }

            // Filtro por tipo de desastre
            if (gui->selected_disaster_type > 0 &&
                gui->selected_disaster_type < gui->disaster_type_count) {
                if (strcmp(record->disaster_type,
                         gui->disaster_types[gui->selected_disaster_type]) != 0) {
                    include = false;
                }
            }

            // Filtro por ano usando slider duplo
            if (record->start_year < gui->start_year ||
                record->start_year > gui->end_year) {
                include = false;
            }

            if (include && gui->filtered_count < MAX_DISASTERS) {
                gui->filtered_disasters[gui->filtered_count] = *record;
                gui->filtered_count++;
                gui->total_affected_filtered += record->total_affected;
                gui->total_deaths_filtered += record->total_deaths;
                gui->total_damage_filtered += record->total_damage;
            }
        }

        clock_t end_time = clock();
        double query_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
        printf("Busca convencional executada em %.4f segundos\n", query_time);
    }

    // Calcular estatísticas por país para gráfico
    gui->country_stats_count = 0;
    for (int i = 0; i < gui->filtered_count && gui->country_stats_count < MAX_COUNTRIES; i++) {
        DisasterRecord *record = &gui->filtered_disasters[i];

        int country_idx = -1;
        for (int j = 0; j < gui->country_stats_count; j++) {
            if (strcmp(gui->country_stats[j].country, record->country) == 0) {
                country_idx = j;
                break;
            }
        }

        if (country_idx == -1 && gui->country_stats_count < MAX_COUNTRIES) {
            strncpy(gui->country_stats[gui->country_stats_count].country,
                   record->country, sizeof(gui->country_stats[0].country) - 1);
            gui->country_stats[gui->country_stats_count].country[sizeof(gui->country_stats[0].country) - 1] = '\0';
            gui->country_stats[gui->country_stats_count].total_affected = record->total_affected;
            gui->country_stats[gui->country_stats_count].total_damage = record->total_damage;
            gui->country_stats[gui->country_stats_count].total_deaths = record->total_deaths;
            gui->country_stats[gui->country_stats_count].disaster_count = 1;
            gui->country_stats_count++;
        } else if (country_idx != -1) {
            gui->country_stats[country_idx].total_affected += record->total_affected;
            gui->country_stats[country_idx].total_damage += record->total_damage;
            gui->country_stats[country_idx].total_deaths += record->total_deaths;
            gui->country_stats[country_idx].disaster_count++;
        }
    }

    // Aplicar ordenação padrão
    SortCountryStats(gui, gui->current_sort_type, gui->current_sort_order);
    SortDisasterTable(gui, gui->current_sort_type);

    printf("Filtros aplicados: %d registros encontrados\n", gui->filtered_count);
}

bool DrawDropdown(Rectangle bounds, const char *label, char options[][50], int option_count, int *selected, bool *open);
bool DrawTextInput(Rectangle bounds, const char *label, char *text, int max_length, bool *is_active, DisasterGUI *gui);

// Função melhorada para autocomplete
void HandleCountryAutocomplete(DisasterGUI *gui) {
    if (!gui->use_optimized_queries || !gui->optimized_dw) return;

    if (strlen(gui->country_input) >= 2) {  // Mínimo 2 caracteres
        int result_count = 0;
        char **suggestions = optimized_autocomplete_country(
            gui->optimized_dw, gui->country_input, &result_count);

        if (suggestions && result_count > 0) {
            printf("Sugestões de países para '%s': ", gui->country_input);
            for (int i = 0; i < result_count && i < 5; i++) {
                printf("'%s' ", suggestions[i]);
            }
            printf("\n");

            // Liberar memória das sugestões corretamente
            for (int i = 0; i < result_count; i++) {
                free(suggestions[i]);
            }
            free(suggestions);
        }
    }
}

// Desenhar cabeçalho da aplicação
void DrawApplicationHeader(Rectangle bounds) {
    DrawRectangleRec(bounds, PRIMARY_COLOR);
    DrawText("Disaster Analysis Dashboard - Sistema com Ordenação B+ Tree", bounds.x + 20, bounds.y + 15, 24, WHITE);
    DrawText("Advanced Sorting & Date Range Filtering with High-Performance Indexes", bounds.x + 20, bounds.y + 35, 14, (Color){200, 200, 200, 255});
}

// Desenhar controles de filtro com slider duplo
void DrawFilterControls(Rectangle bounds, DisasterGUI *gui, bool *filters_changed) {
    DrawRectangleRec(bounds, PANEL_COLOR);
    DrawRectangleLinesEx(bounds, 1, BORDER_COLOR);

    // Input de texto para países
    Rectangle country_input_rect = {bounds.x + 20, bounds.y + 40, 300, 30};
    if (DrawTextInput(country_input_rect, "Countries:", gui->country_input, 50, &gui->country_input_active, gui)) {
        *filters_changed = true;
    }

    // Slider duplo para anos
    Rectangle year_slider_rect = {bounds.x + 350, bounds.y + 40, 400, 50};

    // Labels para os sliders
    DrawText("Start Year:", bounds.x + 350, bounds.y + 20, 14, TEXT_COLOR);
    DrawText(TextFormat("%d", gui->start_year), bounds.x + 430, bounds.y + 20, 14, PRIMARY_COLOR);

    DrawText("End Year:", bounds.x + 550, bounds.y + 20, 14, TEXT_COLOR);
    DrawText(TextFormat("%d", gui->end_year), bounds.x + 620, bounds.y + 20, 14, PRIMARY_COLOR);

    if (DrawDoubleSlider(year_slider_rect, gui->min_year, gui->max_year,
                        &gui->start_year, &gui->end_year,
                        &gui->start_year_slider_active, &gui->end_year_slider_active)) {
        *filters_changed = true;
    }

    // Status do sistema de índices
    const char *index_status = gui->use_optimized_queries ? "Índices Ativos" : "Busca Linear";
    Color status_color = gui->use_optimized_queries ? SECONDARY_COLOR : ACCENT_COLOR;
    DrawText(index_status, bounds.x + 780, bounds.y + 45, 14, status_color);

    // Controles de ordenação
    Rectangle sort_controls_rect = {bounds.x + 20, bounds.y + 80, bounds.width - 40, 40};
    DrawText("Sort by:", sort_controls_rect.x, sort_controls_rect.y, 14, TEXT_COLOR);

    const char *sort_options[] = {"Affected", "Damage", "Deaths", "Count", "Country"};

    for (int i = 0; i < 5; i++) {
        Rectangle button_rect = {sort_controls_rect.x + 80 + i * 100, sort_controls_rect.y, 90, 25};
        Color button_color = (gui->current_sort_type == i) ? PRIMARY_COLOR : (Color){240, 240, 245, 255};
        Color text_color = (gui->current_sort_type == i) ? WHITE : TEXT_COLOR;

        Vector2 mouse_pos = GetMousePosition();
        if (CheckCollisionPointRec(mouse_pos, button_rect)) {
            button_color = (gui->current_sort_type == i) ? SECONDARY_COLOR : (Color){220, 220, 225, 255};
        }

        DrawRectangleRec(button_rect, button_color);
        DrawRectangleLinesEx(button_rect, 1, BORDER_COLOR);

        int text_width = MeasureText(sort_options[i], 12);
        DrawText(sort_options[i], button_rect.x + (button_rect.width - text_width) / 2,
                button_rect.y + 6, 12, text_color);

        if (CheckCollisionPointRec(mouse_pos, button_rect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (gui->current_sort_type == i) {
                // Toggle order se já está selecionado
                gui->current_sort_order = (gui->current_sort_order == SORT_ORDER_DESC) ?
                                         SORT_ORDER_ASC : SORT_ORDER_DESC;
            } else {
                gui->current_sort_type = i;
                gui->current_sort_order = SORT_ORDER_DESC;
            }
            *filters_changed = true;
        }
    }

    // Indicador de ordem de classificação
    const char *order_text = (gui->current_sort_order == SORT_ORDER_DESC) ? "↓ DESC" : "↑ ASC";
    DrawText(order_text, sort_controls_rect.x + 580, sort_controls_rect.y + 6, 12, ACCENT_COLOR);
}

// Desenhar gráfico de barras com ordenação
void DrawBarChart(Rectangle bounds, CountryStats *stats, int count, DisasterGUI *gui) {
    DrawRectangleRec(bounds, PANEL_COLOR);
    DrawRectangleLinesEx(bounds, 1, BORDER_COLOR);

    const char *sort_names[] = {"Total Affected", "Total Damage", "Total Deaths", "Disaster Count", "Country Name"};
    DrawText(TextFormat("Top Countries by %s", sort_names[gui->current_sort_type]),
             bounds.x + 10, bounds.y + 10, 16, TEXT_COLOR);

    if (count == 0) return;

    // Os dados já vêm ordenados da função SortCountryStats
    long long max_value = 0;

    // Encontrar valor máximo baseado no tipo de ordenação atual
    switch (gui->current_sort_type) {
        case SORT_BY_AFFECTED:
            max_value = stats[0].total_affected;
            break;
        case SORT_BY_DAMAGE:
            max_value = stats[0].total_damage;
            break;
        case SORT_BY_DEATHS:
            max_value = stats[0].total_deaths;
            break;
        case SORT_BY_COUNT:
            max_value = stats[0].disaster_count;
            break;
        default:
            max_value = stats[0].total_affected;
            break;
    }

    if (max_value == 0) return;

    // Desenhar barras (máximo 10)
    int bars_to_show = count > 10 ? 10 : count;
    float bar_height = (bounds.height - 60) / bars_to_show;

    for (int i = 0; i < bars_to_show; i++) {
        long long current_value = 0;

        // Obter valor atual baseado no tipo de ordenação
        switch (gui->current_sort_type) {
            case SORT_BY_AFFECTED:
                current_value = stats[i].total_affected;
                break;
            case SORT_BY_DAMAGE:
                current_value = stats[i].total_damage;
                break;
            case SORT_BY_DEATHS:
                current_value = stats[i].total_deaths;
                break;
            case SORT_BY_COUNT:
                current_value = stats[i].disaster_count;
                break;
            default:
                current_value = stats[i].total_affected;
                break;
        }

        float bar_width = (current_value / (float)max_value) * (bounds.width - 305);

        Rectangle bar_rect = {bounds.x + 250, bounds.y + 40 + i * bar_height,
                             bar_width, bar_height - 5};

        Color bar_color = {52 + (i * 20) % 150, 152, 219, 255};
        DrawRectangleRec(bar_rect, bar_color);

        // Nome do país
        DrawText(stats[i].country, bounds.x + 10, bounds.y + 42 + i * bar_height, 12, TEXT_COLOR);

        // Valor formatado
        char value_text[50];
        if (gui->current_sort_type == SORT_BY_DAMAGE) {
            if (current_value >= 1000000000) {
                sprintf(value_text, "$%.1fB", current_value / 1000000000.0);
            } else if (current_value >= 1000000) {
                sprintf(value_text, "$%.1fM", current_value / 1000000.0);
            } else if (current_value >= 1000) {
                sprintf(value_text, "$%.1fK", current_value / 1000.0);
            } else {
                sprintf(value_text, "$%lld", current_value);
            }
        } else if (gui->current_sort_type == SORT_BY_COUNT || gui->current_sort_type == SORT_BY_DEATHS) {
            sprintf(value_text, "%lld", current_value);
        } else {
            if (current_value >= 1000000000) {
                sprintf(value_text, "%.1fB", current_value / 1000000000.0);
            } else if (current_value >= 1000000) {
                sprintf(value_text, "%.1fM", current_value / 1000000.0);
            } else if (current_value >= 1000) {
                sprintf(value_text, "%.1fK", current_value / 1000.0);
            } else {
                sprintf(value_text, "%lld", current_value);
            }
        }

        DrawText(value_text, bar_rect.x + bar_rect.width + 5, bounds.y + 42 + i * bar_height, 12, TEXT_COLOR);
    }
}

// Desenhar painel de estatísticas detalhadas
void DrawDetailedStatsPanel(Rectangle bounds, DisasterGUI *gui) {
    DrawRectangleRec(bounds, PANEL_COLOR);
    DrawRectangleLinesEx(bounds, 1, BORDER_COLOR);
    DrawText("Statistics", bounds.x + 10, bounds.y + 10, 16, TEXT_COLOR);

    int y_offset = 40;
    int line_height = 25;

    // Total de registros
    DrawText(TextFormat("Total Records: %d", gui->filtered_count),
             bounds.x + 20, bounds.y + y_offset, 14, TEXT_COLOR);
    y_offset += line_height;

    // Total de mortes
    DrawText(TextFormat("Total Deaths: %d", gui->total_deaths_filtered),
             bounds.x + 20, bounds.y + y_offset, 14, TEXT_COLOR);
    y_offset += line_height;

    // Total de afetados
    char affected_str[50];
    if (gui->total_affected_filtered >= 1000000000) {
        sprintf(affected_str, "%.1fB", gui->total_affected_filtered / 1000000000.0);
    } else if (gui->total_affected_filtered >= 1000000) {
        sprintf(affected_str, "%.1fM", gui->total_affected_filtered / 1000000.0);
    } else if (gui->total_affected_filtered >= 1000) {
        sprintf(affected_str, "%.1fK", gui->total_affected_filtered / 1000.0);
    } else {
        sprintf(affected_str, "%lld", gui->total_affected_filtered);
    }
    DrawText(TextFormat("Total Affected: %s", affected_str),
             bounds.x + 20, bounds.y + y_offset, 14, TEXT_COLOR);
    y_offset += line_height;

    // Total de danos
    char damage_str[50];
    if (gui->total_damage_filtered >= 1000000000) {
        sprintf(damage_str, "$%.1fB", gui->total_damage_filtered / 1000000000.0);
    } else if (gui->total_damage_filtered >= 1000000) {
        sprintf(damage_str, "$%.1fM", gui->total_damage_filtered / 1000000.0);
    } else if (gui->total_damage_filtered >= 1000) {
        sprintf(damage_str, "$%.1fK", gui->total_damage_filtered / 1000000.0);
    } else {
        sprintf(damage_str, "$%lld", gui->total_damage_filtered);
    }
    DrawText(TextFormat("Total Damage: %s", damage_str),
             bounds.x + 20, bounds.y + y_offset, 14, TEXT_COLOR);
    y_offset += line_height;

    // Intervalo de anos filtrado
    DrawText(TextFormat("Year Range: %d - %d", gui->start_year, gui->end_year),
             bounds.x + 20, bounds.y + y_offset, 12, (Color){100, 100, 100, 255});
    y_offset += line_height;

    // Status de performance
    const char *perf_text = gui->use_optimized_queries ?
                           "High Performance Mode" : "Standard Mode";
    Color perf_color = gui->use_optimized_queries ? SECONDARY_COLOR : ACCENT_COLOR;
    DrawText(perf_text, bounds.x + 20, bounds.y + y_offset, 12, perf_color);
}

// Desenhar lista de tipos de desastre clicáveis
void DrawDisasterTypeList(Rectangle bounds, DisasterGUI *gui, bool *filters_changed) {
    DrawRectangleRec(bounds, PANEL_COLOR);
    DrawRectangleLinesEx(bounds, 1, BORDER_COLOR);
    DrawText("Disaster Types", bounds.x + 10, bounds.y + 10, 16, TEXT_COLOR);

    if (gui->disaster_type_count <= 1) return;

    // Calcular dimensões dos itens
    int items_per_column = (gui->disaster_type_count - 1 + 1) / 2; // -1 para excluir "All Types", +1 para arredondar
    float item_height = (bounds.height - 40) / items_per_column;
    float column_width = bounds.width / 2;

    Vector2 mouse_pos = GetMousePosition();

    // Desenhar itens em duas colunas (começando do índice 1 para pular "All Types")
    for (int i = 1; i < gui->disaster_type_count; i++) {
        int item_index = i - 1; // Ajustar índice para começar de 0
        int column = item_index / items_per_column;
        int row = item_index % items_per_column;

        Rectangle item_rect = {
            bounds.x + 10 + column * column_width,
            bounds.y + 35 + row * item_height,
            column_width - 20,
            item_height - 2
        };

        // Verificar se este tipo está selecionado
        bool is_selected = (gui->selected_disaster_type == i);

        // Cores baseadas no estado
        Color item_color;
        if (is_selected) {
            item_color = PRIMARY_COLOR;
        } else if (CheckCollisionPointRec(mouse_pos, item_rect)) {
            item_color = (Color){220, 220, 225, 255};
        } else {
            item_color = (Color){248, 248, 250, 255};
        }

        DrawRectangleRec(item_rect, item_color);
        DrawRectangleLinesEx(item_rect, 1, BORDER_COLOR);

        // Cor do texto
        Color text_color = is_selected ? WHITE : TEXT_COLOR;

        // Desenhar texto truncado se necessário
        int text_width = MeasureText(gui->disaster_types[i], 12);
        if (text_width > item_rect.width - 10) {
            char truncated[50];
            strncpy(truncated, gui->disaster_types[i], 20);
            truncated[20] = '\0';
            strcat(truncated, "...");
            DrawText(truncated, item_rect.x + 5, item_rect.y + 5, 12, text_color);
        } else {
            DrawText(gui->disaster_types[i], item_rect.x + 5, item_rect.y + 5, 12, text_color);
        }

        // Verificar clique
        if (CheckCollisionPointRec(mouse_pos, item_rect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (gui->selected_disaster_type == i) {
                // Se já está selecionado, desselecionar (voltar para "All Types")
                gui->selected_disaster_type = 0;
            } else {
                // Selecionar este tipo
                gui->selected_disaster_type = i;
            }
            *filters_changed = true;
        }
    }

    // Botão "Clear Filter" no canto inferior
    Rectangle clear_rect = {bounds.x + bounds.width - 80, bounds.y + bounds.height - 25, 70, 20};
    Color clear_color = CheckCollisionPointRec(mouse_pos, clear_rect) ?
                       ACCENT_COLOR : (Color){200, 200, 200, 255};

    DrawRectangleRec(clear_rect, clear_color);
    DrawText("Clear", clear_rect.x + 20, clear_rect.y + 3, 12, WHITE);

    if (CheckCollisionPointRec(mouse_pos, clear_rect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        gui->selected_disaster_type = 0;
        *filters_changed = true;
    }
}

// Desenhar tabela de dados expandida com ordenação clicável
void DrawDataTable(Rectangle bounds, DisasterRecord *records, int count, int *scroll_y, DisasterGUI *gui, bool *filters_changed) {
    DrawRectangleRec(bounds, PANEL_COLOR);
    DrawRectangleLinesEx(bounds, 1, BORDER_COLOR);
    DrawText("Disaster Records (Click headers to sort)", bounds.x + 10, bounds.y + 10, 16, TEXT_COLOR);

    if (count == 0) {
        DrawText("No records found with current filters",
                 bounds.x + 20, bounds.y + 50, 14, TEXT_COLOR);
        return;
    }

    //Larguras das colunas para ocupar toda a tela
    float col_widths[] = {110, 140, 140, 140, 200, 150, 80, 80, 80, 80, 80, 80, 80, 100, 100, 100}; // 16 colunas
    float total_width = 0;
    for (int i = 0; i < 16; i++) {
        total_width += col_widths[i];
    }

    // Ajustar proporcionalmente para ocupar toda largura
    float scale_factor = (bounds.width - 20) / total_width;
    for (int i = 0; i < 16; i++) {
        col_widths[i] *= scale_factor;
    }

    // Cabeçalho da tabela expandido com ordenação clicável
    Rectangle header_rect = {bounds.x, bounds.y + 35, bounds.width, 25};
    DrawRectangleRec(header_rect, (Color){240, 240, 245, 255});

    float x_pos = bounds.x + 5;
    const char *headers[] = {
        "Disaster Group", "Disaster Subgroup", "Disaster Type", "Disaster Subtype",
        "Country", "Subregion", "Region", "Start Year", "Start Month", "Start Day",
        "End Year", "End Month", "End Day", "Total Deaths", "Total Affected", "Total Damage (US$)"
    };

    Vector2 mouse_pos = GetMousePosition();

    // Índices das colunas clicáveis para ordenação
    int sortable_columns[] = {4, 7, 13, 14, 15}; // Country, Start Year, Deaths, Affected, Damage
    SortType column_sort_types[] = {SORT_BY_COUNTRY_NAME, SORT_BY_COUNT, SORT_BY_DEATHS, SORT_BY_AFFECTED, SORT_BY_DAMAGE};

    for (int i = 0; i < 16; i++) {
        Rectangle header_col_rect = {x_pos, bounds.y + 40, col_widths[i], 20};

        // Verificar se é coluna ordenável
        bool is_sortable = false;
        SortType column_sort_type = SORT_BY_AFFECTED;
        for (int j = 0; j < 5; j++) {
            if (sortable_columns[j] == i) {
                is_sortable = true;
                column_sort_type = column_sort_types[j];
                break;
            }
        }

        Color header_color = (Color){240, 240, 245, 255};
        if (is_sortable) {
            if (CheckCollisionPointRec(mouse_pos, header_col_rect)) {
                header_color = (Color){220, 220, 225, 255};
            }
            if (gui->current_sort_type == column_sort_type) {
                header_color = (Color){200, 200, 205, 255};
            }
        }

        DrawRectangleRec(header_col_rect, header_color);

        // Desenhar texto do cabeçalho
        const char *header_text = headers[i];
        if (is_sortable && gui->current_sort_type == column_sort_type) {
            char header_with_arrow[100];
            sprintf(header_with_arrow, "%s %s", header_text,
                   (gui->current_sort_order == SORT_ORDER_DESC) ? "↓" : "↑");
            header_text = header_with_arrow;
        }

        DrawText(header_text, x_pos, bounds.y + 40, 11, TEXT_COLOR);

        // Verificar clique para ordenação
        if (is_sortable && CheckCollisionPointRec(mouse_pos, header_col_rect) &&
            IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (gui->current_sort_type == column_sort_type) {
                // Toggle order se já está selecionado
                gui->current_sort_order = (gui->current_sort_order == SORT_ORDER_DESC) ?
                                         SORT_ORDER_ASC : SORT_ORDER_DESC;
            } else {
                gui->current_sort_type = column_sort_type;
                gui->current_sort_order = SORT_ORDER_DESC;
            }
            *filters_changed = true;
        }

        x_pos += col_widths[i];
    }

    // Scroll com mouse wheel
    float wheel = GetMouseWheelMove();
    *scroll_y -= (int)(wheel * 30);
    if (*scroll_y < 0) *scroll_y = 0;
    if (*scroll_y > count * 20) *scroll_y = count * 20;

    // Desenhar registros
    int visible_rows = (bounds.height - 70) / 20;
    int start_row = *scroll_y / 20;
    int end_row = start_row + visible_rows;
    if (end_row > count) end_row = count;

    for (int i = start_row; i < end_row; i++) {
        DisasterRecord *record = &records[i];
        int y_pos = bounds.y + 65 + (i - start_row) * 20;

        Color row_color = (i % 2 == 0) ? PANEL_COLOR : (Color){248, 248, 250, 255};
        Rectangle row_rect = {bounds.x, y_pos - 2, bounds.width, 20};
        DrawRectangleRec(row_rect, row_color);

        x_pos = bounds.x + 5;

        // Desenhar cada coluna
        DrawText(record->disaster_group, x_pos, y_pos, 10, TEXT_COLOR);
        x_pos += col_widths[0];

        DrawText(record->disaster_subgroup, x_pos, y_pos, 10, TEXT_COLOR);
        x_pos += col_widths[1];

        DrawText(record->disaster_type, x_pos, y_pos, 10, TEXT_COLOR);
        x_pos += col_widths[2];

        DrawText(record->disaster_subtype, x_pos, y_pos, 10, TEXT_COLOR);
        x_pos += col_widths[3];

        DrawText(record->country, x_pos, y_pos, 10, TEXT_COLOR);
        x_pos += col_widths[4];

        DrawText(record->subregion, x_pos, y_pos, 10, TEXT_COLOR);
        x_pos += col_widths[5];

        DrawText(record->region, x_pos, y_pos, 10, TEXT_COLOR);
        x_pos += col_widths[6];

        DrawText(TextFormat("%d", record->start_year), x_pos, y_pos, 10, TEXT_COLOR);
        x_pos += col_widths[7];

        DrawText(TextFormat("%d", record->start_month), x_pos, y_pos, 10, TEXT_COLOR);
        x_pos += col_widths[8];

        DrawText(TextFormat("%d", record->start_day), x_pos, y_pos, 10, TEXT_COLOR);
        x_pos += col_widths[9];

        DrawText(TextFormat("%d", record->end_year), x_pos, y_pos, 10, TEXT_COLOR);
        x_pos += col_widths[10];

        DrawText(TextFormat("%d", record->end_month), x_pos, y_pos, 10, TEXT_COLOR);
        x_pos += col_widths[11];

        DrawText(TextFormat("%d", record->end_day), x_pos, y_pos, 10, TEXT_COLOR);
        x_pos += col_widths[12];

        DrawText(TextFormat("%d", record->total_deaths), x_pos, y_pos, 10, TEXT_COLOR);
        x_pos += col_widths[13];

        // Formatar números grandes para Total Affected
        if (record->total_affected >= 1000000) {
            DrawText(TextFormat("%.1fM", record->total_affected / 1000000.0), x_pos, y_pos, 10, TEXT_COLOR);
        } else if (record->total_affected >= 1000) {
            DrawText(TextFormat("%.1fK", record->total_affected / 1000.0), x_pos, y_pos, 10, TEXT_COLOR);
        } else {
            DrawText(TextFormat("%lld", record->total_affected), x_pos, y_pos, 10, TEXT_COLOR);
        }
        x_pos += col_widths[14];

        // Formatar Total Damage
        if (record->total_damage >= 1000000) {
            DrawText(TextFormat("$%.1fM", record->total_damage / 1000000.0), x_pos, y_pos, 10, TEXT_COLOR);
        } else if (record->total_damage >= 1000) {
            DrawText(TextFormat("$%.1fK", record->total_damage / 1000.0), x_pos, y_pos, 10, TEXT_COLOR);
        } else {
            DrawText(TextFormat("$%lld", record->total_damage), x_pos, y_pos, 10, TEXT_COLOR);
        }
    }
}

// Função para desenhar dropdown
bool DrawDropdown(Rectangle bounds, const char *text, char items[][50],
                       int item_count, int *selected_index, bool *is_open) {

    bool pressed = false;
    Vector2 mouse_pos = GetMousePosition();

    // Desenhar botão principal
    Color button_color = CheckCollisionPointRec(mouse_pos, bounds) ?
                        (Color){220, 220, 225, 255} : PANEL_COLOR;

    DrawRectangleRec(bounds, button_color);
    DrawRectangleLinesEx(bounds, 1, BORDER_COLOR);
    DrawText(text, bounds.x + 5, bounds.y + 5, 14, TEXT_COLOR);
    DrawText("▼", bounds.x + bounds.width - 20, bounds.y + 5, 14, TEXT_COLOR);

    // Verificar clique no botão
    if (CheckCollisionPointRec(mouse_pos, bounds) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        *is_open = !(*is_open);
        pressed = true;
    }

    // Desenhar dropdown se aberto
    if (*is_open) {
        Rectangle dropdown_bounds = {bounds.x, bounds.y + bounds.height,
                                   bounds.width, item_count * 25};
        DrawRectangleRec(dropdown_bounds, PANEL_COLOR);
        DrawRectangleLinesEx(dropdown_bounds, 1, BORDER_COLOR);

        for (int i = 0; i < item_count; i++) {
            Rectangle item_bounds = {bounds.x, bounds.y + bounds.height + i * 25,
                                   bounds.width, 25};

            Color item_color = CheckCollisionPointRec(mouse_pos, item_bounds) ?
                             (Color){240, 240, 245, 255} : PANEL_COLOR;

            DrawRectangleRec(item_bounds, item_color);
            DrawText(items[i], item_bounds.x + 5, item_bounds.y + 3, 14, TEXT_COLOR);

            if (CheckCollisionPointRec(mouse_pos, item_bounds) &&
                IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                *selected_index = i;
                *is_open = false;
                pressed = true;
            }
        }
    }

    return pressed;
}

// Função para desenhar input de texto
bool DrawTextInput(Rectangle bounds, const char *label, char *text, int max_length, bool *is_active, DisasterGUI *gui) {
    Vector2 mouse_pos = GetMousePosition();
    bool text_changed = false;

    // Verificar clique para ativar/desativar
    if (CheckCollisionPointRec(mouse_pos, bounds) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        *is_active = true;
    } else if (!CheckCollisionPointRec(mouse_pos, bounds) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        *is_active = false;
    }

    // Cor do campo baseada no estado
    Color input_color = *is_active ? (Color){255, 255, 255, 255} : (Color){248, 248, 250, 255};
    Color border_color = *is_active ? PRIMARY_COLOR : BORDER_COLOR;

    // Desenhar campo
    DrawRectangleRec(bounds, input_color);
    DrawRectangleLinesEx(bounds, 2, border_color);

    // Desenhar label acima do campo
    DrawText(label, bounds.x, bounds.y - 20, 14, TEXT_COLOR);

    // Capturar entrada de texto se ativo
    if (*is_active) {
        int key = GetCharPressed();
        while (key > 0) {
            if (key >= 32 && key <= 125 && strlen(text) < max_length - 1) {
                text[strlen(text) + 1] = '\0';
                text[strlen(text)] = (char)key;
                text_changed = true;

                // Trigger autocomplete após mudança de texto
                if (gui && gui->use_optimized_queries) {
                    HandleCountryAutocomplete(gui);
                }
            }
            key = GetCharPressed();
        }

        // Backspace
        if (IsKeyPressed(KEY_BACKSPACE) && strlen(text) > 0) {
            text[strlen(text) - 1] = '\0';
            text_changed = true;

            // Trigger autocomplete após mudança de texto
            if (gui && gui->use_optimized_queries) {
                HandleCountryAutocomplete(gui);
            }
        }
    }

    // Mostrar texto ou placeholder
    const char *display_text = (strlen(text) > 0) ? text : "All Countries";
    Color text_color = (strlen(text) > 0) ? TEXT_COLOR : (Color){150, 150, 150, 255};

    DrawText(display_text, bounds.x + 8, bounds.y + 8, 14, text_color);

    // Cursor piscante se ativo
    if (*is_active && ((int)(GetTime() * 2) % 2)) {
        int text_width = MeasureText(text, 14);
        DrawText("|", bounds.x + 8 + text_width, bounds.y + 8, 14, TEXT_COLOR);
    }

    return text_changed;
}

// Função para converter dados do esquema estrela para GUI
void LoadDataFromStarSchema(DisasterGUI *gui, DataWarehouse *dw) {
    if (!dw || dw->fact_count == 0) {
        printf("Nenhum dado disponível no data warehouse\n");
        return;
    }

    gui->disaster_count = dw->fact_count;
    gui->disasters = malloc(gui->disaster_count * sizeof(DisasterRecord));
    gui->filtered_disasters = malloc(MAX_DISASTERS * sizeof(DisasterRecord));

    if (!gui->disasters || !gui->filtered_disasters) {
        printf("Erro ao alocar memória para dados da GUI\n");
        return;
    }

    printf("Convertendo %d fatos do esquema estrela para GUI...\n", dw->fact_count);

    // Converte cada fato para formato da GUI
    for (int i = 0; i < dw->fact_count; i++) {
        DisasterFact *fact = &dw->fact_table[i];
        DisasterRecord *record = &gui->disasters[i];

        // Busca informações das dimensões
        DimTime *time_dim = NULL;
        DimGeography *geo_dim = NULL;
        DimDisasterType *type_dim = NULL;

        // Encontra dimensão tempo
        for (int j = 0; j < dw->time_count; j++) {
            if (dw->dim_time[j].time_key == fact->time_key) {
                time_dim = &dw->dim_time[j];
                break;
            }
        }

        // Encontra dimensão geografia
        for (int j = 0; j < dw->geography_count; j++) {
            if (dw->dim_geography[j].geography_key == fact->geography_key) {
                geo_dim = &dw->dim_geography[j];
                break;
            }
        }

        // Encontra dimensão tipo de desastre
        for (int j = 0; j < dw->disaster_type_count; j++) {
            if (dw->dim_disaster_type[j].disaster_type_key == fact->disaster_type_key) {
                type_dim = &dw->dim_disaster_type[j];
                break;
            }
        }

        // Preenche dados do registro com validação
        if (geo_dim) {
            strncpy(record->country, geo_dim->country, sizeof(record->country) - 1);
            record->country[sizeof(record->country) - 1] = '\0';
            strncpy(record->region, geo_dim->region, sizeof(record->region) - 1);
            record->region[sizeof(record->region) - 1] = '\0';
            strncpy(record->subregion, geo_dim->subregion, sizeof(record->subregion) - 1);
            record->subregion[sizeof(record->subregion) - 1] = '\0';
        } else {
            strcpy(record->country, "Unknown");
            strcpy(record->region, "Unknown");
            strcpy(record->subregion, "Unknown");
        }

        if (type_dim) {
            strncpy(record->disaster_type, type_dim->disaster_type, sizeof(record->disaster_type) - 1);
            record->disaster_type[sizeof(record->disaster_type) - 1] = '\0';
            strncpy(record->disaster_group, type_dim->disaster_group, sizeof(record->disaster_group) - 1);
            record->disaster_group[sizeof(record->disaster_group) - 1] = '\0';
            strncpy(record->disaster_subgroup, type_dim->disaster_subgroup, sizeof(record->disaster_subgroup) - 1);
            record->disaster_subgroup[sizeof(record->disaster_subgroup) - 1] = '\0';
            strncpy(record->disaster_subtype, type_dim->disaster_subtype, sizeof(record->disaster_subtype) - 1);
            record->disaster_subtype[sizeof(record->disaster_subtype) - 1] = '\0';
        } else {
            strcpy(record->disaster_type, "Unknown");
            strcpy(record->disaster_group, "Unknown");
            strcpy(record->disaster_subgroup, "Unknown");
            strcpy(record->disaster_subtype, "Unknown");
        }

        record->start_year = time_dim ? time_dim->start_year : 0;
        record->start_month = time_dim ? time_dim->start_month : 1;
        record->start_day = time_dim ? time_dim->start_day : 1;
        record->end_year = time_dim ? time_dim->end_year : 0;
        record->end_month = time_dim ? time_dim->end_month : 1;
        record->end_day = time_dim ? time_dim->end_day : 1;

        record->total_deaths = fact->total_deaths;
        record->total_affected = fact->total_affected;
        record->total_damage = fact->total_damage;
    }

    // Extrair países únicos
    gui->country_count = 0;
    strcpy(gui->countries[gui->country_count++], "All Countries");

    for (int i = 0; i < gui->disaster_count; i++) {
        bool found = false;
        for (int j = 1; j < gui->country_count; j++) {
            if (strcmp(gui->countries[j], gui->disasters[i].country) == 0) {
                found = true;
                break;
            }
        }
        if (!found && gui->country_count < MAX_COUNTRIES) {
            strcpy(gui->countries[gui->country_count++], gui->disasters[i].country);
        }
    }

    printf("Países únicos extraídos: %d\n", gui->country_count);

    // Extrair tipos de desastre únicos
    gui->disaster_type_count = 0;
    strcpy(gui->disaster_types[gui->disaster_type_count++], "All Types");

    for (int i = 0; i < gui->disaster_count; i++) {
        bool found = false;
        for (int j = 1; j < gui->disaster_type_count; j++) {
            if (strcmp(gui->disaster_types[j], gui->disasters[i].disaster_type) == 0) {
                found = true;
                break;
            }
        }
        if (!found && gui->disaster_type_count < MAX_DISASTER_TYPES) {
            strcpy(gui->disaster_types[gui->disaster_type_count++], gui->disasters[i].disaster_type);
        }
    }

    printf("Tipos de desastre únicos extraídos: %d\n", gui->disaster_type_count);

    // Encontrar intervalo de anos para o slider duplo
    int min_year = 9999, max_year = 0;
    for (int i = 0; i < gui->disaster_count; i++) {
        if (gui->disasters[i].start_year > 0) {
            if (gui->disasters[i].start_year < min_year) min_year = gui->disasters[i].start_year;
            if (gui->disasters[i].start_year > max_year) max_year = gui->disasters[i].start_year;
        }
    }

    // Inicializar filtros
    gui->selected_country = 0;
    gui->selected_disaster_type = 0;
    gui->min_year = min_year > 0 ? min_year : 1900;
    gui->max_year = max_year > 0 ? max_year : 2025;
    gui->start_year = gui->min_year;
    gui->end_year = gui->max_year;
    gui->start_year_slider_active = false;
    gui->end_year_slider_active = false;
    gui->country_dropout_open = false;
    gui->type_dropdown_open = false;
    gui->scroll_offset = 0;
    gui->table_scroll_y = 0;
    memset(gui->country_input, 0, sizeof(gui->country_input));
    gui->country_input_active = false;

    printf("Intervalo de anos: %d - %d\n", gui->min_year, gui->max_year);
    printf("Dados convertidos com sucesso para a GUI\n");
}

// Função para carregar dados
int load_and_convert_to_star_schema(const char *binary_filename, DataWarehouse **dw) {
    FILE *file = fopen(binary_filename, "rb");
    if (!file) {
        printf("Erro ao abrir arquivo: %s\n", binary_filename);
        return 0;
    }

    // Lê o número total de registros
    int total_records;
    if (fread(&total_records, sizeof(int), 1, file) != 1) {
        printf("Erro ao ler número de registros\n");
        fclose(file);
        return 0;
    }

    printf("Arquivo contém %d registros\n", total_records);

    // Cria o data warehouse
    *dw = dw_create();
    if (!*dw) {
        printf("Erro ao criar data warehouse\n");
        fclose(file);
        return 0;
    }

    // Lê cada registro original e converte
    OriginalDisaster disaster;
    int converted_count = 0;
    int error_count = 0;

    printf("Convertendo registros para esquema estrela...\n");

    for (int i = 0; i < total_records; i++) {
        if (fread(&disaster, sizeof(OriginalDisaster), 1, file) == 1) {
            // Validar dados antes de converter
            if (strlen(disaster.country) > 0 && strlen(disaster.disaster_type) > 0 &&
                disaster.start_year > 1900 && disaster.start_year < 2030) {

                if (dw_convert_from_original(*dw, &disaster)) {
                    converted_count++;
                } else {
                    error_count++;
                }
            } else {
                error_count++;
            }
        } else {
            printf("Erro ao ler registro %d\n", i);
            break;
        }

        // Mostrar progresso a cada 1000 registros
        if ((i + 1) % 1000 == 0) {
            printf("Processados %d/%d registros (%.1f%%)\n",
                   i + 1, total_records, ((float)(i + 1) / total_records) * 100);
        }
    }

    fclose(file);

    printf("Conversão concluída:\n");
    printf("   - Registros convertidos: %d\n", converted_count);
    printf("   - Erros encontrados: %d\n", error_count);
    printf("   - Taxa de sucesso: %.1f%%\n",
           total_records > 0 ? ((float)converted_count / total_records) * 100 : 0);

    // Imprimir estatísticas detalhadas do data warehouse
    if (converted_count > 0) {
        dw_print_statistics(*dw);
    }

    return converted_count > 0 ? 1 : 0;
}

// Limpeza dos Índices
void CleanupOptimizedSystem(DisasterGUI *gui) {
    if (gui && gui->optimized_dw) {
        printf("Limpando sistema de índices...\n");

        // Limpeza do cache
        if (gui->optimized_dw->cache) {
            cache_cleanup_expired(gui->optimized_dw->cache);
            cache_print_statistics(gui->optimized_dw->cache);
        }

        optimized_dw_destroy(gui->optimized_dw);
        gui->optimized_dw = NULL;
        gui->use_optimized_queries = false;

        printf("Sistema de índices limpo com sucesso\n");
    }
}

// Função principal
int main() {
    const char *binary_filename = "desastres.bin";
    DataWarehouse *dw = NULL;
    DisasterGUI *gui = InitializeGUI();

    if (!gui) {
        printf("Erro ao inicializar interface gráfica\n");
        return -1;
    }

    printf("Iniciando Disaster Analysis Dashboard com Sistema de Ordenação B+ Tree\n");
    printf("Procurando arquivo: %s\n", binary_filename);

    // Tenta carregar dados originais e converter para esquema estrela
    if (load_and_convert_to_star_schema(binary_filename, &dw)) {
        printf("Dados carregados com sucesso do arquivo binário\n");

        // Verifica se os dados foram carregados corretamente
        if (!dw || dw->fact_count == 0) {
            printf("Nenhum dado válido foi carregado\n");
            CleanupGUI(gui);
            if (dw) dw_destroy(dw);
            return -1;
        }

        printf("=== ESTATÍSTICAS DOS DADOS CARREGADOS ===\n");
        printf("Facts: %d\n", dw->fact_count);
        printf("Dimensões geográficas: %d\n", dw->geography_count);
        printf("Tipos de desastre: %d\n", dw->disaster_type_count);
        printf("Dimensões temporais: %d\n", dw->time_count);

        // Carrega dados na GUI
        LoadDataFromStarSchema(gui, dw);

        printf("=== DADOS NA INTERFACE ===\n");
        printf("Registros na GUI: %d\n", gui->disaster_count);
        printf("Países únicos: %d\n", gui->country_count);
        printf("Tipos de desastre únicos: %d\n", gui->disaster_type_count);
        printf("Intervalo de anos: %d - %d\n", gui->min_year, gui->max_year);

        // Tenta inicializar sistema de índices
        if (InitializeOptimizedSystem(gui, dw)) {
            printf("Sistema otimizado ativo - consultas aceleradas!\n");
        } else {
            printf("Sistema otimizado não disponível - usando consultas convencionais\n");
            gui->use_optimized_queries = false;
        }

    } else {
        printf("Arquivo binário não encontrado ou corrompido\n");
        printf("Certifique-se de que o arquivo %s existe e está no formato correto\n", binary_filename);

        // Não prosseguir sem dados
        CleanupGUI(gui);
        return -1;
    }

    // Inicializa interface gráfica
    printf("Inicializando interface gráfica...\n");
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Disaster Analysis Dashboard - Sistema com Ordenação B+ Tree e Slider de Data");
    SetTargetFPS(60);

    // Aplica filtros iniciais
    printf("Aplicando filtros iniciais...\n");
    ApplyFilters(gui);

    printf("Sistema pronto! Interface carregada com %d registros filtrados\n", gui->filtered_count);

    // Loop principal da interface
    while (!WindowShouldClose()) {
        bool filters_changed = false;

        BeginDrawing();
        ClearBackground(BACKGROUND_COLOR);

        // Divide tela em seções (ajustadas para mais controles)
        Rectangle header_rect = {0, 0, SCREEN_WIDTH, 60};
        Rectangle filter_rect = {0, 60, SCREEN_WIDTH, 150};  // Aumentado para acomodar controles de ordenação
        Rectangle chart_rect = {SCREEN_WIDTH/2, 210, SCREEN_WIDTH/2, 290};  // Ajustado
        Rectangle stats_rect = {0, 210, SCREEN_WIDTH/2 - 600, 290};  // Ajustado
        Rectangle table_rect = {0, 500, SCREEN_WIDTH, SCREEN_HEIGHT - 500};  // Ajustado
        Rectangle disaster_types_rect = {SCREEN_WIDTH/2 - 600, 210, 600, 290};  // Ajustado

        // Desenha componentes
        DrawApplicationHeader(header_rect);
        DrawFilterControls(filter_rect, gui, &filters_changed);
        DrawBarChart(chart_rect, gui->country_stats, gui->country_stats_count, gui);
        DrawDetailedStatsPanel(stats_rect, gui);
        DrawDataTable(table_rect, gui->filtered_disasters, gui->filtered_count, &gui->table_scroll_y, gui, &filters_changed);
        DrawDisasterTypeList(disaster_types_rect, gui, &filters_changed);

        // Atualiza dados filtrados se necessário
        if (filters_changed) {
            printf("Aplicando novos filtros e ordenação...\n");
            ApplyFilters(gui);
        }

        // Limpeza periódica do cache (a cada 5 minutos)
        static time_t last_cache_cleanup = 0;
        time_t current_time = time(NULL);
        if (current_time - last_cache_cleanup > 300) { // 5 minutos
            if (gui->optimized_dw && gui->optimized_dw->cache) {
                cache_cleanup_expired(gui->optimized_dw->cache);
            }
            last_cache_cleanup = current_time;
        }

        EndDrawing();
    }

    // Limpeza final
    printf("Limpando recursos...\n");
    CleanupOptimizedSystem(gui);
    CleanupGUI(gui);
    if (dw) {
        dw_destroy(dw);
    }
    CloseWindow();

    printf("Aplicação encerrada com sucesso!\n");
    return 0;
}
