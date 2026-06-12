// Driver kernel para controlar GPIO en Raspberry Pi 2 mediante comunicación serial

#include <linux/module.h>  // Macros y tipos para módulos del kernel
#include <linux/kernel.h>  // Tipos y funciones básicas del kernel
#include <linux/fs.h>      // Estructuras de archivo y operaciones
#include <linux/uaccess.h> // Funciones para copiar datos entre kernel y user space
#include <linux/io.h>      // Funciones para I/O de memoria (ioremap, ioread, iowrite)
#include <linux/delay.h>   // Funciones de delay (udelay)

// se puede agregar un class_create y el device create para no tener que yo manuealmente crear el archivo para hablar con el driver
// ahorita se ocupa usar el mkmod

// Configuración del dispositivo
#define DEVICE_NAME "gpio_device"   // Nombre del dispositivo de caracteres
#define GPIO_BASE_PHYS 0xFE200000UL // Dirección base de GPIO en memoria física

// Tamaño del mapa de memoria GPIO
#define GPIO_MAP_SIZE 0XB4 // Tamaño del rango de registros GPIO
#define GPIO_PIN_MAX 27    // Pin GPIO máximo disponible

// Pines asignados para comunicación serial
#define WRITE_PIN 17 // Pin GPIO 17 para transmisión (TX)
#define READ_PIN 27  // Pin GPIO 27 para recepción (RX)

// Offsets de registros GPIO (para controlar GPIO 0-31)
#define GPSET0_OFFSET 0x1C // Offset para escribir 1 (encender pin)
#define GPCLR0_OFFSET 0x28 // Offset para escribir 0 (apagar pin)
#define GPLEV0_OFFSET 0x34 // Offset para leer nivel del pin

// Tiempo de delay entre bits en microsegundos
#define BIT_DELAY_TIME 1000 // 1000 μs = 1 ms por bit (velocidad de transmisión)

// Comandos para la máquina CNC
#define CMD_GOTO 'G'    // Comando: mover a coordenadas (sin dibujar)
#define CMD_DRAW 'D'    // Comando: dibujar línea hacia coordenadas
#define CMD_PENUP 'U'   // Comando: levantar la pluma/herramienta
#define CMD_PENDOWN 'P' // Comando: bajar la pluma/herramienta

// Variables globales del driver
static int major;               // Número mayor asignado al dispositivo de caracteres
static void __iomem *gpio_base; // Puntero a la memoria mapeada de GPIO

// Configura un pin GPIO como salida
// Nota: La Raspberry Pi 2 usa 3 bits por pin en los registros GPFSEL
//       Cada registro controla 10 pines, por lo que se debe calcular el registro y el desplazamiento correcto
static void gpio_set_output(int pin)
{
    // Calcular el registro (cada uno controla 10 pines)
    unsigned int reg = pin / 10;
    // Calcular el desplazamiento dentro del registro (3 bits por pin)
    unsigned int shift = (pin % 10) * 3;
    // Leer el valor actual del registro
    unsigned int value = ioread32(gpio_base + (reg * 4));

    // Validar que el pin esté dentro del rango permitido
    if (pin < 0 || pin > GPIO_PIN_MAX)
    {
        pr_alert("gpio_set_output: pin %d invalido\n", pin);
        return;
    }

    // Limpiar los 3 bits para este pin (establecer a 000)
    value &= ~(7U << shift);
    // Establecer a 001 para modo salida
    value |= (1U << shift);

    // Escribir el valor modificado en el registro GPFSEL
    iowrite32(value, gpio_base + (reg * 4));
    pr_info("config GPIO%d como salida (reg=%u shift=%u)\n", pin, reg, shift);
}

// Escribe un valor (0 o 1) en un pin GPIO
static void gpio_write(int pin, int value)
{
    if (value)
        // Escribir 1 lógico: establecer el pin en HIGH
        iowrite32(1u << pin, gpio_base + GPSET0_OFFSET);
    else
        // Escribir 0 lógico: establecer el pin en LOW
        iowrite32(1u << pin, gpio_base + GPCLR0_OFFSET);
}

// Configura un pin GPIO como entrada
static void gpio_set_input(int pin)
{
    unsigned int reg;   // Registro GPFSEL para este pin
    unsigned int shift; // Desplazamiento dentro del registro
    unsigned int value; // Valor actual del registro

    // Validar que el pin esté dentro del rango permitido
    if (pin < 0 || pin > 27)
    {
        pr_alert("gpio_set_input: pin %d invalido\n", pin);
        return;
    }

    // Calcular el registro (cada uno controla 10 pines)
    reg = pin / 10;
    // Calcular el desplazamiento dentro del registro (3 bits por pin)
    shift = (pin % 10) * 3;
    // Leer el valor actual del registro
    value = ioread32(gpio_base + (reg * 4));

    // Limpiar los 3 bits para este pin (establecer a 000 = modo entrada)
    value &= ~(7U << shift);

    // Escribir el valor modificado
    iowrite32(value, gpio_base + (reg * 4));
    pr_info("config GPIO%d como entrada (reg=%u shift=%u)\n", pin, reg, shift);
}

// Lee el valor actual (0 o 1) de un pin GPIO
static int gpio_read(int pin)
{
    u32 level; // Valor del registro GPLEV0

    // Validar que el pin esté dentro del rango permitido
    if (pin < 0 || pin > GPIO_PIN_MAX)
        return 0;

    // Leer el registro que contiene el nivel actual de los pines
    level = ioread32(gpio_base + GPLEV0_OFFSET);

    // Extraer el bit para este pin y retornar 1 o 0
    return (level & (1u << pin)) ? 1 : 0;
}

// Transmite un byte usando comunicación serial UART por GPIO
// Formato: 1 START BIT (0) + 8 BITS DE DATOS (LSB primero) + 1 STOP BIT (1)
static void send_byte(u8 byte)
{
    int i;
    unsigned long start_time;

    // Log de depuración: mostrar el byte a enviar en hex y ASCII
    pr_info("[TX] Enviando byte 0x%02X ('%c')\n",
            byte,
            (byte >= 32 && byte <= 126) ? byte : '.');

    // Asegurar que el pin está en IDLE (HIGH) antes de empezar la transmisión
    gpio_write(WRITE_PIN, 1);
    // Esperar para asegurar sincronización
    udelay(BIT_DELAY_TIME * 2);

    // Transmitir START BIT (nivel bajo)
    gpio_write(WRITE_PIN, 0);
    udelay(BIT_DELAY_TIME);

    // Transmitir 8 BITS DE DATOS en orden LSB primero (bit menos significativo primero)
    for (i = 0; i < 8; i++)
    {
        // Extraer el bit i del byte
        int bit = (byte >> i) & 1;
        gpio_write(WRITE_PIN, bit);

        // Esperar una duración de bit
        udelay(BIT_DELAY_TIME);
    }

    // Transmitir STOP BIT (nivel alto)
    gpio_write(WRITE_PIN, 1);

    // Pausa final para asegurar que el receptor detecte correctamente el stop bit
    udelay(BIT_DELAY_TIME * 4);

    pr_info("[TX] Byte 0x%02X enviado completamente\n", byte);
}

// Recibe un byte usando comunicación serial UART por GPIO
// Espera el START BIT y luego lee 8 bits de datos seguidos del STOP BIT
static int receive_byte(void)
{
    int i;                // Variable de iteración
    u8 byte = 0;          // Byte a recibir
    int timeout = 100000; // Contador de timeout
    unsigned long start_time;
    int bit_value; // Valor del bit actual

    pr_info("[RX] Esperando start bit...\n");

    // Esperar a que llegue el START BIT (pin en bajo) con timeout robusto
    timeout = 100000000;
    while (gpio_read(READ_PIN) == 1)
    {
        if (--timeout == 0)
        {
            pr_alert("[RX] Timeout esperando start bit\n");
            return -ETIMEDOUT; // Retornar error si se agota el timeout
        }
        // Espera mínima para no consumir 100% de CPU
        udelay(1);
    }

    // Esperar a estar en el centro del primer bit de datos
    // Start bit: 0-BIT_DELAY_TIME μs
    // Centro del primer bit: ~1.5 * BIT_DELAY_TIME desde inicio del start bit
    udelay(BIT_DELAY_TIME + BIT_DELAY_TIME / 2);

    // Leer 8 bits de datos en orden LSB primero
    for (i = 0; i < 8; i++)
    {
        // Leer el nivel actual del pin
        bit_value = gpio_read(READ_PIN);

        // Si el bit es 1, establecerlo en la posición i del byte
        if (bit_value)
        {
            byte |= (1U << i);
        }

        // Esperar exactamente un período de bit para el siguiente bit
        udelay(BIT_DELAY_TIME);
    }

    // Verificar el STOP BIT (debería estar en HIGH)
    udelay(BIT_DELAY_TIME);
    int stop_bit = gpio_read(READ_PIN);
    pr_info("[RX] Stop bit = %d\n", stop_bit);

    // Advertencia si el stop bit no está en alto (indicaría un error de transmisión)
    if (stop_bit != 1)
    {
        pr_alert("[RX] Advertencia: Stop bit no es HIGH (valor=%d)\n", stop_bit);
    }

    // Log del byte recibido
    pr_info("[RX] Byte recibido = 0x%02X ('%c')\n",
            byte,
            (byte >= 32 && byte <= 126) ? byte : '.');

    return byte;
}

// Función de escritura del dispositivo de caracteres
// Recibe comandos del espacio de usuario y los envía al microcontrolador
// Formato: "<CMD> <X> <Y>" para GOTO y DRAW, o "<CMD>" para PENUP y PENDOWN
static ssize_t dev_write(struct file *file, const char __user *buf, size_t len, loff_t *offset)
{
    char data[32] = {0}; // Buffer para almacenar los datos del usuario
    char cmd;            // Comando a ejecutar
    int x = 0;           // Coordenada X
    int y = 0;           // Coordenada Y
    int parsed;          // Resultado del parseo
    int ack;             // Confirmación recibida

    // Limitar la longitud de datos a leer
    if (len >= sizeof(data))
        len = sizeof(data) - 1;
    if (len == 0)
        return 0;

    // Copiar datos desde el espacio de usuario al kernel
    if (copy_from_user(data, buf, len))
        return -EFAULT;

    // Asegurar que la cadena esté terminada en nulo
    data[len] = '\0';

    // Parsear el comando y los argumentos del string recibido
    parsed = sscanf(data, "%c %d %d", &cmd, &x, &y);
    if (parsed < 1)
    {
        pr_alert("dev_write: formato invalido\n");
        return -EINVAL;
    }

    // Validar que el comando sea uno de los conocidos
    if (cmd != CMD_GOTO && cmd != CMD_DRAW && cmd != CMD_PENUP && cmd != CMD_PENDOWN)
    {
        pr_alert("dev_write: comando desconocido '%c'\n", cmd);
        return -EINVAL;
    }

    // Validar que las coordenadas estén dentro del rango permitido
    if (x < 0 || x > 10000 || y < 0 || y > 10000)
    {
        pr_alert("dev_write: coordenadas fuera de rango (x=%d, y=%d)\n", x, y);
        return -EINVAL;
    }

    // Log del comando recibido
    pr_info("dev_write: comando='%c', x=%d, y=%d\n", cmd, x, y);

    // Bandera para indicar si se recibió ACK válido
    int ackFlag = 0;

    // Bucle de reintentos hasta recibir ACK válido
    do
    {
        ackFlag = 1; // Asumir éxito

        // Enviar 5 bytes: CMD + X(alto) + X(bajo) + Y(alto) + Y(bajo)
        send_byte((u8)cmd);
        send_byte((u8)(x >> 8)); // Byte alto de X
        send_byte((u8)x & 0xFF); // Byte bajo de X
        send_byte((u8)(y >> 8)); // Byte alto de Y
        send_byte((u8)y & 0xFF); // Byte bajo de Y

        // Esperar confirmación (ACK) del microcontrolador
        ack = receive_byte();
        if (ack < 0)
        {
            pr_alert("dev_write: error al recibir ACK\n");
            ackFlag = 0; // Reintentar
        }
        // Validar que el ACK sea el carácter 'A'
        if ((u8)ack != 'A')
        {
            pr_alert("dev_write: ACK recibido pero invalido: %d\n", ack);
            ackFlag = 0; // Reintentar
        }
    } while (ackFlag == 0);

    pr_info("dev_write: ACK recibido, comando ejecutado correctamente\n");
    return len;
}

// Función de lectura del dispositivo de caracteres
// Por ahora no retorna datos ya que no se necesita lectura desde el espacio de usuario
// Nota: El pin de lectura se usa dentro de dev_write() como confirmación de recepción
static ssize_t dev_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
    return 0; // No hay datos para leer
}

// Estructura que define las operaciones permitidas en el dispositivo de caracteres
static struct file_operations gpio_fops = {
    .owner = THIS_MODULE, // Propietario del módulo
    .read = dev_read,     // Función para lectura (actualmente no implementada)
    .write = dev_write,   // Función para escritura (envía comandos a la CNC)
};

// Función de inicialización del módulo kernel
static int __init gpio_driver_init(void)
{
    pr_info("GPIO Driver Initialized\n");

    // Registrar el dispositivo de caracteres en el kernel
    major = register_chrdev(0, DEVICE_NAME, &gpio_fops);
    if (major < 0)
    {
        pr_alert("Failed to register GPIO device\n");
        return major;
    }

    // Mapear los registros GPIO a la memoria del kernel
    gpio_base = ioremap(GPIO_BASE_PHYS, 0xB4);
    if (!gpio_base)
    {
        pr_alert("Failed to map GPIO memory\n");
        unregister_chrdev(major, DEVICE_NAME);
        return -ENOMEM;
    }

    // Configurar los pines GPIO
    gpio_set_output(WRITE_PIN); // Pin para transmisión
    gpio_set_input(READ_PIN);   // Pin para recepción

    // Establecer el pin de transmisión en IDLE (HIGH) según protocolo UART
    gpio_write(WRITE_PIN, 1);

    pr_info("GPIO Driver loaded. major=%d\n", major);
    return 0;
}

// Función de salida/limpieza del módulo kernel
static void __exit gpio_driver_exit(void)
{
    // Deshacer el mapeo de memoria GPIO
    iounmap(gpio_base);

    // Desregistrar el dispositivo de caracteres
    unregister_chrdev(major, DEVICE_NAME);

    pr_info("GPIO Driver Exited\n");
}

// Registrar las funciones de inicialización y salida
module_init(gpio_driver_init);
module_exit(gpio_driver_exit);

// Información del módulo
MODULE_LICENSE("GPL");             // Licencia del módulo
MODULE_AUTHOR("Operativos 2026");  // Autor
MODULE_DESCRIPTION("GPIO Driver"); // Descripción del módulo
