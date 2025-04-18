#include "funciones.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>





// Variables globales definidas
volatile sig_atomic_t token_recibido = 0;
volatile sig_atomic_t token_valor;
pid_t siguiente_pid;
int m;
int num_procesos;
pid_t *procesos_activos = NULL;
int *indices_pid = NULL;
int mi_indice;
int procesos_vivos;
int token_inicial;





// Entradas: sig (número de señal), si (información de la señal), context (contexto de ejecución, no usado en este caso)
// Salidas: ninguna
// Descripción: Maneja la señal SIGUSR1 que entrega el token. Actualiza su valor y marca que fue recibido (en variable token_recibido)
void manejador_token(int sig, siginfo_t *si, void *context) {
    token_valor = si->si_value.sival_int;
    printf("Proceso %d recibió el token: %d\n", indices_pid[getpid()], token_valor);
    token_recibido = 1;
}



// Entradas: sig (número de señal), si (información de la señal), context (contexto de ejecución, no usado en este caso)
// Salidas: ninguna
// Descripción: Recibe el PID del siguiente proceso en el anillo
void manejador_config(int sig, siginfo_t *si, void *context) {
    siguiente_pid = si->si_value.sival_int;
    //printf("Soy el proceso: %d, mi siguiente es: PID %d\n", getpid(), siguiente_pid);
}



// Entradas: sig (número de señal - SIGRTMIN + índice),  si (información con el PID recibido), context (contexto de ejecución, no usado en este caso)
// Salidas: ninguna
// Descripción: Almacena los PIDs de los procesos activos y crea un mapeo PID → índice, EJ: procesos_activos = [12001, 12003, 12007, 12009] → indices_pid[12001] = 0; indices_pid[12003] = 1; indices_pid[12007] = 2; indices_pid[12009] = 3;
void manejador_pid(int sig, siginfo_t *si, void *context) {
    int idx = sig - SIGRTMIN;
    if (idx >= 0 && idx < num_procesos) {
        pid_t pid_recibido = si->si_value.sival_int;
        procesos_activos[idx] = pid_recibido;

        if (pid_recibido < 100000) {
            indices_pid[pid_recibido] = idx;
        }
    }
}



// Entradas: max( nº entero) → valor máximo del rango
// Salidas: entero aleatorio entre 0 y max (inclusive)
// Descripción: Genera un número aleatorio en el rango [0, max]
int random_between(int max) {
    return rand() % (max + 1);
}



// Entradas: indice (entero) → índice desde el cual se comienza la búsqueda
// Salidas: índice del siguiente proceso activo, o -1 si no hay ninguno
// Descripción: Busca el siguiente proceso activo en el anillo a partir del índice dado
int encontrar_siguiente_activo(int indice) {
    int siguiente = (indice + 1) % num_procesos;
    int contador = 0;

    while (procesos_activos[siguiente] == 0 && contador < num_procesos) {
        siguiente = (siguiente + 1) % num_procesos;
        contador++;
    }

    return (contador < num_procesos) ? siguiente : -1;
}



// Entradas: sig (número de la señal recibida; se espera SIGTERM en este contexto), si (información con el PID eliminado), context (contexto de ejecución, no usado en este caso)
// Salidas: ninguna
// Descripción:
//   Maneja la notificación de eliminación de un proceso. Marca al proceso como inactivo,
//   actualiza el anillo si el proceso eliminado era el siguiente, y reduce el contador de procesos vivos.
//   Si solo queda un proceso, se declara como el GANADOR y el juego finaliza.
void manejador_terminado(int sig, siginfo_t *si, void *context) {
    pid_t proceso_eliminado = si->si_value.sival_int;
    //printf("Proceso %d recibió notificación: proceso %d eliminado\n", getpid(), proceso_eliminado);

    // Obtener indice del eliminado
    int indice_eliminado = -1;
    if (proceso_eliminado < 100000) {
        indice_eliminado = indices_pid[proceso_eliminado];
    } else {
        for (int i = 0; i < num_procesos; i++) {
            if (procesos_activos[i] == proceso_eliminado) {
                indice_eliminado = i;
                break;
            }
        }
    }

    if (indice_eliminado >= 0) {
        procesos_activos[indice_eliminado] = 0; // Marca al proceso como inactivo
        procesos_vivos--;

        if (siguiente_pid == proceso_eliminado) { // Si el eliminado es mi proximo proceso
            int siguiente_indice = encontrar_siguiente_activo(indice_eliminado);
            if (siguiente_indice >= 0) {
                siguiente_pid = procesos_activos[siguiente_indice]; // Guarda el nuevo pid siguiente
                //printf("Proceso %d actualiza su siguiente a: %d\n", getpid(), siguiente_pid);
            }
        }
    }

    // Si soy ganador
    if (procesos_vivos == 1) {
        printf("¡Proceso %d es el GANADOR!\n", indices_pid[getpid()]);
        exit(0);
    }

    // Aplicar el criterio de mayor pid para elegir lider
    pid_t mayor_pid = getpid();
    bool soy_lider = true;
    for (int i = 0; i < num_procesos; i++) {
        if (procesos_activos[i] != 0 && procesos_activos[i] > mayor_pid) {
            soy_lider = false;
            break;
        }
    }

    if (soy_lider) {
        printf("Proceso %d es el nuevo líder. Reiniciando token.\n", indices_pid[getpid()]);
        sleep(1);

        token_valor = token_inicial;
        union sigval val;
        val.sival_int = token_valor;

        // Enviar token al siguiente proceso
        if (siguiente_pid != 0 && siguiente_pid != proceso_eliminado) {
            sigqueue(siguiente_pid, SIGUSR1, val);
        } else {
            for (int i = 0; i < num_procesos; i++) {
                if (procesos_activos[i] != 0 && procesos_activos[i] != getpid()) {
                    sigqueue(procesos_activos[i], SIGUSR1, val);
                    break;
                }
            }
        }
    }
}



// Entradas: programa (nombre del ejecutable como string)
// Salidas: ninguna
// Descripción: Muestra las opciones válidas para ejecutar el programa
void mostrar_ayuda(const char *programa) {
    printf("Uso: %s [opciones]\n", programa);
    printf("Opciones:\n");
    printf("  -m <valor>    Valor máximo a restar del token\n");
    printf("  -t <valor>    Valor inicial del token \n");
    printf("  -p <valor>    Número de procesos en el anillo\n");
    printf("  -h            Mostrar esta ayuda\n");
}



// Entradas: ninguna (usa variables globales)
// Salidas: ninguna
// Descripción: Configura todos los manejadores de señales necesarios para el juego
void configurar_manejadores() {
    struct sigaction sa_token = {0};
    sa_token.sa_flags = SA_SIGINFO;
    sa_token.sa_sigaction = manejador_token;
    if (sigaction(SIGUSR1, &sa_token, NULL)) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    struct sigaction sa_config = {0};
    sa_config.sa_flags = SA_SIGINFO;
    sa_config.sa_sigaction = manejador_config;
    if (sigaction(SIGUSR2, &sa_config, NULL)) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    struct sigaction sa_term = {0};
    sa_term.sa_flags = SA_SIGINFO;
    sa_term.sa_sigaction = manejador_terminado;
    if (sigaction(SIGTERM, &sa_term, NULL)) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    struct sigaction sa_pid = {0};
    sa_pid.sa_flags = SA_SIGINFO;
    sa_pid.sa_sigaction = manejador_pid;

    // Comprobar que no se saturen las señales con muchos procesos
    for (int i = 0; i < num_procesos; i++) {
        if (i + SIGRTMIN > SIGRTMAX) {
            fprintf(stderr, "Demasiados procesos para señales RT\n");
            exit(EXIT_FAILURE);
        }
        if (sigaction(SIGRTMIN + i, &sa_pid, NULL)) {
            perror("sigaction");
            exit(EXIT_FAILURE);
        }
    }
}