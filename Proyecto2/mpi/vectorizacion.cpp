#include "vectorizacion.h"

std::vector<std::vector<cv::Point>>
vectorizar_contornos(
    const std::vector<std::vector<cv::Point>>& contornos,
    double epsilon_factor
)
{
    std::vector<std::vector<cv::Point>> resultado;

    for(const auto& contorno : contornos)
    {
        double perimetro =
            cv::arcLength(
                contorno,
                true
            );

        double epsilon =
            epsilon_factor *
            perimetro;

        std::vector<cv::Point> aproximado;

        cv::approxPolyDP(
            contorno,
            aproximado,
            epsilon,
            true
        );

        resultado.push_back(
            aproximado
        );
    }

    return resultado;
}

void guardar_vectores_debug(
    cv::Size image_size,
    const std::vector<std::vector<cv::Point>>& vectores,
    const char* filename
)
{
    cv::Mat output =
        cv::Mat::zeros(
            image_size,
            CV_8UC3
        );

    cv::drawContours(
        output,
        vectores,
        -1,
        cv::Scalar(255,255,255),
        1
    );

    cv::imwrite(
        filename,
        output
    );
}