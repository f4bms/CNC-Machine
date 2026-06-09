#ifndef GRAFO_H
#define GRAFO_H

#include <opencv2/opencv.hpp>

#include <vector>

struct Nodo
{
    int id;

    cv::Point punto;
};

struct Arista
{
    int origen;

    int destino;

    std::vector<cv::Point> trayectoria;
};

struct Grafo
{
    std::vector<Nodo> nodos;

    std::vector<Arista> aristas;
};

Grafo generar_grafo(
    const cv::Mat& skeleton
);

/*
 * Exporta aristas como texto para depurar
 * la vectorizacion de trayectorias.
 */
void guardar_aristas_debug(
    const Grafo& grafo,
    const char* filename
);

void guardar_grafo_debug(
    const cv::Mat& skeleton,
    const Grafo& grafo,
    const char* filename
);

#endif