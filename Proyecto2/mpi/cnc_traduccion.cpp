// Convierte el grafo del PCB en rutas compatibles con cnc_lib.
#include "cnc_traduccion.h"

#include <algorithm>
#include <limits>

#include <stdio.h>

static long long dist2(
    const CNCPoint& a,
    const CNCPoint& b
)
{
    // Distancia cuadrática para ordenar rutas sin usar raíz cuadrada.
    long long dx =
        (long long)a.x -
        (long long)b.x;

    long long dy =
        (long long)a.y -
        (long long)b.y;

    return dx * dx + dy * dy;
}

static CNCPoint convertir_punto(
    const cv::Point& p,
    int scale_steps
)
{
    // Convierte un píxel del PCB a coordenadas de pasos de la CNC.
    CNCPoint out;
    out.x = p.x * scale_steps;
    out.y = p.y * scale_steps;
    return out;
}

std::vector<CNCPathOwned> convertir_grafo_a_cnc_paths(
    const Grafo& grafo,
    int scale_steps
)
{
    // Cada arista del grafo se vuelve una ruta CNC independiente.
    std::vector<CNCPathOwned> paths;

    if(scale_steps <= 0)
        return paths;

    for(const auto& arista : grafo.aristas)
    {
        if(arista.trayectoria.size() < 2)
            continue;

        CNCPathOwned path;

        path.puntos.reserve(
            arista.trayectoria.size()
        );

        for(const auto& p : arista.trayectoria)
        {
            path.puntos.push_back(
                convertir_punto(
                    p,
                    scale_steps
                )
            );
        }

        paths.push_back(path);
    }

    return paths;
}

void guardar_cnc_paths_debug(
    const std::vector<CNCPathOwned>& paths,
    const char* filename
)
{
    // Guarda las rutas ya escaladas para inspeccionar el orden y sus puntos.
    FILE* fp = fopen(
        filename,
        "w"
    );

    if(fp == NULL)
        return;

    fprintf(
        fp,
        "# CNC paths: %lu\n",
        (unsigned long)
        paths.size()
    );

    for(size_t i = 0; i < paths.size(); i++)
    {
        const CNCPathOwned& p =
            paths[i];

        fprintf(
            fp,
            "path %lu points=%lu\n",
            (unsigned long)i,
            (unsigned long)
            p.puntos.size()
        );

        for(const auto& pt : p.puntos)
        {
            fprintf(
                fp,
                "  %d %d\n",
                (int)pt.x,
                (int)pt.y
            );
        }
    }

    fclose(fp);
}

int ejecutar_cnc_paths(
    const std::vector<CNCPathOwned>& paths,
    const char* device,
    int speed
)
{
    // Reordena rutas para reducir movimientos con pluma arriba y luego las envía al driver.
    std::vector<CNCPathOwned> plan;
    plan.reserve(paths.size());

    std::vector<char> used(
        paths.size(),
        0
    );

    CNCPoint cursor = {0, 0};

    // Heurística greedy: toma la siguiente ruta más cercana y la invierte si conviene.
    for(size_t step = 0; step < paths.size(); step++)
    {
        size_t best_idx = paths.size();
        bool best_reverse = false;

        long long best_cost =
            std::numeric_limits<long long>::max();

        for(size_t i = 0; i < paths.size(); i++)
        {
            if(used[i])
                continue;

            if(paths[i].puntos.empty())
                continue;

            const CNCPoint& start =
                paths[i].puntos.front();

            const CNCPoint& end =
                paths[i].puntos.back();

            long long c_start =
                dist2(cursor, start);

            long long c_end =
                dist2(cursor, end);

            if(c_start < best_cost)
            {
                best_cost = c_start;
                best_idx = i;
                best_reverse = false;
            }

            if(c_end < best_cost)
            {
                best_cost = c_end;
                best_idx = i;
                best_reverse = true;
            }
        }

        if(best_idx == paths.size())
            break;

        CNCPathOwned elegido =
            paths[best_idx];

        if(best_reverse)
        {
            std::reverse(
                elegido.puntos.begin(),
                elegido.puntos.end()
            );
        }

        cursor = elegido.puntos.back();
        used[best_idx] = 1;
        plan.push_back(elegido);
    }

    CNCHandle handle;

    int ret = cnc_open(
        &handle,
        device
    );

    if(ret != CNC_OK)
    {
        fprintf(
            stderr,
            "[CNC] cnc_open fallo: %s\n",
            cnc_strerror(ret)
        );
        return ret;
    }

    if(speed > 0)
    {
        ret = cnc_set_speed(
            &handle,
            speed
        );

        if(ret != CNC_OK)
        {
            fprintf(
                stderr,
                "[CNC] cnc_set_speed fallo: %s\n",
                cnc_strerror(ret)
            );

            cnc_close(&handle);
            return ret;
        }
    }

    ret = cnc_home(&handle);

    if(ret != CNC_OK)
    {
        fprintf(
            stderr,
            "[CNC] cnc_home fallo: %s\n",
            cnc_strerror(ret)
        );

        cnc_close(&handle);
        return ret;
    }

    // Se ejecuta cada ruta ya optimizada sobre la CNC.
    for(size_t i = 0; i < plan.size(); i++)
    {
        const CNCPathOwned& p =
            plan[i];

        if(p.puntos.empty())
            continue;

        CNCPath draw_path;
        draw_path.points =
            const_cast<CNCPoint*>(
                p.puntos.data()
            );
        draw_path.count =
            p.puntos.size();

        ret = cnc_draw_path(
            &handle,
            &draw_path
        );

        if(ret != CNC_OK)
        {
            fprintf(
                stderr,
                "[CNC] cnc_draw_path fallo en path %lu: %s\n",
                (unsigned long)i,
                cnc_strerror(ret)
            );

            cnc_pen_up(&handle);
            cnc_close(&handle);
            return ret;
        }
    }

    cnc_pen_up(&handle);
    cnc_home(&handle);
    cnc_close(&handle);

    return CNC_OK;
}
