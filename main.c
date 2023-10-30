#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_FILENAME_LENGTH 100
#define MAX_FILES 100

// Estructuras
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
    int packed;
} header;

// Variable global que lleva la posicion actual del archivo
off_t currentPos = 0;

// Funciones
void createArchive(const char *outputFile, const char *inputFiles[], int numFiles, int verbose);
void extractArchive(const char *archiveFile, const char *outputDirectory, int verbose);
void deleteFile(const char *archiveFile, const char *fileName, int verbose);
void listFiles(const char *archiveFile);
void readData(const char *archiveFile);
void addHeader(int numFiles, int outputFile, const char *fileNames[], int verbose);
void addHeader_Aux(struct FileHeader newFile, int index);
void addFileContent(const char *outputFileName, const char *fileNames[], int numFiles, int verbose);
void appendFile(const char *archiveFile, const char *fileName, int verbose);
struct FileHeader getHeader(const char *tarFileName, const char *fileName);
int openFile(const char *fileName, int mode);
void updateFile(const char *archiveFile, const char *fileName, int verbose);
void appendFile(const char *archiveFile, const char *fileName, int verbose);
void updateHeader(const char *archiveFile, struct Header *updatedHeader);
void updateContent(const char *archiveFile, const char *fileName);
int findSpaceForFile(struct FileHeader newFile, int verbose);
int getNextHeader(int index);
void packFile(const char *archiveFile, int verbose);
void moveFileContent(int fd, off_t src, off_t dest, off_t size);

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
            if (opciones[i] == 'v' || opciones[i] == 'f')
            {
                verbose++;
            }
            else
            {
                break;
            }
        }
    }

    if (strcmp(opciones, "-c") == 0 || strcmp(opciones, "-cv") == 0 || strcmp(opciones, "-cvv") == 0 || strcmp(opciones, "-cvf") == 0)
    {
        createArchive(archivoSalida, archivos, numArchivos, verbose);
    }
    else if (strcmp(opciones, "-x") == 0 || strcmp(opciones, "-xv") == 0 || strcmp(opciones, "-xvv") == 0 || strcmp(opciones, "-xvf") == 0)
    {
        extractArchive(archivoSalida, "./", verbose); // Puedes especificar un directorio de salida
    }
    else if (strcmp(opciones, "--d") == 0 || strcmp(opciones, "--dv") == 0 || strcmp(opciones, "--dvv") == 0 || strcmp(opciones, "--dvf") == 0 || strcmp(opciones, "--delete") == 0)
    {
        deleteFile(archivoSalida, archivos[0], verbose);
    }
    else if (strcmp(opciones, "-t") == 0)
    {
        listFiles(archivoSalida);
    }
    else if (strcmp(opciones, "-rvf") == 0 || strcmp(opciones, "-rv") == 0 || strcmp(opciones, "-rvv") == 0 || strcmp(opciones, "-r") == 0)
    {
        appendFile(archivoSalida, archivos[0], verbose);
    }
    else if (strcmp(opciones, "-uvf") == 0 || strcmp(opciones, "-uv") == 0 || strcmp(opciones, "-uvv") == 0 || strcmp(opciones, "-u") == 0)
    {
        updateFile(archivoSalida, archivos[0], verbose);
    }
    else if (strcmp(opciones, "-p") == 0 || strcmp(opciones, "-pvf") == 0 || strcmp(opciones, "-pvv") == 0 || strcmp(opciones, "-pv") == 0)
    {
        printf("Packing file...\n");
        packFile(archivoSalida, verbose);
    }
    else
    {
        fprintf(stderr, "Opción no válida: %s\n", opciones);
        exit(1);
    }

    return 0;
}

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

        int file = openFile(fileNames[i], 0);                                 // Abre el archivo que se va a escribir en el tar
        struct FileHeader fileInfo = getHeader(outputFileName, fileNames[i]); // Encuentra el archivo en el header
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

void addHeader_Aux(struct FileHeader newFile, int index)
{
    header.packed = 1;
    if (index == 0)
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
    else
    {
        header.fileList[index] = newFile;
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
        addHeader_Aux(fileHeader, 0);
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

void headerInfo()
{
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (header.fileList[i].size != 0)
        {
            printf("File Name: %s\n", header.fileList[i].fileName);
            printf("File Size: %ld bytes\n", (long)header.fileList[i].size);
            printf("File Mode: %o\n", header.fileList[i].mode);
            printf("Start Position: %ld\n", (long)header.fileList[i].start);
            printf("End Position: %ld\n", (long)header.fileList[i].end);
            printf("\n");
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

void mergeEmptySpaces(int fd, int index)
{
    int nextIndex = getNextHeader(index);
    if (nextIndex == -1)
        return; // No hay más encabezados

    struct FileHeader *currentHeader = &header.fileList[index + 1];
    struct FileHeader *nextHeader = &header.fileList[nextIndex - 1];

    if (currentHeader->fileName[0] == '\0' && nextHeader->fileName[0] == '\0')
    {
        // Fusiona los espacios vacíos
        currentHeader->end = nextHeader->end;
        memset(nextHeader, 0, sizeof(struct FileHeader));

        // Escribe el encabezado actualizado en el archivo empaquetado
        lseek(fd, 0, SEEK_SET);
        write(fd, &header, sizeof(struct Header));
    }
}

void deleteFile(const char *archiveFile, const char *fileName, int verbose)
{
    // Abre el archivo empaquetado en modo lectura y escritura
    printf("Deleting file...\n");
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
    if (verbose >= 1)
    {
        printf("Deleting file: %s\n", fileName);
    }

    // Marca el archivo como eliminado en el encabezado
    header.fileList[fileIndex].deleted = 1;

    // Limpia el contenido del archivo en el buffer
    off_t start = header.fileList[fileIndex].start;
    off_t end = header.fileList[fileIndex].end;
    size_t fileSize = end - start;
    if (verbose >= 2)
    {
        printf("File size: %zu bytes\n", fileSize);
    }
    // Buffer de ceros del mismo tamaño que el archivo
    char *zeroBuffer = (char *)malloc(fileSize);
    if (zeroBuffer == NULL)
    {
        perror("Error al asignar memoria para zeroBuffer");
        close(fd);
        exit(1);
    }

    memset(zeroBuffer, 0, fileSize);

    // Busca la posición inicial y escribe ceros para eliminar los datos.
    lseek(fd, start, SEEK_SET);
    write(fd, zeroBuffer, fileSize);

    // Liberar la memoria del buffer
    free(zeroBuffer);

    // Clear the file entry in the header
    memset(&header.fileList[fileIndex], 0, sizeof(struct FileHeader));

    // Escribe el encabezado actualizado en el archivo empaquetado
    lseek(fd, 0, SEEK_SET);
    write(fd, &header, sizeof(struct Header));

    // Fusiona espacios vacíos adyacentes
    // mergeEmptySpaces(fd, fileIndex);

    close(fd);
    if (verbose >= 1)
    {
        printf("File deleted: %s\n", fileName);
    }
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

    int iterationSize = MAX_FILES;

    if (header.packed == 2)
    {
        printf("Se ha empaquetado el archivo\n");
        int filesCount = 0;
        // Itera a través de los archivos en el encabezado y cuenta cuántos archivos hay
        for (int i = 0; i < MAX_FILES; i++)
        {
            if (header.fileList[i].fileName[0] != '\0')
            {
                filesCount++;
            }
        }
        iterationSize = filesCount;
    }

    // Itera a través de los archivos en el encabezado y muestra su información
    for (int i = 0; i < iterationSize; i++)
    {
        if (header.fileList[i].fileName[0] == '\0' && header.fileList[i].start == 0)
        {
            printf("Index Empty: %d\n", i);
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

    // Busca más allá del encabezado hasta la sección de datos
    lseek(fd, sizeof(struct Header), SEEK_SET);

    // Read and print the data
    char buffer[1024];
    ssize_t bytesRead;

    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0)
    {
        // Procesar los datos aquí, por ejemplo, imprimirlos o analizarlos
        // En este ejemplo, simplemente lo imprimiremos
        write(STDOUT_FILENO, buffer, bytesRead);
    }

    close(fd);
}

int getNextHeader(int index)
{
    for (int i = index + 1; i < MAX_FILES; i++)
    {
        if (header.fileList[i].fileName[0] == '\0')
        {

            // printf("Empty header: %d\n", i);
            continue; // Salta los archivos vacíos
        }
        else
        {
            // printf("get Next header: %d\n", i);
            return i;
        }
    }
    return -1; // No more headers
}

// Encuentra el primer espacio adecuado en el header para colocar el nuevo archivo
int findSpaceForFile(struct FileHeader newFile, int verbose)
{
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (header.fileList[i].fileName[0] == '\0')
        {
            int nextHeaderIndex = getNextHeader(i);
            if (nextHeaderIndex == -1)
            {
                printf("No más header\n");
                return i;
            }
            else
            {
                off_t spaceAvailable = header.fileList[nextHeaderIndex].start - 13600;
                if (spaceAvailable >= newFile.size)
                {
                    if (verbose >= 1)
                    {
                        printf("Se encontró espacio en el header %d con: %ld espacio\n", i + 1, spaceAvailable);
                    }
                    return i;
                }
                else
                {
                    i = nextHeaderIndex;
                }
            }
        }
        else
        {
            if (header.fileList[i + 1].fileName[0] == '\0')
            {
                // printf("Next header is empty: %d\n", i + 1);
                int nextHeaderIndex = getNextHeader(i);
                // printf("Next header: %d\n", nextHeaderIndex);
                if (nextHeaderIndex == -1)
                {
                    // printf("No more headers\n");
                    return i + 1;
                }
                else
                {
                    off_t spaceAvailable = header.fileList[nextHeaderIndex].start - header.fileList[i].end;

                    if (spaceAvailable >= newFile.size)
                    {
                        return i + 1;
                    }
                    else
                    {
                        i = nextHeaderIndex;
                    }
                }
            }
            else
            {
                continue;
            }
        }
    }
    return -1; // No se encontró espacio adecuado
}

void updateHeader(const char *archiveFile, struct Header *updatedHeader)
{
    // Abre el archivo en modo lectura-escritura
    int fd = open(archiveFile, O_RDWR);

    if (fd == -1)
    {
        perror("Error opening the archive file");
        exit(1);
    }

    // Encuentra espacios entre archivos consecutivos
    for (int i = 0; i < MAX_FILES - 1; i++)
    {
        if (updatedHeader->fileList[i].fileName[0] != '\0' && updatedHeader->fileList[i + 1].fileName[0] != '\0')
        {
            off_t spaceBetweenFiles = updatedHeader->fileList[i + 1].start - updatedHeader->fileList[i].end;
            if (spaceBetweenFiles > 0)
            {
                // Comprueba si hay un encabezado vacío entre los dos archivos
                int emptyHeaderBetween = 0;
                int nextHeaderIndex = 0;
                for (int j = i + 1; j < MAX_FILES && updatedHeader->fileList[j].fileName[0] != '\0'; j++)
                {
                    if (updatedHeader->fileList[j].fileName[0] == '\0')
                    {
                        emptyHeaderBetween = 1;
                        nextHeaderIndex = j;
                        break;
                    }
                }

                // Si no hay un encabezado vacío entre los dos archivos, crea un espacio
                if (!emptyHeaderBetween)
                {
                    memmove(&updatedHeader->fileList[i + 2], &updatedHeader->fileList[i + 1], sizeof(struct FileHeader) * (MAX_FILES - i - 2));
                    memset(&updatedHeader->fileList[i + 1], 0, sizeof(struct FileHeader));

                    // Actualiza el inicio y el final del nuevo encabezado vacío
                    updatedHeader->fileList[i + 1].start = updatedHeader->fileList[i].end;
                    updatedHeader->fileList[i + 1].end = updatedHeader->fileList[i + 2].start;
                }
            }
        }
    }

    // Escribe el encabezado actualizado nuevamente en el archivo comprimido
    lseek(fd, 0, SEEK_SET);
    if (write(fd, updatedHeader, sizeof(struct Header)) != sizeof(struct Header))
    {
        perror("Error writing the header to the archive file");
        close(fd);
        exit(1);
    }

    close(fd);
}

void updateContent(const char *archiveFile, const char *fileName)
{
    // Abre el archivo en modo lectura-escritura
    int fd = open(archiveFile, O_RDWR);
    if (fd == -1)
    {
        perror("Error abriendo archivo");
        exit(1);
    }

    // Lee el encabezado del archivo comprimido
    struct Header header;
    if (read(fd, &header, sizeof(struct Header)) != sizeof(struct Header))
    {
        perror("Error leyendo archivo");
        close(fd);
        exit(1);
    }

    // Encuentra el encabezado del archivo especificado
    struct FileHeader *fileHeader = NULL;
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (strcmp(header.fileList[i].fileName, fileName) == 0)
        {
            fileHeader = &header.fileList[i];
            break;
        }
    }

    if (fileHeader == NULL)
    {
        fprintf(stderr, "File %s not found in archive\n", fileName);
        close(fd);
        exit(1);
    }

    // Abre el archivo que se agregará al archivo
    int fileFd = open(fileName, O_RDONLY);
    if (fileFd == -1)
    {
        perror("Error opening file to be added");
        close(fd);
        exit(1);
    }

    // Mover a la posición inicial del archivo en el archivo
    if (lseek(fd, fileHeader->start, SEEK_SET) == -1)
    {
        perror("Error seeking in archive file");
        close(fd);
        close(fileFd);
        exit(1);
    }

    // Copia el contenido del archivo al archivo
    char buffer[4096];
    ssize_t bytesRead;
    ssize_t bytesWritten;
    size_t totalBytesWritten = 0;
    while ((bytesRead = read(fileFd, buffer, sizeof(buffer))) > 0)
    {
        bytesWritten = write(fd, buffer, bytesRead);
        if (bytesWritten == -1)
        {
            perror("Error writing to archive file");
            close(fd);
            close(fileFd);
            exit(1);
        }
        totalBytesWritten += bytesWritten;
        if (totalBytesWritten > fileHeader->size)
        {
            fprintf(stderr, "File %s is larger than its header size\n", fileName);
            close(fd);
            close(fileFd);
            exit(1);
        }
    }
    if (bytesRead == -1)
    {
        perror("Error reading from file to be added");
        close(fd);
        close(fileFd);
        exit(1);
    }

    // Si el archivo es más pequeño que el tamaño del encabezado, llena el espacio restante con ceros
    if (totalBytesWritten < fileHeader->size)
    {
        size_t remainingBytes = fileHeader->size - totalBytesWritten;
        memset(buffer, 0, remainingBytes);
        if (write(fd, buffer, remainingBytes) == -1)
        {
            perror("Error writing to archive file");
            close(fd);
            close(fileFd);
            exit(1);
        }
    }

    close(fd);
    close(fileFd);
}

void appendFile(const char *archiveFile, const char *fileName, int verbose)
{
    struct FileHeader newFile;
    struct stat fileStat;
    if (lstat(fileName, &fileStat) == -1)
    {
        perror("Error al obtener información del archivo");
        exit(1);
    }

    newFile.deleted = 0;
    newFile.mode = fileStat.st_mode;
    newFile.size = fileStat.st_size;

    // Abre el archivo empaquetado en modo lectura
    int fd = open(archiveFile, O_RDONLY);

    if (fd == -1)
    {
        perror("Error al abrir el archivo empaquetado");
        exit(1);
    }
    if (verbose >= 1)
    {
        printf("Appending File: %s\n", fileName);
    }

    // Lee el encabezado del archivo empaquetado
    read(fd, &header, sizeof(struct Header));

    int spaceIndex = findSpaceForFile(newFile, verbose);
    int prevHeaderIndex = spaceIndex - 1;
    if (spaceIndex != -1)
    {

        newFile.start = header.fileList[prevHeaderIndex].end;
        newFile.end = newFile.start + newFile.size;
        strncpy(newFile.fileName, fileName, MAX_FILENAME_LENGTH);
        addHeader_Aux(newFile, spaceIndex);
        if (verbose >= 2)
        {
            printf("Placed on index: %d\n", spaceIndex);
        }

        // Actualiza el encabezado en el archivo comprimido
        updateHeader(archiveFile, &header);

        updateContent(archiveFile, fileName);
    }
    else
    {
        printf("No hay espacio para el archivo\n");
    }
    if (verbose >= 1)
    {
        printf("TAR file apended successfully: %s\n", archiveFile);
    }
}

void updateFile(const char *archiveFile, const char *fileName, int verbose)
{
    struct FileHeader fileToUpdate;
    struct stat fileStat;
    int fileIndex = -1;

    // Abre el archivo en modo lectura
    int fd = open(archiveFile, O_RDONLY);
    if (fd == -1)
    {
        perror("Error abriendo archivo");
        exit(1);
    }
    if (verbose == 1)
    {
        printf("Updating File.\n");
    }
    if (verbose >= 2)
    {
        printf("Updating File: %s\n", fileName);
    }
    // Lee el encabezado del archivo comprimido
    struct Header header;
    if (read(fd, &header, sizeof(struct Header)) != sizeof(struct Header))
    {
        perror("Error leyendo archivo");
        close(fd);
        exit(1);
    }
    close(fd);

    // Encuentra el encabezado del archivo especificado
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (strncmp(header.fileList[i].fileName, fileName, MAX_FILENAME_LENGTH) == 0)
        {
            fileToUpdate = header.fileList[i];
            fileIndex = i;
            break;
        }
    }

    if (fileIndex == -1)
    {
        printf("Archivo no encontrado\n");
        return;
    }

    // Get the information of the new file
    if (lstat(fileName, &fileStat) == -1)
    {
        perror("Error obteniendo información del archivo");
        exit(1);
    }

    if (fileStat.st_size > fileToUpdate.size)
    {
        printf("El nuevo archivo es muy grande\n");
        return;
    }

    // Actualiza el contenido del archivo en el archivo
    updateContent(archiveFile, fileName);

    // Actualiza el encabezado del archivo en el encabezado del archivo
    fileToUpdate.mode = fileStat.st_mode;

    // Actualiza las posiciones inicial y final si el tamaño del archivo ha cambiado
    if (fileStat.st_size != fileToUpdate.size)
    {
        fileToUpdate.end = fileToUpdate.start + fileStat.st_size;
        fileToUpdate.size = fileStat.st_size;
    }

    header.fileList[fileIndex] = fileToUpdate;

    // Actualiza el encabezado en el archivo comprimido
    updateHeader(archiveFile, &header);
    if (verbose >= 1)
    {
        printf("Update completed\n");
    }
}

void packFile(const char *archiveFile, int verbose)
{
    // Abre el archivo empaquetado en modo lectura-escritura
    int fd = open(archiveFile, O_RDWR);
    if (fd == -1)
    {
        perror("Error al abrir el archivo empaquetado");
        exit(1);
    }

    // Lee el encabezado del archivo empaquetado
    if (verbose >= 1)
    {
        printf("Packing\n");
    }

    struct Header header;
    if (read(fd, &header, sizeof(struct Header)) != sizeof(struct Header))
    {
        perror("Error al leer el encabezado del archivo empaquetado");
        close(fd);
        exit(1);
    }

    // Compacta el header eliminando los espacios en blanco
    int writeIndex = 0;
    for (int i = 0; i < MAX_FILES; i++)
    {
        if (header.fileList[i].fileName[0] != '\0')
        {
            if (writeIndex != i)
            {
                header.fileList[writeIndex] = header.fileList[i];
                memset(&header.fileList[i], 0, sizeof(struct FileHeader));
            }
            writeIndex++;
        }
    }

    // Escribe el encabezado actualizado al archivo empaquetado
    lseek(fd, 0, SEEK_SET);
    header.packed = 2;
    if (write(fd, &header, sizeof(struct Header)) != sizeof(struct Header))
    {
        perror("Error al escribir el encabezado actualizado al archivo empaquetado");
        close(fd);
        exit(1);
    }

    if (verbose >= 1)
    {
        printf("Pack completed.\n");
    }

    close(fd);
}
