#include "admin_tareas.h"

#include <stdio.h>
#include <stdlib.h>

#define NUM_PROCESOS_MPI 4

int admin_tareas_ejecutar(const char *ruta_archivo)
{
    if(ruta_archivo == NULL)
    {
        fprintf(stderr,
                "[ADMIN_TAREAS] ruta_archivo es NULL\n");

        return -1;
    }

    char comando[512];

    snprintf(
        comando,
        sizeof(comando),
        "mpirun -np %d ../mpi/mpi_processor %s",
        NUM_PROCESOS_MPI,
        ruta_archivo
    );

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