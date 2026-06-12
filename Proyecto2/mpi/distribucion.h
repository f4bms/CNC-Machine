#ifndef DISTRIBUCION_H
#define DISTRIBUCION_H

#include "tipos.h"

// Divide la imagen entre ranks y coordina scatter/gather con overlap.
// Implementacion en C puro sobre buffers unsigned char (sin OpenCV).

// Calcula el reparto de filas con overlap vertical.
// Devuelve 0 en exito; la distribucion se libera con liberar_distribucion().
int crear_distribucion(
    int rows,
    int cols,
    int ranks,
    int overlap,
    DistribucionMPI* out_dist
);

void liberar_distribucion(
    DistribucionMPI* dist
);

void imprimir_distribucion(
    const DistribucionMPI* dist
);

// Difunde dimensiones (rows/cols) desde el rank 0 a todos.
void broadcast_dimensiones(
    int* rows,
    int* cols
);

// Reparte el fragmento de imagen correspondiente a cada rank.
// El buffer local se asigna con malloc y debe liberarse con free().
int scatter_fragmento(
    const unsigned char* image,
    const DistribucionMPI* dist,
    int rank,
    int* local_rows,
    unsigned char** local_buffer
);

// Reensambla el resultado procesado en el rank 0.
// En rank 0, final_image se asigna con malloc (rows*cols) y debe liberarse.
int gather_fragmento(
    const unsigned char* local_processed,
    const DistribucionMPI* dist,
    int rank,
    unsigned char** final_image
);

#endif
