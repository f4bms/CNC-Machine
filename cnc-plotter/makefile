# ============================================================
#  CNC Plotter — Makefile para arduino-cli en Linux
# ============================================================

PROJECT   := cnc-plotter
SKETCH    := $(PROJECT).ino

# Puerto serie — cámbialo si el tuyo es diferente (ej: ttyUSB1)
PORT      ?= /dev/ttyUSB0
BAUD      := 115200

# FQBN del ESP32 genérico (Espressif Arduino core)
FQBN      := esp32:esp32:esp32

# Directorio de salida de compilación
BUILD_DIR := build

# ============================================================
.PHONY: all compile flash monitor clean install-core help

all: compile

## Compila el sketch sin flashear
compile:
	@echo ">>> Compilando $(SKETCH)..."
	arduino-cli compile \
		--fqbn $(FQBN) \
		--output-dir $(BUILD_DIR) \
		$(SKETCH)
	@echo ">>> Compilación exitosa."

## Compila y flashea el ESP32
flash:
	@echo ">>> Flasheando en $(PORT)..."
	arduino-cli upload \
		--fqbn $(FQBN) \
		--port $(PORT) \
		$(SKETCH)
	@echo ">>> Flash exitoso."

## Abre el monitor serie (Ctrl+C para salir)
monitor:
	@echo ">>> Monitor serie en $(PORT) a $(BAUD) baudios..."
	arduino-cli monitor \
		--port $(PORT) \
		--config baudrate=$(BAUD)

## Flash + monitor en un solo comando
run: flash monitor

## Limpia los archivos de compilación
clean:
	@echo ">>> Limpiando $(BUILD_DIR)/..."
	rm -rf $(BUILD_DIR)

## Instala el core de Espressif si no lo tienes
install-core:
	@echo ">>> Añadiendo URL del core ESP32..."
	arduino-cli config add board_manager.additional_urls \
		https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
	@echo ">>> Actualizando índice..."
	arduino-cli core update-index
	@echo ">>> Instalando core esp32:esp32..."
	arduino-cli core install esp32:esp32
	@echo ">>> Core instalado."

## Muestra ayuda
help:
	@echo ""
	@echo "  make compile      — Solo compila"
	@echo "  make flash        — Compila y flashea"
	@echo "  make monitor      — Abre monitor serie"
	@echo "  make run          — Flash + monitor"
	@echo "  make clean        — Borra archivos de build"
	@echo "  make install-core — Instala el core ESP32"
	@echo ""
	@echo "  Puerto por defecto: $(PORT)"
	@echo "  Cambia con:  make flash PORT=/dev/ttyUSB1"
	@echo ""