#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>

//Headers específicos do projeto
#include "disaster.h"
#include "disaster_star_schema.h"
#include "bplus.h"
#include "trie.h"

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

// Estrutura para armazenar dados de desastre
typedef struct {
    char country[50];
    char disaster_type[50];
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

// Estrutura para estatísticas por país
typedef struct {
    char country[50];
    long long total_affected;
    int disaster_count;
} CountryStats;

// Estrutura para interface
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

    // Estados da interface
    bool country_dropdown_open;
    bool type_dropdown_open;
    int scroll_offset;
    int table_scroll_y;

    // Estatísticas calculadas
    long long total_affected_filtered;
    int total_deaths_filtered;
    long long total_damage_filtered;

    // Stats por país para gráfico
    CountryStats country_stats[MAX_COUNTRIES];
    int country_stats_count;

} DisasterGUI;

// =============================================================================
// IMPLEMENTAÇÕES DAS FUNÇÕES
// =============================================================================

// Inicializar estrutura GUI
DisasterGUI* InitializeGUI() {
    DisasterGUI *gui = malloc(sizeof(DisasterGUI));
    if (!gui) return NULL;

    memset(gui, 0, sizeof(DisasterGUI));
    return gui;
}

// Limpar recursos da GUI
void CleanupGUI(DisasterGUI *gui) {
    if (!gui) return;

    if (gui->disasters) free(gui->disasters);
    if (gui->filtered_disasters) free(gui->filtered_disasters);
    free(gui);
}

// Aplicar filtros aos dados
void ApplyFilters(DisasterGUI *gui) {
    if (!gui || !gui->disasters) return;

    gui->filtered_count = 0;
    gui->total_affected_filtered = 0;
    gui->total_deaths_filtered = 0;
    gui->total_damage_filtered = 0;

    for (int i = 0; i < gui->disaster_count; i++) {
        DisasterRecord *record = &gui->disasters[i];
        bool include = true;

        // Filtro por país
        if (gui->selected_country > 0) {
            if (strcmp(record->country, gui->countries[gui->selected_country]) != 0) {
                include = false;
            }
        }

        // Filtro por tipo de desastre
        if (gui->selected_disaster_type > 0) {
            if (strcmp(record->disaster_type, gui->disaster_types[gui->selected_disaster_type]) != 0) {
                include = false;
            }
        }

        // Filtro por ano
        if (record->start_year < gui->start_year || record->start_year > gui->end_year) {
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

    // Calcular estatísticas por país
    gui->country_stats_count = 0;
    for (int i = 0; i < gui->filtered_count && gui->country_stats_count < MAX_COUNTRIES; i++) {
        DisasterRecord *record = &gui->filtered_disasters[i];

        // Procurar se o país já existe nas estatísticas
        int country_idx = -1;
        for (int j = 0; j < gui->country_stats_count; j++) {
            if (strcmp(gui->country_stats[j].country, record->country) == 0) {
                country_idx = j;
                break;
            }
        }

        if (country_idx == -1) {
            // Novo país
            strcpy(gui->country_stats[gui->country_stats_count].country, record->country);
            gui->country_stats[gui->country_stats_count].total_affected = record->total_affected;
            gui->country_stats[gui->country_stats_count].disaster_count = 1;
            gui->country_stats_count++;
        } else {
            // País existente
            gui->country_stats[country_idx].total_affected += record->total_affected;
            gui->country_stats[country_idx].disaster_count++;
        }
    }
}

bool DrawDropdown(Rectangle bounds, const char *label, char options[][50], int option_count, int *selected, bool *open);

// Desenhar cabeçalho da aplicação
void DrawApplicationHeader(Rectangle bounds) {
    DrawRectangleRec(bounds, PRIMARY_COLOR);
    DrawText("Disaster Analysis Dashboard", bounds.x + 20, bounds.y + 15, 24, WHITE);
    DrawText("Data Warehouse System", bounds.x + 20, bounds.y + 35, 14, (Color){200, 200, 200, 255});
}

// Desenhar controles de filtro
void DrawFilterControls(Rectangle bounds, DisasterGUI *gui, bool *filters_changed) {
    DrawRectangleRec(bounds, PANEL_COLOR);
    DrawRectangleLinesEx(bounds, 1, BORDER_COLOR);

    // Dropdown para países
    Rectangle country_rect = {bounds.x + 20, bounds.y + 20, 300, 30};
    const char *country_text = gui->countries[gui->selected_country];

    if (DrawDropdown(country_rect, country_text, gui->countries,
                          gui->country_count, &gui->selected_country, &gui->country_dropdown_open)) {
        *filters_changed = true;
    }

    // Dropdown para tipos de desastre
    Rectangle type_rect = {bounds.x + 240, bounds.y + 20, 300, 30};
    const char *type_text = gui->disaster_types[gui->selected_disaster_type];

    if (DrawDropdown(type_rect, type_text, gui->disaster_types,
                          gui->disaster_type_count, &gui->selected_disaster_type, &gui->type_dropdown_open)) {
        *filters_changed = true;
    }

    // Controles de ano
    DrawText("Start Year:", bounds.x + 20, bounds.y + 70, 14, TEXT_COLOR);
    DrawText(TextFormat("%d", gui->start_year), bounds.x + 100, bounds.y + 70, 14, TEXT_COLOR);

    DrawText("End Year:", bounds.x + 240, bounds.y + 70, 14, TEXT_COLOR);
    DrawText(TextFormat("%d", gui->end_year), bounds.x + 320, bounds.y + 70, 14, TEXT_COLOR);
}

// Desenhar gráfico de barras
void DrawBarChart(Rectangle bounds, CountryStats *stats, int count) {
    DrawRectangleRec(bounds, PANEL_COLOR);
    DrawRectangleLinesEx(bounds, 1, BORDER_COLOR);
    DrawText("Top Countries by Total Affected", bounds.x + 10, bounds.y + 10, 16, TEXT_COLOR);

    if (count == 0) return;

    // Encontrar valor máximo
    long long max_affected = 0;
    for (int i = 0; i < count; i++) {
        if (stats[i].total_affected > max_affected) {
            max_affected = stats[i].total_affected;
        }
    }

    if (max_affected == 0) return;

    // Desenhar barras (máximo 10)
    int bars_to_show = count > 10 ? 10 : count;
    float bar_height = (bounds.height - 60) / bars_to_show;

    for (int i = 0; i < bars_to_show; i++) {
        float bar_width = (stats[i].total_affected / (float)max_affected) * (bounds.width - 200);

        Rectangle bar_rect = {bounds.x + 150, bounds.y + 40 + i * bar_height,
                             bar_width, bar_height - 5};

        Color bar_color = {52 + (i * 20) % 150, 152, 219, 255};
        DrawRectangleRec(bar_rect, bar_color);

        // Nome do país
        DrawText(stats[i].country, bounds.x + 10, bounds.y + 42 + i * bar_height, 12, TEXT_COLOR);

        // Valor
        char value_text[50];
        if (stats[i].total_affected >= 1000000) {
            sprintf(value_text, "%.1fM", stats[i].total_affected / 1000000.0);
        } else if (stats[i].total_affected >= 1000) {
            sprintf(value_text, "%.1fK", stats[i].total_affected / 1000.0);
        } else {
            sprintf(value_text, "%lld", stats[i].total_affected);
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
        sprintf(damage_str, "$%.1fK", gui->total_damage_filtered / 1000.0);
    } else {
        sprintf(damage_str, "$%lld", gui->total_damage_filtered);
    }
    DrawText(TextFormat("Total Damage: %s", damage_str),
             bounds.x + 20, bounds.y + y_offset, 14, TEXT_COLOR);
}

// Desenhar tabela de dados
void DrawDataTable(Rectangle bounds, DisasterRecord *records, int count, int *scroll_y) {
    DrawRectangleRec(bounds, PANEL_COLOR);
    DrawRectangleLinesEx(bounds, 1, BORDER_COLOR);
    DrawText("Disaster Records", bounds.x + 10, bounds.y + 10, 16, TEXT_COLOR);

    if (count == 0) {
        DrawText("No records found with current filters",
                 bounds.x + 20, bounds.y + 50, 14, TEXT_COLOR);
        return;
    }

    // Cabeçalho da tabela
    Rectangle header_rect = {bounds.x, bounds.y + 35, bounds.width, 25};
    DrawRectangleRec(header_rect, (Color){240, 240, 245, 255});
    DrawText("Country", bounds.x + 10, bounds.y + 40, 12, TEXT_COLOR);
    DrawText("Disaster Type", bounds.x + 150, bounds.y + 40, 12, TEXT_COLOR);
    DrawText("Year", bounds.x + 300, bounds.y + 40, 12, TEXT_COLOR);
    DrawText("Deaths", bounds.x + 350, bounds.y + 40, 12, TEXT_COLOR);
    DrawText("Affected", bounds.x + 420, bounds.y + 40, 12, TEXT_COLOR);
    DrawText("Damage", bounds.x + 520, bounds.y + 40, 12, TEXT_COLOR);

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

        DrawText(record->country, bounds.x + 10, y_pos, 11, TEXT_COLOR);
        DrawText(record->disaster_type, bounds.x + 150, y_pos, 11, TEXT_COLOR);
        DrawText(TextFormat("%d", record->start_year), bounds.x + 300, y_pos, 11, TEXT_COLOR);
        DrawText(TextFormat("%d", record->total_deaths), bounds.x + 350, y_pos, 11, TEXT_COLOR);

        // Formatar números grandes
        if (record->total_affected >= 1000000) {
            DrawText(TextFormat("%.1fM", record->total_affected / 1000000.0),
                     bounds.x + 420, y_pos, 11, TEXT_COLOR);
        } else if (record->total_affected >= 1000) {
            DrawText(TextFormat("%.1fK", record->total_affected / 1000.0),
                     bounds.x + 420, y_pos, 11, TEXT_COLOR);
        } else {
            DrawText(TextFormat("%lld", record->total_affected),
                     bounds.x + 420, y_pos, 11, TEXT_COLOR);
        }

        if (record->total_damage >= 1000000) {
            DrawText(TextFormat("$%.1fM", record->total_damage / 1000000.0),
                     bounds.x + 520, y_pos, 11, TEXT_COLOR);
        } else if (record->total_damage >= 1000) {
            DrawText(TextFormat("$%.1fK", record->total_damage / 1000.0),
                     bounds.x + 520, y_pos, 11, TEXT_COLOR);
        } else {
            DrawText(TextFormat("$%lld", record->total_damage),
                     bounds.x + 520, y_pos, 11, TEXT_COLOR);
        }
    }
}

// Função para desenhar dropdown
bool DrawDropdown(Rectangle bounds, const char *text, char items[][50],
                       int item_count, int *selected_index, bool *is_open) {

//**DEBUG ITENS NO DROPDOWN:
    printf("DrawDropdown called with item_count: %d\n", item_count);
    if (item_count > 0) {
        printf("First item: '%s'\n", items[0]);
        printf("Last item: '%s'\n", items[item_count-1]);
    }

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

// Função para converter dados do esquema estrela para GUI
void LoadDataFromStarSchema(DisasterGUI *gui, DataWarehouse *dw) {
    if (!dw || dw->fact_count == 0) {
        return;
    }

    gui->disaster_count = dw->fact_count;
    gui->disasters = malloc(gui->disaster_count * sizeof(DisasterRecord));
    gui->filtered_disasters = malloc(MAX_DISASTERS * sizeof(DisasterRecord));

    if (!gui->disasters || !gui->filtered_disasters) {
        return;
    }

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

        // Preenche dados do registro
        strcpy(record->country, geo_dim ? geo_dim->country : "Unknown");
        strcpy(record->disaster_type, type_dim ? type_dim->disaster_type : "Unknown");

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

//**DEBUG PAÍSES:
printf("=== UNIQUE EXTRACTION DEBUG ===\n");
printf("Countries extracted (%d total):\n", gui->country_count);
for (int i = 0; i < gui->country_count && i < 20; i++) {
    printf("  %d: '%s'\n", i, gui->countries[i]);
}

// Também verificar dados brutos
printf("\nSample of raw disaster records:\n");
for (int i = 0; i < (gui->disaster_count > 20 ? 20 : gui->disaster_count); i++) {
    printf("  Record %d: '%s' - '%s' - %d\n",
           i, gui->disasters[i].country, gui->disasters[i].disaster_type, gui->disasters[i].start_year);
}

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

//**DEBUG DESASTRES TIPO:
printf("\nDisaster types extracted (%d total):\n", gui->disaster_type_count);
for (int i = 0; i < gui->disaster_type_count && i < 35; i++) {
    printf("  %d: '%s'\n", i, gui->disaster_types[i]);
}

    // Encontrar intervalo de anos
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
    gui->start_year = min_year > 0 ? min_year : 1900;
    gui->end_year = max_year > 0 ? max_year : 2025;
    gui->country_dropdown_open = false;
    gui->type_dropdown_open = false;
    gui->scroll_offset = 0;
    gui->table_scroll_y = 0;
}

// Função para carregar dados originais e converter para esquema estrela
int load_and_convert_to_star_schema(const char *binary_filename, DataWarehouse **dw) {
    FILE *file = fopen(binary_filename, "rb");
    if (!file) {
        return 0;
    }

    // Lê o número total de registros
    int total_records;
    if (fread(&total_records, sizeof(int), 1, file) != 1) {
        fclose(file);
        return 0;
    }

//**DEBUGGIN TOTAL RECORDS:
    printf("Total records in file: %d\n", total_records);

    // Cria o data warehouse
    *dw = dw_create();
    if (!*dw) {
        fclose(file);
        return 0;
    }

    // Lê cada registro original e converte
    OriginalDisaster disaster;
    int converted_count = 0;

    for (int i = 0; i < total_records; i++) {
        if (fread(&disaster, sizeof(OriginalDisaster), 1, file) == 1) {
            if (dw_convert_from_original(*dw, &disaster)) {
                converted_count++;
            }
        } else {
            break;
        }
    }

//**DEBUGGIN REGISTROS CONVERTIDOS COM SUCESSO:
    printf("Successfully converted: %d out of %d records\n", converted_count, total_records);

    fclose(file);
    return converted_count > 0 ? 1 : 0;
}

// Função principal
int main() {
    const char *binary_filename = "desastres.bin";
    DataWarehouse *dw = NULL;
    DisasterGUI *gui = InitializeGUI();

    if (!gui) {
        return -1;
    }

    // Tentar carregar dados originais e converter para esquema estrela
    if (load_and_convert_to_star_schema(binary_filename, &dw)) {
        printf("✅ Dados carregados do arquivo binário com sucesso\n");

//**DEBUGGIN REGISTROS LIDOS COM SUCESSO:
    printf("=== DEBUG INFO ===\n");
    printf("DataWarehouse Facts: %d\n", dw->fact_count);
    printf("Geography Dimensions: %d\n", dw->geography_count);
    printf("Disaster Type Dimensions: %d\n", dw->disaster_type_count);
    printf("Time Dimensions: %d\n", dw->time_count);

        LoadDataFromStarSchema(gui, dw);

//**DEBUGGIN QUANTOS REGISTROS CHEGAM ATÉ A GUI:
    printf("GUI Disaster Count: %d\n", gui->disaster_count);
    printf("GUI Country Count: %d\n", gui->country_count);
    printf("GUI Disaster Type Count: %d\n", gui->disaster_type_count);
    printf("Year Range: %d - %d\n", gui->start_year, gui->end_year);
    printf("==================\n");

    } else {
        printf("⚠️ Arquivo binário não encontrado\n");
        // Usar implementação do disaster_gui.c
    }

    // Inicializa interface gráfica
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Disaster Analysis Dashboard");
    SetTargetFPS(60);

    // Aplica filtros iniciais
    ApplyFilters(gui);

    // Loop principal da interface
    while (!WindowShouldClose()) {
        bool filters_changed = false;

        BeginDrawing();
        ClearBackground(BACKGROUND_COLOR);

        // Dividir tela em seções
        Rectangle header_rect = {0, 0, SCREEN_WIDTH, 60};
        Rectangle filter_rect = {0, 60, SCREEN_WIDTH, 120};
        Rectangle chart_rect = {0, 180, SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 90};
        Rectangle stats_rect = {SCREEN_WIDTH/2, 180, SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 90};
        Rectangle table_rect = {0, SCREEN_HEIGHT/2 + 90, SCREEN_WIDTH, SCREEN_HEIGHT/2 - 90};

        // Desenhar componentes
        DrawApplicationHeader(header_rect);
        DrawFilterControls(filter_rect, gui, &filters_changed);
        DrawBarChart(chart_rect, gui->country_stats, gui->country_stats_count);
        DrawDetailedStatsPanel(stats_rect, gui);
        DrawDataTable(table_rect, gui->filtered_disasters, gui->filtered_count, &gui->table_scroll_y);

        // Atualizar dados filtrados se necessário
        if (filters_changed) {
            ApplyFilters(gui);
        }

        EndDrawing();
    }

    // Limpeza
    CleanupGUI(gui);
    if (dw) {
        dw_destroy(dw);
    }
    CloseWindow();

    return 0;
}
