#include <mpi.h>
#include <opencv2/opencv.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "procesamiento_imagen.h"

int main(int argc, char *argv[])
{
    /*
     * Inicializa el entorno MPI.
     * Todos los procesos deben llamar MPI_Init().
     */
    MPI_Init(&argc, &argv);

    int rank;
    int size;

    /*
     * rank = identificador único del proceso actual.
     * size = cantidad total de procesos MPI.
     */
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    /*
     * Verifica que se haya recibido una ruta de imagen.
     */
    if(argc < 2)
    {
        if(rank == 0)
        {
            printf("Uso:\n");
            printf("./mpi_processor imagen.png\n");
        }

        MPI_Finalize();
        return 1;
    }

    cv::Mat image;

    int rows = 0;
    int cols = 0;

    /*
     * Solamente Rank 0 carga la imagen completa.
     */
    if(rank == 0)
    {
        image = cv::imread(
            argv[1],
            cv::IMREAD_GRAYSCALE
        );

        if(image.empty())
        {
            printf("Error cargando imagen\n");

            MPI_Abort(
                MPI_COMM_WORLD,
                1
            );
        }

        rows = image.rows;
        cols = image.cols;

        printf(
            "\nImagen cargada: %d x %d\n\n",
            rows,
            cols
        );
    }

    /*
     * Se envían dimensiones de la imagen a todos los procesos.
     */
    MPI_Bcast(
        &rows,
        1,
        MPI_INT,
        0,
        MPI_COMM_WORLD
    );

    MPI_Bcast(
        &cols,
        1,
        MPI_INT,
        0,
        MPI_COMM_WORLD
    );

    /*
     * Estructuras necesarias para Scatterv.
     */
    std::vector<int> rows_per_rank(size);
    std::vector<int> sendcounts(size);
    std::vector<int> displs(size);

    /*
     * División equilibrada de filas.
     */
    int base_rows = rows / size;
    int remainder = rows % size;

    int offset_rows = 0;

    /*
     * Rank 0 calcula cuánto recibe cada proceso.
     */
    if(rank == 0)
    {
        for(int i = 0; i < size; i++)
        {
            rows_per_rank[i] =
                base_rows +
                (i < remainder ? 1 : 0);

            sendcounts[i] =
                rows_per_rank[i] * cols;

            displs[i] =
                offset_rows * cols;

            offset_rows +=
                rows_per_rank[i];
        }

        printf("Distribucion:\n\n");

        int inicio = 0;

        for(int i = 0; i < size; i++)
        {
            int fin =
                inicio +
                rows_per_rank[i] - 1;

            printf(
                "Rank %d -> filas %d a %d (%d filas)\n",
                i,
                inicio,
                fin,
                rows_per_rank[i]
            );

            inicio +=
                rows_per_rank[i];
        }

        printf("\n");
    }

    /*
     * Cada rank recibe cuántas filas le corresponden.
     */
    int local_rows;

    MPI_Scatter(
        rows_per_rank.data(),
        1,
        MPI_INT,

        &local_rows,
        1,
        MPI_INT,

        0,
        MPI_COMM_WORLD
    );

    /*
     * Buffer local donde se almacenará
     * el fragmento de imagen asignado.
     */
    std::vector<unsigned char> local_buffer(
        local_rows * cols
    );

    /*
     * Distribución real de los pixeles.
     */
    MPI_Scatterv(
        rank == 0 ? image.data : NULL,

        sendcounts.data(),
        displs.data(),

        MPI_UNSIGNED_CHAR,

        local_buffer.data(),
        local_rows * cols,

        MPI_UNSIGNED_CHAR,

        0,
        MPI_COMM_WORLD
    );

    /*
    * Convierte el buffer recibido por MPI
    * en una imagen OpenCV local.
    *
    * No se copian datos.
    * OpenCV utiliza directamente
    * la memoria de local_buffer.
    */
    cv::Mat local_image(
        local_rows,
        cols,
        CV_8UC1,
        local_buffer.data()
    );

    /*
    * Procesamiento local.
    *
    * Convierte la región asignada
    * a una imagen binaria.
    */

    cv::Mat binary;

    cv::adaptiveThreshold(
        local_image,
        binary,
        255,
        cv::ADAPTIVE_THRESH_GAUSSIAN_C,
        cv::THRESH_BINARY,
        31,
        5
    );

    // Limpia el resultado
    cv::Mat cleaned;

    cv::Mat kernel =
        cv::getStructuringElement(
            cv::MORPH_RECT,
            cv::Size(3,3)
        );

    cv::morphologyEx(
        binary,
        cleaned,
        cv::MORPH_CLOSE,
        kernel
    );

    // Guarda el fragmento recibido por cada rank de nodo
    char filename[64];

    sprintf(
        filename,
        "rank_%d_original.png",
        rank
    );

    cv::imwrite(
        filename,
        local_image
    );

    /*
     * Verificación básica.
     */
    printf(
        "Rank %d recibio %d filas (%lu bytes)\n",
        rank,
        local_rows,
        (unsigned long)local_buffer.size()
    );

    /*
     * Cada proceso suma las intensidades
     * de todos los pixeles que recibió.
     */
    long long suma_local = 0;

    /*
    * Buffer para enviar el resultado
    * procesado de cada rank.
    */

    std::vector<unsigned char> binary_buffer(
        binary.data,
        binary.data +
        binary.total()
    );

    /*
    * Solo Rank 0 almacenará
    * la imagen reconstruida.
    */
    std::vector<unsigned char> reconstructed;

    if(rank == 0)
    {
        reconstructed.resize(
            rows * cols
        );
    }

    MPI_Gatherv(
        binary_buffer.data(),
        local_rows * cols,
        MPI_UNSIGNED_CHAR,

        rank == 0
            ? reconstructed.data()
            : NULL,

        sendcounts.data(),
        displs.data(),

        MPI_UNSIGNED_CHAR,

        0,
        MPI_COMM_WORLD
    );

    if(rank == 0)
    {
        cv::Mat final_image(
            rows,
            cols,
            CV_8UC1,
            reconstructed.data()
        );

        cv::imwrite(
            "pcb_binary.png",
            final_image
        );

        printf(
            "\nImagen reconstruida guardada:\n"
            "pcb_binary.png\n"
        );
    }

    MPI_Finalize();

    return 0;
}