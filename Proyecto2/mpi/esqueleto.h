#ifndef ESQUELETO_H
#define ESQUELETO_H

#include <opencv2/opencv.hpp>

cv::Mat generar_esqueleto(
    const cv::Mat& binary_image
);

#endif