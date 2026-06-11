#include "esqueleto.h"

#include <opencv2/ximgproc.hpp>

cv::Mat generar_esqueleto(
    const cv::Mat& binary_image
)
{
    cv::Mat inverted;

    // thinning espera fondo negro y objeto blanco, por eso se invierte antes.

    cv::bitwise_not(
        binary_image,
        inverted
    );

    cv::Mat skeleton;

    cv::ximgproc::thinning(
        inverted,
        skeleton,
        cv::ximgproc::THINNING_GUOHALL
    );

    return skeleton;
}