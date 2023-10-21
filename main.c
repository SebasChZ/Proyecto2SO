#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_FILENAME_LENGTH 100
#define MAX_FILES 100

struct FileHeader
{
    char fileName[MAX_FILENAME_LENGTH];
    mode_t mode;
    off_t size;
    off_t start;
    off_t end;
    int deleted;
};

struct Header
{
    struct FileHeader fileList[MAX_FILES];
} header;

void createArchive(const char *outputFile, const char *inputFiles[], int numFiles);
void extractArchive(const char *archiveFile, const char *outputDirectory);

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

    if (strcmp(opciones, "-cvf") == 0)
    {
        createArchive(archivoSalida, archivos, numArchivos);
    }
    else if (strcmp(opciones, "-xvf") == 0)
    {
        extractArchive(archivoSalida, "./"); // Puedes especificar un directorio de salida
    }
    // Otras operaciones como adición y eliminación de archivos pueden agregarse aquí.

    return 0;
}

void createArchive(const char *outputFile, const char *inputFiles[], int numFiles)
{
    // Abre el archivo de salida en modo escritura
    int fd = open(outputFile, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);

    if (fd == -1)
    {
        perror("Error al abrir el archivo de salida");
        exit(1);
    }

    // Inicializa el encabezado
    memset(&header, 0, sizeof(struct Header));

    // Abre cada archivo de entrada y copia su contenido al archivo empaquetado
    off_t currentPos = sizeof(struct Header);
    for (int i = 0; i < numFiles; i++)
    {
        int inputFile = open(inputFiles[i], O_RDONLY);
        if (inputFile == -1)
        {
            perror("Error al abrir un archivo de entrada");
            close(fd);
            exit(1);
        }

        // Llena el encabezado con información sobre el archivo
        struct FileHeader fileHeader;
        strcpy(fileHeader.fileName, inputFiles[i]);
        fileHeader.mode = 0; // Puedes ajustar el modo según tus necesidades
        fileHeader.size = lseek(inputFile, 0, SEEK_END);
        fileHeader.start = currentPos;
        fileHeader.end = currentPos + fileHeader.size;
        fileHeader.deleted = 0; // Inicialmente no está marcado como eliminado

        lseek(inputFile, 0, SEEK_SET);

        // Escribe el archivo en el archivo empaquetado
        char buffer[1024];
        ssize_t bytesRead;
        while ((bytesRead = read(inputFile, buffer, sizeof(buffer))) > 0)
        {
            write(fd, buffer, bytesRead);
        }

        // Agrega el archivo al encabezado
        header.fileList[i] = fileHeader;

        // Actualiza la posición actual
        currentPos = fileHeader.end;

        close(inputFile);
    }

    // Escribe el encabezado en el archivo empaquetado
    lseek(fd, 0, SEEK_SET);
    write(fd, &header, sizeof(struct Header));

    close(fd);
}

void extractArchive(const char *archiveFile, const char *outputDirectory)
{
    // Abre el archivo empaquetado en modo lectura
    int fd = open(archiveFile, O_RDONLY);

    if (fd == -1)
    {
        perror("Error al abrir el archivo empaquetado");
        exit(1);
    }

    // Lee el encabezado del archivo empaquetado
    read(fd, &header, sizeof(struct Header));

    // Itera a través de los archivos a extraer
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (header.fileList[i].fileName[0] == '\0' || header.fileList[i].deleted)
        {
            continue; // Salta los archivos vacíos o eliminados
        }

        // Construye la ruta completa para el archivo de salida
        char outputPath[MAX_FILENAME_LENGTH];
        snprintf(outputPath, sizeof(outputPath), "%s/%s", outputDirectory, header.fileList[i].fileName);

        // Crea un nuevo archivo en el sistema de archivos
        int outputFile = open(outputPath, O_WRONLY | O_CREAT, header.fileList[i].mode);

        // Copia el contenido del archivo empaquetado al archivo en el sistema de archivos
        off_t start = header.fileList[i].start;
        off_t size = header.fileList[i].size;
        lseek(fd, start, SEEK_SET);

        char buffer[1024];
        ssize_t bytesRead;

        while (size > 0 && (bytesRead = read(fd, buffer, sizeof(buffer))) > 0)
        {
            write(outputFile, buffer, bytesRead);
            size -= bytesRead;
        }

        close(outputFile);
    }

    close(fd);
}
