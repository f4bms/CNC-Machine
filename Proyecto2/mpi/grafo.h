#ifndef GRAFO_H
#define GRAFO_H

// Tipos topologicos del PCB en C puro (sin OpenCV).
// Este header lo comparten el codigo distribuido (mpi) y el modulo
// de procesamiento de imagen (img) para construir/depurar el grafo.

/**
 * @brief Punto de pixel en el esqueleto del PCB.
 *
 * Representa una coordenada 2D en la imagen binaria procesada.
 */
typedef struct
{
    /** Coordenada X del píxel. */
    int x;

    /** Coordenada Y del píxel. */
    int y;
} PixelPoint;

/**
 * @brief Nodo topológico en el grafo del esqueleto.
 *
 * Un nodo corresponde a un extremo o una bifurcación en el esqueleto.
 */
typedef struct
{
    /** Identificador único del nodo. */
    int id;

    /** Posición del nodo en coordenadas de píxel. */
    PixelPoint point;
} Nodo;

/**
 * @brief Arista topológica conectando dos nodos del grafo.
 *
 * Contiene la secuencia de píxeles que forma la trayectoria entre nodos.
 */
typedef struct
{
    /** Identificador del nodo de origen. */
    int origen;

    /** Identificador del nodo de destino. */
    int destino;

    /** Trayectoria de píxeles que une origen y destino. */
    PixelPoint *trayectoria;

    /** Longitud de la trayectoria en píxeles. */
    int trayectoria_len;
} Arista;

/**
 * @brief Grafo topológico extraído del esqueleto del PCB.
 *
 * Contiene nodos y aristas que representan la estructura vectorial del esqueleto.
 */
typedef struct
{
    /** Arreglo dinámico de nodos. */
    Nodo *nodes;

    /** Número de nodos en el grafo. */
    int node_count;

    /** Arreglo dinámico de aristas. */
    Arista *edges;

    /** Número de aristas en el grafo. */
    int edge_count;
} Grafo;

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Construye un grafo topológico a partir de un esqueleto binario.
     *
     * Detecta nodos y traza aristas siguiendo las líneas del esqueleto.
     *
     * @param skeleton Buffer de imagen binaria del esqueleto (0/255).
     * @param rows Altura de la imagen.
     * @param cols Ancho de la imagen.
     * @param out_graph Salida: grafo construido.
     * @return 0 en caso de éxito, -1 en error.
     */
    int generar_grafo(
        const unsigned char *skeleton,
        int rows,
        int cols,
        Grafo *out_graph);

    /**
     * @brief Libera la memoria asociada a un grafo.
     *
     * Libera los nodos, aristas y las trayectorias de pixeles.
     *
     * @param grafo Grafo a liberar.
     */
    void liberar_grafo(
        Grafo *grafo);

    /**
     * @brief Exporta las aristas del grafo a un archivo de texto.
     *
     * Sirve para depurar la vectorización del esqueleto y verificar aristas.
     *
     * @param grafo Grafo con las aristas a exportar.
     * @param filename Ruta del archivo de salida.
     * @return 0 en caso de éxito, -1 si no se puede abrir el archivo.
     */
    int guardar_aristas_debug(
        const Grafo *grafo,
        const char *filename);

#ifdef __cplusplus
}
#endif

#endif
