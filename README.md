# CNC-Machine
Proyecto Final de Curso de Principios de Sistemas Operativos


Por ahorita el archivo de prueba de las funciones basicas del driver es driver_usage.c

Prueba de gpio real:

make clean && make

sudo rmmod driver 2>/dev/null || true
sudo insmod ./driver.ko
ls -l /dev/gpio_device
dmesg | tail -n 30

escribo: 
echo "1" | sudo tee /dev/gpio_device > /dev/null
echo "0" | sudo tee /dev/gpio_device > /dev/null

