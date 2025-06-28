// =============================================================================
// main.c - Programa principal com esquema estrela
// =============================================================================

#include "disaster.h"
#include "disaster_star_schema.h"
#include "bplus.h"
#include "trie.h"

// Função para carregar dados originais e converter para esquema estrela
int load_and_convert_to_star_schema(const char *binary_filename, DataWarehouse **dw) {
    FILE *file = fopen(binary_filename, "rb");
    if (!file) {
        printf("Erro: Não foi possível abrir %s\n", binary_filename);
        return 0;
    }

    // Lê o número total de registros
    int total_records;
    fread(&total_records, sizeof(int), 1, file);

    // Cria o data warehouse
    *dw = dw_create();
    if (!*dw) {
        printf("Erro ao criar data warehouse\n");
        fclose(file);
        return 0;
    }

    printf("Convertendo %d registros para esquema estrela...\n", total_records);

    // Lê cada registro original e converte
    OriginalDisaster disaster;
    for (int i = 0; i < total_records; i++) {
        if (fread(&disaster, sizeof(OriginalDisaster), 1, file) == 1) {
            // Converte para esquema estrela
            if (!dw_convert_from_original(*dw, &disaster)) {
                printf("Erro ao converter registro %d\n", i + 1);
                continue;
            }

            if ((i + 1) % 1000 == 0) {
                printf("Convertidos %d registros...\n", i + 1);
            }
        }
    }

    fclose(file);
    printf("✅ Conversão para esquema estrela concluída!\n");
    return 1;
}

// Função para construir índices sobre o esquema estrela
int build_star_schema_indexes(DataWarehouse *dw, BPlusTree **year_index,
                             BPlusTree **damage_index, Trie **country_index,
                             Trie **type_index) {

    // Cria todos os índices
    *year_index = bplus_create("star_year_index.dat");
    *damage_index = bplus_create("star_damage_index.dat");
    *country_index = trie_create("star_country_index.dat");
    *type_index = trie_create("star_type_index.dat");

    if (!*year_index || !*damage_index || !*country_index || !*type_index) {
        printf("Erro ao criar índices\n");
        return 0;
    }

    printf("Construindo índices sobre esquema estrela...\n");

    // Indexa a tabela fato
    for (int i = 0; i < dw->fact_count; i++) {
        DisasterFact *fact = &dw->fact_table[i];

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

        // Constrói índices
        if (time_dim) {
            bplus_insert(*year_index, time_dim->start_year, i);
        }

        if (fact->total_damage > 0) {
            int damage_millions = (int)(fact->total_damage / 1000);
            bplus_insert(*damage_index, damage_millions, i);
        }

        if (geo_dim && strlen(geo_dim->country) > 0) {
            trie_insert(*country_index, geo_dim->country, i);
        }

        if (type_dim && strlen(type_dim->disaster_type) > 0) {
            trie_insert(*type_index, type_dim->disaster_type, i);
        }

        if ((i + 1) % 1000 == 0) {
            printf("Indexados %d fatos...\n", i + 1);
        }
    }

    printf("✅ Índices construídos sobre esquema estrela!\n");
    return 1;
}

// Função para buscar no esquema estrela usando índices
void search_star_schema_by_country(DataWarehouse *dw, Trie *country_index,
                                  const char *country) {
    printf("\n🌍 Busca por país no esquema estrela: %s\n", country);

    int count;
    long *fact_positions = trie_search(country_index, country, &count);

    if (count == 0) {
        printf("Nenhum fato encontrado para '%s'\n", country);
        free(fact_positions);
        return;
    }

    printf("Encontrados %d fatos:\n", count);

    // Estatísticas
    long long total_deaths = 0;
    long long total_affected = 0;
    long long total_damage = 0;

    for (int i = 0; i < count && i < 10; i++) {
        int fact_index = fact_positions[i];
        DisasterFact *fact = &dw->fact_table[fact_index];

        // Busca informações das dimensões
        DimTime *time_dim = NULL;
        DimGeography *geo_dim = NULL;
        DimDisasterType *type_dim = NULL;
        DimEvent *event_dim = NULL;

        // Encontra dimensões relacionadas
        for (int j = 0; j < dw->time_count; j++) {
            if (dw->dim_time[j].time_key == fact->time_key) {
                time_dim = &dw->dim_time[j];
                break;
            }
        }

        for (int j = 0; j < dw->geography_count; j++) {
            if (dw->dim_geography[j].geography_key == fact->geography_key) {
                geo_dim = &dw->dim_geography[j];
                break;
            }
        }

        for (int j = 0; j < dw->disaster_type_count; j++) {
            if (dw->dim_disaster_type[j].disaster_type_key == fact->disaster_type_key) {
                type_dim = &dw->dim_disaster_type[j];
                break;
            }
        }

        for (int j = 0; j < dw->event_count; j++) {
            if (dw->dim_event[j].event_key == fact->event_key) {
                event_dim = &dw->dim_event[j];
                break;
            }
        }

        printf("--- Fato %d ---\n", i + 1);
        if (geo_dim) printf("País: %s (%s)\n", geo_dim->country, geo_dim->region);
        if (type_dim) printf("Tipo: %s\n", type_dim->disaster_type);
        if (time_dim) printf("Ano: %d\n", time_dim->start_year);
        if (event_dim) printf("Evento: %s\n", event_dim->event_name);
        printf("Mortes: %d\n", fact->total_deaths);
        printf("Afetados: %lld\n", fact->total_affected);
        printf("Danos: %lld\n", fact->total_damage);
        printf("\n");

        // Acumula estatísticas
        total_deaths += fact->total_deaths;
        total_affected += fact->total_affected;
        total_damage += fact->total_damage;
    }

    printf("📊 ESTATÍSTICAS PARA %s:\n", country);
    printf("Total de mortes: %lld\n", total_deaths);
    printf("Total de afetados: %lld\n", total_affected);
    printf("Total de danos: %lld mil US$\n", total_damage);

    if (count > 10) {
        printf("... e mais %d fatos\n", count - 10);
    }

    free(fact_positions);
}

// Função para análise OLAP usando esquema estrela
void perform_olap_analysis(DataWarehouse *dw) {
    printf("\n📊 ANÁLISE OLAP COM ESQUEMA ESTRELA:\n");

    // Consultas OLAP implementadas
    printf("\n--- Análise por Ano ---\n");
    dw_query_by_year(dw, 2020);

    printf("\n--- Análise por País ---\n");
    dw_query_by_country(dw, "Brazil");

    printf("\n--- Análise por Tipo de Desastre ---\n");
    dw_query_by_disaster_type(dw, "Flood");

    printf("\n--- Análise Combinada (Ano + País) ---\n");
    dw_query_summary_by_year_country(dw, 2019, "United States");

    // Agregações específicas
    printf("\n--- Agregações ---\n");
    printf("Danos totais em 2020: %lld mil US$\n",
           dw_total_damage_by_year(dw, 2020));
    printf("Pessoas afetadas no Brasil: %lld\n",
           dw_total_affected_by_country(dw, "Brazil"));
    printf("Mortes por terremotos: %d\n",
           dw_total_deaths_by_disaster_type(dw, "Earthquake"));
}

int main() {
    printf("=== SISTEMA DE ANÁLISE DE DESASTRES COM ESQUEMA ESTRELA ===\n\n");

    const char *binary_filename = "desastres.bin";
    DataWarehouse *dw = NULL;
    BPlusTree *year_index = NULL;
    BPlusTree *damage_index = NULL;
    Trie *country_index = NULL;
    Trie *type_index = NULL;

    // Etapa 1: Carrega dados originais e converte para esquema estrela
    if (!load_and_convert_to_star_schema(binary_filename, &dw)) {
        printf("Falha ao carregar e converter dados\n");
        return 1;
    }

    // Etapa 2: Mostra estatísticas do data warehouse
    printf("\n📊 ESTATÍSTICAS DO DATA WAREHOUSE:\n");
    dw_print_statistics(dw);

    // Etapa 3: Constrói índices sobre o esquema estrela
    if (!build_star_schema_indexes(dw, &year_index, &damage_index,
                                  &country_index, &type_index)) {
        printf("Falha ao construir índices\n");
        dw_destroy(dw);
        return 1;
    }

    // Etapa 4: Demonstra buscas usando índices no esquema estrela
    printf("\n🔍 BUSCAS COM ÍNDICES NO ESQUEMA ESTRELA:\n");
    search_star_schema_by_country(dw, country_index, "Brazil");
    search_star_schema_by_country(dw, country_index, "United States");

    // Etapa 5: Análise OLAP
    perform_olap_analysis(dw);

    // Etapa 6: Salva estruturas para reutilização
    printf("\n💾 SALVANDO ESTRUTURAS:\n");
    dw_save_to_files(dw, "star_schema");
    bplus_save_to_file(year_index);
    bplus_save_to_file(damage_index);
    trie_save_to_file(country_index);
    trie_save_to_file(type_index);

    // Etapa 7: Demonstra carregamento de estruturas salvas
    printf("\n📂 TESTANDO CARREGAMENTO DE ESTRUTURAS SALVAS:\n");
    DataWarehouse *loaded_dw = dw_load_from_files("star_schema");
    if (loaded_dw) {
        printf("✅ Data warehouse carregado com sucesso!\n");
        dw_print_statistics(loaded_dw);
        dw_destroy(loaded_dw);
    }

    // Limpa memória
    dw_destroy(dw);
    bplus_destroy(year_index);
    bplus_destroy(damage_index);
    trie_destroy(country_index);
    trie_destroy(type_index);

    printf("\n✅ Sistema com esquema estrela executado com sucesso!\n");
    printf("🎯 Benefícios do esquema estrela:\n");
    printf("   - Consultas OLAP otimizadas\n");
    printf("   - Separação entre dimensões e fatos\n");
    printf("   - Melhor performance para agregações\n");
    printf("   - Estrutura mais flexível para análises\n");

    return 0;
}
