#ifndef TIPOS_H
#define TIPOS_H

// Describe como se divide una imagen entre los ranks MPI.
// Todos los arreglos son dinamicos (longitud == ranks) y se liberan
// con liberar_distribucion().
typedef struct
{
    int rows;
    int cols;

    int overlap;

    int ranks;

    // Filas reales procesadas por cada rank, incluyendo overlap.
    int* local_rows;

    // Filas utiles sin overlap.
    int* useful_rows;

    // Parametros para MPI_Scatterv.
    int* sendcounts;
    int* displs;

} DistribucionMPI;

#endif
