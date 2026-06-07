#ifndef CONTORNOS_H
#define CONTORNOS_H

#include <opencv2/opencv.hpp>
#include <vector>

std::vector<std::vector<cv::Point>>
extraer_contornos(
    const cv::Mat& binary
);

void guardar_contornos_debug(
    const cv::Mat& binary,
    const std::vector<std::vector<cv::Point>>& contornos,
    const char* filename
);

#endif