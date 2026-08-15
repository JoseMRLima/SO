#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <glib.h> // or specific GLib header
#include "utils.h"
#define STORAGE "persistant_data.bin"

typedef struct {
    char title[219];       // Max 218 bytes + null
    char authors[219];     // Max 218 bytes + null
    char year[6];          // Max 5 bytes + null (char to support "#N/A")
    char path[65];         // Max 64 bytes + null
} DocumentMeta;

// Hash table: ID do documento -> meta-informação
GHashTable *document_table = NULL;  // Key: int (doc_id), Value: DocumentMeta*
static int next_id = 1; // Próximo ID disponível para novo documento

/**
 * Escreve uma entrada da hash table no ficheiro.
 */
static void write_hash_entry(gpointer key, gpointer value, gpointer user_data) {
    int *fd_ptr = (int*)user_data;
    int doc_id = GPOINTER_TO_INT(key);
    DocumentMeta *meta = (DocumentMeta*)value;

    // Escreve a key (doc_id) no ficheiro
    if (write(*fd_ptr, &doc_id, sizeof(doc_id)) != sizeof(doc_id)) {
        perror("Failed to write doc_id");
        return;
    }

    // Escreve a struct com os metadados
    if (write(*fd_ptr, meta, sizeof(DocumentMeta)) != sizeof(DocumentMeta)) {
        perror("Failed to write metadata");
        return;
    }
}

/**
 * Guarda a tabela de documentos no ficheiro de persistência.
 */
static int save_document_table(const char *filename) {
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("Failed to open file for writing");
        return 0;
    }

    // Conta o numero de entradas
    guint count = g_hash_table_size(document_table);
    
    // Escreve o número total de entradas
    if (write(fd, &count, sizeof(count)) != sizeof(count)) {
        perror("Failed to write count");
        close(fd);
        return 0;
    }

    // Escreve cada entrada
    g_hash_table_foreach(document_table, write_hash_entry, &fd);

    // Verifica se o tamanho do ficheiro está de acordo com o esperado
    off_t expected_size = sizeof(count);
    expected_size += (off_t)count * ((off_t)sizeof(int) + (off_t)sizeof(DocumentMeta));
    off_t actual_size = lseek(fd, 0, SEEK_CUR);
    
    close(fd);
    
    if (actual_size != expected_size) {
        fprintf(stderr, "Error writing file: size mismatch (expected %ld, got %ld)\n", (long)expected_size, (long)actual_size);
        return 0;
    }
    
    return 1;
}

/**
 * Lê do ficheiro e reconstrói a tabela de documentos em memória.
 */
static int load_document_table(const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        if (errno == ENOENT) { 
            // Cria uma nova tabela se o ficheiro ainda não existir
            if (document_table == NULL) {
                document_table = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, free);
            }
            return 1;
        }
        perror("Failed to open file for reading");
        return 0;
    }

    // Lê o número de entradas
    guint count;
    if (read(fd, &count, sizeof(count)) != sizeof(count)) {
        perror("Failed to read count");
        close(fd);
        return 0;
    }

    // Destroi tabela anterior se existir
    if (document_table != NULL) {
        g_hash_table_destroy(document_table);
    }
    // Cria nova tabela
    document_table = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, free);
    int highest_id = 0;

    // Lê cada entrada do ficheiro e adiciona à hash table
    for (guint i = 0; i < count; i++) {
        int doc_id;
        if (read(fd, &doc_id, sizeof(doc_id)) != sizeof(doc_id)) {
            perror("Failed to read doc_id");
            close(fd);
            return 0;
        }
        if (doc_id > highest_id) {
            highest_id = doc_id;
        }
        DocumentMeta *data = malloc(sizeof(DocumentMeta));
        if (!data) {
            perror("Failed to allocate memory for metadata");
            close(fd);
            return 0;
        }
        if (read(fd, data, sizeof(DocumentMeta)) != sizeof(DocumentMeta)) {
            perror("Failed to read metadata");
            free(data);
            close(fd);
            return 0;
        }
        // Add to hash table
        g_hash_table_insert(document_table, GINT_TO_POINTER(doc_id), data);
    }
    close(fd);
    next_id = highest_id + 1; // Atualiza o próximo ID disponível
    return 1;
}

/**
 * Inicializa o sistema de documentos (carrega do ficheiro para a memória).
 */
static void init_document_system(const char *storage_file) {
    if (!load_document_table(storage_file)) {
        fprintf(stderr, "Warning: Could not load document data\n");
        document_table = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, free);
    }
}

/**
 * Encerra o sistema de documentos
 */
static void shutdown_document_system(const char *storage_file) {
    // Tenta guardar a tabela no ficheiro
    if (!save_document_table(storage_file)) {
        fprintf(stderr, "Error: Failed to save document data\n");
    }
    
    // Liberta memória da tabela
    if (document_table != NULL) {
        g_hash_table_destroy(document_table);
        document_table = NULL;
    }
}

/* + Submeter pedido de indexação de um documento, conforme o exemplo abaixo.
$ ./dclient -a "Romeo and Juliet" "William Shakespeare" "1997" "1112.txt"
Document 1 indexed              */
static void index_document(const char *title, const char *author, const char *year, const char *path, char* reply/*, const int chache_size*/) {
    int doc_id = next_id++; // Gera novo ID

    DocumentMeta *meta = malloc(sizeof(DocumentMeta));
    if (!meta) {
        perror("Doc index meatdata malloc");
        exit(EXIT_FAILURE);
    }
    // Copia os campos para a struct
    snprintf(meta->title, sizeof(meta->title), "%s", title);
    snprintf(meta->authors, sizeof(meta->authors), "%s", author);
    snprintf(meta->year, sizeof(meta->year), "%s", year);  // Guarda o ano como uma string (dataset includes value "#N/A")
    snprintf(meta->path, sizeof(meta->path), "%s", path);

    g_hash_table_insert(document_table, GINT_TO_POINTER(doc_id), meta);
    snprintf(reply, MAX_REPLY_SIZE, "Document %d indexed.", doc_id); // Resposta para o cliente
}

/* + Submeter pedido de consulta de um documento.
$ ./dclient -c 1
Title: Romeo and Juliet
Authors: William Shakespeare
Year: 1997
Path: 1112.txt                  */
static void query_document(int id, char* reply) {
    printf("query_document(%d)\n\n", id);

    const DocumentMeta *meta = g_hash_table_lookup(document_table, GINT_TO_POINTER(id));
    
    if (!meta) {
        snprintf(reply, MAX_REPLY_SIZE, "Document %d not found.", id);
        return;
    }
    snprintf(reply, MAX_REPLY_SIZE, "Title: %s\nAuthors: %s\nYear: %s\nPath: %s", meta->title, meta->authors, meta->year, meta->path);
}

/* + Submeter pedido de remoção de um índice.
$ ./dclient -d 1
Index entry 1 deleted           */
static char* delete_document(int id) {
    printf(" delete_document(%d)\n\n", id);

    gboolean removed = g_hash_table_remove(document_table, GINT_TO_POINTER(id));

    static char response[64]; // Resposta a devolver ao cliente
    if (removed) {
        snprintf(response, sizeof(response), "Index entry %d deleted.", id);
    } else {
        snprintf(response, sizeof(response), "Document %d not found.", id);
    }

    return response;
}

/* + Pesquisar número de linhas que contêm uma certa palavra chave.
$ ./dclient -l 1 "Romeo"
150                             */
static char* count_lines_with_word(int id, const char *word, const char *doc_folder) {
    printf(" count_lines_with_word(%d, %s)\n\n", id, word);
    
    const DocumentMeta *meta = g_hash_table_lookup(document_table, GINT_TO_POINTER(id));
    static char response[64]; // Resposta a devolver

    if (!meta) {
        snprintf(response, sizeof(response), "Document %d not found.", id);
        return response;
    }

    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s/%s", doc_folder, meta->path);

    int pipe_fd[2]; // cria  um pipe
    if (pipe(pipe_fd) == -1) { // tratamento de erros
        perror("pipe");
        snprintf(response, sizeof(response), "Internal error.");
        return response;
    }

    pid_t pid = fork(); // cria um novo processo
    if (pid == -1) { //tratamento de erros
        perror("fork");
        snprintf(response, sizeof(response), "Internal error.");
        return response;
    }

    if (pid == 0) {
        // Processo filho: executa o comando grep
        close(pipe_fd[0]); // Fecha lado de leitura do pipe
        dup2(pipe_fd[1], STDOUT_FILENO); // Redireciona stdout para o pipe
        close(pipe_fd[1]); // Fecha duplicado

        // Executa grep (procura ignorando maiúsculas e imprime linhas encontradas)
        execlp("grep", "grep", "-i", word, full_path, NULL);

        // Se grep falhar
        perror("execlp grep");
        _exit(EXIT_FAILURE);
    }

    // Processo pai
    close(pipe_fd[1]); // Fecha lado de escrita

    // Lê os dados do pipe
    char buffer[2048];
    ssize_t bytes_read = read(pipe_fd[0], buffer, sizeof(buffer));
    close(pipe_fd[0]);
    wait(NULL); // Espera que o filho termine

    if (bytes_read <= 0) {
        // Se não foi lida nenhuma linha, devolve 0
        snprintf(response, sizeof(response), "0");
        return response;
    }

    // Conta o número de linhas (número de '\n')
    int lines = 0;
    for (ssize_t i = 0; i < bytes_read; i++) {
        if (buffer[i] == '\n') lines++;
    }

    snprintf(response, sizeof(response), "%d", lines);
    return response;
}

/* + Pesquisar lista de identificadores de documentos que contêm uma certa palavra chave [ou usando vários processos (p.ex., 5).]
$ ./dclient -s "praia"
[2, 3, 1438]

$ ./dclient -s "praia" 5
[2, 3, 1438]                    */
static int compare_ints(const void *a, const void *b) {
    int int_a = *(const int*)a;
    int int_b = *(const int*)b;
    return (int_a > int_b) - (int_a < int_b);
}

static char* search_documents(const char *word, int process_count, const char *doc_folder){
    printf(" search_documents(%s, %d)\n\n", word, process_count);

    GList *keys = g_hash_table_get_keys(document_table);
    size_t doc_count = (size_t)g_list_length(keys);

    // Armazenar os processos filhos, os IDs correspondentes e os matches encontrados
    pid_t *pids = malloc(sizeof(pid_t) * doc_count);
    int *ids = malloc(sizeof(int) * doc_count);
    int *matched_ids = malloc(sizeof(int) * doc_count); 
    size_t  match_count = 0;

    if (!pids || !ids || !matched_ids) {
        fprintf(stderr, "Erro de alocação dinâmica.\n");
        exit(EXIT_FAILURE);
    }

    static char response[MAX_REPLY_SIZE];
    size_t offset = 0;
    offset += (size_t)snprintf(response + offset, MAX_REPLY_SIZE - offset, "[");

    int active_children = 0;
    int total = 0;
    // Itera sobre todos os documentos indexados
    for (GList *l = keys; l != NULL; l = l->next) {
        int id = GPOINTER_TO_INT(l->data);
        DocumentMeta *meta = g_hash_table_lookup(document_table, l->data);

        char full_path[128];
        snprintf(full_path, sizeof(full_path), "%s/%s", doc_folder, meta->path);
        // Cria processo filho
        pid_t pid = fork();
        if (pid == -1) continue;

        if (pid == 0) {
            // Processo filho executa grep -q
            execlp("grep", "grep", "-i", "-q", word, full_path, NULL);
            _exit(EXIT_FAILURE);
        }

        // Pai armazena o PID e ID
        pids[total] = pid;
        ids[total] = id;
        total++;
        active_children++;
        // Se atingiu o limite de processos concorrentes, espera por um filho
        if (active_children >= process_count) {
            int status;
            pid_t finished_pid = wait(&status);
            active_children--;

            for (int i = 0; i < total; i++) {
                if (pids[i] == finished_pid) {
                    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                        matched_ids[match_count++] = ids[i];
                    }
                    pids[i] = -1;
                    break;
                }
            }
        }
    }

    // Espera os restantes filhos
    for (int i = 0; i < total; i++) {
        if (pids[i] != -1) {
            int status;
            waitpid(pids[i], &status, 0);
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                matched_ids[match_count++] = ids[i];
            }
        }
    }

    // Ordena os IDs encontrados
    qsort(matched_ids, match_count, sizeof(int), compare_ints);

    // Escrever resposta ordenada
    for (size_t  i = 0; i < match_count; i++) {
        offset += (size_t)snprintf(response + offset, MAX_REPLY_SIZE - offset, "%d, ", matched_ids[i]);
    }

    g_list_free(keys);
    free(pids);
    free(ids);
    free(matched_ids);
    // Remove vírgula final se necessário
    if (offset > 1 && response[offset - 2] == ',') {
        offset -= 2;
    }
    snprintf(response + offset, MAX_REPLY_SIZE - offset, "]");
    return response;
}

static void reply(uint32_t client_pid, const char* reply_message) {
    char response_fifo[32];
    snprintf(response_fifo, sizeof(response_fifo), RESPONSE_FIFO, client_pid);  // ./tmp/client_fifo_<PID>

    int reply_fd = open(response_fifo, O_WRONLY); //open reply fifo
    if (reply_fd == -1) {
        perror("open reply FIFO");
        exit(EXIT_FAILURE);
    }
    // Escreve a resposta completa (incluindo '\0')
    ssize_t bytes_written = write(reply_fd, reply_message, strlen(reply_message) + 1); 
    if (bytes_written == -1) {
        perror("write to reply FIFO");
        close(reply_fd);
        exit(EXIT_FAILURE);
    } else if (bytes_written < (ssize_t)(strlen(reply_message) + 1)) {
        fprintf(stderr, "Warning: Partial write to FIFO, expected %zu bytes, but only %zd bytes written\n", 
                strlen(reply_message) + 1, bytes_written);
    }
    close(reply_fd); // close fifo
}

static int parse_payload(char* buffer, char* args[], size_t max_args) {
    int arg_num = 0;
    size_t pos = 0;
    
    // extract strings separated by null
    while (pos < PAYLOAD_SIZE && arg_num < (int)max_args) {
        if (buffer[pos] == '\0') {
            pos++;
            continue;
        }   
        args[arg_num] = &buffer[pos];
        arg_num++;
        while (pos < PAYLOAD_SIZE && buffer[pos] != '\0') {
            pos++;
        }
        if (pos >= PAYLOAD_SIZE) break;
        pos++;
    }
    return arg_num;
}

static void handle_request(Request *request, const char* doc_folder /*, int cache_size*/) {
    char reply_message[MAX_REPLY_SIZE];
    char full_path[128];

    // transforma payload em array de strings para facilitar o input
    char* argv[4] = {0};
    int argc = parse_payload(request->payload, argv, 4);
        
    switch (request->command) {
        case 'f':  // Stop server
            snprintf(reply_message, MAX_REPLY_SIZE, "Server is shuting down.");
            break;

        case 's':;  // Search documents
            // Cria processo filho para realizar pesquisa concorrente
            pid_t search_pid = fork();
            if (search_pid == 0) {  // Processo filho
                if(argc == 2) {
                    snprintf(reply_message, MAX_REPLY_SIZE, "%s", search_documents(argv[0], atoi(argv[1]), doc_folder));
                } else {
                    snprintf(reply_message, MAX_REPLY_SIZE, "%s", search_documents(argv[0], 1, doc_folder));
                }
                // envia reply do processo filho
                reply(extract_pid(request->pid_bytes), reply_message);
                _exit(EXIT_SUCCESS);
            }
            else if (search_pid > 0) {  // Pai
                return;
            }
            return;

        case 'l':  // Count lines with word
            snprintf(reply_message, MAX_REPLY_SIZE, "%s", count_lines_with_word(atoi(argv[0]), argv[1], doc_folder));
            break;

        case 'd':  // Delete document
            snprintf(reply_message, MAX_REPLY_SIZE, "%s", delete_document(atoi(argv[0])));
            break;

        case 'c':  // Query document
            query_document(atoi(argv[0]), reply_message);
            break;

        case 'a':  // Index document
            snprintf(full_path, 128, "%s%s", doc_folder, argv[3]);
            if (access(full_path, R_OK)) { // verificar se o ficheiro existe e é legível (without needing to open() and close if it succeeded)
                snprintf(reply_message, MAX_REPLY_SIZE, "File: '%s' is not accessible.", full_path);
            } else {index_document(argv[0], argv[1], argv[2], argv[3], reply_message/*, cache_size*/);}
            break;

        default:
            fprintf(stderr, "Error: Unknown command.\n");
            snprintf(reply_message, MAX_REPLY_SIZE, "Error: Unknown command.\n");
            break;
    }
    // cria e manda reply 
    reply(extract_pid(request->pid_bytes) , reply_message);
}

int main(int argc, const char *argv[]) {
    printf("\nServer is starting...\n");
    const char *document_folder;
    int cache_size;
    // Valida argumentos de arranque
    switch (argc) {
    case 2:
        document_folder = argv[1];
        cache_size = 0;
        break;
    case 3:
        document_folder = argv[1]; 
        cache_size = atoi(argv[2]);
        if (cache_size < 1) cache_size = 0;
        break;
    default:
        fprintf(stderr, "Usage: %s document_folder cache_size (invalid cache values default to 0)\n", argv[0]);
        return EXIT_FAILURE;
    }
    // Cria FIFO principal do servidor
    if (mkfifo(SERVER_FIFO, 0666) == -1 && errno != EEXIST) { 
        perror("mkfifo");
        return EXIT_FAILURE; 
    }
    // Inicializa sistema (carrega dados persistentes)
    init_document_system(STORAGE);
    while (1) {
        // Aguarda pedido do cliente
        int fd = open(SERVER_FIFO, O_RDONLY); // open server fifo
        if (fd == -1) {
            perror("open SERVER_FIFO");
            return EXIT_FAILURE;
        }

        Request req;
        ssize_t numBytes = read(fd, &req, MAX_REQUEST_SIZE);
        if (numBytes < 0) {
            perror("read");
            close(fd);
            continue;
        }
        log_timestamp_usec();
        close(fd);
        // remove zombies
        while (waitpid(-1, NULL, WNOHANG) > 0);
        // Processa pedido recebido
        handle_request(&req, document_folder/*, cache_size*/); // cache size not in use
        // Se comando for 'f', encerra
        if (req.command == 'f') break;
    }
    // Limpeza final
    unlink(SERVER_FIFO);
    shutdown_document_system(STORAGE);
    fprintf(stdout,"Shutting down.\n");

    return EXIT_SUCCESS;
}
