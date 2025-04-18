# Compilador
CC = gcc

# Opciones de compilación: -Wall para mostrar warnings, -g para debug
CFLAGS = -Wall -g

# Archivos fuente
SRCS = main.c funciones.c

# Nombre del ejecutable
TARGET = desafio1

# Regla principal
all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

# Limpiar archivos generados
clean:
	rm -f $(TARGET)