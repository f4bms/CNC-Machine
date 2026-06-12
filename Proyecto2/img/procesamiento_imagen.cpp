// Modulo de procesamiento de imagen del PCB.
// Unica capa que usa OpenCV/C++; expone una interfaz C al codigo distribuido.
#include "procesamiento_imagen.h"

#include <opencv2/opencv.hpp>
#include <opencv2/ximgproc.hpp>

#include <sys/stat.h>
#include <errno.h>
#include <string.h>

// ---- Pipeline OpenCV interno --------------------------------------------

static cv::Mat generar_binario(
    const cv::Mat& imagen
)
{
    // Etapa 1: reduccion fuerte de ruido.
    cv::Mat denoised;
    cv::fastNlMeansDenoising(imagen, denoised, 30, 7, 21);

    // Etapa 2: threshold adaptativo; las pistas quedan negras sobre blanco.
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

    // Etapa 3: apertura ligera para eliminar puntos aislados.
    cv::morphologyEx(
        binary,
        binary,
        cv::MORPH_OPEN,
        cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2, 2))
    );

    // Etapa 4: apertura adicional para limpiar artefactos pequenos.
    cv::Mat opened;
    cv::morphologyEx(
        binary,
        opened,
        cv::MORPH_OPEN,
        cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3))
    );

    // Etapa 5: cierre morfologico para unir pequenas rupturas.
    cv::Mat closed;
    cv::morphologyEx(
        opened,
        closed,
        cv::MORPH_CLOSE,
        cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3))
    );

    // Etapa 6: suavizado previo a la binarizacion final.
    cv::Mat smoothed;
    cv::GaussianBlur(closed, smoothed, cv::Size(3, 3), 0);

    // Etapa 7: rebinarizacion final.
    cv::Mat final_binary;
    cv::threshold(smoothed, final_binary, 127, 255, cv::THRESH_BINARY);

    return final_binary;
}

static cv::Mat generar_esqueleto(
    const cv::Mat& binary_image
)
{
    cv::Mat inverted;

    // thinning espera fondo negro y objeto blanco, por eso se invierte antes.
    cv::bitwise_not(binary_image, inverted);

    cv::Mat skeleton;
    cv::ximgproc::thinning(inverted, skeleton, cv::ximgproc::THINNING_GUOHALL);

    return skeleton;
}

static void ensure_output_dir(
    const char* dir
)
{
    // Crea la carpeta de salida si no existe (rank_*.png y depuracion).
    if(dir == NULL)
        return;

    if(mkdir(dir, 0775) != 0 && errno != EEXIST)
    {
        // No es fatal; OpenCV reportara si la escritura falla.
    }
}

// ---- Interfaz C ----------------------------------------------------------

extern "C" int img_load_grayscale(
    const char* path,
    unsigned char** out_buffer,
    int* out_rows,
    int* out_cols
)
{
    // Punto de entrada desde C: carga imagen completa en un buffer lineal.
    if(path == NULL || out_buffer == NULL || out_rows == NULL || out_cols == NULL)
        return -1;

    cv::Mat image = cv::imread(path, cv::IMREAD_GRAYSCALE);

    if(image.empty())
        return -1;

    // Garantiza layout continuo para copiar con memcpy.
    if(!image.isContinuous())
        image = image.clone();

    size_t total = (size_t)image.rows * (size_t)image.cols;

    *out_buffer = (unsigned char*)malloc(total);

    if(*out_buffer == NULL)
        return -1;

    memcpy(*out_buffer, image.data, total);

    *out_rows = image.rows;
    *out_cols = image.cols;

    return 0;
}

extern "C" int img_save_grayscale(
    const char* path,
    const unsigned char* buffer,
    int rows,
    int cols
)
{
    // Punto de salida desde C: persiste un buffer lineal como PNG/JPG.
    if(path == NULL || buffer == NULL || rows <= 0 || cols <= 0)
        return -1;

    cv::Mat image(rows, cols, CV_8UC1, (void*)buffer);

    return cv::imwrite(path, image) ? 0 : -1;
}

extern "C" int img_process_fragment(
    const unsigned char* input,
    int rows,
    int cols,
    int rank,
    const char* output_dir,
    unsigned char** out_skeleton
)
{
    // Esta funcion corre en cada rank sobre su fragmento local.
    if(input == NULL || out_skeleton == NULL || rows <= 0 || cols <= 0)
        return -1;

    // Si no se pasa output_dir usa ../outputs como destino por defecto.
    const char* dir = (output_dir != NULL && output_dir[0] != '\0')
                          ? output_dir
                          : "../outputs";

    ensure_output_dir(dir);

    cv::Mat local_image(rows, cols, CV_8UC1, (void*)input);

    // Binariza el fragmento local para detectar pistas del PCB.
    cv::Mat local_binary = generar_binario(local_image);

    // Guarda debug por rank para revisar calidad de segmentacion.
    char filename[1024];

    snprintf(filename, sizeof(filename), "%s/rank_%d_binary.png", dir, rank);
    cv::imwrite(filename, local_binary);

    // Esqueletiza el binario local antes del reensamble global.
    cv::Mat local_skeleton = generar_esqueleto(local_binary);

    snprintf(filename, sizeof(filename), "%s/rank_%d_skeleton.png", dir, rank);
    cv::imwrite(filename, local_skeleton);

    // Fuerza continuidad para exportar a buffer C sin strides.
    if(!local_skeleton.isContinuous())
        local_skeleton = local_skeleton.clone();

    size_t total = (size_t)rows * (size_t)cols;

    *out_skeleton = (unsigned char*)malloc(total);

    if(*out_skeleton == NULL)
        return -1;

    memcpy(*out_skeleton, local_skeleton.data, total);

    return 0;
}

extern "C" int img_save_graph_debug(
    const char* path,
    const unsigned char* skeleton,
    int rows,
    int cols,
    const Grafo* grafo
)
{
    // Dibuja nodos en rojo sobre el esqueleto para inspeccion visual.
    if(path == NULL || skeleton == NULL || grafo == NULL || rows <= 0 || cols <= 0)
        return -1;

    cv::Mat sk(rows, cols, CV_8UC1, (void*)skeleton);

    cv::Mat debug;
    cv::cvtColor(sk, debug, cv::COLOR_GRAY2BGR);

    for(int i = 0; i < grafo->node_count; i++)
    {
        cv::circle(
            debug,
            cv::Point(grafo->nodes[i].point.x, grafo->nodes[i].point.y),
            4,
            cv::Scalar(0, 0, 255),
            -1
        );
    }

    return cv::imwrite(path, debug) ? 0 : -1;
}
