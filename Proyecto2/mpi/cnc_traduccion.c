// Convierte el grafo del PCB en rutas compatibles con cnc_lib (C puro).
#include "cnc_traduccion.h"

#include <stdio.h>
#include <stdlib.h>

// CNC_DP_EPSILON controla la tolerancia de simplificacion.
// Unidades: steps (despues de aplicar scale_steps).
// Valores tipicos:
//   1.0 -> simplificacion conservadora, curvas muy precisas
//   2.0 -> balance entre precision y velocidad (recomendado)
//   5.0 -> simplificacion agresiva, menos comandos, curvas menos suaves
#ifndef CNC_DP_EPSILON
#define CNC_DP_EPSILON 2.0
#endif

// Paso recursivo de Douglas-Peucker.
// Marca en keep[] los puntos que superan la tolerancia epsilon.
static void douglas_peucker_recursive(
    const CNCPoint *pts,
    int start,
    int end,
    double epsilon,
    char *keep)
{
    if (end <= start + 1)
        return;

    double x1 = (double)pts[start].x;
    double y1 = (double)pts[start].y;
    double x2 = (double)pts[end].x;
    double y2 = (double)pts[end].y;
    double dx = x2 - x1;
    double dy = y2 - y1;
    double len2 = dx * dx + dy * dy;

    double max_dist = 0.0;
    int max_idx = start;

    for (int i = start + 1; i < end; i++)
    {
        double px = (double)pts[i].x - x1;
        double py = (double)pts[i].y - y1;
        double dist;

        if (len2 == 0.0)
        {
            // start == end: distancia directa al punto
            dist = px * px + py * py;
        }
        else
        {
            // distancia cuadratica del punto a la recta start-end
            double t = (px * dx + py * dy) / len2;
            double ex = px - t * dx;
            double ey = py - t * dy;
            dist = ex * ex + ey * ey;
        }

        if (dist > max_dist)
        {
            max_dist = dist;
            max_idx = i;
        }
    }

    // Si el punto mas lejano supera epsilon, se conserva y se subdivide.
    if (max_dist > epsilon * epsilon)
    {
        keep[max_idx] = 1;
        douglas_peucker_recursive(pts, start, max_idx, epsilon, keep);
        douglas_peucker_recursive(pts, max_idx, end, epsilon, keep);
    }
}

// Simplifica una polilÃ­nea usando Douglas-Peucker.
// Retorna 0 en exito, -1 en error.
static int simplificar_douglas_peucker(
    const CNCPoint *in,
    int in_count,
    CNCPoint **out,
    int *out_count,
    double epsilon)
{
    if (in == NULL || out == NULL || out_count == NULL || in_count <= 0)
        return -1;

    // Con 2 puntos o menos no hay nada que simplificar.
    if (in_count <= 2)
    {
        CNCPoint *tmp =
            (CNCPoint *)malloc((size_t)in_count * sizeof(CNCPoint));

        if (tmp == NULL)
            return -1;

        for (int i = 0; i < in_count; i++)
            tmp[i] = in[i];

        *out = tmp;
        *out_count = in_count;
        return 0;
    }

    char *keep = (char *)calloc((size_t)in_count, sizeof(char));

    if (keep == NULL)
        return -1;

    // Los extremos siempre se conservan.
    keep[0] = 1;
    keep[in_count - 1] = 1;

    douglas_peucker_recursive(in, 0, in_count - 1, epsilon, keep);

    // Cuenta cuantos puntos quedan tras la simplificacion.
    int n = 0;

    for (int i = 0; i < in_count; i++)
        if (keep[i])
            n++;

    CNCPoint *tmp = (CNCPoint *)malloc((size_t)n * sizeof(CNCPoint));

    if (tmp == NULL)
    {
        free(keep);
        return -1;
    }

    int k = 0;

    for (int i = 0; i < in_count; i++)
        if (keep[i])
            tmp[k++] = in[i];

    free(keep);

    *out = tmp;
    *out_count = n;
    return 0;
}

static long long dist2(
    CNCPoint a,
    CNCPoint b)
{
    // Distancia cuadratica para ordenar rutas sin usar raiz cuadrada.
    long long dx = (long long)a.x - (long long)b.x;
    long long dy = (long long)a.y - (long long)b.y;

    return dx * dx + dy * dy;
}

int convertir_grafo_a_cnc_paths(
    const Grafo *grafo,
    int scale_steps,
    CNCPathOwned **out_paths,
    int *out_count)
{
    // Convierte cada arista en una polilinea CNC escalada en steps.
    if (grafo == NULL || out_paths == NULL || out_count == NULL)
        return -1;

    *out_paths = NULL;
    *out_count = 0;

    if (scale_steps <= 0)
        return -1;

    if (grafo->edge_count <= 0)
        return 0;

    CNCPathOwned *paths =
        (CNCPathOwned *)calloc((size_t)grafo->edge_count, sizeof(CNCPathOwned));

    if (paths == NULL)
        return -1;

    int count = 0;

    // Reserva y rellena rutas solo para aristas validas (>= 2 puntos).
    for (int i = 0; i < grafo->edge_count; i++)
    {
        const Arista *a = &grafo->edges[i];

        if (a->trayectoria_len < 2)
            continue;

        CNCPoint *pts =
            (CNCPoint *)malloc((size_t)a->trayectoria_len * sizeof(CNCPoint));

        if (pts == NULL)
        {
            for (int k = 0; k < count; k++)
                free(paths[k].puntos);

            free(paths);
            return -1;
        }

        // Aqui ocurre la conversion pixel -> steps via scale_steps.
        for (int j = 0; j < a->trayectoria_len; j++)
        {
            pts[j].x = a->trayectoria[j].x * scale_steps;
            pts[j].y = a->trayectoria[j].y * scale_steps;
        }

        CNCPoint *simp = NULL;
        int simp_count = 0;

        if (simplificar_douglas_peucker(pts, a->trayectoria_len, &simp, &simp_count, CNC_DP_EPSILON) != 0)
        {
            free(pts);

            for (int k = 0; k < count; k++)
                free(paths[k].puntos);

            free(paths);
            return -1;
        }

        free(pts);

        paths[count].puntos = simp;
        paths[count].count = simp_count;
        count++;
    }

    *out_paths = paths;
    *out_count = count;

    return 0;
}

void liberar_cnc_paths(
    CNCPathOwned *paths,
    int count)
{
    if (paths == NULL)
        return;

    for (int i = 0; i < count; i++)
        free(paths[i].puntos);

    free(paths);
}

int guardar_cnc_paths_debug(
    const CNCPathOwned *paths,
    int count,
    const char *filename)
{
    FILE *fp = fopen(filename, "w");

    if (fp == NULL)
        return -1;

    fprintf(fp, "# CNC paths: %d\n", count);

    for (int i = 0; i < count; i++)
    {
        const CNCPathOwned *p = &paths[i];

        fprintf(fp, "path %d points=%d\n", i, p->count);

        for (int j = 0; j < p->count; j++)
        {
            fprintf(
                fp,
                "  %d %d\n",
                (int)p->puntos[j].x,
                (int)p->puntos[j].y);
        }
    }

    fclose(fp);
    return 0;
}

int ejecutar_cnc_paths(
    const CNCPathOwned *paths,
    int count,
    const char *device)
{
    // Ordena y ejecuta rutas sobre cnc_lib minimizando traslados en vacio.
    if (paths == NULL || count <= 0)
        return CNC_ERR_INVALID;

    // Plan de ejecucion: indices ordenados con heuristica greedy.
    int *order = (int *)malloc((size_t)count * sizeof(int));
    char *used = (char *)calloc((size_t)count, sizeof(char));
    char *reverse = (char *)calloc((size_t)count, sizeof(char));

    if (order == NULL || used == NULL || reverse == NULL)
    {
        free(order);
        free(used);
        free(reverse);
        return CNC_ERR_INVALID;
    }

    int planned = 0;
    CNCPoint cursor = {0, 0};

    // Heuristica greedy: elige la siguiente ruta por distancia al cursor.
    for (int step = 0; step < count; step++)
    {
        int best_idx = -1;
        int best_reverse = 0;
        long long best_cost = -1;

        for (int i = 0; i < count; i++)
        {
            if (used[i] || paths[i].count <= 0)
                continue;

            CNCPoint start = paths[i].puntos[0];
            CNCPoint end = paths[i].puntos[paths[i].count - 1];

            long long c_start = dist2(cursor, start);
            long long c_end = dist2(cursor, end);

            if (best_cost < 0 || c_start < best_cost)
            {
                best_cost = c_start;
                best_idx = i;
                best_reverse = 0;
            }

            if (c_end < best_cost)
            {
                best_cost = c_end;
                best_idx = i;
                best_reverse = 1;
            }
        }

        if (best_idx < 0)
            break;

        used[best_idx] = 1;
        reverse[best_idx] = (char)best_reverse;
        order[planned++] = best_idx;

        if (best_reverse)
            cursor = paths[best_idx].puntos[0];
        else
            cursor = paths[best_idx].puntos[paths[best_idx].count - 1];
    }

    CNCHandle handle;

    // Desde aqui empieza la fase de hardware via cnc_lib.
    int ret = cnc_open(&handle, device);

    if (ret != CNC_OK)
    {
        fprintf(stderr, "[CNC] cnc_open fallo: %s\n", cnc_strerror(ret));
        free(order);
        free(used);
        free(reverse);
        return ret;
    }

    ret = cnc_home(&handle);

    if (ret != CNC_OK)
    {
        fprintf(stderr, "[CNC] cnc_home fallo: %s\n", cnc_strerror(ret));
        cnc_close(&handle);
        free(order);
        free(used);
        free(reverse);
        return ret;
    }

    // Ejecuta cada path ya ordenado y opcionalmente invertido.
    for (int k = 0; k < planned; k++)
    {
        int idx = order[k];
        const CNCPathOwned *p = &paths[idx];

        if (p->count <= 0)
            continue;

        // Copia puntos aplicando reversa si la heuristica lo eligio.
        CNCPoint *buffer =
            (CNCPoint *)malloc((size_t)p->count * sizeof(CNCPoint));

        if (buffer == NULL)
        {
            cnc_pen_up(&handle);
            cnc_close(&handle);
            free(order);
            free(used);
            free(reverse);
            return CNC_ERR_INVALID;
        }

        for (int j = 0; j < p->count; j++)
        {
            if (reverse[idx])
                buffer[j] = p->puntos[p->count - 1 - j];
            else
                buffer[j] = p->puntos[j];
        }

        CNCPath draw_path;
        draw_path.points = buffer;
        draw_path.count = (size_t)p->count;

        // cnc_draw_path gestiona pen-up/pen-down y el movimiento real.
        ret = cnc_draw_path(&handle, &draw_path);

        free(buffer);

        if (ret != CNC_OK)
        {
            fprintf(
                stderr,
                "[CNC] cnc_draw_path fallo en path %d: %s\n",
                idx,
                cnc_strerror(ret));

            cnc_pen_up(&handle);
            cnc_close(&handle);
            free(order);
            free(used);
            free(reverse);
            return ret;
        }
    }

    cnc_home(&handle);
    cnc_close(&handle);

    free(order);
    free(used);
    free(reverse);

    return CNC_OK;
}
