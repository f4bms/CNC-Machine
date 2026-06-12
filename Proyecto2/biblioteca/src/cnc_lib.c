#include "cnc_lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

/* Escribe el string `cmd` al file descriptor del driver.
 * Retorna: CNC_OK si write() tuvo éxito, CNC_ERR_WRITE en caso contrario.
 */
static int _cnc_send_cmd(CNCHandle *handle, const char *cmd)
{
    ssize_t written;
    size_t len;

    len = strlen(cmd);
    written = write(handle->fd, cmd, len);

    if (written < 0)
    {
        fprintf(stderr, "[cnc_lib] write() falló: %s (cmd='%s')\n",
                strerror(errno), cmd);
        return CNC_ERR_WRITE;
    }

    fprintf(stdout, "[cnc_lib] CMD enviado: %s", cmd);

    return CNC_OK;
}

int cnc_open(CNCHandle *handle, const char *device)
{
    const char *path;

    if (handle == NULL)
    {
        return CNC_ERR_INVALID;
    }

    /* Si no se pasa ruta, usamos el default definido en el header */
    path = (device != NULL) ? device : CNC_DEVICE_PATH;

    handle->fd = open(path, O_RDWR);
    handle->is_open = 0;
    handle->x = 0;
    handle->y = 0;

    if (handle->fd < 0)
    {
        fprintf(stderr, "[cnc_lib] No se pudo abrir el device '%s': %s\n",
                path, strerror(errno));
        return CNC_ERR_OPEN;
    }

    handle->is_open = 1;
    fprintf(stdout, "[cnc_lib] Device '%s' abierto correctamente (fd=%d)\n",
            path, handle->fd);

    return CNC_OK;
}

int cnc_close(CNCHandle *handle)
{
    if (handle == NULL)
    {
        return CNC_ERR_INVALID;
    }

    if (handle->is_open && handle->fd >= 0)
    {
        close(handle->fd);
        handle->fd = -1;
        handle->is_open = 0;
        fprintf(stdout, "[cnc_lib] Device cerrado.\n");
    }

    return CNC_OK;
}

//funciones de movimiento

int cnc_move_to(CNCHandle *handle, int32_t x, int32_t y)
{
    char cmd[CNC_CMD_MAX_LEN];

    if (handle == NULL || !handle->is_open)
    {
        return CNC_ERR_NOT_OPEN;
    }

    snprintf(cmd, sizeof(cmd), "G %d %d\n", x, y);
    handle->x = x;
    handle->y = y;
    return _cnc_send_cmd(handle, cmd);
}

int cnc_home(CNCHandle *handle)
{
    if (handle == NULL || !handle->is_open)
    {
        return CNC_ERR_NOT_OPEN;
    }

    return cnc_move_to(handle, 0, 0);
}


int cnc_pen_down(CNCHandle *handle)
{
    if (handle == NULL || !handle->is_open)
    {
        return CNC_ERR_NOT_OPEN;
    }

    return _cnc_send_cmd(handle, "P 0 0\n");
}

int cnc_pen_up(CNCHandle *handle)
{
    if (handle == NULL || !handle->is_open)
    {
        return CNC_ERR_NOT_OPEN;
    }

    return _cnc_send_cmd(handle, "U 0 0\n");
}

//esto es lo que realmente se hace para un path completo
int cnc_draw_path(CNCHandle *handle, const CNCPath *path)
{
    size_t i;
    int ret;

    if (handle == NULL || !handle->is_open)
    {
        return CNC_ERR_NOT_OPEN;
    }

    if (path == NULL || path->points == NULL || path->count == 0)
    {
        fprintf(stderr, "[cnc_lib] cnc_draw_path: ruta inválida o vacía\n");
        return CNC_ERR_INVALID;
    }

    fprintf(stdout, "[cnc_lib] Iniciando trazado: %zu puntos\n", path->count);

    /* 1. Moverse al primer punto sin trazar */
    ret = cnc_move_to(handle, path->points[0].x, path->points[0].y);
    if (ret != CNC_OK)
        return ret;

    /* 2. Bajar la pluma para iniciar el trazado */
    ret = cnc_pen_down(handle);
    if (ret != CNC_OK)
        return ret;

    /* 3. Recorrer el resto de los puntos trazando */
    for (i = 1; i < path->count; i++)
    {
        ret = cnc_move_to(handle, path->points[i].x, path->points[i].y);
        if (ret != CNC_OK)
        {
            /* Si falla en medio del trazado, intentamos levantar la pluma
             * antes de retornar el error para no dañar el PCB */
            cnc_pen_up(handle);
            fprintf(stderr, "[cnc_lib] Error en punto %zu del trazado\n", i);
            return ret;
        }
    }

    /* 4. Levantar la pluma al finalizar el trazado */
    ret = cnc_pen_up(handle);
    if (ret != CNC_OK)
        return ret;

    fprintf(stdout, "[cnc_lib] Trazado completado (%zu puntos)\n", path->count);
    return CNC_OK;
}

//para i/o -> solo se usa el write pq el read no conecta con userspace
int cnc_write(CNCHandle *handle, const char *cmd)
{
    if (handle == NULL || !handle->is_open)
    {
        return CNC_ERR_NOT_OPEN;
    }

    if (cmd == NULL || strlen(cmd) == 0)
    {
        return CNC_ERR_INVALID;
    }

    if (strlen(cmd) > CNC_CMD_MAX_LEN)
    {
        fprintf(stderr, "[cnc_lib] cnc_write: comando demasiado largo (max %d)\n",
                CNC_CMD_MAX_LEN);
        return CNC_ERR_INVALID;
    }

    return _cnc_send_cmd(handle, cmd);
}


//ese cnc read como dice el comentario de más abajo no se usa pq el driver hasta el momento no ocupa recibir nada del 
int cnc_read(CNCHandle *handle, char *buf, size_t len)
{
    ssize_t n;

    if (handle == NULL || !handle->is_open)
    {
        return CNC_ERR_NOT_OPEN;
    }

    if (buf == NULL || len == 0)
    {
        return CNC_ERR_INVALID;
    }

    /*
     * Por ahora el driver solo tiene dev_write implementado.
     * Esta función queda lista para cuando se agregue dev_read al driver.
     * Reabrimos el fd con O_RDONLY para la lectura puntual.
     */
    n = read(handle->fd, buf, len - 1);
    if (n < 0)
    {
        fprintf(stderr, "[cnc_lib] read() falló: %s\n", strerror(errno));
        return CNC_ERR_WRITE; /* Reutilizamos el código de error de I/O */
    }

    buf[n] = '\0';
    return (int)n;
}


    // Utilidades
const char *cnc_strerror(int error_code)
{
    switch (error_code)
    {
    case CNC_OK:
        return "OK - Sin error";
    case CNC_ERR_NOT_OPEN:
        return "Device no abierto (llamar cnc_open primero)";
    case CNC_ERR_WRITE:
        return "Error al escribir al driver";
    case CNC_ERR_INVALID:
        return "Parámetros inválidos";
    case CNC_ERR_OPEN:
        return "No se pudo abrir el device driver";
    default:
        return "Error desconocido";
    }
}
