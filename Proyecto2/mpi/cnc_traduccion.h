#ifndef CNC_TRADUCCION_H
#define CNC_TRADUCCION_H

#include <vector>

extern "C"
{
#include "cnc_lib.h"
}

#include "grafo.h"

/*
 * Wrapper para mantener ownership de puntos
 * y adaptarlos a CNCPath (API C).
 */
typedef struct
{
    std::vector<CNCPoint> puntos;
} CNCPathOwned;

std::vector<CNCPathOwned> convertir_grafo_a_cnc_paths(
    const Grafo& grafo,
    int scale_steps
);

void guardar_cnc_paths_debug(
    const std::vector<CNCPathOwned>& paths,
    const char* filename
);

int ejecutar_cnc_paths(
    const std::vector<CNCPathOwned>& paths,
    const char* device,
    int speed
);

#endif
