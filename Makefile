# MiniHTTPd
# HTTP/1.1 con Lenguaje c

CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic -O2 -g -Iinclude -D_GNU_SOURCE
LDFLAGS =

# Carpetas
SRC_DIR = src
OBJ_DIR = build
BIN     = minihttpd

# source and object list
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))

# binary build
all: $(BIN)

# Enlazado final
$(BIN): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compilacion de cada cada archivo .c 
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)


clean:
	rm -rf $(OBJ_DIR) $(BIN)

# test
run: $(BIN)
	./$(BIN)

.PHONY: all clean run
