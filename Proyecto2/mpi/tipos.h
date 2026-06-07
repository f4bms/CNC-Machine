#ifndef TIPOS_H
#define TIPOS_H

#include <vector>

/*
 * Describe cómo se divide una imagen
 * entre los ranks MPI.
 */
typedef struct
{
    int rows;
    int cols;

    int overlap;

    /*
     * Filas reales procesadas
     * por cada rank.
     *
     * Incluyen overlap.
     */
    std::vector<int> local_rows;

    /*
     * Filas útiles
     * (sin overlap).
     */
    std::vector<int> useful_rows;

    /*
     * Scatterv
     */
    std::vector<int> sendcounts;
    std::vector<int> displs;

} DistribucionMPI;

#endif