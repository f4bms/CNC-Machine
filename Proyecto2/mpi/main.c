#include <mpi.h>
#include <opencv2/opencv.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <vector>

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

    for(size_t i = 0;
        i < local_buffer.size();
        i++)
    {
        suma_local += local_buffer[i];
    }

    printf(
        "Rank %d suma local = %lld\n",
        rank,
        suma_local
    );

    /*
     * MPI suma automáticamente los resultados
     * de todos los ranks y almacena el total
     * únicamente en Rank 0.
     */
    long long suma_global = 0;

    MPI_Reduce(
        &suma_local,
        &suma_global,
        1,
        MPI_LONG_LONG,
        MPI_SUM,
        0,
        MPI_COMM_WORLD
    );

    /*
     * VALIDACIÓN DE INTEGRIDAD
     *
     * Rank 0 vuelve a recorrer la imagen
     * completa localmente y compara el resultado
     * contra la suma obtenida mediante MPI.
     */
    if(rank == 0)
    {
        long long referencia = 0;

        for(int r = 0;
            r < image.rows;
            r++)
        {
            for(int c = 0;
                c < image.cols;
                c++)
            {
                referencia +=
                    image.at<unsigned char>(r, c);
            }
        }

        printf("\n");
        printf("=====================================\n");
        printf("VALIDACION DE INTEGRIDAD\n");
        printf("=====================================\n");

        printf(
            "Referencia (imagen completa): %lld\n",
            referencia
        );

        printf(
            "MPI Reduce: %lld\n",
            suma_global
        );

        if(referencia == suma_global)
        {
            printf(
                "RESULTADO: VALIDACION OK\n"
            );
        }
        else
        {
            printf(
                "RESULTADO: ERROR EN DISTRIBUCION\n"
            );
        }

        printf(
            "=====================================\n\n"
        );
    }

    MPI_Finalize();

    return 0;
}