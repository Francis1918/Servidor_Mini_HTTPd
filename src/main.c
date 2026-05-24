/*
 * main.c - Punto de entrada de MiniHTTPd.
 *
 * Por ahora solo invoca server_run() con el puerto por defecto.
 * Mas adelante podriamos aceptar el puerto desde argv[].
 */

#include "server.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    printf("MiniHTTPd: iniciando...\n");

    if (server_run(DEFAULT_PORT) < 0) {
        fprintf(stderr, "Error fatal al iniciar el servidor.\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
