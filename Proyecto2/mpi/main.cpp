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

    // Verifica que exista al menos una imagen de entrada.

    if(argc < 2)
    {
        if(rank == 0)
        {
            printf(
                "Uso:\n"
                "./mpi_processor imagen.png [--cnc-device /dev/gpio_device] [--cnc-scale 1]\n"
            );
        }

        MPI_Finalize();
        return 1;
    }

    const char* cnc_device = NULL;
    int cnc_scale = DEFAULT_CNC_SCALE;

    for(int i = 2; i < argc; i++)
    {
        if(strcmp(argv[i], "--cnc-device") == 0 && i + 1 < argc)
        {
            cnc_device = argv[++i];
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

    // Rank 0 carga la imagen original y define sus dimensiones.

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

    // Difunde filas y columnas a todos los ranks.

    broadcast_dimensiones(
        rows,
        cols
    );

    // Calcula cómo se parte la imagen entre ranks con overlap vertical.

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

    // Reparte cada fragmento de la imagen a su rank correspondiente.

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

    // Reconstruye el bloque local como Mat para procesarlo con OpenCV.

    cv::Mat local_image(
        local_rows,
        cols,
        CV_8UC1,
        local_buffer.data()
    );

    // Convierte el fragmento local a binario para detectar pistas del PCB.

    cv::Mat local_binary =
        generar_binario(
            local_image
        );

    // Guarda binario por rank para depuración.

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

    // Esqueletiza el binario local antes del reensamble global.

    cv::Mat local_skeleton =
        generar_esqueleto(
            local_binary
        );

    // Guarda el esqueleto parcial de cada rank.

    sprintf(
        filename,
        "rank_%d_skeleton.png",
        rank
    );

    guardar_imagen_debug(
        local_skeleton,
        filename
    );

    // Reensambla el esqueleto completo en el rank 0.

    cv::Mat final_image;

    gather_fragmento(
        local_skeleton,
        dist,
        rank,
        final_image
    );

    // Solo rank 0 continúa con el análisis global y la salida CNC.

    if(rank == 0)
    {
        // Guarda el esqueleto global reconstruido.

        cv::imwrite(
            "pcb_skeleton.png",
            final_image
        );

        printf(
            "\nEsqueleto distribuido guardado:\n"
            "pcb_skeleton.png\n"
        );

        // Convierte el esqueleto en nodos y aristas topológicas.

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

        // Si se habilita el device, ejecuta el trazado físico en la CNC.
        if(cnc_device != NULL)
        {
            printf(
                "Ejecutando cnc_lib en device: %s\n",
                cnc_device
            );

            int cnc_ret = ejecutar_cnc_paths(
                cnc_paths,
                cnc_device
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

          // Siguiente mejora sugerida: optimizar aún más el orden de rutas.
    }

    MPI_Finalize();

    return 0;
}