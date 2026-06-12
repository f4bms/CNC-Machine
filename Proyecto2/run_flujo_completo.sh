#!/usr/bin/env bash
#
# run_flujo_completo.sh
# Automatiza el flujo completo del Proyecto2:
#   compilar -> levantar servidor -> enviar imagen cifrada (cliente)
#   -> procesamiento distribuido MPI -> envio de instrucciones por cnc_lib.
#
# Las salidas se generan dentro de la carpeta outputs/.
#
# Uso:
#   ./run_flujo_completo.sh [opciones]
#
# Opciones:
#   --image <ruta>        Imagen de entrada (default: img/pcb1.png)
#   --server-ip <ip>      IP a la que se conecta el cliente (default: 127.0.0.1)
#   --np <n>              Numero de procesos MPI (default: 4)
#   --hostfile <ruta>     Hostfile MPI (default: mpi/hosts.txt)
#   --server-node <ip>    IP del nodo servidor = rank 0 (default: --server-ip)
#   --cnc-device <ruta>   Device CNC (default: /dev/null para simular)
#   --cnc-scale <n>       Escala CNC (default: 1)
#   --output-dir <ruta>   Carpeta de salidas (default: outputs)
#   --help                Muestra esta ayuda.

set -euo pipefail

# Raiz del proyecto = carpeta donde vive este script.
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Valores por defecto.
IMAGE_PATH="$ROOT_DIR/img/pcb1.png"
SERVER_IP="127.0.0.1"
MPI_NP="4"
HOSTFILE_PATH="$ROOT_DIR/mpi/hosts.txt"
SERVER_NODE_IP=""
CNC_DEVICE="/dev/null"
CNC_SCALE="1"
OUTPUT_DIR="$ROOT_DIR/outputs"
SERVER_LOG="$ROOT_DIR/servidor/servidor_run.log"
SERVER_PORT="8080"

usage()
{
    sed -n '2,30p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
}

# Parseo de argumentos.
while [[ $# -gt 0 ]]; do
    case "$1" in
        --image)       IMAGE_PATH="$2"; shift 2 ;;
        --server-ip)   SERVER_IP="$2"; shift 2 ;;
        --np)          MPI_NP="$2"; shift 2 ;;
        --hostfile)    HOSTFILE_PATH="$2"; shift 2 ;;
        --server-node) SERVER_NODE_IP="$2"; shift 2 ;;
        --cnc-device)  CNC_DEVICE="$2"; shift 2 ;;
        --cnc-scale)   CNC_SCALE="$2"; shift 2 ;;
        --output-dir)  OUTPUT_DIR="$2"; shift 2 ;;
        --help|-h)     usage; exit 0 ;;
        *) echo "Opcion desconocida: $1" >&2; usage; exit 1 ;;
    esac
done

# Por defecto el nodo servidor (rank 0) es la misma IP del servidor.
if [[ -z "$SERVER_NODE_IP" ]]; then
    SERVER_NODE_IP="$SERVER_IP"
fi

# Normaliza rutas a absolutas (cliente/servidor/mpi corren en subdirectorios).
abspath()
{
    local p="$1"
    if [[ -e "$p" ]]; then
        printf '%s\n' "$(cd "$(dirname "$p")" && pwd)/$(basename "$p")"
    else
        case "$p" in
            /*) printf '%s\n' "$p" ;;
            *)  printf '%s\n' "$ROOT_DIR/$p" ;;
        esac
    fi
}

IMAGE_PATH="$(abspath "$IMAGE_PATH")"
HOSTFILE_PATH="$(abspath "$HOSTFILE_PATH")"
OUTPUT_DIR="$(abspath "$OUTPUT_DIR")"

# Validaciones de entrada.
if [[ ! -f "$IMAGE_PATH" ]]; then
    echo "ERROR: no existe la imagen $IMAGE_PATH" >&2
    exit 1
fi

if [[ ! -f "$HOSTFILE_PATH" ]]; then
    echo "ERROR: no existe el hostfile $HOSTFILE_PATH" >&2
    exit 1
fi

echo "==================================================================="
echo " Flujo completo CNC-Machine"
echo "  Imagen        : $IMAGE_PATH"
echo "  Server IP     : $SERVER_IP"
echo "  Server node   : $SERVER_NODE_IP (rank 0)"
echo "  MPI procesos  : $MPI_NP"
echo "  Hostfile      : $HOSTFILE_PATH"
echo "  CNC device    : $CNC_DEVICE"
echo "  CNC scale     : $CNC_SCALE"
echo "  Output dir    : $OUTPUT_DIR"
echo "==================================================================="

# 1) Compilacion de todos los modulos.
echo "[1/6] Compilando biblioteca, cliente, servidor y MPI..."
make -C "$ROOT_DIR/biblioteca" all
make -C "$ROOT_DIR/cliente" all
make -C "$ROOT_DIR/servidor" all
make -C "$ROOT_DIR/mpi" all

# 2) Preparacion de la carpeta de salidas.
echo "[2/6] Preparando carpeta de salidas..."
mkdir -p "$OUTPUT_DIR"
rm -f "$OUTPUT_DIR"/rank_*_binary.png \
      "$OUTPUT_DIR"/rank_*_skeleton.png \
      "$OUTPUT_DIR"/pcb_skeleton.png \
      "$OUTPUT_DIR"/pcb_graph.png \
      "$OUTPUT_DIR"/pcb_edges.txt \
      "$OUTPUT_DIR"/pcb_paths.txt
rm -f "$SERVER_LOG"

# 3) Liberar el puerto del servidor si quedo ocupado.
if command -v fuser >/dev/null 2>&1; then
    if fuser -s -n tcp "$SERVER_PORT" 2>/dev/null; then
        echo "[3/6] Liberando puerto $SERVER_PORT ocupado..."
        fuser -k -n tcp "$SERVER_PORT" 2>/dev/null || true
        sleep 1
    fi
fi

# 4) Levantar el servidor en segundo plano con las variables de entorno.
echo "[4/6] Iniciando servidor..."

SERVER_PID=""

cleanup()
{
    if [[ -n "$SERVER_PID" ]] && kill -0 "$SERVER_PID" 2>/dev/null; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT

(
    cd "$ROOT_DIR/servidor"
    exec env \
        MPI_HOSTFILE="$HOSTFILE_PATH" \
        MPI_NUM_PROCESOS="$MPI_NP" \
        SERVER_NODE_IP="$SERVER_NODE_IP" \
        CNC_DEVICE="$CNC_DEVICE" \
        CNC_SCALE="$CNC_SCALE" \
        OUTPUT_DIR="$OUTPUT_DIR" \
        stdbuf -oL -eL ./servidor
) > "$SERVER_LOG" 2>&1 &

SERVER_PID=$!

# Espera a que el servidor este escuchando.
listo=0
for _ in $(seq 1 30); do
    if grep -q "Servidor escuchando" "$SERVER_LOG" 2>/dev/null; then
        listo=1
        break
    fi
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
        echo "ERROR: el servidor termino antes de tiempo. Log:" >&2
        cat "$SERVER_LOG" >&2
        exit 1
    fi
    sleep 1
done

if [[ "$listo" -eq 0 ]]; then
    if kill -0 "$SERVER_PID" 2>/dev/null; then
        echo "[4/6] Servidor activo (banner no detectado, se continua)."
    else
        echo "ERROR: el servidor no inicio correctamente. Log:" >&2
        cat "$SERVER_LOG" >&2
        exit 1
    fi
fi

# 5) Enviar la imagen con el cliente (cifra + transmite).
echo "[5/6] Enviando imagen con el cliente..."
(
    cd "$ROOT_DIR/cliente"
    ./cliente "$SERVER_IP" "$IMAGE_PATH"
)

# Esperar a que el servidor procese y termine.
echo "[6/6] Esperando a que el servidor termine el procesamiento..."
wait "$SERVER_PID" 2>/dev/null || true
SERVER_PID=""

echo "==================================================================="
echo " Resumen del servidor (log: $SERVER_LOG)"
echo "-------------------------------------------------------------------"
grep -E "Imagen cargada|Nodos detectados|Aristas detectadas|CNC paths generados|Trazado CNC|Ejecucion de hardware" \
     "$SERVER_LOG" 2>/dev/null || true
echo "==================================================================="

# Verificacion de salidas.
faltan=0
for f in pcb_skeleton.png pcb_graph.png pcb_edges.txt pcb_paths.txt; do
    if [[ -f "$OUTPUT_DIR/$f" ]]; then
        echo "  OK  $OUTPUT_DIR/$f"
    else
        echo "  FALTA  $OUTPUT_DIR/$f"
        faltan=1
    fi
done

if [[ "$faltan" -ne 0 ]]; then
    echo "ERROR: faltan archivos de salida. Revise $SERVER_LOG" >&2
    exit 1
fi

echo "Flujo completo finalizado. Salidas en: $OUTPUT_DIR"
echo "Log del servidor: $SERVER_LOG"
