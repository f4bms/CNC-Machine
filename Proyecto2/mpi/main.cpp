#include <mpi.h>

#include <opencv2/opencv.hpp>

#include <stdio.h>

#include <stdlib.h>
#include <string.h>

#include <vector>

#include "tipos.h"
#include "distribucion.h"
#include "procesamiento_imagen.h"
#include "esqueleto.h"
#include "grafo.h"
#include "cnc_traduccion.h"

#define OVERLAP_ROWS 10
#define DEFAULT_CNC_SPEED 200
#define DEFAULT_CNC_SCALE 1

static int parse_int(
    const char* s,
    int fallback
)
{
    if(s == NULL)
        return fallback;

    int v = atoi(s);

    if(v <= 0)
        return fallback;

    return v;
}

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
                "./mpi_processor imagen.png [--cnc-device /dev/gpio_device] [--cnc-speed 200] [--cnc-scale 1]\n"
            );
        }

        MPI_Finalize();
        return 1;
    }

    const char* cnc_device = NULL;
    int cnc_speed = DEFAULT_CNC_SPEED;
    int cnc_scale = DEFAULT_CNC_SCALE;

    for(int i = 2; i < argc; i++)
    {
        if(strcmp(argv[i], "--cnc-device") == 0 && i + 1 < argc)
        {
            cnc_device = argv[++i];
        }
        else if(strcmp(argv[i], "--cnc-speed") == 0 && i + 1 < argc)
        {
            cnc_speed = parse_int(
                argv[++i],
                DEFAULT_CNC_SPEED
            );
        }
        else if(strcmp(argv[i], "--cnc-scale") == 0 && i + 1 < argc)
        {
            cnc_scale = parse_int(
                argv[++i],
                DEFAULT_CNC_SCALE
            );
        }
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
     * continúa con el análisis
     * global del PCB.
     */

    if(rank == 0)
    {
        /*
         * Guardar esqueleto final.
         */

        cv::imwrite(
            "pcb_skeleton.png",
            final_image
        );

        printf(
            "\nEsqueleto distribuido guardado:\n"
            "pcb_skeleton.png\n"
        );

        /*
         * Generación del grafo.
         */

        Grafo grafo =
            generar_grafo(
                final_image
            );

        guardar_grafo_debug(
            final_image,
            grafo,
            "pcb_graph.png"
        );

        guardar_aristas_debug(
            grafo,
            "pcb_edges.txt"
        );

        std::vector<CNCPathOwned> cnc_paths =
            convertir_grafo_a_cnc_paths(
                grafo,
                cnc_scale
            );

        guardar_cnc_paths_debug(
            cnc_paths,
            "pcb_paths.txt"
        );

        printf(
            "Nodos detectados: %lu\n",
            (unsigned long)
            grafo.nodos.size()
        );

        printf(
            "Aristas detectadas: %lu\n",
            (unsigned long)
            grafo.aristas.size()
        );

        printf(
            "CNC paths generados: %lu\n",
            (unsigned long)
            cnc_paths.size()
        );

        printf(
            "Grafo guardado:\n"
            "pcb_graph.png\n"
        );

        printf(
            "Debug rutas guardado:\n"
            "pcb_edges.txt\n"
            "pcb_paths.txt\n"
        );

        if(cnc_device != NULL)
        {
            printf(
                "Ejecutando cnc_lib en device: %s\n",
                cnc_device
            );

            int cnc_ret = ejecutar_cnc_paths(
                cnc_paths,
                cnc_device,
                cnc_speed
            );

            if(cnc_ret != CNC_OK)
            {
                printf(
                    "Error ejecutando cnc_lib\n"
                );
            }
            else
            {
                printf(
                    "Trazado CNC completado\n"
                );
            }
        }
        else
        {
            printf(
                "Ejecucion de hardware omitida (use --cnc-device para habilitar)\n"
            );
        }

        /*
         * Próxima etapa:
         *
            * Siguiente etapa sugerida:
            * optimizar orden de paths
            * para reducir travel.
         */
    }

    MPI_Finalize();

    return 0;
}