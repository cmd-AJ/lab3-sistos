#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/syscall.h>

#define SIZE 9

int sudoku[SIZE][SIZE];  // Arreglo global 9x9

// Verifica si los números del 1 al 9 están presentes en una fila
int verificarFila(int fila) {
    int nums[10] = {0};
    for (int i = 0; i < SIZE; i++) {
        int val = sudoku[fila][i];
        if (val < 1 || val > 9 || nums[val]) return 0;
        nums[val] = 1;
    }
    return 1;
}

// Verifica si los números del 1 al 9 están presentes en una columna
int verificarColumna(int col) {
    int nums[10] = {0};
    for (int i = 0; i < SIZE; i++) {
        int val = sudoku[i][col];
        if (val < 1 || val > 9 || nums[val]) return 0;
        nums[val] = 1;
    }
    return 1;
}

// Verifica si los números del 1 al 9 están presentes en un subcuadro 3x3
int verificarSubcuadro(int filaInicio, int colInicio) {
    int nums[10] = {0};
    for (int i = filaInicio; i < filaInicio + 3; i++) {
        for (int j = colInicio; j < colInicio + 3; j++) {
            int val = sudoku[i][j];
            if (val < 1 || val > 9 || nums[val]) return 0;
            nums[val] = 1;
        }
    }
    return 1;
}

// Función para el thread: verificar todas las columnas
void* threadVerificarColumnas(void* arg) {
    for (int i = 0; i < SIZE; i++) {
        if (!verificarColumna(i)) {
            printf("Columna %d inválida\n", i + 1);
            pthread_exit((void*)0);
        }
    }
    pthread_exit((void*)1);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <archivo_sudoku>\n", argv[0]);
        return 1;
    }

    // Abrir y mapear archivo
    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("Error al abrir el archivo");
        return 1;
    }

    char* data = mmap(NULL, 81, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        perror("Error al mapear archivo");
        close(fd);
        return 1;
    }

    // Copiar datos a arreglo sudoku[9][9]
    for (int i = 0; i < 81; i++) {
        sudoku[i / 9][i % 9] = data[i] - '0';
    }

    // Verificar subcuadros 3x3
    for (int i = 0; i < SIZE; i += 3) {
        for (int j = 0; j < SIZE; j += 3) {
            if (!verificarSubcuadro(i, j)) {
                printf("Subcuadro en [%d,%d] inválido\n", i + 1, j + 1);
                munmap(data, 81);
                close(fd);
                return 1;
            }
        }
    }

    pid_t padre = getpid();
    pid_t hijo = fork();

    if (hijo == 0) {
        // Proceso hijo: ejecutar ps para proceso padre
        char pid_texto[10];
        snprintf(pid_texto, sizeof(pid_texto), "%d", padre);
        execlp("ps", "ps", "-p", pid_texto, "-lLf", NULL);
        perror("execlp fallo");
        exit(1);
    }

    // Proceso padre
    pthread_t thread;
    void* status;

    if (pthread_create(&thread, NULL, threadVerificarColumnas, NULL) != 0) {
        perror("Error al crear thread");
        munmap(data, 81);
        close(fd);
        return 1;
    }

    pthread_join(thread, &status);
    printf("Thread ejecutado con ID: %ld\n", syscall(SYS_gettid));

    wait(NULL); // Esperar a proceso hijo

    // Verificar filas
    int valido = (int)(size_t)status;
    for (int i = 0; i < SIZE; i++) {
        if (!verificarFila(i)) {
            printf("Fila %d inválida\n", i + 1);
            valido = 0;
        }
    }

    if (valido)
        printf("La solución del sudoku es válida\n");
    else
        printf("La solución del sudoku es inválida\n");

    // Segundo fork para comparar LWP's
    pid_t hijo2 = fork();
    if (hijo2 == 0) {
        char pid_texto[10];
        snprintf(pid_texto, sizeof(pid_texto), "%d", padre);
        execlp("ps", "ps", "-p", pid_texto, "-lLf", NULL);
        perror("execlp fallo");
        exit(1);
    }

    wait(NULL); // Esperar al segundo hijo

    munmap(data, 81);
    close(fd);
    return 0;
}
