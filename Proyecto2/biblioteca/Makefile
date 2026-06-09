# Makefile - Biblioteca CNC para trazado de PCB
#
# Estructura del proyecto:
#   include/cnc_lib.h   -> headers públicos (API)
#   src/cnc_lib.c       -> implementación
#   test/test_cnc.c     -> pruebas
#   build/              -> objetos intermedios (generado automáticamente)
#   bin/libcnc.a        -> biblioteca estática final (generado automáticamente)
#
# Uso:
#   make          -> compila bin/libcnc.a
#   make test     -> compila y ejecuta pruebas
#   make clean    -> limpia todo lo generado

CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -std=c99 -pedantic -I./include
AR      = ar
ARFLAGS = rcs

# Directorios
SRC_DIR   = src
INC_DIR   = include
TEST_DIR  = test
BUILD_DIR = build
BIN_DIR   = bin

# Archivos
LIB      = $(BIN_DIR)/libcnc.a
SRCS     = $(wildcard $(SRC_DIR)/*.c)
OBJS     = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

TEST_SRC = $(TEST_DIR)/test_cnc.c
TEST_BIN = $(TEST_DIR)/test_cnc

# ===========================================================================
# Reglas
# ===========================================================================

# Target por defecto
all: $(BUILD_DIR) $(BIN_DIR) $(LIB)

# Crear directorios si no existen
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Compilar cada .c de src/ en un .o dentro de build/
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(INC_DIR)/cnc_lib.h
	$(CC) $(CFLAGS) -c $< -o $@

# Empaquetar todos los .o en la biblioteca estática dentro de bin/
$(LIB): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^
	@echo "---------------------------------------------"
	@echo " Biblioteca generada: $(LIB)"
	@echo " Para usarla desde el servidor:"
	@echo "   gcc servidor.c -L./bin -lcnc -I./include -o servidor"
	@echo "---------------------------------------------"

# Compilar y correr tests
# -L./bin apunta al directorio donde quedó libcnc.a
test: $(BUILD_DIR) $(BIN_DIR) $(LIB)
	$(CC) $(CFLAGS) $(TEST_SRC) -L./$(BIN_DIR) -lcnc -o $(TEST_BIN)
	./$(TEST_BIN)

# Limpiar todo lo generado (build/, bin/ y el binario de test)
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR) $(TEST_BIN)
	@echo "Limpieza completada."

.PHONY: all test clean
