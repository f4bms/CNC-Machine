# CNC-Machine
Proyecto Final de Curso de Principios de Sistemas Operativos


Por ahorita el archivo de prueba de las funciones basicas del driver es driver_usage.c

Para probarlo:
sudo insmod driver_usage.ko
sudo dmesg | tail -n 50
-> se busca la linea donde venga el major

se crea el dev: sudo mknod /dev/gpio_device c MAJOR 0
habilito para que me deje escribir: sudo chmod 666 /dev/gpio_device 

echo "hola" | sudo tee /dev/gpio_device > /dev/null

