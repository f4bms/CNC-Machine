#ifndef DISTRIBUCION_H
#define DISTRIBUCION_H

#include <opencv2/opencv.hpp>
#include <vector>

#include "tipos.h"

/*
 * Calcula la distribución completa.
 */
DistribucionMPI crear_distribucion(
    int rows,
    int cols,
    int ranks,
    int overlap
);

/*
 * Imprime la distribución.
 */
void imprimir_distribucion(
    const DistribucionMPI& dist
);

/*
 * Envía dimensiones.
 */
void broadcast_dimensiones(
    int& rows,
    int& cols
);

/*
 * Distribuye filas.
 */
void scatter_fragmento(
    const cv::Mat& image,
    const DistribucionMPI& dist,
    int rank,
    int& local_rows,
    std::vector<unsigned char>& local_buffer
);

/*
 * Reconstruye imagen completa
 * eliminando overlap.
 */
void gather_fragmento(
    const cv::Mat& local_processed,
    const DistribucionMPI& dist,
    int rank,
    cv::Mat& final_image
);

#endif