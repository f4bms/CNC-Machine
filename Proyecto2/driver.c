//gpio driver para control de leds en raspberry pi2
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/delay.h>


//se puede agregar un class_create y el device create para no tener que yo manuealmente crear el archivo para hablar con el driver
//ahorita se ocupa usar el mkmod

#define DEVICE_NAME "gpio_device"
#define GPIO_BASE_PHYS  0xFE200000UL

#define GPIO_MAP_SIZE 0XB4
#define GPIO_PIN_MAX 27

#define WRITE_PIN 17
#define READ_PIN 27

// offsets GPIO (para GPIO 0-31)
#define GPSET0_OFFSET 0x1C
#define GPCLR0_OFFSET 0x28
#define GPLEV0_OFFSET 0x34

#define BIT_DELAY_TIME 1000

// para la cnc se usan:

#define CMD_GOTO 'G'
#define CMD_DRAW 'D'
#define CMD_PENUP 'U'
#define CMD_PENDOWN 'P'

static int major;
static void __iomem *gpio_base;


//calculo y uso de pines necesarios
//la raspberry 2 usa 3 bits por pin
//cada registro de la raspberry controla 10 pines -> hay que moverse entonces en esos registros
static void gpio_set_output(int pin){


    unsigned int reg = pin / 10;
    unsigned int shift = (pin % 10) * 3;
    unsigned int value = ioread32(gpio_base + (reg * 4));

    if (pin < 0 || pin > GPIO_PIN_MAX) {
    pr_alert("gpio_set_output: pin %d invalido\n", pin);
    return;
    }

    value &= ~(7U << shift);
    value |=  (1U << shift);

    // Set the GPIO pin to output mode
    iowrite32(value, gpio_base + (reg * 4));
    pr_info("config GPIO%d como salida (reg=%u shift=%u)\n", pin, reg, shift);


}

static void gpio_write(int pin, int value){

    if(value)
        iowrite32(1u << pin, gpio_base + GPSET0_OFFSET); // turns on the pin
    else
        iowrite32(1u << pin, gpio_base + GPCLR0_OFFSET); // turns off the pin
}

static void gpio_set_input(int pin)
{
    unsigned int reg;
    unsigned int shift;
    unsigned int value;

    if (pin < 0 || pin > 27) {
        pr_alert("gpio_set_input: pin %d invalido\n", pin);
        return;
    }

    reg = pin / 10;
    shift = (pin % 10) * 3;
    value = ioread32(gpio_base + (reg * 4));

    value &= ~(7U << shift); // 000 = input
    iowrite32(value, gpio_base + (reg * 4));
    pr_info("config GPIO%d como entrada (reg=%u shift=%u)\n", pin, reg, shift);

}

static int gpio_read(int pin)
{
    u32 level;

    if (pin < 0 || pin > GPIO_PIN_MAX)
        return 0;

    level = ioread32(gpio_base + GPLEV0_OFFSET);
    return (level & (1u << pin)) ? 1 : 0;
}

// OPCIÓN 2: Sincronización robusta para driver.c (RPi)
// Reemplaza las funciones send_byte() y receive_byte() en tu driver.c

static void send_byte(u8 byte){
    int i;
    unsigned long start_time;
    
    pr_info("[TX] Enviando byte 0x%02X ('%c')\n", 
            byte, 
            (byte >= 32 && byte <= 126) ? byte : '.');
    
    // IMPORTANTE: Asegura que el pin está en IDLE (HIGH) antes de empezar
    gpio_write(WRITE_PIN, 1);
    udelay(BIT_DELAY_TIME * 2);  // Pausa para asegurar sincronización
    
    // START BIT (LOW)
    gpio_write(WRITE_PIN, 0);
    // pr_info("[TX] Start bit enviado\n");
    udelay(BIT_DELAY_TIME);
    
    // 8 BITS DE DATOS (LSB first)
    for (i = 0; i < 8; i++) {
        int bit = (byte >> i) & 1;
        gpio_write(WRITE_PIN, bit);
        
        // pr_info("[TX] Bit %d = %d\n", i, bit);
        
        udelay(BIT_DELAY_TIME);
    }
    
    // STOP BIT (HIGH)
    gpio_write(WRITE_PIN, 1);
    //pr_info("[TX] Stop bit enviado\n");
    udelay(BIT_DELAY_TIME*4);  // Pausa final para asegurar que el receptor detecte el stop bit
    
    pr_info("[TX] Byte 0x%02X enviado completamente\n", byte);
}

static int receive_byte(void){
    int i;
    u8 byte = 0;
    int timeout = 100000;
    unsigned long start_time;
    int bit_value;
    
    pr_info("[RX] Esperando start bit...\n");
    
    // Espera START BIT (LOW) con timeout robusto
    timeout = 100000000;
    while(gpio_read(READ_PIN) == 1) {
        if (--timeout == 0) {
            pr_alert("[RX] Timeout esperando start bit\n");
            return -ETIMEDOUT;
        }
        udelay(1);  // Sleep mínimo para no usar 100% CPU
    }
    
    //pr_info("[RX] Start bit detectado\n");
    
    // Espera a estar en el CENTRO del primer bit
    // Start bit: 0-500 μs
    // Centro de Bit 0: ~750 μs desde inicio del start bit
    udelay(BIT_DELAY_TIME + BIT_DELAY_TIME / 2);  // 750 μs
    
    // Lee 8 bits
    for (i = 0; i < 8; i++) {
        bit_value = gpio_read(READ_PIN);
        
        if (bit_value) {
            byte |= (1U << i);
        }
        
        // pr_info("[RX] Bit %d = %d\n", i, bit_value);
        
        // Espera exactamente BIT_DELAY_TIME para el siguiente bit
        udelay(BIT_DELAY_TIME);
    }
    
    // Verifica STOP BIT (debería ser HIGH)
    udelay(BIT_DELAY_TIME);
    int stop_bit = gpio_read(READ_PIN);
    pr_info("[RX] Stop bit = %d\n", stop_bit);
    
    if (stop_bit != 1) {
        pr_alert("[RX] Advertencia: Stop bit no es HIGH (valor=%d)\n", stop_bit);
    }
    
    pr_info("[RX] Byte recibido = 0x%02X ('%c')\n", 
            byte, 
            (byte >= 32 && byte <= 126) ? byte : '.');
    
    return byte;
}
//como estoy tocando bajo nivel ocupo escribir en un archivo
//se toman los datos desde el espacio de usuario
//aquí debo de agregar algo que cuando se integre con la biblioteca cargue los datos en orden como se está creando el formato
//el formato siendo con las instrucciones de arriba (CMD, 0, 0) -> ESO PARA GOTO Y DRAW LINE, (CMD) -> PARA PENUP Y PENDOWN
static ssize_t dev_write(struct file *file, const char __user *buf, size_t len, loff_t *offset)
{
    char data[32] = {0};
    char cmd;
    int x = 0;
    int y = 0;
    int parsed;
    int ack;

    if (len >= sizeof(data)) len = sizeof(data) - 1;
    if (len == 0) return 0;

    if (copy_from_user(data, buf, len)) //copia de usuario hacua kernel
        return -EFAULT;

    data[len] = '\0'; // Asegurarse de que la cadena esté terminada en nulo

    // Parsear el comando y los argumentos
    parsed = sscanf(data, "%c %d %d", &cmd, &x, &y);
    if (parsed < 1) {
        pr_alert("dev_write: formato invalido\n");
        return -EINVAL;
    }

    if (cmd != CMD_GOTO && cmd != CMD_DRAW && cmd != CMD_PENUP && cmd != CMD_PENDOWN) {
        pr_alert("dev_write: comando desconocido '%c'\n", cmd);
        return -EINVAL;
    }

    //aquí ocupo setear como la cantidad de valores permitidos de entrada en x y y (se puede cambiar despues si es necesario) -> jeremy revisar

    if (x < 0 || x > 10000 || y < 0 || y > 10000) {
        pr_alert("dev_write: coordenadas fuera de rango (x=%d, y=%d)\n", x, y);
        return -EINVAL;
    }

    pr_info("dev_write: comando='%c', x=%d, y=%d\n", cmd, x, y);
    
    int ackFlag = 0;
    
    do{
        ackFlag = 1;
        //ahora si se envian los datos de 5 bytes, entonces seria CMD X(high) X(low)  Y(high) Y(low)
        send_byte((u8)cmd);
        send_byte((u8)(x >> 8));
        send_byte((u8)x & 0xFF);
        send_byte((u8)(y >> 8));
        send_byte((u8)y & 0xFF);

        ack = receive_byte();
        if (ack < 0) {
            pr_alert("dev_write: error al recibir ACK\n");
            ackFlag = 0;
        }
        if ((u8)ack != 'A') {
            pr_alert("dev_write: ACK recibido pero invalido: %d\n", ack);
            ackFlag = 0;
        }
    }while(ackFlag == 0);

    pr_info("dev_write: ACK recibido, comando ejecutado correctamente\n");
    return len;
}

//por ahorita el read vuelve a hacer nada pq pues no ocupo que el usuario lea nada de la cnc -> preguntarle a jeremy
//IMPORTANTE: se está usando el pin de lectura dentro de la función de escritura como medio de confirmación de recepción
//sin embargo, la funcion como tal no se usa pq no ocupo exponerla al user space
static ssize_t dev_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
    return 0;
}

static struct file_operations gpio_fops = {
    .owner = THIS_MODULE,
    .read  = dev_read,
    .write = dev_write,
};

//initialization
static int __init gpio_driver_init(void)
{
    pr_info("GPIO Driver Initialized\n");

    major = register_chrdev(0, DEVICE_NAME, &gpio_fops);
    if (major < 0) {
        pr_alert("Failed to register GPIO device\n");
        return major;
    }

    //mapea los registros gpio en el espacio de direcciones del kernel
    gpio_base = ioremap(GPIO_BASE_PHYS, 0xB4);
    if (!gpio_base) {
        pr_alert("Failed to map GPIO memory\n");
        unregister_chrdev(major, DEVICE_NAME);
        return -ENOMEM;
    }


    gpio_set_output(WRITE_PIN);
    gpio_set_input(READ_PIN);

    //como nos basamos en uart dejemos el tx(write) en high
    gpio_write(WRITE_PIN, 1);

    pr_info("GPIO Driver loaded. major=%d\n", major);
    return 0;
}

static void __exit gpio_driver_exit(void){
    iounmap(gpio_base);
    unregister_chrdev(major, DEVICE_NAME);

    pr_info("GPIO Driver Exited\n");
}

module_init(gpio_driver_init);
module_exit(gpio_driver_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Operativos 2026");
MODULE_DESCRIPTION("GPIO Driver");
