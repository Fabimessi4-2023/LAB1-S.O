#ifndef FUNCIONES_H
#define FUNCIONES_H
#include <signal.h>
#include <sys/types.h>
#include <stdbool.h>





// Variables globales
extern volatile sig_atomic_t token_recibido;
extern volatile sig_atomic_t token_valor;
extern pid_t siguiente_pid;
extern int m;
extern int num_procesos;
extern pid_t *procesos_activos;
extern int *indices_pid;
extern int mi_indice;
extern int procesos_vivos;
extern int token_inicial;




// Funciones
void manejador_token(int sig, siginfo_t *si, void *context);
void manejador_config(int sig, siginfo_t *si, void *context);
void manejador_pid(int sig, siginfo_t *si, void *context);
void manejador_terminado(int sig, siginfo_t *si, void *context);
int random_between(int max);
int encontrar_siguiente_activo(int indice);
void mostrar_ayuda(const char *programa);
void configurar_manejadores();

#endif //FUNCIONES_H