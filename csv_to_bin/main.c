#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Estrutura para armazenar um registro de desastre
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
} Disaster;

// Função para limpar strings (remover aspas e espaços extras) - CORRIGIDA
void clean_string(char *str) {
    if (!str) return;

    int len = strlen(str);
    if (len == 0) return;

    // Remove aspas do início e fim - CORRIGIDO
    if (str[0] == '"') {
        memmove(str, str + 1, len); // memmove é seguro para sobreposição
        len--;
        if (len > 0) str[len] = '\0'; // Garante terminação
    }

    // Recalcula comprimento após possível mudança
    len = strlen(str);
    if (len > 0 && str[len - 1] == '"') {
        str[len - 1] = '\0';
        len--;
    }

    // Remove quebras de linha
    char *newline = strchr(str, '\n');
    if (newline) *newline = '\0';

    newline = strchr(str, '\r');
    if (newline) *newline = '\0';
}

// Função segura para copiar strings - NOVA
void safe_string_copy(char *dest, const char *src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) return;

    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0'; // Sempre termina com \0
}

// Função para converter string em inteiro (trata campos vazios)
int safe_atoi(const char *str) {
    if (!str || strlen(str) == 0) {
        return 0;
    }
    return atoi(str);
}

// Função para converter string em long long (trata campos vazios)
long long safe_atoll(const char *str) {
    if (!str || strlen(str) == 0) {
        return 0;
    }
    return atoll(str);
}

// Função para fazer parse de uma linha CSV - SIMPLIFICADA
int parse_csv_line(char *line, Disaster *disaster) {
    // Inicializa a estrutura
    memset(disaster, 0, sizeof(Disaster));

    int field = 0;
    int start = 0;
    int i = 0;

    // Percorre a string caractere por caractere
    while (line[i] != '\0' && field < 19) {
        // Verifica se chegou ao delimitador (| ou \n)
        if (line[i] == '|' || line[i] == '\n') {
            // Calcula o tamanho do campo
            int field_len = i - start;

            // Extrai o campo atual
            char field_content[512];

            if (field_len > 0) {
                // Copia o conteúdo do campo
                int copy_len = (field_len < 511) ? field_len : 511;
                strncpy(field_content, &line[start], copy_len);
                field_content[copy_len] = '\0';
                clean_string(field_content);
            } else {
                // Campo vazio
                field_content[0] = '\0';
            }

            // Atribui à estrutura baseado no número do campo
            switch (field) {
                case 0:
                    safe_string_copy(disaster->disaster_group, field_content, sizeof(disaster->disaster_group));
                    break;
                case 1:
                    safe_string_copy(disaster->disaster_subgroup, field_content, sizeof(disaster->disaster_subgroup));
                    break;
                case 2:
                    safe_string_copy(disaster->disaster_type, field_content, sizeof(disaster->disaster_type));
                    break;
                case 3:
                    safe_string_copy(disaster->disaster_subtype, field_content, sizeof(disaster->disaster_subtype));
                    break;
                case 4:
                    safe_string_copy(disaster->event_name, field_content, sizeof(disaster->event_name));
                    break;
                case 5:
                    safe_string_copy(disaster->country, field_content, sizeof(disaster->country));
                    break;
                case 6:
                    safe_string_copy(disaster->subregion, field_content, sizeof(disaster->subregion));
                    break;
                case 7:
                    safe_string_copy(disaster->region, field_content, sizeof(disaster->region));
                    break;
                case 8:
                    safe_string_copy(disaster->origin, field_content, sizeof(disaster->origin));
                    break;
                case 9:
                    safe_string_copy(disaster->associated_types, field_content, sizeof(disaster->associated_types));
                    break;
                case 10:
                    disaster->start_year = safe_atoi(field_content);
                    break;
                case 11:
                    disaster->start_month = safe_atoi(field_content);
                    break;
                case 12:
                    disaster->start_day = safe_atoi(field_content);
                    break;
                case 13:
                    disaster->end_year = safe_atoi(field_content);
                    break;
                case 14:
                    disaster->end_month = safe_atoi(field_content);
                    break;
                case 15:
                    disaster->end_day = safe_atoi(field_content);
                    break;
                case 16:
                    disaster->total_deaths = safe_atoi(field_content);
                    break;
                case 17:
                    disaster->total_affected = safe_atoll(field_content);
                    break;
                case 18:
                    disaster->total_damage = safe_atoll(field_content);
                    break;
            }

            field++;
            start = i + 1;

            // Se encontrou \n, para o processamento
            if (line[i] == '\n') {
                break;
            }
        }
        i++;
    }

    // Retorna 1 se processou exatamente 19 campos (0-18)
    return (field == 19);
}

// Função para ler linha longa de forma segura - MODIFICADA
char* read_long_line(FILE* file) {
    size_t buffer_size = 1024;
    size_t length = 0;
    char* buffer = malloc(buffer_size);

    if (!buffer) return NULL;

    int c;
    while ((c = fgetc(file)) != EOF) {
        // Expande buffer se necessário
        if (length >= buffer_size - 1) {
            buffer_size *= 2;
            char* new_buffer = realloc(buffer, buffer_size);
            if (!new_buffer) {
                free(buffer);
                return NULL;
            }
            buffer = new_buffer;
        }
        buffer[length++] = c;

        // Para no \n (mantém o \n na string para o parse)
        if (c == '\n') {
            break;
        }
    }

    // Se não leu nada e chegou ao EOF
    if (length == 0 && c == EOF) {
        free(buffer);
        return NULL;
    }

    buffer[length] = '\0';
    return buffer;
}

// Função principal para converter CSV para binário - MELHORADA
int convert_csv_to_binary(const char *csv_filename, const char *bin_filename) {
    FILE *csv_file = fopen(csv_filename, "r");
    if (!csv_file) {
        printf("Erro: Não foi possível abrir o arquivo CSV: %s\n", csv_filename);
        return 0;
    }

    FILE *bin_file = fopen(bin_filename, "wb");
    if (!bin_file) {
        printf("Erro: Não foi possível criar o arquivo binário: %s\n", bin_filename);
        fclose(csv_file);
        return 0;
    }

    // Reserva espaço para o contador no início
    int placeholder = 0;
    fwrite(&placeholder, sizeof(int), 1, bin_file);

    Disaster disaster;
    int records_count = 0;
    int line_number = 0;

    printf("Iniciando conversão de %s para %s...\n", csv_filename, bin_filename);

    // Pula o cabeçalho (primeira linha) - USANDO FUNÇÃO SEGURA
    char* header_line = read_long_line(csv_file);
    if (header_line) {
        line_number++;
        printf("Cabeçalho ignorado: %.100s...\n", header_line);
        free(header_line);
    }

    // Processa cada linha do CSV - USANDO FUNÇÃO SEGURA
    char* line;
    while ((line = read_long_line(csv_file)) != NULL) {
        line_number++;

        if (parse_csv_line(line, &disaster)) {
            // Escreve o registro no arquivo binário
            if (fwrite(&disaster, sizeof(Disaster), 1, bin_file) == 1) {
                records_count++;

                // Feedback a cada 1000 registros
                if (records_count % 1000 == 0) {
                    printf("Processados %d registros...\n", records_count);
                }
            } else {
                printf("Erro ao escrever registro %d no arquivo binário\n", records_count + 1);
            }
        } else {
            printf("Aviso: Linha %d com formato inválido ignorada\n", line_number);
        }

        free(line); // IMPORTANTE: Libera memória
    }

    fclose(csv_file);

    // Atualiza o contador no início do arquivo
    fseek(bin_file, 0, SEEK_SET);
    fwrite(&records_count, sizeof(int), 1, bin_file);
    fclose(bin_file);

    printf("\n✅ Conversão concluída com sucesso!\n");
    printf("📊 Total de registros convertidos: %d\n", records_count);
    printf("💾 Arquivo binário salvo como: %s\n", bin_filename);
    printf("📏 Tamanho de cada registro: %zu bytes\n", sizeof(Disaster));

    return records_count;
}

// Função para ler e exibir alguns registros do arquivo binário (para teste)
void test_binary_file(const char *bin_filename, int num_records_to_show) {
    FILE *bin_file = fopen(bin_filename, "rb");
    if (!bin_file) {
        printf("Erro: Não foi possível abrir o arquivo binário para teste\n");
        return;
    }

    int total_records;
    if (fread(&total_records, sizeof(int), 1, bin_file) != 1) {
        printf("Erro ao ler contador de registros\n");
        fclose(bin_file);
        return;
    }

    printf("\n🔍 TESTE DO ARQUIVO BINÁRIO:\n");
    printf("Total de registros no arquivo: %d\n\n", total_records);

    Disaster disaster;
    for (int i = 0; i < num_records_to_show && i < total_records; i++) {
        if (fread(&disaster, sizeof(Disaster), 1, bin_file) == 1) {
            printf("--- Registro %d ---\n", i + 1);
            printf("País: %s\n", disaster.country);
            printf("Tipo: %s\n", disaster.disaster_type);
            printf("Ano: %d\n", disaster.start_year);
            printf("Mortes: %d\n", disaster.total_deaths);
            printf("Afetados: %lld\n", disaster.total_affected);
            printf("Danos (mil US$): %lld\n", disaster.total_damage);
            printf("\n");
        }
    }

    fclose(bin_file);
}

int main() {
    const char *csv_filename = "dados-EM-DAT.csv";
    const char *bin_filename = "desastres.bin";

    printf("=== CONVERSOR CSV PARA BINÁRIO - DESASTRES EM-DAT ===\n\n");

    // Converte o arquivo CSV para binário
    int records_converted = convert_csv_to_binary(csv_filename, bin_filename);

    if (records_converted > 0) {
        // Testa o arquivo binário criado
        test_binary_file(bin_filename, 5);

        printf("\n🎯 Próximos passos:\n");
        printf("1. Implementar índices B+ e TRIE\n");
        printf("2. Criar funções de busca e filtro\n");
        printf("3. Desenvolver interface com raylib\n");
    } else {
        printf("❌ Falha na conversão. Verifique se o arquivo %s existe.\n", csv_filename);
    }

    return 0;
}
