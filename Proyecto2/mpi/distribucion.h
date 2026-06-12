#ifndef DISTRIBUCION_H
#define DISTRIBUCION_H

#include "tipos.h"

// Divide la imagen entre ranks y coordina scatter/gather con overlap.
// Implementacion en C puro sobre buffers unsigned char (sin OpenCV).

/**
 * @brief Calcula el reparto de filas con overlap vertical para MPI.
 *
 * Crea la estructura de distribución que describe filas útiles,
 * filas locales, sendcounts y displacements para Scatterv/Gatherv.
 *
 * @param rows Altura total de la imagen en filas.
 * @param cols Ancho total de la imagen en columnas.
 * @param ranks Número de ranks MPI.
 * @param overlap Cantidad de filas de solapamiento entre particiones.
 * @param out_dist Puntero a la estructura de distribución a llenar.
 * @return 0 en caso de éxito, -1 en error.
 */
int crear_distribucion(
    int rows,
    int cols,
    int ranks,
    int overlap,
    DistribucionMPI* out_dist
);

/**
 * @brief Libera la memoria asociada a una distribución MPI.
 *
 * Libera los arreglos dinámicos de local_rows, useful_rows,
 * sendcounts y displs.
 *
 * @param dist Distribución MPI a liberar.
 */
void liberar_distribucion(
    DistribucionMPI* dist
);

/**
 * @brief Imprime en consola la distribución de filas entre ranks.
 *
 * Muestra filas útiles y reales asignadas a cada rank para depuración.
 *
 * @param dist Distribución MPI a imprimir.
 */
void imprimir_distribucion(
    const DistribucionMPI* dist
);

/**
 * @brief Difunde las dimensiones de la imagen desde el rank 0 a todos.
 *
 * Asegura que todos los ranks trabajen con las mismas filas y columnas.
 *
 * @param rows Puntero a las filas compartidas.
 * @param cols Puntero a las columnas compartidas.
 */
void broadcast_dimensiones(
    int* rows,
    int* cols
);

/**
 * @brief Reparte el fragmento de imagen correspondiente a cada rank.
 *
 * Primero envía el número de filas locales, reserva el buffer local y luego
 * reparte los pixeles usando MPI_Scatterv.
 *
 * @param image Buffer de imagen completo en rank 0.
 * @param dist Distribución MPI preparada.
 * @param rank Rank actual del proceso.
 * @param local_rows Salida: filas locales asignadas a este rank.
 * @param local_buffer Salida: buffer local asignado con malloc.
 * @return 0 en caso de éxito, -1 en error.
 */
int scatter_fragmento(
    const unsigned char* image,
    const DistribucionMPI* dist,
    int rank,
    int* local_rows,
    unsigned char** local_buffer
);

/**
 * @brief Reensambla el resultado procesado en el rank 0.
 *
 * Cada rank envía únicamente su región útil (sin overlap duplicado).
 * El rank 0 asigna final_image y lo rellena con MPI_Gatherv.
 *
 * @param local_processed Buffer procesado local de este rank.
 * @param dist Distribución MPI usada.
 * @param rank Rank actual del proceso.
 * @param final_image Salida: puntero asignado en rank 0 con la imagen final.
 * @return 0 en caso de éxito, -1 en error.
 */
int gather_fragmento(
    const unsigned char* local_processed,
    const DistribucionMPI* dist,
    int rank,
    unsigned char** final_image
);

#endif
