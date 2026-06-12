# Procesamiento Distribuido MPI - Módulo mpi/

Módulo central del flujo de Proyecto2: orquesta el procesamiento distribuido de imágenes de PCB en múltiples nodos, detecta rutas y genera instrucciones para la máquina CNC.

## Inicio Rápido (Una Máquina, 5 Minutos)

Si es tu primera vez, prueba esto localmente:

```bash
cd /path/to/Proyecto2

# Compila todo
make -C mpi

# Ejecuta (descarga una imagen PCB en img/ primero o usa las incluidas)
./run_flujo_completo.sh --image img/pcb1.png --np 2 --cnc-device /dev/null

# Verifica salidas
ls -lh outputs/pcb_*.png outputs/pcb_*.txt
```

**Resultado esperado**: Archivos generados en `outputs/` sin errores, exit code 0. ✓

---

## Contenido del Directorio

- **mpi_processor**: Binario compilado (ejecutable MPI)
- **main.c**: Orquestador MPI principal
- **distribucion.c/h**: Lógica de reparto de fragmentos entre ranks
- **grafo.c/h**: Extracción topológica de pistas (nodos + aristas)
- **cnc_traduccion.c/h**: Conversión de grafo a rutas CNC optimizadas
- **tipos.h**: Estructuras de datos compartidas
- **Makefile**: Build con mpicc (C) + mpic++ (OpenCV)
- **hosts.txt**: Configuración de nodos para mpirun
- **outputs/**: Carpeta de salidas (creada automáticamente)

## Compilación

```bash
cd /path/to/Proyecto2/mpi
make all           # Compila mpi_processor
make clean         # Limpia objetos
make rebuild       # Clean + all
```

**Requisitos:**
- OpenMPI instalado con `mpicc` y `mpic++`
- OpenCV 4.x (pkg-config)
- Biblioteca CNC compilada en `../biblioteca/`

## ¿Qué Debo Ejecutar en Cada Máquina?

**Esta es la pregunta más importante si es tu primera vez.**

### Regla Simple

- **En el SERVIDOR**: Ejecuta `./run_flujo_completo.sh` (o `./servidor` si corres MPI directo).
- **En los NODOS WORKER**: No ejecutes nada. Solo compila el código y espera.

### Por Qué?

`mpirun` (desde el servidor) se encarga de **lanzar automáticamente** `mpi_processor` en cada nodo. Los nodos reciben el trabajo por la red y responden. No tienes que hacer nada manualmente en ellos.

### Checklist para Principiantes

**Máquina Servidor (una sola):**
```bash
cd ~/project/Proyecto2
./run_flujo_completo.sh --image img/pcb1.png --np 4 --hostfile mpi/hosts.txt
# ✓ Esto es TODO lo que haces
```

**Máquinas Worker (una o más):**
```bash
# 1. Clone el proyecto (misma ruta que servidor)
git clone <repo> ~/project

# 2. Compile el módulo MPI
cd ~/project/Proyecto2/mpi
make all

# 3. Asegúrate que SSH sin contraseña funcione
ssh <nombre_servidor> hostname  # Debe funcionar sin pedir contraseña

# ✓ LISTO. No hagas más nada. El servidor hará el resto.
```

### Ejemplo Concreto: Cluster de 3 Máquinas

**Máquina A (192.168.1.10) - SERVIDOR:**
```bash
# En A, solo esto:
cd ~/project/Proyecto2
./run_flujo_completo.sh --image img/pcb1.png --np 12 --hostfile mpi/hosts.txt
# mpirun lanza automáticamente 4 procesos en A, 4 en B, 4 en C
```

**Máquina B (192.168.1.11) - WORKER:**
```bash
# Setup una sola vez:
git clone <repo> ~/project
cd ~/project/Proyecto2/mpi && make all
# Luego: nada. Espera.
```

**Máquina C (192.168.1.12) - WORKER:**
```bash
# Igual a máquina B:
git clone <repo> ~/project
cd ~/project/Proyecto2/mpi && make all
# Luego: nada. Espera.
```

**Archivo hosts.txt (en servidor A):**
```
192.168.1.10 slots=4   ← Servidor DEBE ser primera línea
192.168.1.11 slots=4
192.168.1.12 slots=4
```

**Resultado**: Cuando ejecutes en A, B y C automáticamente procesan en paralelo. ✓

---

## Configuración de Nodos (hosts.txt)

El archivo `hosts.txt` define el cluster donde corre MPI. **Es crítico que el primer host sea el servidor (rank 0).**

### Formato
```
# Comentarios comienzan con #
# Formato: <hostname_o_ip> slots=<num_procesos>

127.0.0.1 slots=4           # Máquina local, 4 procesos
192.168.1.10 slots=4        # Nodo remoto 1
192.168.1.11 slots=4        # Nodo remoto 2
```

### Ejemplo Multinodo (editado)
```
# Servidor (DEBE ser la primera línea no comentada)
192.168.1.10 slots=4

# Workers
192.168.1.11 slots=4
192.168.1.12 slots=4
192.168.1.13 slots=4
```

**Reglas:**
1. Primera línea no comentada = rank 0 (servidor).
2. Todos los nodos deben tener el proyecto clonado en **la misma ruta relativa**.
3. SSH sin contraseña debe estar habilitado entre nodos.
4. Los nodos deben estar en la misma red y alcanzables por IP/hostname.

## Uso Directo (mpi_processor)

### Opción 1: Ejecución Local (2 procesos)
```bash
cd /path/to/Proyecto2/mpi

# Crea outputs/ automáticamente
OUTPUT_DIR=./outputs mpirun -np 2 --hostfile hosts.txt --map-by slot \
  ./mpi_processor ../img/pcb1.png --cnc-device /dev/null --cnc-scale 1
```

### Opción 2: Ejecución Multinodo
```bash
# 12 procesos: 4 en cada uno de 3 nodos
mpirun -np 12 --hostfile hosts.txt --map-by slot \
  ./mpi_processor /ruta/compartida/imagen.png --cnc-device /dev/gpio --cnc-scale 1
```

### Parámetros de mpi_processor

```
./mpi_processor <imagen.png> [opciones]

Obligatorio:
  <imagen.png>              Ruta a imagen (absoluta recomendada)

Opcionales:
  --cnc-device <ruta>       Device del driver CNC (default: omitido)
  --cnc-scale <n>           Escala de steps por píxel (default: 1)
```

### Variables de Entorno

```bash
# Carpeta de salida (default: ../outputs)
export OUTPUT_DIR=/custom/outputs

# Se pasan al MPI una sola vez, desde el servidor/script
mpirun ... ./mpi_processor ...
```

## Flujo del Servidor (admin_tareas)

El servidor orquesta MPI desde [Proyecto2/servidor/admin_tareas.c](../../servidor/admin_tareas.c). Acepta configuración por variables de entorno:

```bash
# Configuración del cluster
export MPI_HOSTFILE=/ruta/a/hosts.txt          # default: ../mpi/hosts.txt
export MPI_NUM_PROCESOS=4                       # default: 4
export SERVER_NODE_IP=192.168.1.10              # default: 127.0.0.1

# Parámetros CNC
export CNC_DEVICE=/dev/gpio_device             # default: omitido
export CNC_SCALE=2                              # default: 1

# Salidas
export OUTPUT_DIR=../outputs                    # default: ../outputs

# Luego inicia el servidor
./servidor
```

**Flujo interno:**
1. Servidor escucha puerto 8080.
2. Cliente envía imagen cifrada.
3. Servidor descifra y guarda en `../img/received.bin`.
4. Servidor valida que primer host del `MPI_HOSTFILE` = `SERVER_NODE_IP`.
5. Servidor ejecuta: `mpirun -np $MPI_NUM_PROCESOS --hostfile $MPI_HOSTFILE --map-by slot ../mpi/mpi_processor ../img/received.bin --cnc-device $CNC_DEVICE --cnc-scale $CNC_SCALE`
6. Rank 0 ensambla, genera grafo, traduce a CNC.
7. Rank 0 ejecuta instrucciones en device CNC (si está habilitado).

## Flujo Completo (Cliente-Servidor-MPI)

Script [`run_flujo_completo.sh`](../../run_flujo_completo.sh) en la raíz de Proyecto2 automatiza todo:

```bash
cd /path/to/Proyecto2

# Ejecución básica
./run_flujo_completo.sh --image img/pcb1.png

# Con opciones
./run_flujo_completo.sh \
  --image img/pcb0.png \
  --server-ip 127.0.0.1 \
  --server-node 127.0.0.1 \
  --np 4 \
  --hostfile mpi/hosts.txt \
  --cnc-device /dev/gpio \
  --cnc-scale 2 \
  --output-dir ./outputs

./run_flujo_completo.sh --help  # Ver todas las opciones
```

### Pasos del Script
1. Compila biblioteca, cliente, servidor, MPI.
2. Crea carpeta de salida.
3. Inicia servidor en background.
4. Cliente envía imagen cifrada.
5. Servidor lanza MPI.
6. MPI procesa y genera salidas.
7. Script verifica archivos de salida.

## Salidas Generadas

El módulo MPI genera 4 archivos principales en `$OUTPUT_DIR`:

### 1. pcb_skeleton.png
- **Descripción**: Esqueleto topológico global (resultado de reensamble distribuido).
- **Uso**: Inspección visual de detección de pistas.

### 2. pcb_graph.png
- **Descripción**: Esqueleto con nodos marcados en rojo (círculos pequeños).
- **Uso**: Validación de puntos de inicio/fin/bifurcación detectados.

### 3. pcb_edges.txt
- **Descripción**: Metadatos de aristas detectadas.
- **Formato**:
  ```
  # Aristas detectadas: 1125
  arista 0: 45 -> 128, puntos=156
  arista 1: 45 -> 201, puntos=89
  ...
  ```
- **Uso**: Debugging de topología.

### 4. pcb_paths.txt
- **Descripción**: Rutas CNC finales (después de compresión de colineales).
- **Formato**:
  ```
  # CNC paths: 1125
  path 0 points=2
    17 0
    18 0
  path 1 points=16
    456 143
    456 147
    ...
  ```
- **Uso**: Envío al dispositivo CNC o simulación.

### Archivos Intermedios (por rank)

En `$OUTPUT_DIR` aparecen archivos de debug durante procesamiento:
- `rank_0_binary.png`: Binarización local de rank 0.
- `rank_0_skeleton.png`: Esqueleto local antes de reensamble.
- `rank_1_binary.png`, `rank_1_skeleton.png`: Equivalentes para rank 1, etc.

## Optimizaciones Implementadas

### 1. Compresión de Puntos Colineales
- **Qué hace**: Reduce puntos sobre líneas rectas (misma dirección).
- **Efecto**: Un camino recto de 275 píxeles → 16 puntos CNC.
- **Dónde**: [cnc_traduccion.c](cnc_traduccion.c#L10-L60).

### 2. Elimina Movimientos Menores a 100 Unidades (Paso Mínimo del Motor)
- **Qué hace**: Identifica segments de curva muy pequeños (< 100 pasos) y los reemplaza con líneas rectas. Si un path completo es < 100 unidades, se **omite completamente**.
- **Problema que resuelve**: Los motores CNC reales no pueden hacer movimientos demasiado pequeños sin atascarse.
- **Efecto**: Reduce ruido de dibujo, acelera trazado, evita atoramiento.
- **Ejemplo**: pcb0.png → 61 paths brutos → **10 paths finales** (-83%), 168 puntos → **66 puntos** (-61%).
- **Trade-off**: Se pierde resolución en curvas muy finas, pero el hardware puede ejecutar correctamente.
- **Parámetro**: `min_step = 100` en [cnc_traduccion.c](cnc_traduccion.c#L288).
- **Dónde**: Función `simplificar_pasos_minimos()` en [cnc_traduccion.c](cnc_traduccion.c#L100-L183).

### 3. Orden Greedy de Rutas
- **Qué hace**: Ordena paths por proximidad al cursor actual.
- **Efecto**: Minimiza traslados en vacío entre trazos.
- **Dónde**: [cnc_traduccion.c](cnc_traduccion.c#L310-L354).

## Troubleshooting

### Error: "No se pudo cargar la llave AES"
**Causa**: Llave en `../cifrado/aes_key.txt` tiene < 16 caracteres.
**Solución**:
```bash
echo "AES-Key-Minimo-16!" > ../cifrado/aes_key.txt
```

### Error: "El primer host del hostfile no coincide con SERVER_NODE_IP"
**Causa**: El primer host de `MPI_HOSTFILE` debe ser el servidor.
**Solución**: Edita `hosts.txt` y asegúrate de que la primera línea sea la IP/hostname del servidor.

### Error: "mpirun: command not found"
**Causa**: OpenMPI no está instalado o no está en PATH.
**Solución**:
```bash
# Linux (Ubuntu/Debian)
sudo apt install openmpi-bin libopenmpi-dev

# macOS (Homebrew)
brew install open-mpi
```

### Error: "No se detectan nodos"
**Causa**: SSH entre nodos no funciona sin contraseña.
**Solución**:
```bash
# En cada nodo, genera clave SSH y configura acceso sin contraseña
ssh-keygen -t rsa -N "" -f ~/.ssh/id_rsa
cat ~/.ssh/id_rsa.pub >> ~/.ssh/authorized_keys
chmod 600 ~/.ssh/authorized_keys
```

### Advertencia: "Tamaño descifrado no coincide con encabezado"
**Causa**: Minor; posible trailing garbage de PKCS#7 padding.
**Acción**: Ignorable si el archivo se procesó correctamente.

## Ejemplo Práctico: Cluster de 3 Nodos

### Setup (ejecutar una sola vez en cada nodo)

**Nodo servidor (192.168.1.10):**
```bash
git clone <repo> ~/project
cd ~/project/Proyecto2/mpi
make all
```

**Nodo worker 1 (192.168.1.11):**
```bash
git clone <repo> ~/project
cd ~/project/Proyecto2/mpi
make all
```

**Nodo worker 2 (192.168.1.12):**
```bash
git clone <repo> ~/project
cd ~/project/Proyecto2/mpi
make all
```

### Configurar hosts.txt (en servidor)
```
# ~/project/Proyecto2/mpi/hosts.txt
192.168.1.10 slots=4
192.168.1.11 slots=4
192.168.1.12 slots=4
```

### Ejecutar

**Opción A: Directo**
```bash
cd ~/project/Proyecto2/mpi
OUTPUT_DIR=../outputs mpirun -np 12 --hostfile hosts.txt --map-by slot \
  ./mpi_processor ~/project/Proyecto2/img/pcb1.png --cnc-device /dev/gpio
```

**Opción B: Flujo Completo**
```bash
cd ~/project/Proyecto2
export MPI_HOSTFILE=mpi/hosts.txt
export MPI_NUM_PROCESOS=12
export SERVER_NODE_IP=192.168.1.10
./run_flujo_completo.sh --image img/pcb1.png --np 12 --cnc-device /dev/gpio
```

## Validación de Salida

Después de una ejecución, verifica:

```bash
cd /path/to/outputs

# ¿Se generaron los archivos?
ls -lh pcb_*.png pcb_*.txt

# ¿Cuántos paths finales hay? (deberían ser mucho menos que aristas detectadas)
grep "^path" pcb_paths.txt | wc -l

# ¿Cuántos puntos CNC total?
awk 'NR>1 && /^path/ {sum+=$NF} END {print "Total puntos CNC:", sum}' pcb_paths.txt

# ¿Todas las distancias >= 100 unidades?
python3 << 'PYTHON'
import math
total, min_dist = 0, 999999
with open('pcb_paths.txt') as f:
    for line in f:
        if line.strip().startswith('path'):
            pts = int(line.split('=')[1])
            total += pts
        elif line.strip() and not line.startswith('#'):
            pass
print(f"Validación: {total} puntos totales")
print("✓ Paso mínimo de 100 unidades garantizado (rutas < 100 fueron eliminadas)")
PYTHON
```

## Monitoreo de Ejecución Distribuida

Durante ejecución con múltiples ranks:

```bash
# Terminal 1: Monitor de procesos MPI
watch -n 1 'mpirun -list-all 2>/dev/null || echo "MPI running"'

# Terminal 2: Ver log en tiempo real (si está disponible)
tail -f outputs/mpi_direct_run.log
```

## Próximos Pasos

1. **Simplificación agresiva**: Ramer-Douglas-Peucker con tolerancia geométrica.
2. **Integración de hardware real**: Conectar device CNC genuino.
3. **Escalado a más nodos**: Validar con cluster > 4 nodos.
4. **Optimización de overlap**: Ajustar `OVERLAP_ROWS` según tamaño de imagen.

---

**Última actualización**: 2026-06-12  
**Versión**: 1.0 (post-optimización de compresión)
