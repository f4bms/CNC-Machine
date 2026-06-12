#include "admin_tareas.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Valores por defecto para la configuración del cluster.
// Estas variables pueden ser sobreescritas por variables de entorno.
#define DEFAULT_MPI_HOSTFILE "../mpi/hosts.txt"
#define DEFAULT_SERVER_NODE "127.0.0.1"
#define NUM_PROCESOS_MPI 4

/**
 * @brief Convierte una cadena a un entero positivo.
 *
 * Si la cadena es NULL, vacía o no representa un entero positivo,
 * retorna el valor de fallback especificado.
 *
 * @param s Cadena que contiene el entero a convertir.
 * @param fallback Valor a retornar cuando la cadena no es válida.
 * @return El entero positivo convertido o el valor de fallback.
 */
static int parse_positive_int(const char *s, int fallback)
{
    if (s == NULL || s[0] == '\0')
        return fallback;

    int v = atoi(s);

    if (v <= 0)
        return fallback;

    return v;
}

/**
 * @brief Lee el primer host válido del archivo de hosts MPI.
 *
 * Escanea el archivo línea por línea, ignora líneas vacías y comentarios
 * que comienzan con '#', y extrae el primer token no vacío.
 *
 * @param hostfile Ruta al archivo que contiene la lista de hosts.
 * @param out Buffer donde se almacenará el primer host leído.
 * @param out_size Tamaño del buffer de salida.
 * @return 0 si se leyó correctamente el primer host, -1 en caso de error.
 */
static int read_first_host_from_hostfile(const char *hostfile, char *out, size_t out_size)
{
    FILE *fp = fopen(hostfile, "r");

    if (fp == NULL)
        return -1;

    char linea[256];

    while (fgets(linea, sizeof(linea), fp) != NULL)
    {
        char *p = linea;

        // Saltar espacios iniciales
        while (*p != '\0' && isspace((unsigned char)*p))
            p++;

        // Ignorar líneas vacías y comentarios
        if (*p == '\0' || *p == '#')
            continue;

        // Toma el primer token hasta el siguiente espacio
        char *fin = p;

        while (*fin != '\0' && !isspace((unsigned char)*fin))
            fin++;

        size_t len = (size_t)(fin - p);

        if (len == 0 || len >= out_size)
        {
            fclose(fp);
            return -1;
        }

        memcpy(out, p, len);
        out[len] = '\0';

        fclose(fp);
        return 0;
    }

    fclose(fp);
    return -1;
}

int admin_tareas_ejecutar(const char *ruta_archivo)
{
    if (ruta_archivo == NULL)
    {
        fprintf(stderr,
                "[ADMIN_TAREAS] ruta_archivo es NULL\n");

        return -1;
    }

    // Parámetros de cluster que pueden ser configurados mediante variables de entorno.
    const char *hostfile = getenv("MPI_HOSTFILE");

    if (hostfile == NULL || hostfile[0] == '\0')
        hostfile = DEFAULT_MPI_HOSTFILE;

    const char *server_node = getenv("SERVER_NODE_IP");

    if (server_node == NULL || server_node[0] == '\0')
        server_node = DEFAULT_SERVER_NODE;

    int num_procesos =
        parse_positive_int(getenv("MPI_NUM_PROCESOS"), NUM_PROCESOS_MPI);

    // El servidor debe ser rank 0: la primera entrada del hostfile
    // debe coincidir con el nodo servidor configurado.
    char primer_host[256];

    if (read_first_host_from_hostfile(hostfile, primer_host, sizeof(primer_host)) != 0)
    {
        fprintf(stderr,
                "[ADMIN_TAREAS] No se pudo leer el hostfile %s\n",
                hostfile);

        return -1;
    }

    if (strcmp(primer_host, server_node) != 0)
    {
        fprintf(stderr,
                "[ADMIN_TAREAS] El primer host del hostfile (%s) no coincide "
                "con el nodo servidor (%s).\n"
                "[ADMIN_TAREAS] El servidor debe ser el rank 0; corrija %s.\n",
                primer_host,
                server_node,
                hostfile);

        return -1;
    }

    // Parámetros opcionales para el dispositivo CNC.
    const char *cnc_device = getenv("CNC_DEVICE");
    const char *cnc_scale = getenv("CNC_SCALE");

    char comando[1024];

    // Construir el comando MPI base, respetando el orden del hostfile
    // para que rank 0 sea el servidor.
    int wrote = snprintf(
        comando,
        sizeof(comando),
        "mpirun -np %d --hostfile %s --map-by slot ../mpi/mpi_processor %s --cnc-device /dev/gpio_device",
        num_procesos,
        hostfile,
        ruta_archivo);

    if (wrote < 0 || wrote >= (int)sizeof(comando))
    {
        fprintf(stderr,
                "[ADMIN_TAREAS] Error construyendo comando base MPI\n");

        return -1;
    }

    // Agregar argumentos opcionales según variables de entorno.
    if (cnc_device != NULL && cnc_device[0] != '\0')
    {
        wrote += snprintf(
            comando + wrote,
            sizeof(comando) - (size_t)wrote,
            " --cnc-device %s",
            cnc_device);
    }

    if (cnc_scale != NULL && cnc_scale[0] != '\0')
    {
        wrote += snprintf(
            comando + wrote,
            sizeof(comando) - (size_t)wrote,
            " --cnc-scale %s",
            cnc_scale);
    }

    if (wrote < 0 || wrote >= (int)sizeof(comando))
    {
        fprintf(stderr,
                "[ADMIN_TAREAS] Comando MPI excede buffer\n");

        return -1;
    }

    // Mostrar el comando completo para trazabilidad.
    printf("\n");
    printf("[ADMIN_TAREAS] Ejecutando MPI\n");
    printf("[ADMIN_TAREAS] Hostfile : %s (rank 0 = %s)\n", hostfile, server_node);
    printf("[ADMIN_TAREAS] %s\n", comando);
    printf("\n");

    int resultado = system(comando);

    if (resultado != 0)
    {
        fprintf(stderr,
                "[ADMIN_TAREAS] Error ejecutando MPI\n");
    }

    return resultado;
}
