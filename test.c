#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>





// Definiciones de las etapas que puede tomar un proceso
#define ENVIAR_PROCESOS 1 // Enviar el arreglo con todos los procesos
#define INICIAR_DESAFIO 2 // Enviar el token al primer proceso
#define ENVIAR_TOKEN 3 // Enviar el token al siguiente proceso
#define PROCESO_ELIMINADO 4 // Notificar que el proceso actual fue eliminado
//#define RECONOCIMIENTO 5 // Notificar que la señal ya fue procesada





volatile sig_atomic_t senal_instruccion_recibida = 0; // Reconocimiento de la instruccion
volatile sig_atomic_t senal_dato_recibido = 0; // Reconocimiento del dato
volatile sig_atomic_t instruccion_recibida = 0; // Variable para recibir la instruccion y modificar el comportamiento
volatile sig_atomic_t dato_recibido = 0; // Variable para recibir el dato
// *Se usará senal_instruccion_recibida para que el proceso padre sepa si el hijo termino la instruccion
// *Se usará senal_dato_recibido para que el proceso padre sepa si el hijo recibió el dato





// El manejador para SIGUSR1
// Se encarga de entregar la señal y modificar el comportamiento del proceso
// Permite que el proceso entienda como recibir la informacion del manejador 2
void manejador_comportamiento(int sig, siginfo_t *ins, void *context) {
    instruccion_recibida = ins->si_value.sival_int;
    senal_instruccion_recibida = 1; // Indica que se recibió la instrucción

    //printf("Proceso %d recibió la instrucción: %d\n", getpid(), instruccion_recibida);
}



// El manejador para SIGUSR2
// Recibe los datos adicionales
// Se usa siginfo_t para recibir el pid
void manejador_datos(int sig, siginfo_t *si, void *context) {
    dato_recibido = si->si_value.sival_int;
    senal_dato_recibido = 1; // Indica que se recibió el dato

    //printf("Proceso %d recibió el dato: %d\n", getpid(), dato_recibido);
}






// Variables globales
// Valores de ejemplo
int p = 4;
int t = 10;
int M = 3;
int main(int argc, char *argv[]) {
    srand(time(NULL)); // Iniciar los numeros random
    int i, j;







    struct sigaction sa1, sa2; // Estructura para manejar las señales SIGUSR1 y SIGUSR2
    sigset_t mask1, oldmask1; // Máscara de señales. Esta máscara se usa para bloquear la señal mientras se espera
    sigset_t mask2, oldmask2; // Máscara de señales. Esta máscara se usa para bloquear la señal mientras se espera







    
    // ************************************************ INCOMPLETO ************************************************ //

    


    


    // ****************************** Preparar manejadores ****************************** //

    // Configuración del manejador 1 (instruccion) para SIGUSR1
    sa1.sa_flags = SA_SIGINFO; // Permite acceder a información extendida
    sa1.sa_sigaction = manejador_comportamiento; // Asignar el manejador de la señal
    sigemptyset(&sa1.sa_mask); // Inicializar la máscara de señales
    if (sigaction(SIGUSR1, &sa1, NULL) == -1) { // Instala el manejador
        perror("sigaction1");
        exit(EXIT_FAILURE);
    }


    // Bloquea a SIGUSR1
    sigemptyset(&mask1);
    sigaddset(&mask1, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &mask1, &oldmask1) < 0) { // Bloquear (o desbloquear) señales, modificando la mascara de señales del proceso
        perror("sigprocmask1");
        exit(EXIT_FAILURE);
    }





    // Configuración del manejador 2 (datos) para SIGUSR2
    sa2.sa_flags = SA_SIGINFO; // Permite acceder a información extendida
    sa2.sa_sigaction = manejador_datos; // Asignar el manejador de la señal
    sigemptyset(&sa2.sa_mask); // Inicializar la máscara de señales
    if (sigaction(SIGUSR2, &sa2, NULL) == -1) { // Instala el manejador
        perror("sigaction");
        exit(EXIT_FAILURE);
    }


    // Bloquea a SIGUSR2
    sigemptyset(&mask2);
    sigaddset(&mask2, SIGUSR2);
    if (sigprocmask(SIG_BLOCK, &mask2, &oldmask2) < 0) { // Bloquear (o desbloquear) señales, modificando la mascara de señales del proceso
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }





    // ****************************** Crear hijos ****************************** //

    // El proceso principal crea un conjunto de procesos hijos
    i = 0;
    //int pid;
    pid_t pid;
    int parent_pid = getpid();
    int *pid_array = (int *)malloc(sizeof(int) * p); // Aquí se guardan todos lo pids de los hijos
    while (i < p) {
        if (getpid() == parent_pid) { // Si soy el padre, creo al hijo
            pid = fork();
            pid_array[i] = pid;
        } else {
            break;
        }
        i++;
    }





    // ****************************** Entregar pid ****************************** //

    union sigval value; // Estructura para enviar el pid
    
    // El padre envía el pid de cada hijo
    if (getpid() == parent_pid) { // Soy padre
        for (i = 0; i < p; i++) {
            value.sival_int = ENVIAR_PROCESOS;
            if (sigqueue(pid_array[i], SIGUSR1, value) == -1) { // Enviar la señal al hijo
                perror("sigqueue");
                exit(EXIT_FAILURE);
            }


            // Esperar a que el hijo termine la instruccion
            while (!senal_instruccion_recibida) {
                sigsuspend(&oldmask1); // Esperar la señal
            }
            senal_instruccion_recibida = 0; // Reiniciar la variable


            for (j = 0; j < p; j++) {
                value.sival_int = pid_array[j];
                if (sigqueue(pid_array[i], SIGUSR2, value) == -1) { // Enviar el pid del proceso j al hijo i
                    perror("sigqueue");
                    exit(EXIT_FAILURE);
                }


                // Esperar a que el hijo termine la instruccion
                while (!senal_dato_recibido) {
                    sigsuspend(&oldmask2); // Esperar la señal
                }
                senal_dato_recibido = 0; // Reiniciar la variable
            }
        }
    } else { // Soy hijo
        while (!senal_instruccion_recibida) {
            sigsuspend(&oldmask1); // Esperar la señal
        }
        senal_instruccion_recibida = 0; // Reiniciar la variable


        // Reconocer la instruccion
        if (sigqueue(parent_pid, SIGUSR1, value) == -1) { // Enviar la señal al padre
            perror("sigqueue");
            exit(EXIT_FAILURE);
        }


        if (instruccion_recibida == ENVIAR_PROCESOS) {
            i = 0;
            while (i < p) {
                while (!senal_dato_recibido) {
                    sigsuspend(&oldmask2); // Esperar la señal
                }
                senal_dato_recibido = 0; // Reiniciar la variable


                pid_array[i] = dato_recibido;


                // Reconocer el dato recibido
                if (sigqueue(parent_pid, SIGUSR2, value) == -1) { // Enviar la señal al padre
                    perror("sigqueue");
                    exit(EXIT_FAILURE);
                }

                i++;
            }
        }
    }





    // ****************************** Imprimir arreglo de pids ****************************** //

    // Cada proceso imprime su arreglo de pids, empezando por el padre
    // "Soy pid n, y estos es mi arreglo: ..."
    if (getpid() == parent_pid) {
        printf("Hola, soy pid %d, y este es el arreglo de pids: ", getpid());
        i = 0;
        while (i < p) {
            printf("%d   ", pid_array[i]);
            i++;
        }
        printf("\n");


        // Siguiente proceso
        value.sival_int = 0;
        if (sigqueue(pid_array[value.sival_int], SIGUSR2, value) == -1) { // Enviar la señal al hijo
            perror("sigqueue");
            exit(EXIT_FAILURE);
        }


    } else { // Soy hijo
        while (!senal_dato_recibido) {
            sigsuspend(&oldmask2); // Esperar la señal
        }
        senal_dato_recibido = 0; // Reiniciar la variable


        if (getpid() == pid_array[p - 1]) { // Si soy el último hijo
            printf("Hola, soy pid %d, y este es el arreglo de pids: ", getpid());
            i = 0;
            while (i < p) {
                printf("%d   ", pid_array[i]);
                i++;
            }
            printf("\n");


            exit(0);
        } else {
            printf("Hola, soy pid %d, y este es el arreglo de pids: ", getpid());
            i = 0;
            while (i < p) {
                printf("%d   ", pid_array[i]);
                i++;
            }
            printf("\n");


            // Siguiente proceso
            value.sival_int = dato_recibido + 1;
            if (sigqueue(pid_array[value.sival_int], SIGUSR2, value) == -1) { // Enviar la señal al hijo
                perror("sigqueue");
                exit(EXIT_FAILURE);
            }


            exit(0); // Terminar el proceso hijo
        }
    }





    // ****************************** Terminar programa ****************************** //

    if (getpid() == parent_pid) {
        wait(NULL);
        printf("Final del programa\n");
    }
    free(pid_array);
    exit(0);
}