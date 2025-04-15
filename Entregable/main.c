#include "funciones.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    int token_inicial;

    int opt;
    while ((opt = getopt(argc, argv, "m:t:p:h")) != -1) {
        switch (opt) {
            case 'm':
                m = atoi(optarg);
                if (m <= 0) {
                    fprintf(stderr, "Error: El valor de -m debe ser mayor que 0\n");
                    mostrar_ayuda(argv[0]);
                    return EXIT_FAILURE;
                }
                break;
            case 't':
                token_inicial = atoi(optarg);
                if (token_inicial <= 0) {
                    fprintf(stderr, "Error: El token inicial debe ser mayor que 0\n");
                    mostrar_ayuda(argv[0]);
                    return EXIT_FAILURE;
                }
                break;
            case 'p':
                num_procesos = atoi(optarg);
                if (num_procesos <= 0) {
                    fprintf(stderr, "Error: El número de procesos debe ser mayor que 0\n");
                    mostrar_ayuda(argv[0]);
                    return EXIT_FAILURE;
                }
                break;
            case 'h':
            default:
                mostrar_ayuda(argv[0]);
                return EXIT_SUCCESS;
        }
    }
    // Inicializa la semilla para números aleatorios
    srand(time(NULL) ^ getpid());
    // Inicializa estructuras de datos dinámicas
    procesos_activos = calloc(num_procesos, sizeof(pid_t));
    indices_pid = calloc(100000, sizeof(int));
    // Actualiza el valor de procesos_vivos
    procesos_vivos = num_procesos;

    configurar_manejadores();

    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigaddset(&mask, SIGTERM);
    for (int i = 0; i < num_procesos; i++) {
        sigaddset(&mask, SIGRTMIN + i);
    }
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
    // Se crean procesos
    pid_t *pids = malloc(num_procesos * sizeof(pid_t));
    int es_hijo = 0;

    for (int i = 0; i < num_procesos && !es_hijo; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
            es_hijo = 1;
            mi_indice = i;
        }
    }

    if (!es_hijo) { // Codigo del padre
        printf("Padre: m=%d, token inicial=%d, procesos=%d\n", m, token_inicial, num_procesos);
        // Configura el anillo enviando a cada hijo el PID del siguiente (desde el padre)
        for (int i = 0; i < num_procesos; i++) {
            union sigval val = { .sival_int = pids[(i + 1) % num_procesos] };
            sigqueue(pids[i], SIGUSR2, val);
        }
        // Cada proceso i recibirá num_procesos señales,
        // Cada una con el PID de un proceso j, usando la señal SIGRTMIN + j.
        for (int i = 0; i < num_procesos; i++) {
            for (int j = 0; j < num_procesos; j++) {
                union sigval val = { .sival_int = pids[j] };
                sigqueue(pids[i], SIGRTMIN + j, val);
            }
        }

        sleep(1);
        // Inicia el token desde el primer hijo
        union sigval val = { .sival_int = token_inicial };
        sigqueue(pids[0], SIGUSR1, val);
        // Espera a que los hijos terminen
        for (int i = 0; i < num_procesos; i++) {
            waitpid(pids[i], NULL, 0);
        }
        // Libera memoria
        free(procesos_activos);
        free(indices_pid);
        free(pids);
        printf("¡Juego terminado!\n");
    } else { // VUELVE A SER CODIGO DEL HIJO
        // No se necesita el array de pids en el hijo
        free(pids);
        bool configurado = false;
        // Espera a recibir la configuración completa
        while (!configurado) {
            if (siguiente_pid == 0) {
                sigsuspend(&oldmask);
                continue;
            }

            configurado = true;
            for (int i = 0; i < num_procesos; i++) {
                if (procesos_activos[i] == 0) {
                    configurado = false;
                    break;
                }
            }

            if (!configurado) {
                sigsuspend(&oldmask);
            }
        }
        // Realiza mapeo de PIDs a índices
        for (int i = 0; i < num_procesos; i++) {
            indices_pid[procesos_activos[i]] = i;
        }

        printf("Proceso %d listo para jugar\n", getpid());
        // Bucle principal de juego
        while (1) {
            // Reinicia el flag de token recibido
            token_recibido = 0;
            // Espera a recibir el token
            while (!token_recibido) {
                sigsuspend(&oldmask);
            }
            // Procesa el token (le resta random_between(m))
            int resta = random_between(m);
            int nuevo_token = token_valor - resta;
            printf("Proceso %d: token %d - %d = %d\n", getpid(), token_valor, resta, nuevo_token);
            token_valor = nuevo_token;
            //  Si el token es válido, envialo al siguiente
            if (token_valor >= 0) {
                union sigval val = { .sival_int = token_valor };
                sigqueue(siguiente_pid, SIGUSR1, val);
                sleep(1);
            } else {
                // Me eliminaron
                printf("Proceso %d eliminado con token %d\n", getpid(), token_valor);
                // Notificar a todos los procesos activos sobre mi eliminación
                for (int i = 0; i < num_procesos; i++) {
                    if (procesos_activos[i] != 0 && procesos_activos[i] != getpid()) {
                        union sigval val = { .sival_int = getpid() };
                        sigqueue(procesos_activos[i], SIGTERM, val);
                    }
                }
                // Liberar memoria antes de salir
                free(procesos_activos);
                free(indices_pid);
                exit(0);
            }
        }
    }

    return 0;
}