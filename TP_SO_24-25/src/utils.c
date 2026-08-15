#include "utils.h"

/**
 * Obtém o timestamp atual (hora:min:seg.ms) e imprime para stdout.
 */
void log_timestamp_usec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *tm = gmtime(&tv.tv_sec);

    char buf[64];
    strftime(buf, sizeof(buf), "%Hh:%Mm:%S", tm);
    printf("[Timestamp: %s.%03lds]\n", buf, tv.tv_usec / 1000);
}

/**
 * Remove espaços no início e fim de uma string.
 */
static char* trim_whitespace(char *str) {
    if (!str) return str;

    while (isspace((unsigned char)*str)) str++; // espaço à esquerda

    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--; // espaço à direita
    end[1] = '\0';

    return str;
}

/**
 * Verifica se uma string representa um número positivo.
 */
static int is_valid_number(const char *str) {
    if (!str || *str == '\0') return 0;
    for (; *str; str++) {
        if (!isdigit((unsigned char)*str)) return 0;
    }
    return 1;
}

/**
 * Remove caracteres não imprimíveis de uma string.
 */
static void filter_non_printables(char *str) {
    if (!str) return;
    
    char *src = str;
    char *dst = str;
    
    while (*src) {
        if (isprint((unsigned char)*src)) {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
}

/**
 * Remove '\r' (carriage return) de uma string (necessário ao usar scripts Windows).
 */
static void strip_carriage_return(char *str) { 
    if (!str) return;
    char *src = str;  // Source pointer for reading
    char *dst = str;  // Destination pointer for writing
    while (*src) {
        if (*src != '\r') {
            *dst++ = *src; 
        }
        src++;
    }
    *dst = '\0';
}

/**
 * Valida os argumentos recebidos pelo cliente (dclient).
 * Garante número e formato adequados, e verifica limites de tamanho.
 */
int validate_args(int argc, char *argv[]) {
    // Validação básica da quantidade de argumentos
    if (argc < 2) return ERR_MISSING_ARGS;
    if (argc > 6) return ERR_TOO_MANY_ARGS;

    // Remove '\r' do comando (corrige problemas em scripts Windows)
    strip_carriage_return(argv[1]); 

    // Verifica se o comando está no formato "-x"
    if (argv[1][0] != '-' || strlen(argv[1]) != 2) {
        // Caso seja pedido de ajuda, imprime e termina
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            print_client_help(argv[0]);
            exit(EXIT_SUCCESS);
        }
        return ERR_UNKNOWN_COMMAND;
    }

    // Valida cada argumento
    size_t arg_size = 0;
    for (int i = 1; i < argc; i++) {
        filter_non_printables(argv[i]);  // Remove caracteres não imprimíveis
        const char *trimmed = trim_whitespace(argv[i]); // Remove espaços nas extremidades
        if (trimmed[0] == '\0')  return ERR_INVALID_ARGS; // Argumento vazio após limpeza

        // Soma do tamanho total dos argumentos (incluindo '\0' internos)
        if (i>1) arg_size += strlen(argv[i]) + !(i+1==argc);
        if (arg_size > PAYLOAD_SIZE) return ERR_ARG_SIZE_EXCEEDS_LIMIT;
    }

    // Verificação final do tamanho do payload
    if (arg_size > PAYLOAD_SIZE) return ERR_ARG_SIZE_EXCEEDS_LIMIT;
    // Verifica tipo de comando e argumentos associados
    const char cmd = argv[1][1];
    switch (cmd) {
        case 'a':  // Add document
            if (argc != 6) return ERR_INVALID_ARGS_A;
            if (strlen(trim_whitespace(argv[2])) > 218 ||
                strlen(trim_whitespace(argv[3])) > 218 ||
                strlen(trim_whitespace(argv[4])) > 5 ||
                strlen(trim_whitespace(argv[5])) > 64) {
                return ERR_ARG_SIZE_EXCEEDS_LIMIT;
            }
            break;
        case 'c':  // Query document
            if (argc != 3 || !is_valid_number(argv[2])) return ERR_INVALID_ARGS_C;
            break;
        case 'd':  // Delete document
            if (argc != 3 || !is_valid_number(argv[2])) return ERR_INVALID_ARGS_D;
            break;
        case 'l':  // Count lines
            if (argc != 4 || !is_valid_number(argv[2])) return ERR_INVALID_ARGS_L;
            break;
        case 's':  // Search
            if (argc != 3 && argc != 4) return ERR_INVALID_ARGS_S;
            if (argc == 4 && (!is_valid_number(argv[3]) || atoi(argv[3]) <= 0)) return ERR_INVALID_ARGS_S;
            break;
        case 'f':  // Stop server
            if (argc != 2) return ERR_INVALID_ARGS_F;
            break;
        case 'h': // Help page/Usage manual
            if (argc != 2) return ERR_INVALID_ARGS;
            print_client_help(argv[0]);
            exit(EXIT_SUCCESS);
        default:
            return ERR_UNKNOWN_COMMAND;
    }
    return SUCCESS;
}

/**
 * Imprime mensagem de erro com base no código de erro e termina o programa.
 */
void handle_error_code(const char *arg, int error_code) {
    switch (error_code) {
        case ERR_MISSING_ARGS:
            fprintf(stderr, "Error: Missing arguments.\n\n");
            break;
        case ERR_TOO_MANY_ARGS:
            fprintf(stderr, "Error: Too many arguments.\n\n");
            break;
        case ERR_ARG_SIZE_EXCEEDS_LIMIT:
            fprintf(stderr, "Error: Argument size exceeds the limit.\n\n");
            break;
        case ERR_INVALID_ARGS_A:
            fprintf(stderr, "Error: Invalid arguments for -a.\n\n");
            break;
        case ERR_INVALID_ARGS_C:
            fprintf(stderr, "Error: Invalid arguments for -c.\n\n");
            break;
        case ERR_INVALID_ARGS_D:
            fprintf(stderr, "Error: Invalid arguments for -d.\n\n");
            break;
        case ERR_INVALID_ARGS_L:
            fprintf(stderr, "Error: Invalid arguments for -l.\n\n");
            break;
        case ERR_INVALID_ARGS_S:
            fprintf(stderr, "Error: Invalid arguments for -s.\n\n");
            break;
        case ERR_INVALID_ARGS_F:
            fprintf(stderr, "Error: Invalid arguments for -f.\n\n");
            break;
        case ERR_INVALID_ARGS:
            fprintf(stderr, "Error: At least 1 invalid argument.\n\n");
            break;
        case ERR_UNKNOWN_COMMAND:
            fprintf(stderr, "Error: Unknown command.\n\n");
            break;
        default:
            fprintf(stderr, "Error: Unhandled error code.\n\n");
            break;
    }
    print_client_help(arg);
    exit(EXIT_FAILURE);
}

/**
 * Imprime o menu de ajuda e exemplos de uso.
 */
void print_client_help(const char *arg) {
    printf("Usage: %s [OPTION] [ARGS]\n\n", arg);
    printf("Options:\n");
    printf("  -a \"TITLE\" \"AUTHOR\" \"YEAR\" \"FILE\"      Add document to index\n");
    printf("  -c ID                                  Query document by ID\n");
    printf("  -d ID                                  Delete document index by ID\n");
    printf("  -l ID \"WORD\"                           Count lines in document containing WORD\n");
    printf("  -s \"WORD\"                              Search document IDs containing WORD\n");
    printf("  -s \"WORD\" PROCESSES                    Search using multiple processes\n");
    printf("  -f                                     Shutdown the server\n");
    printf("  -h                                     Show this help message and exit\n\n");

    printf("Examples:\n");
    printf("  %s -a \"Romeo and Juliet\" \"William Shakespeare\" \"1997\" \"1112.txt\"\n", arg);
    printf("  %s -c 1\n", arg);
    printf("  %s -d 1\n", arg);
    printf("  %s -l 1 \"Romeo\"\n", arg);
    printf("  %s -s \"praia\"\n", arg);
    printf("  %s -s \"praia\" 5\n", arg);
    printf("  %s -f\n\n", arg);
}

/**
 * Reconstrói o PID a partir de 3 bytes recebidos na struct Request.
 */
uint32_t extract_pid(const uint8_t pid_bytes[3]) {
    return ((uint32_t)pid_bytes[0]) |
           ((uint32_t)pid_bytes[1] << 8) |
           ((uint32_t)pid_bytes[2] << 16);
}

/**
 * Codifica o PID atual em 3 bytes (usado no cliente).
 */
void store_pid(uint8_t pid_bytes[3]) {
    uint32_t pid = (uint32_t)getpid();
    pid_bytes[0] = (pid >>  0) & 0xFF;  // LSB
    pid_bytes[1] = (pid >>  8) & 0xFF;
    pid_bytes[2] = (pid >> 16) & 0xFF;
}