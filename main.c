#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_FILENAME_LENGTH 100
#define MAX_FILES 100

// Definición de la estructura FileHeader
struct FileHeader
{
    char fileName[MAX_FILENAME_LENGTH];
    mode_t mode;
    off_t size;
    off_t start;
    off_t end;
};

// Definición de la estructura Header
struct Header
{
    struct FileHeader fileList[MAX_FILES];
} header;

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        fprintf(stderr, "Uso: %s <opciones> <archivoSalida> <archivo1> <archivo2> ... <archivoN>\n", argv[0]);
        exit(1);
    }

    const char *opciones = argv[1];
    const char *archivoSalida = argv[2];

    // Calcula el número de archivos a empaquetar
    int numArchivos = argc - 3;

    // Lista de nombres de archivos a empaquetar
    const char *archivos[numArchivos];
    for (int i = 0; i < numArchivos; i++)
    {
        archivos[i] = argv[i + 3];
    }

    // Ahora puedes trabajar con las opciones, el archivo de salida y la lista de archivos
    printf("Opciones: %s\n", opciones);
    printf("Archivo de salida: %s\n", archivoSalida);
    printf("Archivos a empaquetar:\n");
    for (int i = 0; i < numArchivos; i++)
    {
        printf("%s\n", archivos[i]);
    }

    return 0;
}