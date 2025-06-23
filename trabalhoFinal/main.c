// =============================================================================
// main.c - Programa principal integrado
// =============================================================================

#include "disaster.h"
#include "bplus.h"
#include "trie.h"

// Função para construir todos os índices
int build_all_indexes(const char *binary_filename, BPlusTree **year_index,
                     BPlusTree **damage_index, Trie **country_index, Trie **type_index) {
    FILE *file = fopen(binary_filename, "rb");
    if (!file) {
        printf("Erro: Não foi possível abrir %s\n", binary_filename);
        return 0;
    }

    // Lê o número total de registros
    int total_records;
    fread(&total_records, sizeof(int), 1, file);

    // Cria todos os índices
    *year_index = bplus_create("year_index.dat");
    *damage_index = bplus_create("damage_index.dat");
    *country_index = trie_create("country_index.dat");
    *type_index = trie_create("type_index.dat");

    if (!*year_index || !*damage_index || !*country_index || !*type_index) {
        printf("Erro ao criar índices\n");
        fclose(file);
        return 0;
    }

    printf("Construindo todos os índices para %d registros...\n", total_records);

    Disaster disaster;
    long file_pos = sizeof(int);

    for (int i = 0; i < total_records; i++) {
        if (fread(&disaster, sizeof(Disaster), 1, file) == 1) {
            // Índices B+
            if (disaster.start_year > 0) {
                bplus_insert(*year_index, disaster.start_year, file_pos);
            }

            if (disaster.total_damage > 0) {
                int damage_millions = (int)(disaster.total_damage / 1000);
                bplus_insert(*damage_index, damage_millions, file_pos);
            }

            // Índices TRIE
            if (strlen(disaster.country) > 0) {
                trie_insert(*country_index, disaster.country, file_pos);
            }

            if (strlen(disaster.disaster_type) > 0) {
                trie_insert(*type_index, disaster.disaster_type, file_pos);
            }

            file_pos += sizeof(Disaster);

            if ((i + 1) % 1000 == 0) {
                printf("Processados %d registros...\n", i + 1);
            }
        }
    }

    fclose(file);
    printf("✅ Todos os índices construídos com sucesso!\n");
    return 1;
}

// Função para buscar desastres por país
void search_by_country(Trie *country_index, const char *binary_filename, const char *country) {
    printf("\n🌍 Buscando desastres no país: %s\n", country);

    int count;
    long *positions = trie_search(country_index, country, &count);

    if (count == 0) {
        printf("Nenhum desastre encontrado para '%s'\n", country);
        printf("Tentando busca por prefixo...\n");

        // Tenta busca por prefixo
        positions = trie_prefix_search(country_index, country, &count);
        if (count == 0) {
            printf("Nenhum resultado encontrado\n");
            return;
        }
        printf("Encontrados %d registros com prefixo '%s':\n", count, country);
    } else {
        printf("Encontrados %d registros exatos:\n", count);
    }

    FILE *file = fopen(binary_filename, "rb");
    if (!file) {
        printf("Erro ao abrir arquivo de dados\n");
        free(positions);
        return;
    }

    Disaster disaster;
    for (int i = 0; i < count && i < 10; i++) {
        fseek(file, positions[i], SEEK_SET);
        if (fread(&disaster, sizeof(Disaster), 1, file) == 1) {
            printf("--- Registro %d ---\n", i + 1);
            printf("País: %s\n", disaster.country);
            printf("Tipo: %s\n", disaster.disaster_type);
            printf("Ano: %d\n", disaster.start_year);
            printf("Mortes: %d\n", disaster.total_deaths);
            printf("Afetados: %lld\n", disaster.total_affected);
            printf("\n");
        }
    }

    if (count > 10) {
        printf("... e mais %d registros\n", count - 10);
    }

    fclose(file);
    free(positions);
}

// Função para buscar desastres por tipo
void search_by_type(Trie *type_index, const char *binary_filename, const char *type) {
    printf("\n🌪️ Buscando desastres do tipo: %s\n", type);

    int count;
    long *positions = trie_search(type_index, type, &count);

    if (count == 0) {
        printf("Tipo não encontrado. Buscando por prefixo...\n");
        positions = trie_prefix_search(type_index, type, &count);
        if (count == 0) {
            printf("Nenhum resultado encontrado\n");
            return;
        }
    }

    printf("Encontrados %d registros\n", count);

    FILE *file = fopen(binary_filename, "rb");
    if (!file) {
        printf("Erro ao abrir arquivo de dados\n");
        free(positions);
        return;
    }

    Disaster disaster;
    for (int i = 0; i < count && i < 10; i++) {
        fseek(file, positions[i], SEEK_SET);
        if (fread(&disaster, sizeof(Disaster), 1, file) == 1) {
            printf("--- Registro %d ---\n", i + 1);
            printf("País: %s\n", disaster.country);
            printf("Tipo: %s\n", disaster.disaster_type);
            printf("Ano: %d\n", disaster.start_year);
            printf("Danos: %lld mil US$\n", disaster.total_damage);
            printf("\n");
        }
    }

    if (count > 10) {
        printf("... e mais %d registros\n", count - 10);
    }

    fclose(file);
    free(positions);
}

int main() {
    printf("=== SISTEMA COMPLETO DE ANÁLISE DE DESASTRES ===\n\n");

    const char *binary_filename = "desastres.bin";
    BPlusTree *year_index = NULL;
    BPlusTree *damage_index = NULL;
    Trie *country_index = NULL;
    Trie *type_index = NULL;

    // Constrói todos os índices
    if (!build_all_indexes(binary_filename, &year_index, &damage_index,
                          &country_index, &type_index)) {
        printf("Falha ao construir índices\n");
        return 1;
    }

    // Estatísticas dos índices
    printf("\n📊 ESTATÍSTICAS DOS ÍNDICES:\n");
    printf("\n--- Índices B+ ---\n");
    bplus_print_tree(year_index);

    printf("\n--- Índices TRIE ---\n");
    trie_print_stats(country_index);
    trie_print_stats(type_index);

    // Exemplos de busca textual
    printf("\n🔍 EXEMPLOS DE BUSCA TEXTUAL:\n");
    search_by_country(country_index, binary_filename, "Brazil");
    search_by_country(country_index, binary_filename, "United States");
    search_by_type(type_index, binary_filename, "Flood");
    search_by_type(type_index, binary_filename, "Earthquake");

    // Mostra alguns países disponíveis
    printf("\n📋 Exemplos de países no índice:\n");
    trie_print_words_with_prefix(country_index, "B");

    printf("\n📋 Exemplos de tipos de desastre:\n");
    trie_print_words_with_prefix(type_index, "F");

    // Salva todos os índices
    bplus_save_to_file(year_index);
    bplus_save_to_file(damage_index);
    trie_save_to_file(country_index);
    trie_save_to_file(type_index);

    // Limpa memória
    bplus_destroy(year_index);
    bplus_destroy(damage_index);
    trie_destroy(country_index);
    trie_destroy(type_index);

    printf("\n✅ Sistema completo executado com sucesso!\n");
    printf("🎯 Próximo passo: Interface gráfica com raylib\n");

    return 0;
}
