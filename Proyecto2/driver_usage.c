//gpio driver para control de leds en raspberry pi2
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/string.h>



#define DEVICE_NAME "gpio_device"
#define MOCK_BUF_SIZE 128

static int major;

static char last_write[MOCK_BUF_SIZE];
static size_t last_write_len;


//como estoy tocando bajo nivel ocupo escribir en un archivo
//se toman los datos desde el espacio de usuario
static ssize_t dev_write(struct file *file, const char __user *buf, size_t len, loff_t *offset)
{
    size_t n;

    n = min_t(size_t, len, (size_t)MOCK_BUF_SIZE - 1);

    if (copy_from_user(last_write, buf, n)) //copia de usuario hacua kernel
        return -EFAULT;

    last_write[n] = '\0';
    last_write_len = n;

    pr_info("%s: write(%zu bytes): '%s'\n", DEVICE_NAME, n, last_write);
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
    pr_info("Mock char-device driver Initialized\n");

    major = register_chrdev(0, DEVICE_NAME, &gpio_fops);
    if (major < 0) {
        pr_alert("Failed to register GPIO device\n");
        return major;
    }

    last_write_len = 0;
    memset(last_write, 0, sizeof(last_write));

    pr_info("Mock driver loaded. major=%d (device: /dev/%s)\n", major, DEVICE_NAME);
    return 0;
}

static void __exit gpio_driver_exit(void){
    unregister_chrdev(major, DEVICE_NAME);
    pr_info("Mock driver Exited\n");
}

module_init(gpio_driver_init);
module_exit(gpio_driver_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Operativos 2026");
MODULE_DESCRIPTION("GPIO Driver");