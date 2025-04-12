//#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>





// Definiciones de las etapas que puede tomar un proceso
#define ENVIAR_PROCESOS 0 // Enviar el arreglo con todos los procesos
#define INICIAR_DESAFIO 1 // Enviar el token al primer proceso
#define ENVIAR_TOKEN 2 // Enviar el token al siguiente proceso
#define PROCESO_ELIMINADO 3 // Notificar que el proceso actual fue eliminado





volatile sig_atomic_t senal_instruccion_recibida = 0;
volatile sig_atomic_t senal_dato_recibido = 0;
volatile sig_atomic_t instruccion_recibida = -1; // Variable para recibir la instruccion y modificar el comportamiento
volatile sig_atomic_t dato_recibido = 0; // Variable para recibir el dato





// El manejador para SIGUSR1
// Se encarga de entregar la señal y modificar el comportamiento del proceso
// Permite que el proceso entienda como recibir la informacion del manejador 2
void manejador_comportamiento(int sig, siginfo_t *ins, void *context) {
    instruccion_recibida = ins->si_value.sival_int;
    senal_dato_recibido = 1; // Indica que se recibió la instrucción

    printf("Proceso %d recibió la instrucción: %d\n", getpid(), instruccion_recibida);
}



// El manejador para SIGUSR2
// Recibe los datos adicionales
// Se usa siginfo_t para recibir el pid
void manejador_datos(int sig, siginfo_t *si, void *context) {
    dato_recibido = si->si_value.sival_int;

    printf("Proceso %d recibió el dato: %d\n", getpid(), dato_recibido);
}






// Variables globales
// Valores de ejemplo
int p = 4;
int t = 10;
int M = 3;
int main(int argc, char *argv[]) {
    srand(time(NULL)); // Iniciar los numeros random







    struct sigaction sa1, sa2; // Estructura para manejar las señales SIGUSR1 y SIGUSR2
    sigset_t mask, oldmask; // Máscara de señales. Esta máscara se usa para bloquear la señal mientras se espera







    
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
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &mask, &oldmask) < 0) { // Bloquear (o desbloquear) señales, modificando la mascara de señales del proceso
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
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR2);
    if (sigprocmask(SIG_BLOCK, &mask, &oldmask) < 0) { // Bloquear (o desbloquear) señales, modificando la mascara de señales del proceso
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }





    // ****************************** Crear hijos ****************************** //

    // El proceso principal crea un conjunto de procesos hijos
    int i = 0;
    int pid;
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
    if (getpid() == parent_pid) {
        i = 0;
        while (i < p) { // Soy padre
            value.sival_int = pid_array[i]; // El pid del hijo
            if (sigqueue(pid_array[i], SIGUSR1, value) == -1) { // Enviar el pid al hijo
                perror("sigqueue");
                exit(EXIT_FAILURE);
            }
            i++;
        }
    } else { // Soy hijo
        while (!instruccion_recibida) {
            sigsuspend(&oldmask); // Esperar la señal
        }
        instruccion_recibida = 0; // Reiniciar la variable

        if (instruccion == ENVIAR_PROCESOS) { // Si la instrucción es enviar procesos
            
       }
    }



    /*if (getpid() == parent_pid) { // Si soy padre
        //printf("\nLos procesos creados son:\n");
        i = 0;
        while (i < p) {
            printf("%d\n", pid_array[i]);
            i++;
        }
    }
    printf("\n");

    //if (getpid() != parent_pid)
    if (pid > 0) {
        printf("Hola!!! recibi el pid: %d\n", pid); // El padre imprime el último pid que obtuvo de fork()
    }*/




    



    /*if (pid == 0) { // Soy hijo
        while (!token_recibido) {
            sigsuspend(&oldmask);
        }
        exit(0);
    } else { // Soy padre
        union sigval value;
        i = 0;
        while (i < p) {
            value.sival_int = rand() % M;
            if (sigqueue(pid_array[i], SIGUSR1, value) == -1) {
                perror("sigqueue");
                exit(EXIT_FAILURE);
            }
            wait(NULL);
            i++;
        }
    }*/


    i = 0;
    while (how_many(...) != 1) {
        if (getpid() != pid_array[i]) { // Si no soy el proceso actual, esperar
            while(!token_recibido) {
                sigsuspend(&oldmask);
            }
        } else {
            ...
        }
    }






 
 
 
    printf("Hola!!! Mi pid es: %d\n", getpid());

    return 0;
}