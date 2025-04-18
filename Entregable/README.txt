Este proyecto compila un programa en C que simula un juego de comunicación entre procesos en forma de anillo, utilizando señales.

ARCHIVOS DEL PROYECTO:
-----------------------
- main.c          → Archivo principal
- funciones.c     → Implementación de funciones
- funciones.h     → Declaración de funciones
- Makefile        → Script de compilación
- desafio1        → Ejecutable generado

REQUISITOS:
-----------
- Sistema compatible con POSIX (Linux o similar)
- Compilador GCC
- make

COMPILACIÓN:
------------
Para compilar el proyecto, abre la terminal en este directorio y ejecuta:

    make

Esto generará el ejecutable llamado "desafio1".

EJECUCIÓN:
----------
Para ejecutar el programa con argumentos, por ejemplo:

    ./desafio1 -m 5 -t 20 -p 3

Donde:
- -m : número máximo que puede restar un proceso al token
- -t : valor inicial del token
- -p : número de procesos a crear

