#include <fcntl.h>
#include <sys/stat.h>
#include "utils.h"

// Caminho FIFO único para receber resposta do servidor
static char reply_fifo[32];

/**
 * Envia o pedido ao FIFO do servidor.
 */
static void send_request(const Request *request) {
    int server_fd = open(SERVER_FIFO, O_WRONLY);
    if (server_fd == -1) {
        perror("open SERVER_FIFO");
        unlink(reply_fifo);
        exit(EXIT_FAILURE);
    }

    if (write(server_fd, request, sizeof(Request)) == -1) {
        perror("write to SERVER_FIFO");
        close(server_fd);
        unlink(reply_fifo);
        exit(EXIT_FAILURE);
    }
    
    close(server_fd);
}

/**
 * Lê a resposta do servidor a partir do FIFO de resposta.
 */
static void read_reply(char *reply) {

    // Open reply FIFO
    int reply_fd = open(reply_fifo, O_RDONLY);
    if (reply_fd == -1) {
        perror("open reply FIFO");
        unlink(reply_fifo);
        exit(EXIT_FAILURE);
    }

    ssize_t total_bytes = 0, bytes_read;
    size_t remaining_space;

    // Lê a resposta garantindo que não ultrapassa o limite
    while ((remaining_space = MAX_REPLY_SIZE - 1 - (size_t)total_bytes) > 0 && (bytes_read = read(reply_fd, reply + total_bytes, remaining_space)) > 0) {
        total_bytes += bytes_read;
    }

    reply[total_bytes] = '\0';  // Termina a string
    close(reply_fd);
}

/**
 * Constrói a estrutura Request a partir dos argumentos da linha de comandos.
 */
static Request build_request(int argc, char *argv[]) {
    // fill struct with nulls
    Request req;
    memset(&req, 0, sizeof(Request));

    store_pid(req.pid_bytes); // Guarda o PID para resposta
    req.command = argv[1][1]; // Guarda o comando

    // Junta os argumentos separados por '\0' no payload
    size_t offset = 0;
    for (int i = 2; i < argc; i++) {
        size_t len = strlen(argv[i]);
        if (offset + len > PAYLOAD_SIZE) exit(EXIT_FAILURE);
        memcpy(req.payload + offset, argv[i], len);
        offset += len+1;
    }

    return req;
}

int main(int argc, char *argv[]) {
    int input = validate_args(argc, argv); // Validação de argumentos
    if (input) handle_error_code(argv[0], input);
    
    // Criação de FIFO de resposta único para este cliente 
    snprintf(reply_fifo, sizeof(reply_fifo), RESPONSE_FIFO, getpid());
    if (mkfifo(reply_fifo, 0666) == -1) {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }

    // Criação do pedido e buffer de resposta
    Request request = build_request(argc, argv);
    char reply[MAX_REPLY_SIZE] = {0};

    // Cronometrar tempo da operação
    struct timespec start, end;
    long elapsed_ns;

    clock_gettime(CLOCK_MONOTONIC, &start); // timing

    send_request(&request);
    read_reply(reply);

    clock_gettime(CLOCK_MONOTONIC, &end); // end

    unlink(reply_fifo); // Remove FIFO

    // Calculo do tempo decorrido
    elapsed_ns = (end.tv_sec - start.tv_sec) * 1000000000L;
    elapsed_ns += (end.tv_nsec - start.tv_nsec);

    double seconds = (double)elapsed_ns / 1000000000.0;
    double milliseconds = (double)elapsed_ns / 1000000.0;

    // Apresentação dos resultados
    printf("\n%s\n", reply);
    printf("\nStopwatch:\n");
    printf("  %f s\n", seconds);
    printf("  %f ms\n", milliseconds);
    return EXIT_SUCCESS;
}
