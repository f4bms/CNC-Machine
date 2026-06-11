#ifndef CNC_LIB_H
#define CNC_LIB_H

/*
 * cnc_lib.h
 * Biblioteca de control para máquina CNC distribuida - Trazado de PCB
 *
 * Esta biblioteca es la ÚNICA capa que interactúa con el device driver GPIO.
 * El servidor/cluster debe llamar exclusivamente a estas funciones para
 * enviar instrucciones de movimiento al hardware.
 *
 * Interfaz con el driver: escritura en /dev/gpio_device mediante write()
 */

#include <stddef.h>
#include <stdint.h>

/* =========================================================================
 * Constantes de configuración
 * ========================================================================= */

#define CNC_DEVICE_PATH "/dev/gpio_device" /* Ruta al char device del driver */
#define CNC_CMD_MAX_LEN 64                 /* Longitud máxima de un comando   */


/* =========================================================================
 * Códigos de retorno
 * ========================================================================= */

#define CNC_OK 0
#define CNC_ERR_NOT_OPEN -1 /* El device no fue abierto antes de operar  */
#define CNC_ERR_WRITE -2    /* Falló write() al driver                   */
#define CNC_ERR_INVALID -3  /* Parámetros inválidos                      */
#define CNC_ERR_OPEN -4     /* No se pudo abrir /dev/gpio_device         */

/* =========================================================================
 * Tipos y estructuras
 * ========================================================================= */

/*
 * CNCHandle: contexto de la conexión con el driver.
 * El servidor obtiene un handle al llamar cnc_open() y lo pasa
 * a todas las funciones posteriores.
 */
typedef struct
{
    int fd;      /* File descriptor del device  */
    int is_open; /* Flag de estado               */
} CNCHandle;

/*
 * CNCPoint: coordenada 2D en el plano de trabajo del PCB.
 * Las unidades son pasos del motor (steps); la conversión
 * mm→steps depende del hardware y queda a cargo del servidor.
 */
typedef struct
{
    int32_t x;
    int32_t y;
} CNCPoint;

/*
 * CNCPath: secuencia de puntos que forman una traza del PCB.
 * El servidor construye este arreglo a partir de la vectorización
 * y lo entrega a cnc_draw_path().
 */
typedef struct
{
    CNCPoint *points; /* Arreglo de puntos            */
    size_t count;     /* Número de puntos en el arreglo */
} CNCPath;

/* =========================================================================
 * API pública — Ciclo de vida
 * ========================================================================= */

/*
 * cnc_open()
 * Abre el device driver y prepara el handle para su uso.
 *
 * Parámetros:
 *   handle  - puntero al CNCHandle que se inicializará
 *   device  - ruta al device (pasar CNC_DEVICE_PATH o NULL para default)
 *
 * Retorna: CNC_OK si tuvo éxito, CNC_ERR_OPEN si falla al abrir el device.
 */
int cnc_open(CNCHandle *handle, const char *device);

/*
 * cnc_close()
 * Cierra el file descriptor y marca el handle como inactivo.
 * Siempre llamar al final para liberar recursos.
 *
 * Retorna: CNC_OK.
 */
int cnc_close(CNCHandle *handle);

/* =========================================================================
 * API pública — Movimiento
 * ========================================================================= */

/*
 * cnc_move_to()
 * Mueve el cabezal a una posición absoluta (x, y).
 * Genera el comando "MOVE x y" y lo escribe en el driver.
 *
 * Retorna: CNC_OK, CNC_ERR_NOT_OPEN o CNC_ERR_WRITE.
 */
int cnc_move_to(CNCHandle *handle, int32_t x, int32_t y);

/*
 * cnc_move_right()
 * Desplaza el cabezal hacia la derecha un número de pasos.
 * Equivalente a incrementar X en `steps` unidades.
 *
 * Retorna: CNC_OK, CNC_ERR_NOT_OPEN o CNC_ERR_WRITE.
 */
int cnc_move_right(CNCHandle *handle, int32_t steps);

/*
 * cnc_move_left()
 * Desplaza el cabezal hacia la izquierda `steps` pasos.
 *
 * Retorna: CNC_OK, CNC_ERR_NOT_OPEN o CNC_ERR_WRITE.
 */
int cnc_move_left(CNCHandle *handle, int32_t steps);

/*
 * cnc_move_up()
 * Desplaza el cabezal hacia arriba `steps` pasos (incrementa Y).
 *
 * Retorna: CNC_OK, CNC_ERR_NOT_OPEN o CNC_ERR_WRITE.
 */
int cnc_move_up(CNCHandle *handle, int32_t steps);

/*
 * cnc_move_down()
 * Desplaza el cabezal hacia abajo `steps` pasos (decrementa Y).
 *
 * Retorna: CNC_OK, CNC_ERR_NOT_OPEN o CNC_ERR_WRITE.
 */
int cnc_move_down(CNCHandle *handle, int32_t steps);

/*
 * cnc_home()
 * Envía el cabezal a la posición de origen (0, 0).
 * Útil para inicializar o resetear la posición antes de una tarea.
 *
 * Retorna: CNC_OK, CNC_ERR_NOT_OPEN o CNC_ERR_WRITE.
 */
int cnc_home(CNCHandle *handle);

/* =========================================================================
 * API pública — Control del marcador/pluma
 * ========================================================================= */

/*
 * cnc_pen_down()
 * Baja el marcador para iniciar el trazado sobre el PCB.
 * Genera el comando "PEN DOWN" al driver.
 *
 * Retorna: CNC_OK, CNC_ERR_NOT_OPEN o CNC_ERR_WRITE.
 */
int cnc_pen_down(CNCHandle *handle);

/*
 * cnc_pen_up()
 * Levanta el marcador para moverse sin trazar.
 * Genera el comando "PEN UP" al driver.
 *
 * Retorna: CNC_OK, CNC_ERR_NOT_OPEN o CNC_ERR_WRITE.
 */
int cnc_pen_up(CNCHandle *handle);

/* =========================================================================
 * API pública — Trazado de rutas completas
 * ========================================================================= */

/*
 * cnc_draw_path()
 * Recibe una ruta vectorizada (CNCPath) y la dibuja completa:
 *   1. Levanta la pluma y se mueve al primer punto.
 *   2. Baja la pluma.
 *   3. Recorre todos los puntos en orden.
 *   4. Levanta la pluma al terminar.
 *
 * Esta es la función principal que el servidor/cluster invoca
 * después de que los nodos generan las trayectorias.
 *
 * Retorna: CNC_OK, CNC_ERR_NOT_OPEN, CNC_ERR_INVALID o CNC_ERR_WRITE.
 */
int cnc_draw_path(CNCHandle *handle, const CNCPath *path);

/* =========================================================================
 * API pública — I/O raw (lectura/escritura directa al driver)
 * ========================================================================= */

/*
 * cnc_write()
 * Escribe un comando arbitrario directamente al driver.
 * Usada internamente por todas las funciones anteriores.
 * El servidor puede llamarla para comandos no cubiertos por la API.
 *
 * Parámetros:
 *   cmd - cadena de texto con el comando (máx CNC_CMD_MAX_LEN bytes)
 *
 * Retorna: CNC_OK, CNC_ERR_NOT_OPEN, CNC_ERR_INVALID o CNC_ERR_WRITE.
 */
int cnc_write(CNCHandle *handle, const char *cmd);

/*
 * cnc_read()
 * Lee datos desde el driver (para futuras extensiones del driver que
 * implementen dev_read). Actualmente reservada para compatibilidad.
 *
 * Parámetros:
 *   buf  - buffer donde se almacena la lectura
 *   len  - tamaño máximo a leer
 *
 * Retorna: bytes leídos (>= 0) o código de error negativo.
 */
int cnc_read(CNCHandle *handle, char *buf, size_t len);

/* =========================================================================
 * API pública — Utilidades
 * ========================================================================= */

/*
 * cnc_strerror()
 * Convierte un código de error CNC a cadena legible.
 * Útil para logging en el servidor.
 *
 * Retorna: puntero a string estático con la descripción del error.
 */
const char *cnc_strerror(int error_code);

#endif /* CNC_LIB_H */
