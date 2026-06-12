#ifndef IMG_PROCESAMIENTO_IMAGEN_H
#define IMG_PROCESAMIENTO_IMAGEN_H

#include "../mpi/grafo.h"

#ifdef __cplusplus
extern "C"
{
#endif

// Carga una imagen en escala de grises. El buffer se asigna con malloc
// y debe liberarse con free(). Devuelve 0 en exito.
int img_load_grayscale(
    const char* path,
    unsigned char** out_buffer,
    int* out_rows,
    int* out_cols
);

// Guarda un buffer en escala de grises como imagen. Devuelve 0 en exito.
int img_save_grayscale(
    const char* path,
    const unsigned char* buffer,
    int rows,
    int cols
);

// Binariza y esqueletiza un fragmento local (OpenCV).
// Escribe imagenes de debug por rank en output_dir.
// out_skeleton se asigna con malloc (rows*cols) y debe liberarse con free().
int img_process_fragment(
    const unsigned char* input,
    int rows,
    int cols,
    int rank,
    const char* output_dir,
    unsigned char** out_skeleton
);

// Guarda una imagen de debug del grafo (nodos dibujados) sobre el esqueleto.
int img_save_graph_debug(
    const char* path,
    const unsigned char* skeleton,
    int rows,
    int cols,
    const Grafo* grafo
);

#ifdef __cplusplus
}
#endif

#endif
