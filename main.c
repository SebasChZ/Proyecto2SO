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

off_t currentPos = 0;

void createArchive(const char *outputFile, const char *inputFiles[], int numFiles, int verbose);
void extractArchive(const char *archiveFile, const char *outputDirectory, int verbose);
void deleteFile(const char *archiveFile, const char *fileName);
void listFiles(const char *archiveFile);
void readData(const char *archiveFile);
void addHeader(int numFiles, int outputFile, const char *fileNames[], int verbose);
void addHeader_Aux(struct FileHeader newFile);
void addFileContent(const char *outputFileName, const char *fileNames[], int numFiles, int verbose);
struct FileHeader getHeader(const char *tarFileName, const char *fileName);
int openFile(const char *fileName, int mode);

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
    int verbose = 0;
    // Lista de nombres de archivos a empaquetar
    const char *archivos[numArchivos];
    for (int i = 0; i < numArchivos; i++)
    {
        archivos[i] = argv[i + 3];
    }
    
    if (argc > 1)
    {
        int len = strlen(opciones);
        for (int i = len - 1; i >= 0; i--)
        {
            if (opciones[i] == 'v')
            {
                verbose++;
            }
            else
            {
                break;
            }
        }
    }

    if (strcmp(opciones, "-c") == 0 || strcmp(opciones, "-cv") == 0 || strcmp(opciones, "-cvv") == 0)
    {
        createArchive(archivoSalida, archivos, numArchivos, verbose);
    }
    else if (strcmp(opciones, "-x") == 0 || strcmp(opciones, "-xv") == 0 || strcmp(opciones, "-xvv") == 0)
    {
        extractArchive(archivoSalida, "./", verbose); // Puedes especificar un directorio de salida
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
// void writeFileContentToTar(const char *tarFileName, const char *fileName)
// {                                                           //! Reemplazar por append
//     int file = openFile(fileName, 0);                       // Abre el archivo que se va a escribir en el tar
//     struct File fileInfo = findFile(tarFileName, fileName); // Encuentra el archivo en el header
//     char buffer[fileInfo.size];                             // Buffer para guardar el contenido del archivo a guardar
//     read(file, buffer, sizeof(buffer));                     // Lee el contenido del archivo y lo guarda en el buffer
//     int tarFile = openFile(tarFileName, 0);
//     lseek(tarFile, fileInfo.start, SEEK_SET); // Coloca el puntero del tar donde de escribir.

//     if (write(tarFile, buffer, sizeof(buffer)) == -1)
//     {
//         perror("writeFileContentToTar: Error al escribir en el archivo");
//         close(tarFile);
//         exit(1);
//     }
//     close(file);
//     close(tarFile);
// }

int openFile(const char *fileName, int mode)
{
    int fd;
    if (mode == 0)
    { // Lectura y escritura sin borrar contenido
        fd = open(fileName, O_RDWR | O_CREAT, 0666);
        if (fd == -1)
        {
            perror("Error al crear el archivo empacado");
            exit(1);
        }
        // printf("Se abre el archivo en opcion: 0 -> O_RDWR  | O_CREAT\n");
        return fd;
    }
    else if (mode == 1)
    { // Crear archivo vacio
        fd = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd == -1)
        {
            perror("Error al crear el archivo empacado");
            exit(1);
        }
        // printf("Se abre el archivo en opcion: 1 -> O_WRONLY | O_CREAT | O_TRUNC\n");
        return fd;
    }
    else if (mode == 2)
    { // Extender el archivo (para el body)
        fd = open(fileName, O_WRONLY | O_APPEND, 0666);
        if (fd == -1)
        {
            perror("Error al crear el archivo empacado");
            exit(1);
        }
        // printf("Se abre el archivo en opcion: 2 -> O_WRONLY | O_APPEND\n");
        return fd;
    }
    else
    {
        printf("Opcion de creacion de archivo incorrecta...\n");
        return -1;
    }
}

struct FileHeader getHeader(const char *tarFileName, const char *fileName)
{
    int tarFile = openFile(tarFileName, 0);
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (strcmp(header.fileList[i].fileName, fileName) == 0)
        { // Encontro el archivo
            close(tarFile);
            return header.fileList[i];
        }
    }
    close(tarFile);
    printf("Error al leer el encabezado\n");
    exit(1);
}

void addFileContent(const char *outputFileName, const char *fileNames[], int numFiles, int verbose)
{
    int outputFile = open(outputFileName, 1);
    for (int i = 0; i < numFiles; i++)
    {
        struct stat fileStat;

        int file = openFile(fileNames[i], 0);
        if (verbose >= 1)
        {
            printf("Adding file: %s\n", fileNames[i]);
        }                                 // Abre el archivo que se va a escribir en el tar
        struct FileHeader fileInfo = getHeader(outputFileName, fileNames[i]); // Encuentra el archivo en el header
        if (verbose >= 2)
        {
            printf("File Size: %ld bytes\n", (long)fileInfo.size);
        }
        char buffer[fileInfo.size];                                           // Buffer para guardar el contenido del archivo a guardar
        read(file, buffer, sizeof(buffer));                                   // Lee el contenido del archivo y lo guarda en el buffer
        int outputFile = openFile(outputFileName, 0);
        lseek(outputFile, fileInfo.start, SEEK_SET); // Coloca el puntero del tar donde de escribir.

        if (write(outputFile, buffer, sizeof(buffer)) == -1)
        {
            perror("writeFileContentToTar: Error al escribir en el archivo");
            close(outputFile);
            exit(1);
        }
        close(file);
        close(outputFile);
    }
}

void addHeader_Aux(struct FileHeader newFile)
{
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (header.fileList[i].size == 0)
        { // Posicion vacia
            header.fileList[i] = newFile;
            break;
        }
    }
}

void addHeader(int numFiles, int outputFile, const char *fileNames[], int verbose)
{
    const char *filename;
    struct FileHeader fileHeader;
    struct stat fileStat;

    for (int i = 0; i < numFiles; i++)
    {
        filename = fileNames[i];
        if (lstat(filename, &fileStat) == -1)
        {
            perror("Error al abrir un archivo de entrada");
            close(outputFile);
            exit(1);
        }

        strncpy(fileHeader.fileName, filename, MAX_FILENAME_LENGTH);
        fileHeader.mode = fileStat.st_mode; // Puedes ajustar el modo según tus necesidades
        if (verbose >= 1)
        {
            printf("Adding file to the archive: %s\n", filename);
        }
        fileHeader.size = fileStat.st_size;
        fileHeader.deleted = 0; // Inicialmente no está marcado como eliminado

        if (currentPos == 0) // Primer archivo
            fileHeader.start = currentPos = sizeof(header) + 1;
        else
            fileHeader.start = currentPos;
        fileHeader.end = currentPos = fileHeader.start + fileStat.st_size;
        addHeader_Aux(fileHeader);
        if (verbose >= 2)
        {
            printf("File Size: %ld bytes\n", (long)fileHeader.size);
        }
    }

    char headerBlock[sizeof(header)];
    memset(headerBlock, 0, sizeof(headerBlock));
    memcpy(headerBlock, &header, sizeof(header));
    if (write(outputFile, headerBlock, sizeof(headerBlock)) != sizeof(headerBlock))
    {
        exit(1);
    }
    close(outputFile);
    if (verbose >= 1)
    {
        printf("Header added to the archive.\n");
    }

}

void printHeader()
{
    printf("HEADER: \n");
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (header.fileList[i].size != 0)
        {
            printf("\tArchivo: %s \t Size: %ld \t Start: %ld \t End: %ld\n", header.fileList[i].fileName, header.fileList[i].size, header.fileList[i].start, header.fileList[i].end);
        }
    }
}

void createArchive(const char *outputFile, const char *inputFiles[], int numFiles, int verbose)
{
    if (verbose >= 1)
    {
        printf("\nCreating TAR file: %s\n", outputFile);
    }
    if (numFiles > MAX_FILES)
    {
        fprintf(stderr, "Error: el número máximo de archivos es %d\n", MAX_FILES);
        exit(1);
    }
    if (verbose >= 1)
    {
        printf("Adding files to the TAR archive:\n");
    }

    addHeader(numFiles, openFile(outputFile, 1), inputFiles, verbose);
    if (verbose >= 2)
    {
        printHeader();
    }
    printHeader();
    addFileContent(outputFile, inputFiles, numFiles, verbose);
    if (verbose >= 1)
    {
        printf("TAR file created successfully: %s\n", outputFile);
    }
}

void extractArchive(const char *archiveFile, const char *outputDirectory, int verbose)
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
    if (verbose >= 1)
    {
        printf("Extracting files from %s\n\n", archiveFile);
    }
    int numFilesExtracted = 0;

    for (int i = 0; i < MAX_FILES; i++)
    {
        if (header.fileList[i].fileName[0] == '\0' || header.fileList[i].deleted)
        {
            continue; // Salta los archivos vacíos o eliminados
        }

        // Construye la ruta completa para el archivo de salida
        char outputPath[1024]; // Aumentar el tamaño del búfer
        snprintf(outputPath, sizeof(outputPath), "%s/%s", outputDirectory, header.fileList[i].fileName);
        if (verbose >= 1)
        {
            printf("Extracting file: %s\n", header.fileList[i].fileName);
        }
        
        if (verbose >= 2)
        {
            printf("File Size: %ld bytes\n", (long)header.fileList[i].size);
            printf("Start Position: %ld\n", (long)header.fileList[i].start);
            printf("End Position: %ld\n", (long)header.fileList[i].end);
        }
        // Crea un nuevo archivo en el sistema de archivos
        int outputFile = open(outputPath, O_WRONLY | O_CREAT, header.fileList[i].mode);

        // Copia el contenido del archivo empaquetado al archivo en el sistema de archivos
        off_t start = header.fileList[i].start;
        off_t end = header.fileList[i].end;
        off_t size = end - start;
        lseek(fd, start, SEEK_SET);

        char buffer[size];
        ssize_t bytesRead;

        while (size > 0 && (bytesRead = read(fd, buffer, sizeof(buffer))) > 0)
        {
            write(outputFile, buffer, bytesRead);
            size -= bytesRead;
        }

        close(outputFile);
        numFilesExtracted++;
        if (verbose >= 1)
        {
            printf("Extracted file: %s\n\n", header.fileList[i].fileName);
        }
    }

    close(fd);
    if (verbose >= 1)
    {
        printf("Extraction complete from %s\n", archiveFile);
    }

    if (verbose >= 2)
    {
        printf("Total files extracted: %d\n", numFilesExtracted);
    }
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