#ifndef GRAFO_H
#define GRAFO_H

// Tipos topologicos del PCB en C puro (sin OpenCV).
// Este header lo comparten el codigo distribuido (mpi) y el modulo
// de procesamiento de imagen (img) para construir/depurar el grafo.

typedef struct
{
    int x;
    int y;
} PixelPoint;

typedef struct
{
    int id;
    PixelPoint point;
} Nodo;

typedef struct
{
    int origen;
    int destino;

    // Trayectoria de pixeles entre origen y destino.
    PixelPoint* trayectoria;
    int trayectoria_len;
} Arista;

typedef struct
{
    Nodo* nodes;
    int node_count;

    Arista* edges;
    int edge_count;
} Grafo;

#ifdef __cplusplus
extern "C"
{
#endif

// Construye un grafo topologico a partir de un esqueleto (buffer 0/255).
// Devuelve 0 en exito; el grafo resultante se libera con liberar_grafo().
int generar_grafo(
    const unsigned char* skeleton,
    int rows,
    int cols,
    Grafo* out_graph
);

void liberar_grafo(
    Grafo* grafo
);

// Exporta aristas como texto para depurar la vectorizacion de trayectorias.
int guardar_aristas_debug(
    const Grafo* grafo,
    const char* filename
);

#ifdef __cplusplus
}
#endif

#endif
