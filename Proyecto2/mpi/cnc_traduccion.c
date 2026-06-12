// Convierte el grafo del PCB en rutas compatibles con cnc_lib (C puro).
#include "cnc_traduccion.h"

#include <stdio.h>
#include <stdlib.h>

static int same_point(
    CNCPoint a,
    CNCPoint b
)
{
    return a.x == b.x && a.y == b.y;
}

// Compacta tramos rectos: conserva puntos de cambio de direccion y extremos.
static int simplificar_tramos_colineales(
    const CNCPoint* in,
    int in_count,
    CNCPoint** out,
    int* out_count
)
{
    if(in == NULL || out == NULL || out_count == NULL || in_count <= 0)
        return -1;

    CNCPoint* tmp = (CNCPoint*)malloc((size_t)in_count * sizeof(CNCPoint));

    if(tmp == NULL)
        return -1;

    int n = 0;

    // Siempre conserva el primer punto del path.
    tmp[n++] = in[0];

    for(int i = 1; i < in_count - 1; i++)
    {
        CNCPoint a = tmp[n - 1];
        CNCPoint b = in[i];
        CNCPoint c = in[i + 1];

        if(same_point(a, b))
            continue;

        // El punto final del path se preserva al salir del bucle.
        if(same_point(b, c))
            continue;

        long long v1x = (long long)b.x - (long long)a.x;
        long long v1y = (long long)b.y - (long long)a.y;
        long long v2x = (long long)c.x - (long long)b.x;
        long long v2y = (long long)c.y - (long long)b.y;

        long long cross = v1x * v2y - v1y * v2x;
        long long dot = v1x * v2x + v1y * v2y;

        // Si sigue en la misma direccion sobre la misma recta, omite el punto medio.
        if(cross == 0 && dot > 0)
            continue;

        tmp[n++] = b;
    }

    // Siempre conserva el ultimo punto del path.
    if(in_count > 1 && !same_point(tmp[n - 1], in[in_count - 1]))
        tmp[n++] = in[in_count - 1];

    *out = tmp;
    *out_count = n;
    return 0;
}

static long long dist2(
    CNCPoint a,
    CNCPoint b
)
{
    // Distancia cuadratica para ordenar rutas sin usar raiz cuadrada.
    long long dx = (long long)a.x - (long long)b.x;
    long long dy = (long long)a.y - (long long)b.y;

    return dx * dx + dy * dy;
}

int convertir_grafo_a_cnc_paths(
    const Grafo* grafo,
    int scale_steps,
    CNCPathOwned** out_paths,
    int* out_count
)
{
    // Convierte cada arista en una polilinea CNC escalada en steps.
    if(grafo == NULL || out_paths == NULL || out_count == NULL)
        return -1;

    *out_paths = NULL;
    *out_count = 0;

    if(scale_steps <= 0)
        return -1;

    if(grafo->edge_count <= 0)
        return 0;

    CNCPathOwned* paths =
        (CNCPathOwned*)calloc((size_t)grafo->edge_count, sizeof(CNCPathOwned));

    if(paths == NULL)
        return -1;

    int count = 0;

    // Reserva y rellena rutas solo para aristas validas (>= 2 puntos).
    for(int i = 0; i < grafo->edge_count; i++)
    {
        const Arista* a = &grafo->edges[i];

        if(a->trayectoria_len < 2)
            continue;

        CNCPoint* pts =
            (CNCPoint*)malloc((size_t)a->trayectoria_len * sizeof(CNCPoint));

        if(pts == NULL)
        {
            for(int k = 0; k < count; k++)
                free(paths[k].puntos);

            free(paths);
            return -1;
        }

        // Aqui ocurre la conversion pixel -> steps via scale_steps.
        for(int j = 0; j < a->trayectoria_len; j++)
        {
            pts[j].x = a->trayectoria[j].x * scale_steps;
            pts[j].y = a->trayectoria[j].y * scale_steps;
        }

        CNCPoint* simp = NULL;
        int simp_count = 0;

        if(simplificar_tramos_colineales(pts, a->trayectoria_len, &simp, &simp_count) != 0)
        {
            free(pts);

            for(int k = 0; k < count; k++)
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
    CNCPathOwned* paths,
    int count
)
{
    if(paths == NULL)
        return;

    for(int i = 0; i < count; i++)
        free(paths[i].puntos);

    free(paths);
}

int guardar_cnc_paths_debug(
    const CNCPathOwned* paths,
    int count,
    const char* filename
)
{
    FILE* fp = fopen(filename, "w");

    if(fp == NULL)
        return -1;

    fprintf(fp, "# CNC paths: %d\n", count);

    for(int i = 0; i < count; i++)
    {
        const CNCPathOwned* p = &paths[i];

        fprintf(fp, "path %d points=%d\n", i, p->count);

        for(int j = 0; j < p->count; j++)
        {
            fprintf(
                fp,
                "  %d %d\n",
                (int)p->puntos[j].x,
                (int)p->puntos[j].y
            );
        }
    }

    fclose(fp);
    return 0;
}

int ejecutar_cnc_paths(
    const CNCPathOwned* paths,
    int count,
    const char* device
)
{
    // Ordena y ejecuta rutas sobre cnc_lib minimizando traslados en vacio.
    if(paths == NULL || count <= 0)
        return CNC_ERR_INVALID;

    // Plan de ejecucion: indices ordenados con heuristica greedy.
    int* order = (int*)malloc((size_t)count * sizeof(int));
    char* used = (char*)calloc((size_t)count, sizeof(char));
    char* reverse = (char*)calloc((size_t)count, sizeof(char));

    if(order == NULL || used == NULL || reverse == NULL)
    {
        free(order);
        free(used);
        free(reverse);
        return CNC_ERR_INVALID;
    }

    int planned = 0;
    CNCPoint cursor = {0, 0};

    // Heuristica greedy: elige la siguiente ruta por distancia al cursor.
    for(int step = 0; step < count; step++)
    {
        int best_idx = -1;
        int best_reverse = 0;
        long long best_cost = -1;

        for(int i = 0; i < count; i++)
        {
            if(used[i] || paths[i].count <= 0)
                continue;

            CNCPoint start = paths[i].puntos[0];
            CNCPoint end = paths[i].puntos[paths[i].count - 1];

            long long c_start = dist2(cursor, start);
            long long c_end = dist2(cursor, end);

            if(best_cost < 0 || c_start < best_cost)
            {
                best_cost = c_start;
                best_idx = i;
                best_reverse = 0;
            }

            if(c_end < best_cost)
            {
                best_cost = c_end;
                best_idx = i;
                best_reverse = 1;
            }
        }

        if(best_idx < 0)
            break;

        used[best_idx] = 1;
        reverse[best_idx] = (char)best_reverse;
        order[planned++] = best_idx;

        if(best_reverse)
            cursor = paths[best_idx].puntos[0];
        else
            cursor = paths[best_idx].puntos[paths[best_idx].count - 1];
    }

    CNCHandle handle;

    // Desde aqui empieza la fase de hardware via cnc_lib.
    int ret = cnc_open(&handle, device);

    if(ret != CNC_OK)
    {
        fprintf(stderr, "[CNC] cnc_open fallo: %s\n", cnc_strerror(ret));
        free(order);
        free(used);
        free(reverse);
        return ret;
    }

    ret = cnc_home(&handle);

    if(ret != CNC_OK)
    {
        fprintf(stderr, "[CNC] cnc_home fallo: %s\n", cnc_strerror(ret));
        cnc_close(&handle);
        free(order);
        free(used);
        free(reverse);
        return ret;
    }

    // Ejecuta cada path ya ordenado y opcionalmente invertido.
    for(int k = 0; k < planned; k++)
    {
        int idx = order[k];
        const CNCPathOwned* p = &paths[idx];

        if(p->count <= 0)
            continue;

        // Copia puntos aplicando reversa si la heuristica lo eligio.
        CNCPoint* buffer =
            (CNCPoint*)malloc((size_t)p->count * sizeof(CNCPoint));

        if(buffer == NULL)
        {
            cnc_pen_up(&handle);
            cnc_close(&handle);
            free(order);
            free(used);
            free(reverse);
            return CNC_ERR_INVALID;
        }

        for(int j = 0; j < p->count; j++)
        {
            if(reverse[idx])
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

        if(ret != CNC_OK)
        {
            fprintf(
                stderr,
                "[CNC] cnc_draw_path fallo en path %d: %s\n",
                idx,
                cnc_strerror(ret)
            );

            cnc_pen_up(&handle);
            cnc_close(&handle);
            free(order);
            free(used);
            free(reverse);
            return ret;
        }
    }

    cnc_pen_up(&handle);
    cnc_home(&handle);
    cnc_close(&handle);

    free(order);
    free(used);
    free(reverse);

    return CNC_OK;
}
