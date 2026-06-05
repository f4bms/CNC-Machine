#include "procesamiento_imagen.h"

#include <opencv2/opencv.hpp>

cv::Mat procesar_fragmento(
    const cv::Mat& imagen
)
{
    /*
     * =====================================
     * Etapa 1
     * Eliminación de ruido impulsivo
     * =====================================
     */

    cv::Mat filtered;

    cv::medianBlur(
        imagen,
        filtered,
        5
    );

    /*
     * =====================================
     * Etapa 2
     * Binarización adaptativa
     * =====================================
     *
     * Funciona mejor que threshold fijo
     * para fotografías tomadas con celular.
     */

    cv::Mat binary;

    cv::adaptiveThreshold(
        filtered,
        binary,
        255,
        cv::ADAPTIVE_THRESH_GAUSSIAN_C,
        cv::THRESH_BINARY,
        31,
        2
    );

    /*
     * =====================================
     * Etapa 3
     * Apertura morfológica
     * =====================================
     *
     * Elimina pequeños puntos aislados.
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
     * Etapa 4
     * Cierre morfológico
     * =====================================
     *
     * Une pequeñas discontinuidades
     * en las pistas.
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
     * Etapa 5
     * Suavizado ligero de bordes
     * =====================================
     *
     * Reduce pequeñas irregularidades
     * antes de extraer contornos.
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
     * Etapa 6
     * Re-binarización
     * =====================================
     *
     * GaussianBlur introduce niveles
     * intermedios de gris.
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