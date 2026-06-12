#include <mpi.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include "distribucion.h"
#include "grafo.h"
#include "cnc_traduccion.h"
#include "../img/procesamiento_imagen.h"

/**
 * @brief Cantidad de filas que se solapan entre fragmentos.
 */
#define OVERLAP_ROWS 10

/**
 * @brief Escala por defecto para convertir pixeles a pasos CNC.
 */
#define DEFAULT_CNC_SCALE 5

/**
 * @brief Convierte un texto a entero con valor por defecto.
 *
 * @param s Cadena de texto que representa un número.
 * @param fallback Valor retornado si la cadena es NULL, no es positiva o no se puede convertir.
 * @return Entero convertido o el valor fallback.
 */
static int parse_int(
    const char *s,
    int fallback)
{
    if (s == NULL)
        return fallback;

    int v = atoi(s);

    if (v <= 0)
        return fallback;

    return v;
}

// Carpeta de salida: variable de entorno OUTPUT_DIR o ../outputs por defecto.
/**
 * @brief Obtiene el directorio de salida para archivos generados.
 *
 * Busca la variable de entorno OUTPUT_DIR y, si no existe,
 * devuelve la ruta por defecto "../outputs".
 *
 * @return Cadena con el directorio de salida.
 */
static const char *get_output_dir(void)
{
    const char *dir = getenv("OUTPUT_DIR");

    if (dir != NULL && dir[0] != '\0')
        return dir;

    return "../outputs";
}

/**
 * @brief Garantiza que exista el directorio de salida.
 *
 * Intenta crear el directorio si no existe y muestra un error
 * en stderr cuando la creación falla por una razón distinta a EEXIST.
 *
 * @param dir Directorio de salida a crear.
 */
static void ensure_output_dir(
    const char *dir)
{
    if (mkdir(dir, 0775) != 0 && errno != EEXIST)
    {
        fprintf(
            stderr,
            "[MPI] No se pudo crear el directorio de salida %s\n",
            dir);
    }
}

/**
 * @brief Punto de entrada del programa MPI de procesamiento de PCB para CNC.
 *
 * Describe el flujo principal:
 * 1. Inicializa MPI y obtiene rank/size.
 * 2. Rank 0 valida argumentos, carga la imagen y crea el directorio de salida.
 * 3. Se comparte el tamaño de la imagen entre todos los procesos.
 * 4. Se distribuye el fragmento local de la imagen a cada rank.
 * 5. Cada rank procesa su fragmento de imagen para obtener el esqueleto.
 * 6. Se recopilan los fragmentos esqueléticos en rank 0.
 * 7. Rank 0 genera el grafo topológico, guarda imágenes/archivos de depuración
 *    y convierte el grafo a rutas CNC.
 * 8. Opcionalmente, si se pasa --cnc-device, ejecuta el trazado en la CNC.
 * 9. Libera recursos y finaliza MPI.
 *
 * @param argc Cuenta de argumentos de línea de comandos.
 * @param argv Arreglo de argumentos de línea de comandos.
 * @return 0 en éxito, 1 si hay error de uso o error de procesamiento.
 */
int main(
    int argc,
    char *argv[])
{
    // Inicializa el runtime MPI en todos los procesos.
    MPI_Init(&argc, &argv);

    int rank;
    int size;

    // rank identifica este proceso; size es el total de procesos.
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 2)
    {
        if (rank == 0)
        {
            printf(
                "Uso:\n"
                "./mpi_processor imagen.png "
                "[--cnc-device /dev/gpio_device] "
                "[--cnc-scale 1]\n");
        }

        MPI_Finalize();
        return 1;
    }

    const char *cnc_device = NULL;
    int cnc_scale = DEFAULT_CNC_SCALE;

    // Parsea argumentos opcionales del pipeline CNC.
    for (int i = 2; i < argc; i++)
    {
        if (strcmp(argv[i], "--cnc-device") == 0 && i + 1 < argc)
        {
            cnc_device = argv[++i];
        }
        else if (strcmp(argv[i], "--cnc-scale") == 0 && i + 1 < argc)
        {
            cnc_scale = parse_int(argv[++i], DEFAULT_CNC_SCALE);
        }
    }

    // Define la carpeta de artefactos (rank_*.png, pcb_*.png, *.txt).
    const char *output_dir = get_output_dir();

    // Solo rank 0 crea el directorio compartido de salida.
    if (rank == 0)
        ensure_output_dir(output_dir);

    unsigned char *image = NULL;
    int rows = 0;
    int cols = 0;

    // Rank 0 carga la imagen original (OpenCV via interfaz C de img/).
    if (rank == 0)
    {
        if (img_load_grayscale(argv[1], &image, &rows, &cols) != 0)
        {
            printf("Error cargando imagen\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        printf("\nImagen cargada: %d x %d\n", rows, cols);
    }

    // Difunde dimensiones a todos los ranks.
    broadcast_dimensiones(&rows, &cols);

    // Calcula el reparto con overlap vertical.
    DistribucionMPI dist;

    if (crear_distribucion(rows, cols, size, OVERLAP_ROWS, &dist) != 0)
    {
        fprintf(stderr, "Rank %d: error creando distribucion\n", rank);
        free(image);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // Muestra el plan de reparto solo una vez para evitar ruido de logs.
    if (rank == 0)
        imprimir_distribucion(&dist);

    // Reparte el fragmento local a cada rank.
    int local_rows = 0;
    unsigned char *local_buffer = NULL;

    if (scatter_fragmento(image, &dist, rank, &local_rows, &local_buffer) != 0)
    {
        fprintf(stderr, "Rank %d: error en scatter\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    printf("Rank %d recibio %d filas\n", rank, local_rows);

    // Procesa el fragmento local con OpenCV (binarizacion + esqueleto).
    unsigned char *local_skeleton = NULL;

    if (img_process_fragment(
            local_buffer,
            local_rows,
            cols,
            rank,
            output_dir,
            &local_skeleton) != 0)
    {
        fprintf(stderr, "Rank %d: error procesando fragmento\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // Reensambla el esqueleto completo en el rank 0.
    unsigned char *final_image = NULL;

    if (gather_fragmento(local_skeleton, &dist, rank, &final_image) != 0)
    {
        fprintf(stderr, "Rank %d: error en gather\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // Cada rank libera su memoria local despues de enviar su region util.
    free(local_buffer);
    free(local_skeleton);

    // Desde aqui solo rank 0 hace ensamblado global, grafo y CNC.
    if (rank == 0)
    {
        char path_skeleton[1024];
        char path_graph[1024];
        char path_edges[1024];
        char path_paths[1024];

        // Construye rutas absolutas/relativas para todos los artefactos finales.
        snprintf(path_skeleton, sizeof(path_skeleton), "%s/pcb_skeleton.png", output_dir);
        snprintf(path_graph, sizeof(path_graph), "%s/pcb_graph.png", output_dir);
        snprintf(path_edges, sizeof(path_edges), "%s/pcb_edges.txt", output_dir);
        snprintf(path_paths, sizeof(path_paths), "%s/pcb_paths.txt", output_dir);

        // Guarda el esqueleto global reconstruido.
        img_save_grayscale(path_skeleton, final_image, rows, cols);

        printf("\nEsqueleto distribuido guardado:\n%s\n", path_skeleton);

        // Construye el grafo topologico (C puro).
        Grafo grafo;

        if (generar_grafo(final_image, rows, cols, &grafo) != 0)
        {
            fprintf(stderr, "Error generando grafo\n");
            free(final_image);
            liberar_distribucion(&dist);
            MPI_Finalize();
            return 1;
        }

        // Debug del grafo (dibujo con OpenCV) y de aristas.
        img_save_graph_debug(path_graph, final_image, rows, cols, &grafo);
        guardar_aristas_debug(&grafo, path_edges);

        // Traduce el grafo a rutas CNC.
        CNCPathOwned *cnc_paths = NULL;
        int cnc_count = 0;

        if (convertir_grafo_a_cnc_paths(&grafo, cnc_scale, &cnc_paths, &cnc_count) != 0)
        {
            fprintf(stderr, "Error generando rutas CNC\n");
            liberar_grafo(&grafo);
            free(final_image);
            liberar_distribucion(&dist);
            MPI_Finalize();
            return 1;
        }

        // Guarda un volcado textual de rutas para depurar movimiento CNC.
        guardar_cnc_paths_debug(cnc_paths, cnc_count, path_paths);

        printf("Nodos detectados: %d\n", grafo.node_count);
        printf("Aristas detectadas: %d\n", grafo.edge_count);
        printf("CNC paths generados: %d\n", cnc_count);

        printf("Grafo guardado:\n%s\n", path_graph);
        printf("Debug rutas guardado:\n%s\n%s\n", path_edges, path_paths);

        // Si se habilita el device, ejecuta el trazado fisico en la CNC.
        if (cnc_device != NULL)
        {
            printf("Ejecutando cnc_lib en device: %s\n", cnc_device);

            int cnc_ret =
                ejecutar_cnc_paths(cnc_paths, cnc_count, cnc_device);

            if (cnc_ret != CNC_OK)
                printf("Error ejecutando cnc_lib\n");
            else
                printf("Trazado CNC completado\n");
        }
        else
        {
            printf("Ejecucion de hardware omitida (use --cnc-device para habilitar)\n");
        }

        // Libera todo lo asignado en rank 0 antes de terminar MPI.
        liberar_cnc_paths(cnc_paths, cnc_count);
        liberar_grafo(&grafo);
        free(final_image);
    }

    // Libera metadatos comunes de distribucion en todos los ranks.
    liberar_distribucion(&dist);

    // Cierra el runtime MPI de forma ordenada.
    MPI_Finalize();

    return 0;
}
