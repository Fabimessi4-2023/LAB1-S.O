#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    int opcion=0;
    int num_procesos = 0;

    // Leer -p desde línea de comandos
    while ((opcion = getopt(argc, argv, "p:")) != -1) {
        if (opcion == 'p') {
            num_procesos = atoi(optarg);
            printf( "Se eligieron: %d procesos\n", num_procesos);
        } else {
            fprintf(stderr, "\n");
            exit(EXIT_FAILURE);
        }
    }
    
}