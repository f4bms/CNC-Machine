#ifndef PROCESAMIENTO_IMAGEN_H
#define PROCESAMIENTO_IMAGEN_H

#include <opencv2/opencv.hpp>

/*
 * Genera imagen binaria a partir
 * de una fotografía o escaneo de PCB.
 */
cv::Mat generar_binario(
    const cv::Mat& imagen
);

/*
 * Guarda imágenes de depuración.
 */
void guardar_imagen_debug(
    const cv::Mat& imagen,
    const char* nombre
);

#endif