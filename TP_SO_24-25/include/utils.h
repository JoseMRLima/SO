#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdint.h> // for uintxx_t
#include <time.h>
#include <sys/time.h> 

// Caminho para FIFO do servidor e formato do FIFO de resposta do cliente
#define SERVER_FIFO "./tmp/server_fifo"
#define RESPONSE_FIFO "./tmp/client_fifo_%u"

// Tamanho máximo de um pedido e resposta
#define MAX_REQUEST_SIZE 512
#define HEADER_SIZE 4
#define PAYLOAD_SIZE (MAX_REQUEST_SIZE - HEADER_SIZE)
#define MAX_REPLY_SIZE 16384 // not sure if it's enough, {./dclient -s "common_word"} can return a huge list of IDs


/*  $ cat /proc/sys/kernel/pid_max 
    4194304 (= 2^22, only 22 bits needed)*/
    #pragma pack(push, 1) 
    typedef struct {
        uint8_t pid_bytes[3];           // 3 bytes for PID
        char command;                   // 1 byte command
        char payload[PAYLOAD_SIZE];
    } Request;
    #pragma pack(pop) 
    
    // Códigos de erro para validação de argumentos
    typedef enum {
        SUCCESS = 0,
        ERR_MISSING_ARGS,
        ERR_TOO_MANY_ARGS,
        ERR_ARG_SIZE_EXCEEDS_LIMIT,
        ERR_INVALID_ARGS_A,
        ERR_INVALID_ARGS_C,
        ERR_INVALID_ARGS_D,
        ERR_INVALID_ARGS_L,
        ERR_INVALID_ARGS_S,
        ERR_INVALID_ARGS_F,
        ERR_INVALID_ARGS,
        ERR_UNKNOWN_COMMAND
    } ErrorCode;
    
    // Enumeração para comandos reconhecidos (não usado diretamente mas útil)
    typedef enum {
        CMD_ADD    = 0x01,  // -a
        CMD_QUERY  = 0x02,  // -c
        CMD_DELETE = 0x03,  // -d
        CMD_COUNT  = 0x04,  // -l
        CMD_SEARCH = 0x05,  // -s
        CMD_STOP   = 0x06   // -f
    } Command;

void store_pid(uint8_t pid_bytes[3]);
uint32_t extract_pid(const uint8_t pid_bytes[3]);
void log_timestamp_usec(void);
void print_client_help(const char *arg);
int validate_args(int argc, char *argv[]);
void handle_error_code(const char *arg, int error_code);

#endif