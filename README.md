# CNC-Machine
Proyecto Final de Curso de Principios de Sistemas Operativos


Para probarlo:
sudo insmod driver_usage.ko
sudo dmesg | tail -n 50
-> se busca la linea donde venga el major

se crea el dev: sudo mknod /dev/gpio_device c MAJOR 0
habilito para que me deje escribir: sudo chmod 666 /dev/gpio_device 

echo "hola" | sudo tee /dev/gpio_device > /dev/null

## Program Execution and Compiling:

### Compiling all the needed parts:
make -C biblioteca clean && make -C biblioteca all
make -C cliente clean && make -C cliente all
make -C servidor clean && make -C servidor all
make -C mpi clean && make -C mpi all

### Verification:
ls -la biblioteca/bin/libcnc.a cliente/cliente servidor/servidor mpi/mpi_processor

### Execution for only 1 node:

Terminal 1: Server(port 8000)
```bash
cd /home/fabs/Github/CNC-Machine/Proyecto2/servidor
./servidor
```
Terminal 2: Client
```bash
cd /home/fabs/Github/CNC-Machine/Proyecto2/cliente
./cliente localhost ./imagen.png
```

### Execution for more than 1 node:

agregar aquí como es con varias compus


## Driver Instalation and Compiling:
For removing copies of a previous driver:
```bash
sudo rmmod driver
```
Driver instalation:
```bash
sudo insmod driver.ko
```
Removing previous copies:
```bash
sudo rm /dev/gpio_device
```
For visualization:
```bash
sudo dmesg | tail -n 50
```
For creating the device file(Sustituir major por el numero correspondiente):
```bash
mknod /dev/gpio_device c MAJOR 0
```
For giving permissions:
```bash
sudo chmod 666 /dev/gpio_device
```
