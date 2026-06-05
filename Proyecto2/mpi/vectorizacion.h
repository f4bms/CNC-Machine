#ifndef VECTORIZACION_H
#define VECTORIZACION_H

#include <opencv2/opencv.hpp>
#include <vector>

std::vector<std::vector<cv::Point>>
vectorizar_contornos(
    const std::vector<std::vector<cv::Point>>& contornos,
    double epsilon_factor
);

void guardar_vectores_debug(
    cv::Size image_size,
    const std::vector<std::vector<cv::Point>>& vectores,
    const char* filename
);

#endif