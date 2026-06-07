#ifndef PROCESAMIENTO_IMAGEN_H
#define PROCESAMIENTO_IMAGEN_H

#include <opencv2/opencv.hpp>

/*
 * Procesa un fragmento de imagen.
 *
 * Pipeline actual:
 *
 * Imagen gris
 *      ↓
 * Adaptive Threshold
 *      ↓
 * Morphological Close
 *      ↓
 * Resultado binario limpio
 */
cv::Mat procesar_fragmento(
    const cv::Mat& imagen
);

/*
 * Utilidad para guardar imágenes
 * durante depuración.
 */
void guardar_imagen_debug(
    const cv::Mat& imagen,
    const char* nombre
);

#endif