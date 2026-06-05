#include <mpi.h>

#include <opencv2/opencv.hpp>

#include <stdio.h>

#include <vector>

#include "tipos.h"
#include "distribucion.h"
#include "procesamiento_imagen.h"
#include "contornos.h"
#include "vectorizacion.h"
#include "esqueleto.h"

#define OVERLAP_ROWS 10

int main(
    int argc,
    char *argv[]
)
{
    MPI_Init(
        &argc,
        &argv
    );

    int rank;
    int size;

    MPI_Comm_rank(
        MPI_COMM_WORLD,
        &rank
    );

    MPI_Comm_size(
        MPI_COMM_WORLD,
        &size
    );

    /*
     * Verifica argumentos.
     */

    if(argc < 2)
    {
        if(rank == 0)
        {
            printf(
                "Uso:\n"
                "./mpi_processor imagen.png\n"
            );
        }

        MPI_Finalize();
        return 1;
    }

    cv::Mat image;

    int rows = 0;
    int cols = 0;

    /*
     * Rank 0 carga imagen completa.
     */

    if(rank == 0)
    {
        image = cv::imread(
            argv[1],
            cv::IMREAD_GRAYSCALE
        );

        if(image.empty())
        {
            printf(
                "Error cargando imagen\n"
            );

            MPI_Abort(
                MPI_COMM_WORLD,
                1
            );
        }

        rows = image.rows;
        cols = image.cols;

        printf(
            "\nImagen cargada: %d x %d\n",
            rows,
            cols
        );
    }

    /*
     * Todos reciben dimensiones.
     */

    broadcast_dimensiones(
        rows,
        cols
    );

    /*
     * Crear distribución MPI.
     */

    DistribucionMPI dist =
        crear_distribucion(
            rows,
            cols,
            size,
            OVERLAP_ROWS
        );

    if(rank == 0)
    {
        imprimir_distribucion(
            dist
        );
    }

    /*
     * Distribuir imagen.
     */

    int local_rows = 0;

    std::vector<unsigned char>
        local_buffer;

    scatter_fragmento(
        image,
        dist,
        rank,
        local_rows,
        local_buffer
    );

    printf(
        "Rank %d recibio %d filas\n",
        rank,
        local_rows
    );

    /*
     * Reconstruye imagen local.
     */

    cv::Mat local_image(
        local_rows,
        cols,
        CV_8UC1,
        local_buffer.data()
    );

    /*
     * Procesamiento local.
     */

    cv::Mat processed =
        procesar_fragmento(
            local_image
        );

    /*
     * Debug por rank.
     */

    char filename[64];

    sprintf(
        filename,
        "rank_%d_processed.png",
        rank
    );

    guardar_imagen_debug(
        processed,
        filename
    );

    /*
     * Reconstrucción global.
     */

    cv::Mat final_image;

    gather_fragmento(
        processed,
        dist,
        rank,
        final_image
    );

    /*
     * Solamente Rank 0 continúa
     * con análisis posterior.
     */

    if(rank == 0)
    {
        /*
         * Imagen binaria reconstruida.
         */

        cv::imwrite(
            "pcb_binary.png",
            final_image
        );

        cv::Mat skeleton =
            generar_esqueleto(
                final_image
            );

        cv::imwrite(
            "pcb_skeleton.png",
            skeleton
        );

        printf(
            "Esqueleto guardado:\n"
            "pcb_skeleton.png\n"
        );

        printf(
            "\nImagen binaria guardada:\n"
            "pcb_binary.png\n"
        );

        /*
         * OpenCV encuentra objetos blancos
         * sobre fondo negro.
         *
         * La salida actual del procesamiento es:
         *
         * Fondo  = blanco
         * Pistas = negro
         *
         * Por eso se invierte.
         */

        cv::Mat contours_input;

        cv::bitwise_not(
            final_image,
            contours_input
        );

        cv::imwrite(
            "pcb_inverted.png",
            contours_input
        );

        /*
         * Extracción de contornos.
         */

        auto contornos =
            extraer_contornos(
                contours_input
            );

        printf(
            "Contornos encontrados: %lu\n",
            (unsigned long)
            contornos.size()
        );

        guardar_contornos_debug(
            contours_input,
            contornos,
            "pcb_contours.png"
        );

        /*
         * Simplificación geométrica.
         */

        auto vectores =
            vectorizar_contornos(
                contornos,
                0.001
            );

        guardar_vectores_debug(
            contours_input.size(),
            vectores,
            "pcb_vectors.png"
        );

        printf(
            "Contornos guardados:\n"
            "pcb_contours.png\n"
        );

        printf(
            "Vectores guardados:\n"
            "pcb_vectors.png\n"
        );
    }

    MPI_Finalize();

    return 0;
}