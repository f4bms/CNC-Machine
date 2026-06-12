#ifndef ADMIN_TAREAS_H
#define ADMIN_TAREAS_H

/**
 * @brief Lanza el pipeline MPI sobre el archivo CNC recibido por el servidor.
 *
 * Construye el comando mpirun usando el hostfile configurado y valida que
 * el primer host del hostfile coincida con el nodo servidor.
 *
 * @param ruta_archivo Ruta al archivo CNC que se pasará al proceso MPI.
 * @return Código de salida del comando MPI, o -1 en caso de error de validación.
 */
int admin_tareas_ejecutar(const char *ruta_archivo);

#endif
