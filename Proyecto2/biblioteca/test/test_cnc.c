/*
 * test_cnc.c
 * Programa de prueba para la biblioteca cnc_lib.
 * Simula el comportamiento del servidor al recibir trayectorias del cluster.
 *
 * Para ejecutar en modo de prueba sin la Raspberry Pi real,
 * el test abre /dev/null como device simulado.
 *
 * Compilar: make test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cnc_lib.h"

/* Device simulado para pruebas sin hardware real */
#define TEST_DEVICE "/dev/null"

/* Macro de verificación para los tests */
#define CHECK(expr, msg)                                                 \
    do                                                                   \
    {                                                                    \
        int _r = (expr);                                                 \
        if (_r != CNC_OK)                                                \
        {                                                                \
            fprintf(stderr, "[FAIL] %s -> %s\n", msg, cnc_strerror(_r)); \
            cnc_close(&handle);                                          \
            return EXIT_FAILURE;                                         \
        }                                                                \
        printf("[OK]   %s\n", msg);                                      \
    } while (0)

int main(void)
{
    CNCHandle handle;
    int ret;

    printf("=== Test de biblioteca cnc_lib ===\n");
    printf("    Device de prueba: %s\n\n", TEST_DEVICE);

    /* ------------------------------------------------------------------
     * 1. Apertura del device
     * ------------------------------------------------------------------ */
    ret = cnc_open(&handle, TEST_DEVICE);
    if (ret != CNC_OK)
    {
        fprintf(stderr, "[FAIL] cnc_open -> %s\n", cnc_strerror(ret));
        return EXIT_FAILURE;
    }
    printf("[OK]   cnc_open('%s')\n", TEST_DEVICE);

    /* ------------------------------------------------------------------
     * 2. Pruebas de configuración
     * ------------------------------------------------------------------ */
    CHECK(cnc_set_speed(&handle, 200), "cnc_set_speed(200)");

    /* Velocidad inválida debe retornar error */
    ret = cnc_set_speed(&handle, -10);
    if (ret == CNC_ERR_INVALID)
    {
        printf("[OK]   cnc_set_speed(-10) correctamente rechazado\n");
    }
    else
    {
        fprintf(stderr, "[FAIL] cnc_set_speed(-10) debería retornar CNC_ERR_INVALID\n");
    }

    /* ------------------------------------------------------------------
     * 3. Pruebas de movimiento absoluto
     * ------------------------------------------------------------------ */
    CHECK(cnc_home(&handle), "cnc_home()");
    CHECK(cnc_move_to(&handle, 100, 200), "cnc_move_to(100, 200)");
    CHECK(cnc_move_to(&handle, 0, 0), "cnc_move_to(0, 0)");

    /* ------------------------------------------------------------------
     * 4. Pruebas de movimiento relativo
     * ------------------------------------------------------------------ */
    CHECK(cnc_move_right(&handle, 50), "cnc_move_right(50)");
    CHECK(cnc_move_left(&handle, 30), "cnc_move_left(30)");
    CHECK(cnc_move_up(&handle, 40), "cnc_move_up(40)");
    CHECK(cnc_move_down(&handle, 20), "cnc_move_down(20)");

    /* Steps inválidos deben retornar error */
    ret = cnc_move_right(&handle, 0);
    if (ret == CNC_ERR_INVALID)
    {
        printf("[OK]   cnc_move_right(0) correctamente rechazado\n");
    }

    /* ------------------------------------------------------------------
     * 5. Pruebas de control de pluma
     * ------------------------------------------------------------------ */
    CHECK(cnc_pen_down(&handle), "cnc_pen_down()");
    CHECK(cnc_pen_up(&handle), "cnc_pen_up()");

    /* ------------------------------------------------------------------
     * 6. Prueba de trazado completo de una ruta (simula salida del cluster)
     *    Ruta triangular de ejemplo: representa una traza PCB simple
     * ------------------------------------------------------------------ */
    printf("\n--- Simulando trazado de ruta PCB ---\n");

    CNCPoint vertices[] = {
        {0, 0},
        {100, 0},
        {100, 150},
        {50, 200},
        {0, 150},
        {0, 0} /* Cierre del contorno */
    };

    CNCPath ruta = {
        .points = vertices,
        .count = sizeof(vertices) / sizeof(vertices[0])};

    CHECK(cnc_draw_path(&handle, &ruta), "cnc_draw_path(ruta triangular)");

    /* Ruta nula debe retornar error */
    ret = cnc_draw_path(&handle, NULL);
    if (ret == CNC_ERR_INVALID)
    {
        printf("[OK]   cnc_draw_path(NULL) correctamente rechazado\n");
    }

    /* ------------------------------------------------------------------
     * 7. Prueba de escritura raw (para comandos futuros del driver)
     * ------------------------------------------------------------------ */
    CHECK(cnc_write(&handle, "SPEED 300\n"), "cnc_write('SPEED 300')");

    /* ------------------------------------------------------------------
     * 8. Prueba de cnc_strerror
     * ------------------------------------------------------------------ */
    printf("\n--- Mensajes de error ---\n");
    printf("  CNC_OK           : %s\n", cnc_strerror(CNC_OK));
    printf("  CNC_ERR_NOT_OPEN : %s\n", cnc_strerror(CNC_ERR_NOT_OPEN));
    printf("  CNC_ERR_WRITE    : %s\n", cnc_strerror(CNC_ERR_WRITE));
    printf("  CNC_ERR_INVALID  : %s\n", cnc_strerror(CNC_ERR_INVALID));
    printf("  CNC_ERR_OPEN     : %s\n", cnc_strerror(CNC_ERR_OPEN));

    /* ------------------------------------------------------------------
     * 9. Prueba de operación con handle cerrado
     * ------------------------------------------------------------------ */
    cnc_close(&handle);
    printf("[OK]   cnc_close()\n");

    ret = cnc_move_to(&handle, 10, 10);
    if (ret == CNC_ERR_NOT_OPEN)
    {
        printf("[OK]   cnc_move_to después de close() correctamente rechazado\n");
    }

    printf("\n=== Todos los tests pasaron ===\n");
    return EXIT_SUCCESS;
}
