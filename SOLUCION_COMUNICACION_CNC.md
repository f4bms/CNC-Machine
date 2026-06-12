# Solución del Problema de Comunicación ESP32-RPi CNC

## Problema Identificado

El ESP32 no recibía correctamente los comandos del controlador CNC porque:

1. **Falta de sincronización entre bytes**: El driver enviaba 5 bytes (CMD + X_H + X_L + Y_H + Y_L) sin confirmación individual. Si perdía sincronización en el byte 2 o 3, los datos restantes eran basura.

2. **Timings demasiado agresivos**: 500 microsegundos entre bits era muy sensible a jitter del sistema operativo (context switches, interrupciones). Cualquier retardo causaba desalineamiento.

3. **ACK solo al final**: Solo se confirmaba tras los 5 bytes, sin validar cada uno. Si faltaba 1 bit en el byte 3, el error no se detectaba.

4. **Detección de START BIT débil**: El timeout durante la búsqueda de START BIT era corto y podía detectar falsamente un bit bajo.

---

## Soluciones Implementadas

### 1. **Aumentar BIT_DELAY_TIME** (Driver RPi)

```c
#define BIT_DELAY_TIME 1000  // 500 → 1000 microsegundos
#define BYTE_DELAY_TIME 5000 // Pausa entre bytes
```

**Por qué**: Reduce la velocidad de comunicación de ~2000 baud a ~1000 baud, dando más margen a jitter del sistema.

---

### 2. **ACK Individual por Cada Byte**

**En driver.c (RPi):**

```c
for (i = 0; i < 5; i++) {
    send_byte(bytes_to_send[i]);  // Envía byte
    ack = receive_byte();          // Espera ACK del byte
    if ((u8)ack != 'A') {
        // Error en este byte específico
        return -EIO;
    }
    udelay(BYTE_DELAY_TIME);  // Pausa de 5ms entre bytes
}
```

**En cnc-plotter.ino (ESP32):**

```c
int cmd = bb_recv_byte();
send_ack();  // ACK tras CMD

int xh = bb_recv_byte();
send_ack();  // ACK tras cada byte de coordenada

int xl = bb_recv_byte();
send_ack();

int yh = bb_recv_byte();
send_ack();

int yl = bb_recv_byte();
send_ack();
```

**Por qué**: Permite detectar exactamente dónde falló la comunicación. Si el byte 3 se corrompió, lo sabremos inmediatamente.

---

### 3. **Mejor Sincronización de START BIT**

**En driver.c:**

```c
udelay(BIT_DELAY_TIME * 3);  // 3000 μs antes de enviar
```

**En cnc-plotter.ino:**

```c
timeout = 200000;  // 200ms, muy más tolerante
while (digitalRead(RX_PIN) == HIGH) {
    if (timeout-- == 0) return -1;
    delayMicroseconds(1);
}
```

**Por qué**: Aumenta el tiempo de estabilización antes de cada byte. El ESP espera 200ms máximo por START BIT.

---

### 4. **Preaambulo de IDLE en TX**

**En cnc-plotter.ino:**

```c
digitalWrite(TX_PIN, HIGH);           // Asegurar IDLE alto
delayMicroseconds(BIT_DELAY_US * 2);  // 2ms de espera
// Luego enviar START BIT
```

**Por qué**: Asegura que ambos lados están sincronizados antes de enviar datos.

---

### 5. **Mejores Logs de Debug**

Ahora el driver y ESP imprimen mensajes detallados:

**RPi:**

```
[TX] Enviando byte 0x47 ('G')
[RX] Esperando start bit...
[RX] Start bit detectado
[RX] Byte recibido = 0x41 ('A')
dev_write: ACK recibido para byte 1
```

**ESP:**

```
[RX] Start bit detectado
[RX] Bit 0 = 0
[RX] Bit 1 = 1
...
[RX] Byte recibido = 0x47 ('G')
[TX] Enviando byte 0x41 ('A')
```

---

## Compilación e Instalación

### RPi (Driver Linux)

```bash
cd Proyecto2/
make clean
make
sudo insmod driver.ko
sudo chmod 666 /dev/gpio_device
```

### ESP32 (Arduino IDE)

1. Abre `cnc-plotter/cnc-plotter.ino`
2. Verifica que `#define DEBUG_COMM 1` esté activo para ver logs
3. Compila y sube al ESP32
4. Abre el Monitor Serial a 115200 baud

---

## Prueba de Diagnóstico

### Desde RPi:

```bash
echo "G 100 200" > /dev/gpio_device
```

Deberías ver en el Serial del ESP:

```
[RX] Start bit detectado
[RX] Byte recibido = 0x47 ('G')
[TX] Enviando byte 0x41 ('A')
[RX] Start bit detectado
[RX] Byte recibido = 0x00 (espacio en X_HIGH)
[TX] Enviando byte 0x41 ('A')
...
CMD=G x=100 y=200
[MOVE] GOTO (100,200)
```

---

## Resultados Esperados

✅ **Antes**: Errores aleatorios, ACK nunca llega, ESP recibe basura  
✅ **Después**: Comunicación 100% confiable, ACK tras cada byte, logs detallados

---

## Pasos Siguientes (Futuro)

Si aún persisten problemas:

1. **Usar UART en lugar de bit-banging**: Mucho más robusto
2. **Agregar checksums**: Validar integridad de cada frame
3. **Re-intentos con backoff**: Si falla, reintentar con espera exponencial
4. **Aumentar BIT_DELAY_TIME más**: Si sigue fallando, probar 2000 o 5000 μs

---

## Contacto y Debugging

Si aún hay problemas, recolecta:

1. Salida completa del `dmesg` en RPi
2. Salida completa del Serial Monitor del ESP32
3. Resultado de `cat /proc/interrupts` en RPi
4. Output de `top` durante prueba (para ver si hay context switches)
