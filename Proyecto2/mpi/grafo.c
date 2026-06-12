// Construye un grafo topologico del esqueleto del PCB (C puro, sin OpenCV).
#include "grafo.h"

#include <stdio.h>
#include <stdlib.h>

// ---- Acceso a pixeles del esqueleto -------------------------------------

/**
 * @brief Verifica si una coordenada está dentro de los límites de la imagen.
 *
 * @param rows Filas totales de la imagen.
 * @param cols Columnas totales de la imagen.
 * @param y Coordenada Y.
 * @param x Coordenada X.
 * @return 1 si está dentro, 0 si está fuera.
 */
static int dentro(
    int rows,
    int cols,
    int y,
    int x
)
{
    return x >= 0 && y >= 0 && x < cols && y < rows;
}

/**
 * @brief Comprueba si un píxel del esqueleto está activo.
 *
 * Valida primero los límites y luego retorna si el valor del píxel es mayor que cero.
 *
 * @param img Imagen de esqueleto.
 * @param rows Filas de la imagen.
 * @param cols Columnas de la imagen.
 * @param y Coordenada Y.
 * @param x Coordenada X.
 * @return 1 si el píxel está activo, 0 en caso contrario.
 */
static int pixel_activo(
    const unsigned char* img,
    int rows,
    int cols,
    int y,
    int x
)
{
    if(!dentro(rows, cols, y, x))
        return 0;

    return img[(size_t)y * (size_t)cols + (size_t)x] > 0 ? 1 : 0;
}

/**
 * @brief Cuenta vecinos activos en la conectividad 8-neighbors.
 *
 * Se usa para clasificar extremos y bifurcaciones en el esqueleto.
 *
 * @param img Imagen de esqueleto.
 * @param rows Filas de la imagen.
 * @param cols Columnas de la imagen.
 * @param y Coordenada Y.
 * @param x Coordenada X.
 * @return Número de vecinos activos.
 */
static int contar_vecinos(
    const unsigned char* img,
    int rows,
    int cols,
    int y,
    int x
)
{
    // Cuenta conectividad 8-neighbors para clasificar extremos/bifurcaciones.
    int count = 0;

    for(int dy = -1; dy <= 1; dy++)
    {
        for(int dx = -1; dx <= 1; dx++)
        {
            if(dx == 0 && dy == 0)
                continue;

            if(pixel_activo(img, rows, cols, y + dy, x + dx))
                count++;
        }
    }

    return count;
}

/**
 * @brief Enumera los vecinos activos de un píxel en el esqueleto.
 *
 * Rellena el arreglo de salida con las coordenadas de los vecinos activos.
 *
 * @param img Imagen de esqueleto.
 * @param rows Filas de la imagen.
 * @param cols Columnas de la imagen.
 * @param x Coordenada X del píxel actual.
 * @param y Coordenada Y del píxel actual.
 * @param out Salida: vecinos activos encontrados.
 * @return Cantidad de vecinos activos.
 */
// Llena hasta 8 vecinos activos; devuelve la cantidad encontrada.
static int vecinos_activos(
    const unsigned char* img,
    int rows,
    int cols,
    int x,
    int y,
    PixelPoint* out
)
{
    int n = 0;

    for(int dy = -1; dy <= 1; dy++)
    {
        for(int dx = -1; dx <= 1; dx++)
        {
            if(dx == 0 && dy == 0)
                continue;

            int nx = x + dx;
            int ny = y + dy;

            if(pixel_activo(img, rows, cols, ny, nx))
            {
                out[n].x = nx;
                out[n].y = ny;
                n++;
            }
        }
    }

    return n;
}

// ---- Arreglo dinamico de PixelPoint para la trayectoria ------------------

typedef struct
{
    PixelPoint* data;
    int len;
    int cap;
} PuntoVec;

/**
 * @brief Inserta un punto en un vector dinámico de PixelPoint.
 *
 * Expande la capacidad del vector si es necesario y anexa el punto.
 *
 * @param v Vector dinámico de puntos.
 * @param p Punto a agregar.
 * @return 0 en caso de éxito, -1 si no hay memoria.
 */
static int pv_push(
    PuntoVec* v,
    PixelPoint p
)
{
    if(v->len >= v->cap)
    {
        int nuevo_cap = v->cap == 0 ? 16 : v->cap * 2;

        PixelPoint* nd =
            (PixelPoint*)realloc(v->data, (size_t)nuevo_cap * sizeof(PixelPoint));

        if(nd == NULL)
            return -1;

        v->data = nd;
        v->cap = nuevo_cap;
    }

    v->data[v->len++] = p;
    return 0;
}

// ---- Generacion del grafo ------------------------------------------------

/**
 * @brief Genera un grafo topológico a partir del esqueleto de PCB.
 *
 * Detecta nodos en extremos y bifurcaciones, y traza aristas entre ellos
 * siguiendo el esqueleto binario.
 *
 * @param skeleton Imagen del esqueleto.
 * @param rows Filas de la imagen.
 * @param cols Columnas de la imagen.
 * @param out_graph Salida: grafo generado.
 * @return 0 en caso de éxito, -1 en caso de error.
 */
int generar_grafo(
    const unsigned char* skeleton,
    int rows,
    int cols,
    Grafo* out_graph
)
{
    // Entrada: esqueleto binario; salida: nodos y aristas topologicas.
    if(skeleton == NULL || out_graph == NULL || rows <= 0 || cols <= 0)
        return -1;

    out_graph->nodes = NULL;
    out_graph->node_count = 0;
    out_graph->edges = NULL;
    out_graph->edge_count = 0;

    // Mapa pixel -> id de nodo (-1 si no es nodo).
    int* pixel_to_node =
        (int*)malloc((size_t)rows * (size_t)cols * sizeof(int));

    if(pixel_to_node == NULL)
        return -1;

    for(size_t i = 0; i < (size_t)rows * (size_t)cols; i++)
        pixel_to_node[i] = -1;

    // Primera pasada: detecta nodos (extremos o bifurcaciones).
    int node_cap = 0;
    int node_count = 0;
    Nodo* nodes = NULL;

    // Barrido completo para detectar nodos candidatos.
    for(int y = 0; y < rows; y++)
    {
        for(int x = 0; x < cols; x++)
        {
            if(!pixel_activo(skeleton, rows, cols, y, x))
                continue;

            int vecinos = contar_vecinos(skeleton, rows, cols, y, x);

            if(vecinos == 1 || vecinos >= 3)
            {
                if(node_count >= node_cap)
                {
                    int nuevo_cap = node_cap == 0 ? 64 : node_cap * 2;

                    Nodo* nn =
                        (Nodo*)realloc(nodes, (size_t)nuevo_cap * sizeof(Nodo));

                    if(nn == NULL)
                    {
                        free(nodes);
                        free(pixel_to_node);
                        return -1;
                    }

                    nodes = nn;
                    node_cap = nuevo_cap;
                }

                nodes[node_count].id = node_count;
                nodes[node_count].point.x = x;
                nodes[node_count].point.y = y;

                pixel_to_node[(size_t)y * (size_t)cols + (size_t)x] = node_count;

                node_count++;
            }
        }
    }

    // Segunda pasada: traza aristas siguiendo la linea entre nodos.
    int edge_cap = 0;
    int edge_count = 0;
    Arista* edges = NULL;

    // Guardia anti-bucle por si la traza queda ciclica o corrupta.
    long long guard_limit = (long long)rows * (long long)cols;

    for(int ni = 0; ni < node_count; ni++)
    {
        PixelPoint inicio = nodes[ni].point;
        int nodo_id = nodes[ni].id;

        PixelPoint vecinos[8];

        int nv =
            vecinos_activos(skeleton, rows, cols, inicio.x, inicio.y, vecinos);

        for(int vi = 0; vi < nv; vi++)
        {
            PuntoVec tray;
            tray.data = NULL;
            tray.len = 0;
            tray.cap = 0;

            if(pv_push(&tray, inicio) != 0 ||
               pv_push(&tray, vecinos[vi]) != 0)
            {
                free(tray.data);
                continue;
            }

            PixelPoint previo = inicio;
            PixelPoint actual = vecinos[vi];

            int destino_id = -1;

            // Avanza pixel a pixel hasta encontrar otro nodo o una ambiguedad.
            for(long long guard = 0; guard < guard_limit; guard++)
            {
                int actual_node =
                    pixel_to_node[(size_t)actual.y * (size_t)cols + (size_t)actual.x];

                if(actual_node != -1 && actual_node != nodo_id)
                {
                    destino_id = actual_node;
                    break;
                }

                PixelPoint siguientes[8];

                int ns =
                    vecinos_activos(skeleton, rows, cols, actual.x, actual.y, siguientes);

                PixelPoint candidatos[8];
                int nc = 0;

                for(int si = 0; si < ns; si++)
                {
                    if(siguientes[si].x == previo.x &&
                       siguientes[si].y == previo.y)
                        continue;

                    candidatos[nc++] = siguientes[si];
                }

                if(nc != 1)
                    break;

                previo = actual;
                actual = candidatos[0];

                if(pv_push(&tray, actual) != 0)
                    break;
            }

            // Evita aristas incompletas y duplicadas (solo id menor -> mayor).
            if(destino_id < 0 || nodo_id >= destino_id)
            {
                free(tray.data);
                continue;
            }

            if(edge_count >= edge_cap)
            {
                int nuevo_cap = edge_cap == 0 ? 64 : edge_cap * 2;

                Arista* ne =
                    (Arista*)realloc(edges, (size_t)nuevo_cap * sizeof(Arista));

                if(ne == NULL)
                {
                    free(tray.data);
                    break;
                }

                edges = ne;
                edge_cap = nuevo_cap;
            }

            edges[edge_count].origen = nodo_id;
            edges[edge_count].destino = destino_id;
            edges[edge_count].trayectoria = tray.data;
            edges[edge_count].trayectoria_len = tray.len;
            edge_count++;
        }
    }

    free(pixel_to_node);

    out_graph->nodes = nodes;
    out_graph->node_count = node_count;
    out_graph->edges = edges;
    out_graph->edge_count = edge_count;

    return 0;
}

/**
 * @brief Libera la memoria del grafo y reinicia su estado.
 *
 * Libera todas las trayectorias de aristas, nodos y arreglos asociados.
 *
 * @param grafo Grafo a liberar.
 */
void liberar_grafo(
    Grafo* grafo
)
{
    // Libera trayectoria por trayectoria y luego arreglos principales.
    if(grafo == NULL)
        return;

    for(int i = 0; i < grafo->edge_count; i++)
        free(grafo->edges[i].trayectoria);

    free(grafo->edges);
    free(grafo->nodes);

    grafo->nodes = NULL;
    grafo->node_count = 0;
    grafo->edges = NULL;
    grafo->edge_count = 0;
}

/**
 * @brief Guarda información de depuración de las aristas detectadas.
 *
 * Escribe un archivo de texto con el número de aristas y sus metadatos.
 *
 * @param grafo Grafo que se va a exportar.
 * @param filename Nombre del archivo de salida.
 * @return 0 en caso de éxito, -1 si no se puede abrir el archivo.
 */
int guardar_aristas_debug(
    const Grafo* grafo,
    const char* filename
)
{
    // Exporta metadatos de aristas para inspeccion textual rapida.
    FILE* fp = fopen(filename, "w");

    if(fp == NULL)
        return -1;

    fprintf(
        fp,
        "# Aristas detectadas: %d\n",
        grafo->edge_count
    );

    for(int i = 0; i < grafo->edge_count; i++)
    {
        const Arista* a = &grafo->edges[i];

        fprintf(
            fp,
            "arista %d: %d -> %d, puntos=%d\n",
            i,
            a->origen,
            a->destino,
            a->trayectoria_len
        );
    }

    fclose(fp);
    return 0;
}
