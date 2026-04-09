#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "config.h"
#include "bridge.h"
#include "mode_carnage.h"
#include "mode_semaphore.h"
#include "mode_officer.h"
#include "gui.h"

static void imprimir_uso(const char *nombre_programa) {
    fprintf(stderr,
        "Usage: %s <config_file> <mode>\n"
        "  mode 1 = Carnage (FIFO)\n"
        "  mode 2 = Traffic Lights\n"
        "  mode 3 = Traffic Officer\n",
        nombre_programa);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        imprimir_uso(argv[0]);
        return 1;
    }

    srand((unsigned)time(NULL));

    Config configuracion;
    if (config_cargar(argv[1], &configuracion) != 0) {
        fprintf(stderr, "Failed to load config from '%s'\n", argv[1]);
        return 1;
    }
    config_imprimir(&configuracion);

    int modo = atoi(argv[2]);
    if (modo < 1 || modo > 3) {
        fprintf(stderr, "Invalid mode %d. Must be 1, 2 or 3.\n", modo);
        imprimir_uso(argv[0]);
        return 1;
    }

    puente_inicializar(&configuracion);


    if (gui_iniciar(&configuracion, modo) != 0)
        fprintf(stderr, "[INFO] GUI no disponible — corriendo solo en consola.\n");

    switch (modo) {
        case 1: modo_carnage_ejecutar(&configuracion);   break;
        case 2: modo_semaforo_ejecutar(&configuracion);  break;
        case 3: modo_oficial_ejecutar(&configuracion);   break;
    }


    gui_detener();
    puente_destruir();
    return 0;
}
