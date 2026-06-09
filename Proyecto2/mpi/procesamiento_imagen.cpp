#include "procesamiento_imagen.h"

#include <opencv2/opencv.hpp>

cv::Mat generar_binario(
    const cv::Mat& imagen
)
{
    /*
     * =====================================
     * Etapa 1
     * Reducción fuerte de ruido
     * =====================================
     */

    cv::Mat denoised;

    cv::fastNlMeansDenoising(
        imagen,
        denoised,
        30,
        7,
        21
    );

    /*
     * =====================================
     * Etapa 2
     * Threshold adaptativo
     * =====================================
     *
     * Pistas = negro
     * Fondo  = blanco
     */

    cv::Mat binary;

    cv::adaptiveThreshold(
        denoised,
        binary,
        255,
        cv::ADAPTIVE_THRESH_GAUSSIAN_C,
        cv::THRESH_BINARY_INV,
        91,
        -4
    );

    /*
     * =====================================
     * Etapa 3
     * Apertura ligera
     * =====================================
     *
     * Elimina puntos aislados.
     */

    cv::morphologyEx(
        binary,
        binary,
        cv::MORPH_OPEN,
        cv::getStructuringElement(
            cv::MORPH_RECT,
            cv::Size(2, 2)
        )
    );

    /*
     * =====================================
     * Etapa 4
     * Apertura adicional
     * =====================================
     */

    cv::Mat opened;

    cv::Mat kernel_open =
        cv::getStructuringElement(
            cv::MORPH_RECT,
            cv::Size(3, 3)
        );

    cv::morphologyEx(
        binary,
        opened,
        cv::MORPH_OPEN,
        kernel_open
    );

    /*
     * =====================================
     * Etapa 5
     * Cierre morfológico
     * =====================================
     *
     * Une pequeñas rupturas.
     */

    cv::Mat closed;

    cv::Mat kernel_close =
        cv::getStructuringElement(
            cv::MORPH_RECT,
            cv::Size(3, 3)
        );

    cv::morphologyEx(
        opened,
        closed,
        cv::MORPH_CLOSE,
        kernel_close
    );

    /*
     * =====================================
     * Etapa 6
     * Suavizado
     * =====================================
     */

    cv::Mat smoothed;

    cv::GaussianBlur(
        closed,
        smoothed,
        cv::Size(3, 3),
        0
    );

    /*
     * =====================================
     * Etapa 7
     * Re-binarización
     * =====================================
     */

    cv::Mat final_binary;

    cv::threshold(
        smoothed,
        final_binary,
        127,
        255,
        cv::THRESH_BINARY
    );

    return final_binary;
}

void guardar_imagen_debug(
    const cv::Mat& imagen,
    const char* nombre
)
{
    cv::imwrite(
        nombre,
        imagen
    );
}