#ifndef DISTRIBUCION_H
#define DISTRIBUCION_H

#include <opencv2/opencv.hpp>
#include <vector>

#include "tipos.h"

// Divide la imagen entre ranks y coordina scatter/gather con overlap.

DistribucionMPI crear_distribucion(
    int rows,
    int cols,
    int ranks,
    int overlap
);

void imprimir_distribucion(
    const DistribucionMPI& dist
);

void broadcast_dimensiones(
    int& rows,
    int& cols
);

void scatter_fragmento(
    const cv::Mat& image,
    const DistribucionMPI& dist,
    int rank,
    int& local_rows,
    std::vector<unsigned char>& local_buffer
);

void gather_fragmento(
    const cv::Mat& local_processed,
    const DistribucionMPI& dist,
    int rank,
    cv::Mat& final_image
);

#endif