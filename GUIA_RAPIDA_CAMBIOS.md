# Guía Rápida de Cambios - CNC Communication Fix

## Cambios en Archivos

### 📁 Proyecto2/driver.c (RPi - Driver Linux)

| Cambio              | Antes         | Después    | Razón                            |
| ------------------- | ------------- | ---------- | -------------------------------- |
| **BIT_DELAY_TIME**  | 500 μs        | 1000 μs    | Mayor tolerancia a jitter        |
| **BYTE_DELAY_TIME** | N/A           | 5000 μs    | Pausa sincronización entre bytes |
| **Timeout RX**      | 100ms         | 500ms      | Mejor tolerancia a retrasos      |
| **Pausa antes TX**  | 2× delay      | 3× delay   | Mejor sincronización de IDLE     |
| **ACK por byte**    | Solo al final | Individual | Detecta errores específicos      |

**Líneas clave que cambiaron:**

- Línea ~28: `#define BIT_DELAY_TIME 1000`
- Línea ~29: `#define BYTE_DELAY_TIME 5000`
- Línea ~150-170: Función `dev_write()` ahora itera y espera ACK por cada byte

---

### 📁 cnc-plotter/cnc-plotter.ino (ESP32)

| Cambio              | Antes         | Después  | Razón                     |
| ------------------- | ------------- | -------- | ------------------------- |
| **Timeout inicio**  | 100ms         | 200ms    | Mayor paciencia esperando |
| **ACK por byte**    | Solo al final | 5 ACKs   | Confirmación individual   |
| **Preámbulo TX**    | 2× delay      | Incluido | Sincronización mejorada   |
| **Validación STOP** | Solo log      | Incluido | Detectar corrupción       |
| **Logs de debug**   | Minimal       | Completo | Diagnóstico mejor         |

**Líneas clave que cambiaron:**

- Función `bb_recv_byte()`: timeout aumentado, mejor validación
- Función `recv_and_execute()`: **5 `send_ack()` en lugar de 1**
- Función `bb_send_byte()`: Preámbulo mejorado

---

## Secuencia de Comunicación - ANTES vs DESPUÉS

### ❌ ANTES (Comunicación Simple)

```
RPi (TX)                    ESP (RX)
   |                           |
   |--[CMD byte]-->            |
   |                    [RX_OK? No → ????]
   |--[X_HIGH byte]-->         |
   |              [PERDIDO o CORROMPIDO]
   |--[X_LOW byte]-->          |
   |                           |
   |--[Y_HIGH byte]-->         |
   |--[Y_LOW byte]-->          |
   |                           |
   |<--[ACK final]--           |
   |              [Problema en byte 3?]
   |              [No se sabe cuál falló]
```

### ✅ DESPUÉS (Comunicación Robusta)

```
RPi (TX)                    ESP (RX)
   |                           |
   |--[CMD byte]-->            |
   |              ✓ Recibido   |
   |<--[ACK]--                 |
   |                           |
   |--[X_HIGH]-->              |
   |           ✓ Recibido      |
   |<--[ACK]--                 |
   |                           |
   |--[X_LOW]-->               |
   |          ✓ Recibido       |
   |<--[ACK]--                 |
   |                           |
   |--[Y_HIGH]-->              |
   |           ✓ Recibido      |
   |<--[ACK]--                 |
   |                           |
   |--[Y_LOW]-->               |
   |          ✓ Recibido       |
   |<--[ACK]--                 |
   |                           |
   |         [5ms delay]       |
   |    [Listo para siguiente] |
```

---

## Testing Rápido

### 1️⃣ Compilar Driver RPi

```bash
cd ~/CNC-Machine/Proyecto2
make clean
make
sudo insmod driver.ko
sudo chmod 666 /dev/gpio_device
```

### 2️⃣ Subir código a ESP32

- Abre Arduino IDE
- Abre `cnc-plotter/cnc-plotter.ino`
- Verifica `#define DEBUG_COMM 1` (línea 1)
- Compile y sube
- Abre Serial Monitor @ 115200 baud

### 3️⃣ Prueba Simple

```bash
# En RPi
echo "G 50 75" > /dev/gpio_device
```

### 4️⃣ Verifica Salida ESP

Deberías ver:

```
[RX] Start bit detectado
[RX] Byte recibido = 0x47 ('G')    ← Comando
[TX] Enviando byte 0x41 ('A')      ← ACK

[RX] Start bit detectado
[RX] Byte recibido = 0x00          ← 50 >> 8
[TX] Enviando byte 0x41 ('A')

[RX] Start bit detectado
[RX] Byte recibido = 0x32          ← 50 & 0xFF
[TX] Enviando byte 0x41 ('A')

[RX] Start bit detectado
[RX] Byte recibido = 0x00          ← 75 >> 8
[TX] Enviando byte 0x41 ('A')

[RX] Start bit detectado
[RX] Byte recibido = 0x4B          ← 75 & 0xFF
[TX] Enviando byte 0x41 ('A')

CMD=G x=50 y=75
[MOVE] GOTO (50,75)
[MOVE] GOTO terminado
```

---

## Solución de Problemas

### Síntoma: "Timeout esperando start bit"

**Causa**: ESP no recibe bytes  
**Solución**:

- Verifica conexiones GPIO (RPi 17 → ESP 32, RPi 27 → ESP 33)
- Intenta aumentar `BYTE_DELAY_TIME` a 10000

### Síntoma: "ACK recibido pero invalido"

**Causa**: Dato corrompido en transmisión  
**Solución**:

- Aumenta `BIT_DELAY_TIME` a 1500 o 2000
- Acorta cables entre RPi y ESP

### Síntoma: Bytes desalineados (0x4700 en lugar de 0x47)

**Causa**: START BIT falso detectado  
**Solución**:

- Agrega resistencias pull-up 10kΩ en pines RX/TX
- Verifica que no hay crosstalk en otros pines GPIO

---

## Cuándo Escalar a Soluciones Más Avanzadas

Si después de esto sigue fallando, considera:

| Problema                                         | Solución                                       |
| ------------------------------------------------ | ---------------------------------------------- |
| Comunicación inestable incluso con delays largos | Usar UART HW en RPi GPIO o MAX3232 transceiver |
| Errores aleatorios intermitentes                 | Agregar checksums CRC-8                        |
| Necesidad de mayor velocidad                     | Usar comunicación SPI                          |
| Múltiples dispositivos                           | Agregar addressing y múltiples canales         |
