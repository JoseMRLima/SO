# --- Compiler and Linker Flags ---
CC = gcc

GLIB_CFLAGS = $(shell pkg-config --cflags glib-2.0)
GLIB_LIBS = $(shell pkg-config --libs glib-2.0)
LDFLAGS = -fsanitize=address,undefined,leak $(GLIB_LIBS)
CFLAGS = -Wall -Wextra -Wpedantic -Werror -Wformat=2 \
         -Wstrict-overflow=5 -Wshadow -Wcast-align \
         -Wcast-qual -Wconversion -Wdouble-promotion \
         -Wlogical-op -Wmissing-prototypes \
         -Wmissing-declarations -Wstrict-prototypes \
         -Wredundant-decls -Wundef \
         -fno-omit-frame-pointer -fstack-protector-strong \
         -D_FORTIFY_SOURCE=2 \
         -fsanitize=undefined,address,leak \
         -ggdb3 -Og -m64 -Iinclude $(GLIB_CFLAGS)

# --- Source and Object Files ---
SRC_DIR := src
INC_DIR := include
OBJ_DIR := obj
BIN_DIR := bin
TMP_DIR := tmp

SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC))

# --- Executables ---
DSERVER_EXEC := $(BIN_DIR)/dserver
DCLIENT_EXEC := $(BIN_DIR)/dclient

# --- Targets ---
all: build

build: folders $(DSERVER_EXEC) $(DCLIENT_EXEC)

folders:
	@echo "$'\n>> Creating Directories..."
	@mkdir -p $(SRC_DIR) $(INC_DIR) $(OBJ_DIR) $(BIN_DIR) $(TMP_DIR)

$(DSERVER_EXEC): $(OBJ_DIR)/dserver.o $(OBJ_DIR)/utils.o
	@echo "$'\n>> Linking dserver..."
	$(CC) $(LDFLAGS) $^ -o $@

$(DCLIENT_EXEC): $(OBJ_DIR)/dclient.o $(OBJ_DIR)/utils.o
	@echo "$'\n>> Linking dclient..."
	$(CC) $(LDFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "$'\n>> Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo "$'\n>> Cleaning Project..."
	@rm -rf $(OBJ_DIR)/* $(TMP_DIR)/* $(BIN_DIR)/* *_fifo

.PHONY: all build folders clean

#   make            (Copies, builds)
#   make clean      (Removes build artifacts)
