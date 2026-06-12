#include "admin_tareas.h"

#include <stdio.h>
#include <stdlib.h>

#define NUM_PROCESOS_MPI 4

int admin_tareas_ejecutar(const char *ruta_archivo)
{
    // Construye el comando mpirun y agrega parámetros CNC opcionales desde el entorno.
    if(ruta_archivo == NULL)
    {
        fprintf(stderr,
                "[ADMIN_TAREAS] ruta_archivo es NULL\n");

        return -1;
    }

    char comando[768];

    const char* cnc_device = getenv("CNC_DEVICE");
    const char* cnc_speed = getenv("CNC_SPEED");
    const char* cnc_scale = getenv("CNC_SCALE");

    int wrote = snprintf(
        comando,
        sizeof(comando),
        "mpirun -np %d ../mpi/mpi_processor %s --cnc-device /dev/gpio_device",
        NUM_PROCESOS_MPI,
        ruta_archivo
    );

    if(wrote < 0 || wrote >= (int)sizeof(comando))
    {
        fprintf(stderr,
                "[ADMIN_TAREAS] Error construyendo comando base MPI\n");

        return -1;
    }

    if(cnc_device != NULL && cnc_device[0] != '\0')
    {
        wrote += snprintf(
            comando + wrote,
            sizeof(comando) - (size_t)wrote,
            " --cnc-device %s",
            cnc_device
        );
    }

    if(cnc_speed != NULL && cnc_speed[0] != '\0')
    {
        wrote += snprintf(
            comando + wrote,
            sizeof(comando) - (size_t)wrote,
            " --cnc-speed %s",
            cnc_speed
        );
    }

    if(cnc_scale != NULL && cnc_scale[0] != '\0')
    {
        wrote += snprintf(
            comando + wrote,
            sizeof(comando) - (size_t)wrote,
            " --cnc-scale %s",
            cnc_scale
        );
    }

    if(wrote < 0 || wrote >= (int)sizeof(comando))
    {
        fprintf(stderr,
                "[ADMIN_TAREAS] Comando MPI excede buffer\n");

        return -1;
    }

    // Se imprime el comando final para que el flujo de servidor a MPI sea trazable.
    printf("\n");
    printf("[ADMIN_TAREAS] Ejecutando MPI\n");
    printf("[ADMIN_TAREAS] %s\n", comando);
    printf("\n");

    int resultado = system(comando);

    if(resultado != 0)
    {
        fprintf(stderr,
                "[ADMIN_TAREAS] Error ejecutando MPI\n");
    }

    return resultado;
}