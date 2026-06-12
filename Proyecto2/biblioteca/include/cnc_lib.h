#ifndef CNC_LIB_H
#define CNC_LIB_H

#include <stddef.h>
#include <stdint.h>

#define CNC_DEVICE_PATH "/dev/gpio_device" // esto se usa como default cuando no se envia por comando pero tmb se está enviando en el admin de tareas
#define CNC_CMD_MAX_LEN 64 //longitud

// Códigos de retorno
#define CNC_OK 0
#define CNC_ERR_NOT_OPEN -1 /* El device no fue abierto antes de operar  */
#define CNC_ERR_WRITE -2    /* Falló write() al driver                   */
#define CNC_ERR_INVALID -3  /* Parámetros inválidos                      */
#define CNC_ERR_OPEN -4     /* No se pudo abrir /dev/gpio_device         */


/*
 * CNCHandle: contexto de la conexión con el driver.
 * El servidor obtiene un handle al llamar cnc_open() y lo pasa
 * a todas las funciones posteriores.
 */
typedef struct
{
    int fd;      // File descriptor del device
    int is_open; // Flag de estado
    int32_t x;   // Posición actual en X
    int32_t y;   // Posición actual en Y
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


/*
 * Abre el device driver y prepara el handle para su uso
 *
 * Parámetros:
 *   handle  - puntero al CNCHandle que se inicializará
 *   device  - ruta al device (pasar CNC_DEVICE_PATH o NULL para default)
 *
 * Retorna: CNC_OK si tuvo éxito, CNC_ERR_OPEN si falla al abrir el device.
 */
int cnc_open(CNCHandle *handle, const char *device);

/* Cierra el file descriptor y marca el handle como inactivo.
 * Siempre llamar al final para liberar recursos.
 *
 * Retorna: CNC_OK.
 */
int cnc_close(CNCHandle *handle);


/* Mueve el cabezal a una posición absoluta (x, y).
 * Genera el comando "G x y" y lo escribe en el driver.
 *
 * Retorna: CNC_OK, CNC_ERR_NOT_OPEN o CNC_ERR_WRITE.
 */
int cnc_move_to(CNCHandle *handle, int32_t x, int32_t y);


/* cnc_home()
 * Envía el cabezal a la posición de origen (0, 0).
 * Genera el comando "G 0 0" para el driver.
 * Útil para inicializar o resetear la posición antes de una tarea.
 *
 * Retorna: CNC_OK, CNC_ERR_NOT_OPEN o CNC_ERR_WRITE.
 */
int cnc_home(CNCHandle *handle);


/* Baja el marcador para iniciar el trazado sobre el PCB.
 * Genera el comando "P 0 0" al driver.
 *
 * Retorna: CNC_OK, CNC_ERR_NOT_OPEN o CNC_ERR_WRITE.
 */
int cnc_pen_down(CNCHandle *handle);

/*
 * Levanta el marcador para moverse sin trazar.
 * Genera el comando "U 0 0" al driver.
 *
 * Retorna: CNC_OK, CNC_ERR_NOT_OPEN o CNC_ERR_WRITE.
 */
int cnc_pen_up(CNCHandle *handle);


/* Recibe una ruta vectorizada (CNCPath) y la dibuja completa:
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

/* Ajusta la velocidad de la CNC enviando un comando raw compatible.
 * El driver puede usar este comando para configurar su temporización.
 *
 * Retorna: CNC_OK, CNC_ERR_NOT_OPEN, CNC_ERR_INVALID o CNC_ERR_WRITE.
 */
int cnc_set_speed(CNCHandle *handle, int32_t speed);

/* Escribe un comando arbitrario directamente al driver.
 * Usada internamente por todas las funciones anteriores.
 * El servidor puede llamarla para comandos no cubiertos por la API.
 *
 * Parámetros:
 *   cmd - cadena de texto con el comando (máx CNC_CMD_MAX_LEN bytes)
 *
 * Retorna: CNC_OK, CNC_ERR_NOT_OPEN, CNC_ERR_INVALID o CNC_ERR_WRITE.
 */
int cnc_write(CNCHandle *handle, const char *cmd);

/* Lee datos desde el driver (para futuras extensiones del driver que
 * implementen dev_read). Actualmente reservada para compatibilidad.
 *
 * Parámetros:
 *   buf  - buffer donde se almacena la lectura
 *   len  - tamaño máximo a leer
 *
 * Retorna: bytes leídos (>= 0) o código de error negativo.
 */
int cnc_read(CNCHandle *handle, char *buf, size_t len);

/* Convierte un código de error CNC a cadena legible.
 * Útil para logging en el servidor.
 *
 * Retorna: puntero a string estático con la descripción del error.
 */
const char *cnc_strerror(int error_code);

#endif /* CNC_LIB_H */
