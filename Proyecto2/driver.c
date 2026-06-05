//gpio driver para control de leds en raspberry pi2
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>

//se puede agregar un class_create y el device create para no tener que yo manuealmente crear el archivo para hablar con el driver
//ahorita se ocupa usar el mkmod


#define DEVICE_NAME "gpio_device"
#define GPIO_BASE_PHYS  0X7FE200000
#define NUM_LEDS 1
#define LED_PIN 17

static int major;
static void __iomem *gpio_base;


//calculo y uso de pines necesarios
//la raspberry 2 usa 3 bits por pin
//cada registro de la raspberry controla 10 pines -> hay que moverse entonces en esos registros
static void gpio_set_output(int pin){
    unsigned int reg = pin / 10;
    unsigned int shift = (pin % 10) * 3;
    unsigned int value = ioread32(gpio_base + (reg * 4));

    pr_info("config GPIO%d como salida (reg=%u shift=%u)\n", pin, reg, shift);
    value &= ~(7 << shift);
    value |= (1 << shift);

    // Set the GPIO pin to output mode
    iowrite32(value, gpio_base + (reg * 4));

}

static void gpio_write(int pin, int value){

    if(value)
        iowrite32(1u << pin, gpio_base + 0x1C); // turns on the pin
    else
        iowrite32(1u << pin, gpio_base + 0x28); // turns off the pin
}

//como estoy tocando bajo nivel ocupo escribir en un archivo
//se toman los datos desde el espacio de usuario
static ssize_t dev_write(struct file *file, const char __user *buf, size_t len, loff_t *offset)
{
    char data[NUM_LEDS] = {0};

    if (len > NUM_LEDS) len = NUM_LEDS; // se limita la longitud a 1 byte
    if (len == 0) return 0;
    
    if (copy_from_user(data, buf, len)) //copia de usuario hacua kernel
        return -EFAULT;

    // se espera 1 para encenderlo, cualquier otro valor lo apaga
    if (data[0] == '1') {
        gpio_write(LED_PIN, 1);
        pr_info("LED encendido {GPIO%d}\n", LED_PIN);
    } else {
        gpio_write(LED_PIN, 0);
        pr_info("LED apagado {GPIO%d}\n", LED_PIN);
    }

    return len;
}

//aqui agrego la otra funcion de read cuando vaya a hacer lo demás
static struct file_operations gpio_fops = {
    .owner = THIS_MODULE,
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
    gpio_base = ioremap(GPIO_BASE_PHYS, 0x86);
    if (!gpio_base) {
        pr_alert("Failed to map GPIO memory\n");
        unregister_chrdev(major, DEVICE_NAME);
        return -ENOMEM;
    }


    gpio_set_output(LED_PIN);

    pr_info("GPIO Driver loaded. major=%d\n", major);
    return 0;
}

static void __exit gpio_driver_exit(void){
    gpio_write(LED_PIN, 0);
    iounmap(gpio_base);
    unregister_chrdev(major, DEVICE_NAME);

    pr_info("GPIO Driver Exited\n");
}

module_init(gpio_driver_init);
module_exit(gpio_driver_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Operativos 2026");
MODULE_DESCRIPTION("GPIO Driver");