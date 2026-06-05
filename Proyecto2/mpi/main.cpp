#include <mpi.h>

#include <opencv2/opencv.hpp>

#include <stdio.h>

#include <vector>

#include "tipos.h"
#include "distribucion.h"
#include "procesamiento_imagen.h"

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
     * Rank 0 carga imagen.
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
     * Crear distribución.
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
     * Distribución MPI.
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
     * Reconstruir fragmento local.
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
     * Opcional:
     * guardar resultado local.
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
     * Guardado final.
     */

    if(rank == 0)
    {
        cv::imwrite(
            "pcb_binary.png",
            final_image
        );

        printf(
            "\nResultado guardado:\n"
            "pcb_binary.png\n"
        );
    }

    MPI_Finalize();

    return 0;
}