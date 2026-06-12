#ifndef TIPOS_H
#define TIPOS_H

/**
 * @brief Describe cómo se reparte una imagen entre los ranks MPI.
 *
 * La estructura contiene los tamaños globales, el solapamiento vertical y
 * los vectores necesarios para Scatterv/Gatherv. Todos los arreglos son
 * dinámicos y deben liberarse con liberar_distribucion().
 */
typedef struct
{
    /** Filas totales de la imagen. */
    int rows;

    /** Columnas totales de la imagen. */
    int cols;

    /** Filas de overlap vertical entre fragmentos adyacentes. */
    int overlap;

    /** Número de ranks MPI participantes. */
    int ranks;

    /** Filas reales procesadas por cada rank, incluyendo overlap. */
    int *local_rows;

    /** Filas útiles procesadas por cada rank, sin el overlap duplicado. */
    int *useful_rows;

    /** Cantidad de elementos enviados a cada rank para MPI_Scatterv. */
    int *sendcounts;

    /** Desplazamientos de envío para MPI_Scatterv, en elementos. */
    int *displs;

} DistribucionMPI;

#endif
