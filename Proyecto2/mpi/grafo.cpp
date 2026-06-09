#include "grafo.h"

#include <opencv2/opencv.hpp>

#include <map>
#include <set>
#include <algorithm>
#include <stdio.h>

static int key_pixel(
    int x,
    int y,
    int cols
)
{
    return y * cols + x;
}

static bool dentro(
    const cv::Mat& img,
    int y,
    int x
)
{
    return x >= 0 &&
           y >= 0 &&
           x < img.cols &&
           y < img.rows;
}

static std::vector<cv::Point> vecinos_activos(
    const cv::Mat& img,
    const cv::Point& p
)
{
    std::vector<cv::Point> vecinos;

    for(int dy = -1; dy <= 1; dy++)
    {
        for(int dx = -1; dx <= 1; dx++)
        {
            if(dx == 0 && dy == 0)
                continue;

            int nx = p.x + dx;
            int ny = p.y + dy;

            if(!dentro(img, ny, nx))
                continue;

            if(img.at<unsigned char>(ny, nx) > 0)
            {
                vecinos.push_back(
                    cv::Point(nx, ny)
                );
            }
        }
    }

    return vecinos;
}

static int contar_vecinos(
    const cv::Mat& img,
    int y,
    int x
)
{
    int count = 0;

    for(int dy=-1; dy<=1; dy++)
    {
        for(int dx=-1; dx<=1; dx++)
        {
            if(dx==0 && dy==0)
                continue;

            int nx = x + dx;
            int ny = y + dy;

            if(nx < 0 || ny < 0)
                continue;

            if(nx >= img.cols || ny >= img.rows)
                continue;

            if(img.at<unsigned char>(ny,nx) > 0)
                count++;
        }
    }

    return count;
}

Grafo generar_grafo(
    const cv::Mat& skeleton
)
{
    Grafo g;

    std::map<int,int> pixel_to_node;

    int next_id = 0;

    for(int y=0; y<skeleton.rows; y++)
    {
        for(int x=0; x<skeleton.cols; x++)
        {
            if(skeleton.at<unsigned char>(y,x) == 0)
                continue;

            int vecinos =
                contar_vecinos(
                    skeleton,
                    y,
                    x
                );

            if(vecinos == 1 || vecinos >= 3)
            {
                Nodo n;

                n.id = next_id++;

                n.punto =
                    cv::Point(x,y);

                pixel_to_node[
                    y*skeleton.cols+x
                ] = n.id;

                g.nodos.push_back(n);
            }
        }
    }

    for(const auto& nodo : g.nodos)
    {
        const cv::Point inicio = nodo.punto;

        std::vector<cv::Point> vecinos =
            vecinos_activos(
                skeleton,
                inicio
            );

        for(const auto& primer : vecinos)
        {
            std::vector<cv::Point> trayectoria;

            trayectoria.push_back(
                inicio
            );

            trayectoria.push_back(
                primer
            );

            cv::Point previo = inicio;
            cv::Point actual = primer;

            int destino_id = -1;

            for(int guard = 0; guard < skeleton.rows * skeleton.cols; guard++)
            {
                int actual_key =
                    key_pixel(
                        actual.x,
                        actual.y,
                        skeleton.cols
                    );

                auto it_actual_nodo =
                    pixel_to_node.find(
                        actual_key
                    );

                if(it_actual_nodo != pixel_to_node.end() &&
                   it_actual_nodo->second != nodo.id)
                {
                    destino_id =
                        it_actual_nodo->second;
                    break;
                }

                std::vector<cv::Point> siguientes =
                    vecinos_activos(
                        skeleton,
                        actual
                    );

                std::vector<cv::Point> candidatos;

                for(const auto& sig : siguientes)
                {
                    if(sig == previo)
                        continue;

                    candidatos.push_back(sig);
                }

                if(candidatos.empty())
                {
                    break;
                }

                if(candidatos.size() > 1)
                {
                    break;
                }

                previo = actual;
                actual = candidatos[0];

                trayectoria.push_back(
                    actual
                );
            }

            if(destino_id < 0)
            {
                continue;
            }

            if(nodo.id >= destino_id)
            {
                continue;
            }

            Arista a;
            a.origen = nodo.id;
            a.destino = destino_id;
            a.trayectoria = trayectoria;

            g.aristas.push_back(a);
        }
    }

    return g;
}

void guardar_aristas_debug(
    const Grafo& grafo,
    const char* filename
)
{
    FILE* fp = fopen(
        filename,
        "w"
    );

    if(fp == NULL)
        return;

    fprintf(
        fp,
        "# Aristas detectadas: %lu\n",
        (unsigned long)
        grafo.aristas.size()
    );

    for(size_t i = 0; i < grafo.aristas.size(); i++)
    {
        const Arista& a =
            grafo.aristas[i];

        fprintf(
            fp,
            "arista %lu: %d -> %d, puntos=%lu\n",
            (unsigned long)i,
            a.origen,
            a.destino,
            (unsigned long)
            a.trayectoria.size()
        );
    }

    fclose(fp);
}

void guardar_grafo_debug(
    const cv::Mat& skeleton,
    const Grafo& grafo,
    const char* filename
)
{
    cv::Mat debug;

    cv::cvtColor(
        skeleton,
        debug,
        cv::COLOR_GRAY2BGR
    );

    for(const auto& nodo : grafo.nodos)
    {
        cv::circle(
            debug,
            nodo.punto,
            4,
            cv::Scalar(0,0,255),
            -1
        );
    }

    cv::imwrite(
        filename,
        debug
    );
}