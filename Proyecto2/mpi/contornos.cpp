#include "contornos.h"

std::vector<std::vector<cv::Point>>
extraer_contornos(
    const cv::Mat& binary
)
{
    std::vector<std::vector<cv::Point>> contornos;

    std::vector<cv::Vec4i> hierarchy;

    cv::findContours(
        binary,
        contornos,
        hierarchy,
        cv::RETR_LIST,
        cv::CHAIN_APPROX_NONE
    );

    /*
     * Filtra contornos pequeños.
     */
    std::vector<std::vector<cv::Point>> filtrados;

    for(const auto& c : contornos)
    {
        double area =
            cv::contourArea(c);

        if(area >= 50.0)
        {
            filtrados.push_back(c);
        }
    }

    return filtrados;
}

void guardar_contornos_debug(
    const cv::Mat& binary,
    const std::vector<std::vector<cv::Point>>& contornos,
    const char* filename
)
{
    cv::Mat output =
        cv::Mat::zeros(
            binary.size(),
            CV_8UC3
        );

    cv::drawContours(
        output,
        contornos,
        -1,
        cv::Scalar(255,255,255),
        1
    );

    cv::imwrite(
        filename,
        output
    );
}