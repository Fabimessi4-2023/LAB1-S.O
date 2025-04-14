#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <stdbool.h>
#include <getopt.h>
#include <string.h>

// VARIABLES GLOBALES (TOKEN)
volatile sig_atomic_t token_recibido = 0;
volatile sig_atomic_t token_valor = 100;
pid_t siguiente_pid = 0; // PID del siguiente proceso en el anillo
int m = 10; // Valor máximo para restar (por defecto)
int num_procesos; // Número de procesos (por defecto)
pid_t *procesos_activos = NULL;
int *indices_pid = NULL; // Mapeo de PIDs a índices ----- indices_pid[12001] = 0; indices_pid[12005] = 1;
int mi_indice = -1;
int procesos_vivos;

// MANEJADOR DE SENALES TOKEN
void manejador_token(int sig, siginfo_t *si, void *context) {
    token_valor = si->si_value.sival_int;
    printf("Proceso %d recibió el token: %d\n", getpid(), token_valor);
    token_recibido = 1;
}

// MANEJADOR DE SENALES para la configuración del anillo
void manejador_config(int sig, siginfo_t *si, void *context) {
    siguiente_pid = si->si_value.sival_int;
    printf("Soy el proceso: %d, mi siguiente es: PID %d\n", getpid(), siguiente_pid);
}

// MANEJADOR para recibir los PIDs de los procesos
// Cada proceso hijo debe saber quiénes son todos los procesos en el anillo, no solo su "siguiente", para poder notificar y Determinar si es el nuevo líder
// SIGRTMIN = Real-Time Signals
void manejador_pid(int sig, siginfo_t *si, void *context) {
    int idx = sig - SIGRTMIN;
    if (idx >= 0 && idx < num_procesos) {
        pid_t pid_recibido = si->si_value.sival_int;
        procesos_activos[idx] = pid_recibido;

        // Guardar el mapeo de PID a índice para referencia rápida
        if (pid_recibido < 100000) {
            indices_pid[pid_recibido] = idx;
        }
    }
}

// Genera un número aleatorio entre 0 y max (inclusive)
int random_between(int max) {
    return rand() % (max + 1);
}

// Encuentra el siguiente proceso activo después del índice dado
// Busca el siguiente proceso que no haya sido eliminado.
int encontrar_siguiente_activo(int indice) {
    int siguiente = (indice + 1) % num_procesos;
    int contador = 0;

    while (procesos_activos[siguiente] == 0 && contador < num_procesos) {
        siguiente = (siguiente + 1) % num_procesos;
        contador++;
    }

    if (contador < num_procesos) {
        return siguiente;
    } else {
        return -1;
    }
}

// MANEJADOR para notificación de eliminación
// Marca procesos eliminados, Si el proceso eliminado era su siguiente, actualiza siguiente_pid, Si sólo queda 1 proceso → ese es el GANADOR.
void manejador_terminado(int sig, siginfo_t *si, void *context) {
    pid_t proceso_eliminado = si->si_value.sival_int;
    printf("Proceso %d recibió notificación: proceso %d eliminado\n", getpid(), proceso_eliminado);

    // Obtener el índice del proceso eliminado
    int indice_eliminado = -1;
    if (proceso_eliminado < 100000) {
        indice_eliminado = indices_pid[proceso_eliminado];
    } else {
        // Búsqueda lineal como fallback
        for (int i = 0; i < num_procesos; i++) {
            if (procesos_activos[i] == proceso_eliminado) {
                indice_eliminado = i;
                break;
            }
        }
    }

    if (indice_eliminado >= 0) {
        // Marcar el proceso como inactivo
        procesos_activos[indice_eliminado] = 0;
        procesos_vivos--;

        // Si el proceso eliminado era mi siguiente, actualizar mi siguiente
        if (siguiente_pid == proceso_eliminado) {
            // Encontrar el siguiente proceso activo
            int siguiente_indice = encontrar_siguiente_activo(indice_eliminado);

            if (siguiente_indice >= 0) {
                siguiente_pid = procesos_activos[siguiente_indice];
                printf("Proceso %d actualiza su siguiente a: %d\n", getpid(), siguiente_pid);
            }
        }
    }

    // Verificar si soy el último proceso vivo
    if (procesos_vivos == 1) {
        printf("¡Proceso %d es el GANADOR!\n", getpid());
        exit(0);
    }

    // Verificar si debo ser el líder (proceso con mayor PID)
    pid_t mayor_pid = getpid();
    bool soy_lider = true;

    for (int i = 0; i < num_procesos; i++) {
        if (procesos_activos[i] != 0 && procesos_activos[i] != getpid() && procesos_activos[i] > mayor_pid) {
            soy_lider = false;
            break;
        }
    }

    // Si soy líder, reiniciar token
    if (soy_lider) {
        printf("Proceso %d es el nuevo líder. Reiniciando token.\n", getpid());
        sleep(1);  // Esperar para que todos reciban la notificación de eliminación

        // Reiniciar token con un valor aleatorio
        token_valor = 20 + random_between(10);

        // Enviar token al siguiente proceso
        union sigval val;
        val.sival_int = token_valor;

        if (siguiente_pid != 0 && siguiente_pid != proceso_eliminado) {
            sigqueue(siguiente_pid, SIGUSR1, val);
            printf("Líder %d reinicia token con valor %d y lo envía a %d\n",
                   getpid(), token_valor, siguiente_pid);
        } else {
            // Si mi siguiente fue eliminado y no se actualizó correctamente
            for (int i = 0; i < num_procesos; i++) {
                if (procesos_activos[i] != 0 && procesos_activos[i] != getpid()) {
                    sigqueue(procesos_activos[i], SIGUSR1, val);
                    printf("Líder %d reinicia token con valor %d y lo envía a %d (recuperación)\n",
                           getpid(), token_valor, procesos_activos[i]);
                    break;
                }
            }
        }
    }
}

// Función para mostrar el uso del programa
void mostrar_ayuda(const char *programa) {
    printf("Uso: %s [opciones]\n", programa);
    printf("Opciones:\n");
    printf("  -m <valor>    Valor máximo a restar del token\n");
    printf("  -t <valor>    Valor inicial del token \n");
    printf("  -p <valor>    Número de procesos en el anillo\n");
    printf("  -h            Mostrar esta ayuda\n");
}

int main(int argc, char *argv[]) {
    // Valores por defecto
    int token_inicial = 20;

    // Procesar argumentos de línea de comandos
    int opt;
    while ((opt = getopt(argc, argv, "m:t:p:h")) != -1) {
        switch (opt) {
            case 'm':
                m = atoi(optarg);
                if (m <= 0) {
                    fprintf(stderr, "Error: El valor máximo de resta debe ser mayor que 0\n");
                    mostrar_ayuda(argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            case 't':
                token_inicial = atoi(optarg);
                if (token_inicial <= 0) {
                    fprintf(stderr, "Error: El valor inicial del token debe ser mayor que 0\n");
                    mostrar_ayuda(argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'p':
                num_procesos = atoi(optarg);
                if (num_procesos <= 0) {
                    fprintf(stderr, "Error: El número de procesos debe ser mayor que 0\n");
                    mostrar_ayuda(argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            case 'h':
                mostrar_ayuda(argv[0]);
                exit(EXIT_SUCCESS);
                break;
            default:
                mostrar_ayuda(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // Inicializar la semilla para números aleatorios
    srand(time(NULL) ^ getpid());

    // Inicializar estructuras de datos dinámicas
    procesos_activos = (pid_t *)calloc(num_procesos, sizeof(pid_t));
    if (!procesos_activos) {
        perror("calloc para procesos_activos");
        exit(EXIT_FAILURE);
    }

    indices_pid = (int *)calloc(100000, sizeof(int)); // Asumiendo PIDs menores a 100000
    if (!indices_pid) {
        perror("calloc para indices_pid");
        free(procesos_activos);
        exit(EXIT_FAILURE);
    }

    // Actualizar el valor de procesos_vivos
    procesos_vivos = num_procesos;

    // ESTRUCTURA MANEJADOR DE SENALES para el token
    struct sigaction sa_token;
    sigset_t mask, oldmask;

    // Configurar el manejador de señales para SIGUSR1 (token)
    sa_token.sa_flags = SA_SIGINFO;
    sa_token.sa_sigaction = manejador_token;
    sigemptyset(&sa_token.sa_mask);
    if (sigaction(SIGUSR1, &sa_token, NULL) == -1) {
        perror("sigaction");
        free(procesos_activos);
        free(indices_pid);
        exit(EXIT_FAILURE);
    }

    // Configurar el manejador de señales para SIGUSR2 (configuración)
    struct sigaction sa_config;
    sa_config.sa_flags = SA_SIGINFO;
    sa_config.sa_sigaction = manejador_config;
    sigemptyset(&sa_config.sa_mask);
    if (sigaction(SIGUSR2, &sa_config, NULL) == -1) {
        perror("sigaction");
        free(procesos_activos);
        free(indices_pid);
        exit(EXIT_FAILURE);
    }

    // Configurar el manejador de señales para SIGTERM (eliminacion)
    struct sigaction sa_term;
    sa_term.sa_flags = SA_SIGINFO;
    sa_term.sa_sigaction = manejador_terminado;
    sigemptyset(&sa_term.sa_mask);
    if (sigaction(SIGTERM, &sa_term, NULL) == -1) {
        perror("sigaction");
        free(procesos_activos);
        free(indices_pid);
        exit(EXIT_FAILURE);
    }

    // Configurar el manejador para recibir PIDs
    struct sigaction sa_pid;
    sa_pid.sa_flags = SA_SIGINFO;
    sa_pid.sa_sigaction = manejador_pid;
    sigemptyset(&sa_pid.sa_mask);

    // Registrar manejadores para SIGRTMIN, SIGRTMIN+1, SIGRTMIN+2, ...
    for (int i = 0; i < num_procesos; i++) {
        if (i + SIGRTMIN > SIGRTMAX) {
            fprintf(stderr, "Error: Demasiados procesos para las señales RT disponibles\n");
            free(procesos_activos);
            free(indices_pid);
            exit(EXIT_FAILURE);
        }

        if (sigaction(SIGRTMIN + i, &sa_pid, NULL) == -1) {
            perror("sigaction");
            free(procesos_activos);
            free(indices_pid);
            exit(EXIT_FAILURE);
        }
    }

    // Bloquear señales temporalmente
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigaddset(&mask, SIGTERM);
    for (int i = 0; i < num_procesos; i++) {
        sigaddset(&mask, SIGRTMIN + i);
    }
    if (sigprocmask(SIG_BLOCK, &mask, &oldmask) < 0) {
        perror("sigprocmask");
        free(procesos_activos);
        free(indices_pid);
        exit(EXIT_FAILURE);
    }

    // CREAR PROCESOS
    pid_t *pids = (pid_t *)malloc(num_procesos * sizeof(pid_t)); // arreglo de pids
    if (!pids) {
        perror("malloc para pids");
        free(procesos_activos);
        free(indices_pid);
        exit(EXIT_FAILURE);
    }

    int es_hijo = 0;

    for (int i = 0; i < num_procesos && !es_hijo; i++) { //solo el padre ejecuta el ciclo
        pids[i] = fork();

        if (pids[i] < 0) {
            perror("fork");
            free(procesos_activos);
            free(indices_pid);
            free(pids);
            exit(EXIT_FAILURE);
        }
        else if (pids[i] == 0) { // estoy en el hijo
            es_hijo = 1;
            mi_indice = i;
        }
    }

    if (!es_hijo) {  //PROCESO PADRE
        // El padre informa sobre el anillo
        printf("Padre: Configuración del juego - m=%d, token inicial=%d, procesos=%d\n",
               m, token_inicial, num_procesos);

        printf("Padre: Hijos creados con PIDs: ");
        for (int i = 0; i < num_procesos; i++) {
            printf("%d ", pids[i]);
        }
        printf("\n");

        // Configurar el anillo enviando a cada hijo el PID del siguiente (desde el padre)
        for (int i = 0; i < num_procesos; i++) {
            union sigval val;
            val.sival_int = pids[(i + 1) % num_procesos];  // PID del siguiente proceso (prepara para enviar)
            sigqueue(pids[i], SIGUSR2, val);  // SIGUSR2 para configurar el anillo
        }

        // También enviar a cada hijo la lista completa de PIDs
        for (int i = 0; i < num_procesos; i++) {
            for (int j = 0; j < num_procesos; j++) {
                union sigval val;
                val.sival_int = pids[j];
                sigqueue(pids[i], SIGRTMIN + j, val); // Usar señales SIGRTMIN+j para enviar los PIDs
            }
        }

        // Dar tiempo para que todos los hijos procesen la configuración
        sleep(1);

        // Iniciar el token desde el primer hijo
        union sigval val;
        val.sival_int = token_inicial;  // Token inicial
        sigqueue(pids[0], SIGUSR1, val);  // El primer hijo recibe el token

        // Esperar a que los hijos terminen
        for (int i = 0; i < num_procesos; i++) {
            waitpid(pids[i], NULL, 0);
        }

        printf("¡Juego terminado!\n");

        // Liberar memoria
        free(procesos_activos);
        free(indices_pid);
        free(pids);
    }
    else {  // VUELVE A SER CODIGO DEL HIJO
        // No necesitamos el array de pids en el hijo
        free(pids);

        // Esperar a recibir la configuración completa
        bool configuracion_completa = false;
        while (!configuracion_completa) {
            if (siguiente_pid == 0) {
                sigsuspend(&oldmask);
                continue;
            }

            configuracion_completa = true;
            for (int i = 0; i < num_procesos; i++) {
                if (procesos_activos[i] == 0) {
                    configuracion_completa = false;
                    break;
                }
            }

            if (!configuracion_completa) {
                sigsuspend(&oldmask);
            }
        }

        // Configurar mapeo rápido de PIDs a índices
        for (int i = 0; i < num_procesos; i++) {
            if (procesos_activos[i] < 100000) {
                indices_pid[procesos_activos[i]] = i;
            }
        }

        printf("Proceso %d: Configuración completa. Listo para jugar.\n", getpid());

        // Bucle principal de juego
        while (1) {
            // Reiniciar el flag de token recibido
            token_recibido = 0;

            // Esperar a recibir el token
            while (!token_recibido) {
                sigsuspend(&oldmask);
            }

            // Procesar el token
            int resta = random_between(m);
            int nuevo_token = token_valor - resta;
            printf("Proceso %d: token %d - %d = %d\n", getpid(), token_valor, resta, nuevo_token);
            token_valor = nuevo_token;

            if (token_valor >= 0) {
                // Token válido, enviarlo al siguiente
                union sigval val;
                val.sival_int = token_valor;
                sigqueue(siguiente_pid, SIGUSR1, val);
                printf("Proceso %d pasó el token: %d al proceso %d\n",
                       getpid(), token_valor, siguiente_pid);

                // Pequeña pausa para visualizar mejor
                sleep(1);
            } else {
                // Me elimino
                printf("Proceso %d ELIMINADO con token: %d\n", getpid(), token_valor);

                // Notificar a todos los procesos activos sobre mi eliminación
                for (int i = 0; i < num_procesos; i++) {
                    if (procesos_activos[i] != 0 && procesos_activos[i] != getpid()) {
                        union sigval val;
                        val.sival_int = getpid();
                        sigqueue(procesos_activos[i], SIGTERM, val);
                    }
                }

                // Liberar memoria antes de salir
                free(procesos_activos);
                free(indices_pid);

                // Salir del proceso
                exit(0);
            }
        }
    }

    return 0;
}