# Makefile do projeto "consulta" (tabela hash + filtro de Bloom)
#
# Alvos principais:
#   make           -> compila o binario em bin/consulta
#   make run       -> compila e executa carregando data/usuarios.txt
#   make clean     -> remove artefatos de compilacao

CC      := gcc
# -Wall -Wextra: liga avisos importantes | -O2: otimizacao | -std=c11: padrao
CFLAGS  := -Wall -Wextra -O2 -std=c11
LDFLAGS := -lm                # necessario por causa de log()/exp()/pow() no Bloom

SRC_DIR := src
OBJ_DIR := build
BIN_DIR := bin

SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))
TARGET  := $(BIN_DIR)/consulta

.PHONY: all run clean

all: $(TARGET)

# Liga os objetos no binario final.
$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "Compilado: $@"

# Compila cada .c em um .o.
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Cria diretorios de saida se nao existirem.
$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

# Compila e ja executa com o arquivo de exemplo.
run: $(TARGET)
	./$(TARGET) data/usuarios.txt

clean:
	@rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "Artefatos removidos."
