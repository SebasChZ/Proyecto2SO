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
void deleteFile(const char *archiveFile, const char *fileName);
void listFiles(const char *archiveFile);
void readData(const char *archiveFile);

int main(int argc, char *argv[])
{
    if (argc < 1)
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
    else if (strcmp(opciones, "--delete") == 0)
    {
        deleteFile(archivoSalida, archivos[0]);
    }
    else if (strcmp(opciones, "-t") == 0)
    {
        listFiles(archivoSalida);
    }
    else if (strcmp(opciones, "-r") == 0)
    {
        readData(archivoSalida);
    }
    else
    {
        fprintf(stderr, "Opción no válida: %s\n", opciones);
        exit(1);
    }

    return 0;
}

// TEST
/*
    Funcion para escribir el contenido de un archivo en el tar.
    tarFileName es el nombre del archivo tar.
    fileName es el nombre del archivo del cual se grabara el contenido en el body del tar.
*/
void writeFileContentToTar(const char *tarFileName, const char *fileName)
{                                                           //! Reemplazar por append
    int file = openFile(fileName, 0);                       // Abre el archivo que se va a escribir en el tar
    struct File fileInfo = findFile(tarFileName, fileName); // Encuentra el archivo en el header
    char buffer[fileInfo.size];                             // Buffer para guardar el contenido del archivo a guardar
    read(file, buffer, sizeof(buffer));                     // Lee el contenido del archivo y lo guarda en el buffer
    int tarFile = openFile(tarFileName, 0);
    lseek(tarFile, fileInfo.start, SEEK_SET); // Coloca el puntero del tar donde de escribir.

    if (write(tarFile, buffer, sizeof(buffer)) == -1)
    {
        perror("writeFileContentToTar: Error al escribir en el archivo");
        close(tarFile);
        exit(1);
    }
    close(file);
    close(tarFile);
}

void createArchive(const char *outputFile, const char *inputFiles[], int numFiles)
{
    if (numFiles > MAX_FILES)
    {
        fprintf(stderr, "Error: el número máximo de archivos es %d\n", MAX_FILES);
        exit(1);
    }

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

        lseek(inputFile, currentPos, SEEK_SET);

        // Escribe el archivo en el archivo empaquetado
        char buffer[fileHeader.size];
        ssize_t bytesRead;
        while ((bytesRead = read(inputFile, buffer, sizeof(buffer))) > 0)
        {
            write(fd, buffer, bytesRead);
        }

        // Add a print statement to show what is being written
        printf("Writing file: %s, size: %ld bytes\n", fileHeader.fileName, (long)fileHeader.size);

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
        // snprintf(outputPath, sizeof(outputPath), "%s/%s", outputDirectory, header.fileList[i].fileName);

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

void listFiles(const char *archiveFile)
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

    // Itera a través de los archivos en el encabezado y muestra su información
    for (int i = 0; i < MAX_FILES; i++)
    {

        if (header.fileList[i].fileName[0] == '\0')
        {
            printf("Index deleted: %d\n", i);
            continue; // Salta los archivos vacíos
        }

        printf("File Name: %s\n", header.fileList[i].fileName);
        printf("File Size: %ld bytes\n", (long)header.fileList[i].size);
        printf("File Mode: %o\n", header.fileList[i].mode);
        printf("Start Position: %ld\n", (long)header.fileList[i].start);
        printf("End Position: %ld\n", (long)header.fileList[i].end);
        printf("Deleted: %s\n", header.fileList[i].deleted ? "Yes" : "No");
        printf("Index: %d\n", i);
        printf("\n");
    }

    close(fd);
}

void deleteFile(const char *archiveFile, const char *fileName)
{
    // Abre el archivo empaquetado en modo lectura y escritura
    int fd = open(archiveFile, O_RDWR);

    if (fd == -1)
    {
        perror("Error al abrir el archivo empaquetado");
        exit(1);
    }

    // Lee el encabezado del archivo empaquetado
    read(fd, &header, sizeof(struct Header));

    // Busca el archivo por nombre en el encabezado
    int fileIndex = -1;
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (strcmp(header.fileList[i].fileName, fileName) == 0)
        {
            fileIndex = i;
            break;
        }
    }

    if (fileIndex == -1)
    {
        fprintf(stderr, "Archivo no encontrado: %s\n", fileName);
        close(fd);
        exit(1);
    }

    // Marca el archivo como eliminado en el encabezado
    header.fileList[fileIndex].deleted = 1;

    // Limpia el contenido del archivo en el buffer
    off_t start = header.fileList[fileIndex].start;
    off_t end = header.fileList[fileIndex].end;
    size_t fileSize = end - start;

    // Buffer de ceros del mismo tamaño que el archivo
    char *zeroBuffer = (char *)malloc(fileSize);
    if (zeroBuffer == NULL)
    {
        perror("Error al asignar memoria para zeroBuffer");
        close(fd);
        exit(1);
    }

    memset(zeroBuffer, 0, fileSize);

    // Seek to the start position and write zeros to delete the data
    lseek(fd, start, SEEK_SET);
    write(fd, zeroBuffer, fileSize);

    // Liberar la memoria del buffer
    free(zeroBuffer);

    // Clear the file entry in the header
    memset(&header.fileList[fileIndex], 0, sizeof(struct FileHeader));

    // Escribe el encabezado actualizado en el archivo empaquetado
    lseek(fd, 0, SEEK_SET);
    write(fd, &header, sizeof(struct Header));

    close(fd);
}

void readData(const char *archiveFile)
{
    // Abre el archivo empaquetado en modo lectura
    fprintf(stderr, "Archivo no encontrado: %s\n", archiveFile);
    int fd = open(archiveFile, O_RDONLY);

    if (fd == -1)
    {
        perror("Error al abrir el archivo empaquetado");
        exit(1);
    }

    // Lee el encabezado del archivo empaquetado
    read(fd, &header, sizeof(struct Header));

    // Seek past the header to the data section
    lseek(fd, sizeof(struct Header), SEEK_SET);

    // Read and print the data
    char buffer[1024];
    ssize_t bytesRead;

    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0)
    {
        // Process the data here, e.g., print it or analyze it
        // In this example, we'll just print it
        write(STDOUT_FILENO, buffer, bytesRead);
    }

    close(fd);
}