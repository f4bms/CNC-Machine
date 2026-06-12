#ifndef CNC_TRADUCCION_H
#define CNC_TRADUCCION_H

#include "../biblioteca/include/cnc_lib.h"
#include "grafo.h"

// Wrapper que mantiene el ownership de los puntos de una ruta CNC.
typedef struct
{
    CNCPoint* puntos;
    int count;
} CNCPathOwned;

// Convierte cada arista del grafo en una ruta CNC escalada.
// Devuelve 0 en exito; las rutas se liberan con liberar_cnc_paths().
int convertir_grafo_a_cnc_paths(
    const Grafo* grafo,
    int scale_steps,
    CNCPathOwned** out_paths,
    int* out_count
);

void liberar_cnc_paths(
    CNCPathOwned* paths,
    int count
);

int guardar_cnc_paths_debug(
    const CNCPathOwned* paths,
    int count,
    const char* filename
);

// Reordena (greedy) y envia las rutas al driver via cnc_lib.
int ejecutar_cnc_paths(
    const CNCPathOwned* paths,
    int count,
    const char* device
);

#endif
