#include <mpi.h>

#include <opencv2/opencv.hpp>

#include <stdio.h>

#include <vector>

#include "tipos.h"
#include "distribucion.h"
#include "procesamiento_imagen.h"
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
     * Generación distribuida
     * de imagen binaria.
     */

    cv::Mat local_binary =
        generar_binario(
            local_image
        );

    /*
     * Debug binario local.
     */

    char filename[64];

    sprintf(
        filename,
        "rank_%d_binary.png",
        rank
    );

    guardar_imagen_debug(
        local_binary,
        filename
    );

    /*
     * Generación distribuida
     * del esqueleto.
     */

    cv::Mat local_skeleton =
        generar_esqueleto(
            local_binary
        );

    /*
     * Debug esqueleto local.
     */

    sprintf(
        filename,
        "rank_%d_skeleton.png",
        rank
    );

    guardar_imagen_debug(
        local_skeleton,
        filename
    );

    /*
     * Reconstrucción global
     * del esqueleto.
     */

    cv::Mat final_image;

    gather_fragmento(
        local_skeleton,
        dist,
        rank,
        final_image
    );

    /*
     * Solamente Rank 0
     * guarda el resultado final.
     */

    if(rank == 0)
    {
        cv::imwrite(
            "pcb_skeleton.png",
            final_image
        );

        printf(
            "\nEsqueleto distribuido guardado:\n"
            "pcb_skeleton.png\n"
        );

        /*
         * Próxima etapa:
         *
         * Construcción del grafo
         * a partir de final_image.
         */
    }

    MPI_Finalize();

    return 0;
}