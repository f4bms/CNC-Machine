# cnc_lib — Biblioteca de Control CNC

## ¿Qué es esta biblioteca?

`cnc_lib` es una biblioteca estática en C (`bin/libcnc.a`) que actúa como la **única capa de software autorizada para comunicarse con el device driver GPIO** (`/dev/gpio_device`). Ningún otro componente del sistema (servidor, cliente, nodos del clúster) puede abrir o escribir al driver directamente; todo pasa obligatoriamente por esta biblioteca.

Su propósito es abstraer los detalles de bajo nivel del driver y exponer una API legible orientada al dominio del CNC: mover ejes, controlar la pluma, trazar rutas completas.

---

## Estructura de archivos

```
cnc_lib/
├── include/
│   └── cnc_lib.h       ← API pública: tipos, constantes y firmas de funciones
├── src/
│   └── cnc_lib.c       ← implementación de todas las funciones
├── test/
│   └── test_cnc.c      ← pruebas funcionales (se ejecutan sin hardware real)
├── build/              ← objetos .o intermedios (generado por make)
├── bin/
│   └── libcnc.a        ← biblioteca estática final (generado por make)
└── Makefile
```

Solo `include/cnc_lib.h` y `bin/libcnc.a` son relevantes para quien consuma la biblioteca desde el servidor. Los demás directorios son internos.

---

## Protocolo de comunicación con el driver

La biblioteca se comunica con `/dev/gpio_device` mediante `write()` usando comandos de texto plano. Este diseño fue elegido porque el driver actual implementa `dev_write` con `copy_from_user`, por lo que espera bytes de texto desde espacio de usuario.

| Comando enviado | Función que lo genera  | Significado                         |
| --------------- | ---------------------- | ----------------------------------- |
| `MOVE x y\n`    | `cnc_move_to(h, x, y)` | Mover cabezal a coordenada absoluta |
| `RIGHT n\n`     | `cnc_move_right(h, n)` | Desplazar n pasos a la derecha      |
| `LEFT n\n`      | `cnc_move_left(h, n)`  | Desplazar n pasos a la izquierda    |
| `UP n\n`        | `cnc_move_up(h, n)`    | Desplazar n pasos hacia arriba      |
| `DOWN n\n`      | `cnc_move_down(h, n)`  | Desplazar n pasos hacia abajo       |
| `HOME\n`        | `cnc_home(h)`          | Ir al origen (0, 0)                 |
| `PEN DOWN\n`    | `cnc_pen_down(h)`      | Bajar la pluma (iniciar trazo)      |
| `PEN UP\n`      | `cnc_pen_up(h)`        | Levantar la pluma (sin trazo)       |
| `SPEED v\n`     | `cnc_set_speed(h, v)`  | Configurar velocidad del motor      |

Todos los comandos pasan por la función interna privada `_cnc_send_cmd()`, que es la única que llama a `write()`. Esto centraliza el manejo de errores de I/O en un solo lugar.

---

## API completa

### Tipos principales

```c
// Contexto de conexión con el driver. Se obtiene con cnc_open().
typedef struct {
    int fd;        // file descriptor de /dev/gpio_device
    int speed;     // velocidad de movimiento actual
    int is_open;   // 1 si está activo, 0 si fue cerrado
} CNCHandle;

// Coordenada 2D en pasos de motor
typedef struct {
    int32_t x;
    int32_t y;
} CNCPoint;

// Ruta vectorizada: arreglo de puntos que forman una traza PCB
typedef struct {
    CNCPoint *points;
    size_t    count;
} CNCPath;
```

### Ciclo de vida

```c
int cnc_open(CNCHandle *handle, const char *device);
int cnc_close(CNCHandle *handle);
```

`cnc_open` debe ser la primera llamada. Abre el device driver y prepara el handle. Pasar `NULL` como `device` usa la ruta por defecto `/dev/gpio_device`. `cnc_close` libera el file descriptor; siempre llamarla al terminar.

### Movimiento

```c
int cnc_move_to(CNCHandle *handle, int32_t x, int32_t y);  // posición absoluta
int cnc_move_right(CNCHandle *handle, int32_t steps);
int cnc_move_left(CNCHandle *handle, int32_t steps);
int cnc_move_up(CNCHandle *handle, int32_t steps);
int cnc_move_down(CNCHandle *handle, int32_t steps);
int cnc_home(CNCHandle *handle);                            // volver a (0,0)
```

### Control de pluma

```c
int cnc_pen_down(CNCHandle *handle);  // iniciar trazo
int cnc_pen_up(CNCHandle *handle);    // detener trazo
```

### Trazado de ruta completa

```c
int cnc_draw_path(CNCHandle *handle, const CNCPath *path);
```

Esta es la función que el servidor usa principalmente. Internamente ejecuta la secuencia completa:

1. `PEN UP` — levanta la pluma
2. `MOVE` al primer punto — se posiciona sin trazar
3. `PEN DOWN` — baja la pluma
4. `MOVE` por cada punto restante — traza la ruta
5. `PEN UP` — levanta la pluma al terminar

Si ocurre un error en medio del trazado, la función intenta levantar la pluma antes de retornar para no dañar el PCB.

### Configuración y utilidades

```c
int cnc_set_speed(CNCHandle *handle, int speed);
int cnc_write(CNCHandle *handle, const char *cmd);      // comando raw al driver
int cnc_read(CNCHandle *handle, char *buf, size_t len); // reservado para dev_read
const char *cnc_strerror(int error_code);               // texto del error
```

### Códigos de retorno

| Código             | Valor | Significado                                      |
| ------------------ | ----- | ------------------------------------------------ |
| `CNC_OK`           | 0     | Operación exitosa                                |
| `CNC_ERR_NOT_OPEN` | -1    | Se olvidó llamar `cnc_open()` antes              |
| `CNC_ERR_WRITE`    | -2    | `write()` al driver falló                        |
| `CNC_ERR_INVALID`  | -3    | Parámetros nulos, vacíos o con valores inválidos |
| `CNC_ERR_OPEN`     | -4    | No se pudo abrir `/dev/gpio_device`              |

---

## Cómo el servidor usa la biblioteca

### Compilación

```bash
# Primero construir la biblioteca
cd cnc_lib && make

# Luego compilar el servidor enlazando contra ella
gcc servidor.c -L./cnc_lib/bin -lcnc -I./cnc_lib/include -o servidor
```

### Flujo típico en el servidor

```c
#include "cnc_lib.h"

int main(void) {
    CNCHandle handle;

    // 1. Abrir el driver
    if (cnc_open(&handle, NULL) != CNC_OK) {
        fprintf(stderr, "Error abriendo driver\n");
        return 1;
    }

    // 2. Configurar velocidad inicial
    cnc_set_speed(&handle, 150);

    // 3. Ir al origen antes de empezar
    cnc_home(&handle);

    // 4. Recibir trayectoria del clúster y trazar
    //    (el clúster entrega un CNCPath con los puntos vectorizados)
    CNCPath trayectoria = { .points = puntos_del_cluster, .count = n };
    int ret = cnc_draw_path(&handle, &trayectoria);
    if (ret != CNC_OK) {
        fprintf(stderr, "Error en trazado: %s\n", cnc_strerror(ret));
    }

    // 5. Cerrar el driver al terminar
    cnc_close(&handle);
    return 0;
}
```

---

## Cómo ejecutar las pruebas

Las pruebas corren sin hardware real usando `/dev/null` como device simulado, por lo que funcionan en cualquier máquina Linux.

```bash
cd cnc_lib
make test
```

Las pruebas verifican: apertura y cierre del device, todos los movimientos, rechazo de parámetros inválidos, trazado completo de una ruta de 6 puntos, comportamiento correcto tras cerrar el handle, y todos los mensajes de `cnc_strerror`.
