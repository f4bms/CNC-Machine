#include "distribucion.h"

#include <mpi.h>

#include <stdio.h>

DistribucionMPI crear_distribucion(
    int rows,
    int cols,
    int ranks,
    int overlap
)
{
    DistribucionMPI dist;

    dist.rows = rows;
    dist.cols = cols;
    dist.overlap = overlap;

    dist.local_rows.resize(ranks);
    dist.useful_rows.resize(ranks);

    dist.sendcounts.resize(ranks);
    dist.displs.resize(ranks);

    int base_rows = rows / ranks;
    int remainder = rows % ranks;

    int useful_offset = 0;

    for(int i = 0; i < ranks; i++)
    {
        int useful =
            base_rows +
            (i < remainder ? 1 : 0);

        dist.useful_rows[i] = useful;

        int local = useful;

        if(i > 0)
            local += overlap;

        if(i < ranks - 1)
            local += overlap;

        dist.local_rows[i] = local;

        int start_row =
            useful_offset -
            (i > 0 ? overlap : 0);

        dist.sendcounts[i] =
            local * cols;

        dist.displs[i] =
            start_row * cols;

        useful_offset += useful;
    }

    return dist;
}

void imprimir_distribucion(
    const DistribucionMPI& dist
)
{
    printf("\nDistribucion MPI\n\n");

    int useful_start = 0;

    int ranks =
        dist.local_rows.size();

    for(int i = 0; i < ranks; i++)
    {
        int useful_end =
            useful_start +
            dist.useful_rows[i] - 1;

        printf(
            "Rank %d\n"
            "  Filas utiles : %d -> %d (%d)\n"
            "  Filas reales : %d\n\n",
            i,
            useful_start,
            useful_end,
            dist.useful_rows[i],
            dist.local_rows[i]
        );

        useful_start +=
            dist.useful_rows[i];
    }
}

void broadcast_dimensiones(
    int& rows,
    int& cols
)
{
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
}

void scatter_fragmento(
    const cv::Mat& image,
    const DistribucionMPI& dist,
    int rank,
    int& local_rows,
    std::vector<unsigned char>& local_buffer
)
{
    MPI_Scatter(
        const_cast<int*>(
            dist.local_rows.data()
        ),
        1,
        MPI_INT,

        &local_rows,
        1,
        MPI_INT,

        0,
        MPI_COMM_WORLD
    );

    local_buffer.resize(
        local_rows * dist.cols
    );

    MPI_Scatterv(
        rank == 0
            ? image.data
            : NULL,

        const_cast<int*>(
            dist.sendcounts.data()
        ),

        const_cast<int*>(
            dist.displs.data()
        ),

        MPI_UNSIGNED_CHAR,

        local_buffer.data(),
        local_rows * dist.cols,

        MPI_UNSIGNED_CHAR,

        0,
        MPI_COMM_WORLD
    );
}

void gather_fragmento(
    const cv::Mat& local_processed,
    const DistribucionMPI& dist,
    int rank,
    cv::Mat& final_image
)
{
    int ranks =
        dist.local_rows.size();

    int useful_rows =
        dist.useful_rows[rank];

    int start_local = 0;

    if(rank > 0)
    {
        start_local =
            dist.overlap;
    }

    cv::Mat useful_region =
        local_processed.rowRange(
            start_local,
            start_local + useful_rows
        );

    std::vector<unsigned char> send_buffer(
        useful_region.data,
        useful_region.data +
        useful_region.total()
    );

    std::vector<int> recvcounts;
    std::vector<int> recvdispls;

    if(rank == 0)
    {
        recvcounts.resize(ranks);
        recvdispls.resize(ranks);

        int offset = 0;

        for(int i = 0; i < ranks; i++)
        {
            recvcounts[i] =
                dist.useful_rows[i] *
                dist.cols;

            recvdispls[i] =
                offset;

            offset +=
                recvcounts[i];
        }

        final_image =
            cv::Mat(
                dist.rows,
                dist.cols,
                CV_8UC1
            );
    }

    MPI_Gatherv(
        send_buffer.data(),

        useful_rows *
        dist.cols,

        MPI_UNSIGNED_CHAR,

        rank == 0
            ? final_image.data
            : NULL,

        rank == 0
            ? recvcounts.data()
            : NULL,

        rank == 0
            ? recvdispls.data()
            : NULL,

        MPI_UNSIGNED_CHAR,

        0,
        MPI_COMM_WORLD
    );
}