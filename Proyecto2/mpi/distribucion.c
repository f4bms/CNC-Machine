#include "distribucion.h"

#include <mpi.h>

#include <stdio.h>
#include <stdlib.h>

int crear_distribucion(
    int rows,
    int cols,
    int ranks,
    int overlap,
    DistribucionMPI *out_dist)
{
    // Reserva y calcula la geometria de reparto para Scatterv/Gatherv.
    if (out_dist == NULL || ranks <= 0)
        return -1;

    out_dist->rows = rows;
    out_dist->cols = cols;
    out_dist->overlap = overlap;
    out_dist->ranks = ranks;

    out_dist->local_rows = (int *)calloc((size_t)ranks, sizeof(int));
    out_dist->useful_rows = (int *)calloc((size_t)ranks, sizeof(int));
    out_dist->sendcounts = (int *)calloc((size_t)ranks, sizeof(int));
    out_dist->displs = (int *)calloc((size_t)ranks, sizeof(int));

    if (out_dist->local_rows == NULL ||
        out_dist->useful_rows == NULL ||
        out_dist->sendcounts == NULL ||
        out_dist->displs == NULL)
    {
        liberar_distribucion(out_dist);
        return -1;
    }

    // Distribucion balanceada: algunos ranks reciben una fila extra.
    int base_rows = rows / ranks;
    int remainder = rows % ranks;

    int useful_offset = 0;

    for (int i = 0; i < ranks; i++)
    {
        int useful =
            base_rows +
            (i < remainder ? 1 : 0);

        out_dist->useful_rows[i] = useful;

        int local = useful;

        if (i > 0)
            local += overlap;

        if (i < ranks - 1)
            local += overlap;

        out_dist->local_rows[i] = local;

        // start_row incluye overlap superior para ranks > 0.
        int start_row =
            useful_offset -
            (i > 0 ? overlap : 0);

        out_dist->sendcounts[i] = local * cols;
        out_dist->displs[i] = start_row * cols;

        useful_offset += useful;
    }

    return 0;
}

void liberar_distribucion(
    DistribucionMPI *dist)
{
    if (dist == NULL)
        return;

    free(dist->local_rows);
    free(dist->useful_rows);
    free(dist->sendcounts);
    free(dist->displs);

    dist->local_rows = NULL;
    dist->useful_rows = NULL;
    dist->sendcounts = NULL;
    dist->displs = NULL;
    dist->ranks = 0;
}

void imprimir_distribucion(
    const DistribucionMPI *dist)
{
    printf(
        "\nDistribucion MPI\n\n");

    int useful_start = 0;

    for (int i = 0; i < dist->ranks; i++)
    {
        int useful_end =
            useful_start +
            dist->useful_rows[i] - 1;

        printf(
            "Rank %d\n"
            "  Filas utiles : %d -> %d (%d)\n"
            "  Filas reales : %d\n\n",
            i,
            useful_start,
            useful_end,
            dist->useful_rows[i],
            dist->local_rows[i]);

        useful_start += dist->useful_rows[i];
    }
}

void broadcast_dimensiones(
    int *rows,
    int *cols)
{
    // rank 0 publica rows/cols para que todos procesen con el mismo tamano.
    MPI_Bcast(
        rows,
        1,
        MPI_INT,
        0,
        MPI_COMM_WORLD);

    MPI_Bcast(
        cols,
        1,
        MPI_INT,
        0,
        MPI_COMM_WORLD);
}

int scatter_fragmento(
    const unsigned char *image,
    const DistribucionMPI *dist,
    int rank,
    int *local_rows,
    unsigned char **local_buffer)
{
    // Primero reparte solo el numero de filas reales por rank.
    MPI_Scatter(
        dist->local_rows,
        1,
        MPI_INT,

        local_rows,
        1,
        MPI_INT,

        0,
        MPI_COMM_WORLD);

    // Luego reserva el buffer exacto para su fragmento local.
    size_t count = (size_t)(*local_rows) * (size_t)dist->cols;

    *local_buffer = (unsigned char *)malloc(count);

    if (*local_buffer == NULL)
        return -1;

    // Finalmente reparte pixeles usando sendcounts/displs precomputados.
    MPI_Scatterv(
        rank == 0
            ? (void *)image
            : NULL,

        dist->sendcounts,
        dist->displs,

        MPI_UNSIGNED_CHAR,

        *local_buffer,

        (*local_rows) * dist->cols,

        MPI_UNSIGNED_CHAR,

        0,
        MPI_COMM_WORLD);

    return 0;
}

int gather_fragmento(
    const unsigned char *local_processed,
    const DistribucionMPI *dist,
    int rank,
    unsigned char **final_image)
{
    // Cada rank solo envia su region util (sin overlap duplicado).
    int useful_rows = dist->useful_rows[rank];

    int start_local = 0;

    if (rank > 0)
        start_local = dist->overlap;

    // Region util (sin overlap) que aporta este rank.
    const unsigned char *send_ptr =
        local_processed + (size_t)start_local * (size_t)dist->cols;

    int *recvcounts = NULL;
    int *recvdispls = NULL;

    if (rank == 0)
    {
        // rank 0 prepara buffers de recepcion para recomponer la imagen final.
        recvcounts = (int *)calloc((size_t)dist->ranks, sizeof(int));
        recvdispls = (int *)calloc((size_t)dist->ranks, sizeof(int));

        if (recvcounts == NULL || recvdispls == NULL)
        {
            free(recvcounts);
            free(recvdispls);
            return -1;
        }

        int offset = 0;

        for (int i = 0; i < dist->ranks; i++)
        {
            recvcounts[i] = dist->useful_rows[i] * dist->cols;
            recvdispls[i] = offset;
            offset += recvcounts[i];
        }

        *final_image =
            (unsigned char *)malloc((size_t)dist->rows * (size_t)dist->cols);

        if (*final_image == NULL)
        {
            free(recvcounts);
            free(recvdispls);
            return -1;
        }
    }

    // Gatherv ensambla el esqueleto final en el rank 0.
    MPI_Gatherv(
        (void *)send_ptr,

        useful_rows * dist->cols,

        MPI_UNSIGNED_CHAR,

        rank == 0 ? *final_image : NULL,

        recvcounts,
        recvdispls,

        MPI_UNSIGNED_CHAR,

        0,
        MPI_COMM_WORLD);

    if (rank == 0)
    {
        free(recvcounts);
        free(recvdispls);
    }

    return 0;
}
