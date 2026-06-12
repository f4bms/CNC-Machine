#ifndef CNC_TRADUCCION_H
#define CNC_TRADUCCION_H

#include "../biblioteca/include/cnc_lib.h"
#include "grafo.h"

/**
 * @brief Wrapper que mantiene la propiedad de los puntos de una ruta CNC.
 *
 * Contiene el arreglo de puntos y su tamaño para facilitar la gestión de
 * memoria de las rutas generadas a partir del grafo.
 */
typedef struct
{
    /** Puntero al arreglo de puntos CNC. */
    CNCPoint *puntos;

    /** Número de puntos en la ruta. */
    int count;
} CNCPathOwned;

/**
 * @brief Convierte cada arista del grafo en una ruta CNC escalada.
 *
 * Toma un grafo topológico y genera rutas CNC escaladas en pasos,
 * simplificando cada trayectoria con el algoritmo Douglas-Peucker.
 *
 * @param grafo Grafo de entrada con aristas y trayectorias en pixeles.
 * @param scale_steps Factor de escala para convertir pixeles a pasos.
 * @param out_paths Puntero donde se guardará el arreglo de rutas creadas.
 * @param out_count Puntero donde se escribirá el número de rutas generadas.
 * @return 0 en caso de éxito, -1 en error.
 */
int convertir_grafo_a_cnc_paths(
    const Grafo *grafo,
    int scale_steps,
    CNCPathOwned **out_paths,
    int *out_count);

/**
 * @brief Libera la memoria usada por rutas CNC generadas.
 *
 * Libera cada arreglo de puntos de ruta y luego el arreglo de rutas.
 *
 * @param paths Arreglo de rutas CNC a liberar.
 * @param count Cantidad de rutas en el arreglo.
 */
void liberar_cnc_paths(
    CNCPathOwned *paths,
    int count);

/**
 * @brief Guarda un volcado textual de las rutas CNC para depuración.
 *
 * Imprime en un archivo la cantidad de rutas y las coordenadas de cada punto.
 *
 * @param paths Arreglo de rutas CNC a volcar.
 * @param count Cantidad de rutas en el arreglo.
 * @param filename Ruta del archivo de salida.
 * @return 0 en caso de éxito, -1 si no se puede abrir el archivo.
 */
int guardar_cnc_paths_debug(
    const CNCPathOwned *paths,
    int count,
    const char *filename);

/**
 * @brief Reordena y envía las rutas al driver usando cnc_lib.
 *
 * Esta función ejecuta el envío de las rutas CNC al dispositivo físico.
 *
 * @param paths Arreglo de rutas CNC a ejecutar.
 * @param count Cantidad de rutas en el arreglo.
 * @param device Ruta del dispositivo CNC.
 * @return Código de retorno de cnc_lib.
 */
int ejecutar_cnc_paths(
    const CNCPathOwned *paths,
    int count,
    const char *device);

#endif
