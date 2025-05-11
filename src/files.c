/* Contiene la implementacion de las funciones para gestionar y almacenar las tuplas. */ 

// claves.c
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "../inc/files.h"

#define MAX_FILENAME_LENGTH 255






int get_filename(int key, char *filename){
    // obtiene el nombre del archivo a partir de la key
    snprintf(filename, MAX_FILENAME_LENGTH, "./tuplas/%d.dat", key);
    printf("Filename: %s\n", filename);
    return 0;
}

int check_len_string(char* value1){
    if(strlen(value1) > 256){
        perror("Error: value1 debe tener una longitud menor a 256");
        return -1; // Retorna -1 si value1>256
    }
    return 0;
}

int exist_user(char *user) {
    char full_path[512]; // Buffer para almacenar la ruta completa
    snprintf(full_path, sizeof(full_path), "users/%s", user); // Construye la ruta completa

    DIR *direct = opendir(full_path);
    if (direct == NULL) {
        return -1; // Retorna -1 en caso de error
    }
    closedir(direct);
    return 0; // Éxito
}

int exist_dir(char *user){
    DIR *direct = opendir(user);
    if (direct == NULL) {
        return -1; // Retorna -1 en caso de error
    }
    closedir(direct);
    return 0; // Exito
}

// Verifica la existencia de un archivo
int exist_file(char * filename){
    FILE *file;
    file = fopen(filename, "rb");
    if (file == NULL) {
        return -1; // Retorna -1 en caso de error en la creacion
    }
    fclose(file);       
    return 0; // Exito
}

// crea un archivo para wb o rb
int create_file(char * filename, char * mode){
    FILE *file = fopen(filename, mode);
    if (file == NULL) {
        return -1; // Retorna -1 en caso de error en la creacion
    }
    fclose(file);       
    return 0; // Exito
}

int create_user(char *user) {

    
    char full_path[512]; // Buffer para almacenar la ruta completa
    snprintf(full_path, sizeof(full_path), "users/%s", user); // Construye la ruta completa
    if (mkdir(full_path, 0777) != 0) {
        perror("Error al crear el directorio");
        return -1; // Retorna -1 en caso de error
    }
    return 0; // Éxito
}

int connect_user(char *user, char* ip, int port) {

    
    char full_path[512]; 
    snprintf(full_path, sizeof(full_path), "connect/%s.dat", user); 

    FILE *file = fopen(full_path, "wb"); 
    if (file == NULL) {
        perror("Error al crear el archivo en el directorio");
        return -1; // Retorna -1 en caso de error
    }
    if (fwrite(ip, sizeof(char), strlen(ip) + 1, file) != strlen(ip) + 1) {
        fclose(file);
        return -1; // Retorna -1 en caso de error de escritura
    }
    if (fwrite(&port, sizeof(int), 1, file) != 1) {
        fclose(file);
        return -1; // Retorna -1 en caso de error de escritura
    }
    fclose(file);
    return 0; // Éxito
}

int exist_content(char *path, char *user) {
    char full_path[512]; // Buffer para almacenar la ruta completa
    snprintf(full_path, sizeof(full_path), "users/%s/%s", user, path); // Construye la ruta completa

    FILE *file = fopen(full_path, "rb");
    if (file == NULL) {
        return -1; // Retorna -1 en caso de error
    }
    fclose(file);
    return 0; // Éxito
}

int create_content(char *path, char *user) {
    char full_path[512];
    char temp_path[512];
    char *p = NULL;
    size_t len;

    // Construye el path completo con el usuario
    snprintf(full_path, sizeof(full_path), "users/%s/%s", user, path);

    // Copia el path completo a un buffer temporal
    snprintf(temp_path, sizeof(temp_path), "%s", full_path);
    len = strlen(temp_path);

    // Elimina el nombre del archivo del path
    for (p = temp_path + len - 1; p > temp_path && *p != '/'; p--);
    if (p > temp_path) {
        *p = '\0'; // Termina la cadena en el último '/'
    }

    // Crea los directorios necesarios
    for (p = temp_path + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(temp_path, 0777) != 0 && errno != EEXIST) {
                perror("Error al crear directorio");
                return -1;
            }
            *p = '/';
        }
    }

    // Crea el último directorio si no existe
    if (mkdir(temp_path, 0777) != 0 && errno != EEXIST) {
        perror("Error al crear directorio");
        return -1;
    }

    // Crea el archivo al final del path
    FILE *file = fopen(full_path, "wb");
    if (file == NULL) {
        perror("Error al crear el archivo");
        return -1;
    }
    fclose(file);
    return 0; // Éxito
}

// Función recursiva auxiliar para eliminar directorios y su contenido
int delete_directory(const char *path) {
    DIR *dir;
    struct dirent *entry;
    char full_path[512];

    // Abre el directorio
    dir = opendir(path);
    if (dir == NULL) {
        perror("Error al abrir el directorio");
        return -1;
    }

    // Recorre el contenido del directorio
    while ((entry = readdir(dir)) != NULL) {
        // Omite "." y ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construye la ruta completa
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        // Obtiene información del archivo
        struct stat statbuf;
        if (stat(full_path, &statbuf) == -1) {
            perror("Error al obtener información del archivo");
            closedir(dir);
            return -1;
        }

        // Si es un directorio, elimina recursivamente
        if (S_ISDIR(statbuf.st_mode)) {
            if (delete_directory(full_path) == -1) {
                closedir(dir);
                return -1;
            }
        } else {
            // Es un archivo, elimínalo
            if (remove(full_path) == -1) {
                perror("Error al eliminar archivo");
                closedir(dir);
                return -1;
            }
        }
    }

    closedir(dir);

    // Finalmente, elimina el directorio
    if (rmdir(path) == -1) {
        perror("Error al eliminar directorio");
        return -1;
    }

    return 0;
}

int delete_content(char *path, char *user) {
    char full_path[512]; // Buffer para almacenar la ruta completa
    snprintf(full_path, sizeof(full_path), "users/%s/%s", user, path); // Construye la ruta completa
    
    // Determina si el path es un archivo o directorio
    struct stat statbuf;
    if (stat(full_path, &statbuf) == -1) {
        perror("Error al obtener información del archivo/directorio");
        return -1;
    }
    
    // Si es un directorio, usa la función recursiva
    if (S_ISDIR(statbuf.st_mode)) {
        return delete_directory(full_path);
    } else {
        // Es un archivo, usa remove como antes
        if (remove(full_path) != 0) {
            perror("Error al eliminar el archivo");
            return -1;
        }
        return 0; // Éxito
    }
}

int count_files(const char *path) {
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char full_path[512];
    int file_count = 0;

    // Abre el directorio
    dir = opendir(path);
    if (dir == NULL) {
        perror("Error al abrir el directorio");
        return -1; // Retorna -1 en caso de error
    }

    // Recorre el contenido del directorio
    while ((entry = readdir(dir)) != NULL) {
        // Omite las entradas "." y ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construye la ruta completa
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        // Obtiene información del archivo
        if (stat(full_path, &statbuf) == -1) {
            perror("Error al obtener información del archivo");
            closedir(dir);
            return -1;
        }

        // Si es un directorio, llama recursivamente
        if (S_ISDIR(statbuf.st_mode)) {
            int subdir_count = count_files(full_path);
            if (subdir_count == -1) {
                closedir(dir);
                return -1; // Propaga el error
            }
            file_count += subdir_count;
        } else if (S_ISREG(statbuf.st_mode)) {
            // Si es un archivo regular, incrementa el contador
            file_count++;
        }
    }

    closedir(dir);
    return file_count; // Retorna el número total de archivos
}

int count_directories(const char *path) {
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char full_path[512];
    int file_count = 0;

    // Abre el directorio
    dir = opendir(path);
    if (dir == NULL) {
        perror("Error al abrir el directorio");
        return -1; // Retorna -1 en caso de error
    }

    // Recorre el contenido del directorio
    while ((entry = readdir(dir)) != NULL) {
        // Omite las entradas "." y ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construye la ruta completa
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        // Obtiene información del archivo
        if (stat(full_path, &statbuf) == -1) {
            perror("Error al obtener información del archivo");
            closedir(dir);
            return -1;
        }

        // Si es un directorio, llama recursivamente
        if (S_ISDIR(statbuf.st_mode)) {
            int subdir_count = count_files(full_path);
            if (subdir_count == -1) {
                closedir(dir);
                return -1; // Propaga el error
            }
            file_count += subdir_count;
        } else if (S_ISREG(statbuf.st_mode)) {
            // Si es un archivo regular, incrementa el contador
            file_count++;
        }
    }

    closedir(dir);
    return file_count; // Retorna el número total de archivos
}

int disconnect_user(char *user) {
    char full_path[512]; // Buffer para almacenar la ruta completa
    snprintf(full_path, sizeof(full_path), "connect/%s.dat", user); // Construye la ruta completa
    if (remove(full_path) != 0) {
        perror("Error al eliminar el directorio");
        return -1; // Retorna -1 en caso de error
    }
    return 0; // Éxito
}

int get_user_information(char *user, char *ip, int *port) {
    char full_path[512]; 
    FILE *file;
    
    // Construct the path to the user's connection file
    snprintf(full_path, sizeof(full_path), "connect/%s", user);
    printf("Path: %s\n", full_path);
    
    // Open the file for reading
    file = fopen(full_path, "rb");
    if (file == NULL) {
        perror("Error al abrir el archivo de conexión del usuario");
        return -1; // Return error
    }
    
    // Read the IP (stored as a null-terminated string)
    char c;
    int i = 0;
    while (fread(&c, sizeof(char), 1, file) == 1 && c != '\0' && i < 255) {
        ip[i++] = c;
    }
    ip[i] = '\0'; // Ensure null-termination
    
    // Read the port (stored as an integer)
    if (fread(port, sizeof(int), 1, file) != 1) {
        perror("Error al leer el puerto del archivo");
        fclose(file);
        return -1;
    }
    
    fclose(file);
    return 0; // Success
}
int delete_user(char *user) {
    DIR *dir;
    struct dirent *entry;
    char path[512]; // Buffer para almacenar la ruta completa
    snprintf(path, sizeof(path), "users/%s", user); // Construye la ruta completa
    char full_path[1024]; // Buffer para almacenar la ruta completa

    // Abre el directorio
    dir = opendir(path);
    if (dir == NULL) {
        perror("Error al abrir el directorio");
        return -1;
    }

    // Recorre el contenido del directorio
    while ((entry = readdir(dir)) != NULL) {
        // Omite "." y ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construye la ruta completa
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        // Obtiene información del archivo
        struct stat statbuf;
        if (stat(full_path, &statbuf) == -1) {
            perror("Error al obtener información del archivo");
            closedir(dir);
            return -1;
        }

        // Si es un directorio, elimina recursivamente
        if (S_ISDIR(statbuf.st_mode)) {
            if (delete_directory(full_path) == -1) {
                closedir(dir);
                return -1;
            }
        } else {
            // Es un archivo, elimínalo
            if (remove(full_path) == -1) {
                perror("Error al eliminar archivo");
                closedir(dir);
                return -1;
            }
        }
    }

    closedir(dir);

    // Finalmente, elimina el directorio
    if (rmdir(path) == -1) {
        perror("Error al eliminar directorio");
        return -1;
    }

    return 0;
}

int user_connected(char *user){
    char full_path[512]; // Buffer para almacenar la ruta completa
    snprintf(full_path, sizeof(full_path), "connect/%s.dat", user); // Construye la ruta completa
    FILE *file = fopen(full_path, "rb");
    if (file == NULL) {
        return -1; // Retorna -1 en caso de error
    }
    fclose(file);
    return 0; // Éxito
}

int write_to_file(void* data, size_t size, size_t count, FILE* file) {
    if (fwrite(data, size, count, file) != count) {
        fclose(file);
        return -1; // Retorna -1 en caso de error de escritura
    }
    return 0;
}

int check_value1(char* value1){
    if(strlen(value1) > 256){
        perror("Error: value1 debe tener una longitud menor a 256");
        return -1; // Retorna -1 si value1>256
    }
    return 0;
}

int check_Nvalue2(int N_value2){
    if (N_value2 < 1 || N_value2 > 32) {
        perror("Error: N_value2 debe estar entre 1 y 32");
        return -1; // Retorna -1 si N_value>32 o N_value<1
    }
    return 0;
}


int set_value_server(int key, char *value1, int N_value2, double *V_value2,
    struct Coord value3) {
    char filename[MAX_FILENAME_LENGTH]; // Nombre del archivo
    FILE *file; 
    
    get_filename(key, filename);

    if (check_len_string(value1) != 0) {
        return -1; // Retorna -1 si value1>256
    }

    if (check_Nvalue2(N_value2) != 0) {
        return -1; // Retorna -1 si N_value>32 o N_value<1
    }

    // Si ya existe un archivo con el mismo nombre retorna -1
    if (exist_file(filename) == 0) {
        perror("SETVALUE Error, el fichero ya existe");
        return -1; 
    }

    // Crea el archivo binario (lo abre para escritura)
    file = fopen(filename, "wb");
    if (file == NULL) {
        perror("SETVALUE Error, error al abrir el archivo");
        return -1; // Retorna -1 en caso de error
    }

    // Escribe el valor1 en el archivo binario
    if (write_to_file(value1, sizeof(char), strlen(value1) + 1, file) != 0) {
        perror("SETVALUE Error, error al escribir 'value1' en el archivo");
        return -1;
    }

    // Escribe el N_value2 en el archivo binario 
    if (write_to_file(&N_value2, sizeof(int), 1, file) != 0) {
        perror("SETVALUE Error, error al escribir 'N_value2' en el archivo");
        return -1;
    }

    // Escribe el valor2 en el archivo binario
    if (write_to_file(V_value2, sizeof(double), N_value2, file) != 0) {
        perror("SETVALUE Error, error al escribir 'V_value2' en el archivo");
        return -1;
    }

    // Escribe el valor3 en el archivo binario
    if (write_to_file(&value3, sizeof(struct Coord), 1, file) != 0) {
        perror("SETVALUE Error, error al escribir 'value3' en el archivo");
        return -1;
    }

    printf("Peticion SET_VALUE realizada con exito\n");
    fclose(file);
    return 0; // Éxito    
}


int get_value_server(int key, char *value1, int *N_value2, double *V_value2, struct Coord *value3) {
    char filename[MAX_FILENAME_LENGTH];
    FILE *file;
    
    get_filename(key, filename);

    // Comprueba si existe un archivo para dicha key
    if (exist_file(filename) != 0) {
        perror("GETVALUE Error, el archivo no existe");
        return -1;
    }

    // Abre el archivo para lectura
    file = fopen(filename, "rb");

    // Variables para almacenar la cadena leída y su longitud
    char cadena[256];
    size_t longitud = 0;
    char caracter;

    // Lee el archivo hasta encontrar el carácter nulo (valo1 y valor2 e escriben de manera consecutiva -> sabemos que el valor1 termina en '\0' y despues empieza el valor2)
    while (fread(&caracter, sizeof(char), 1, file) == 1 && caracter != '\0') {
        // Aumenta el tamaño del buffer y agrega el carácter leído
        cadena[longitud++] = caracter;
    }

    // Agrega el carácter nulo al final de la cadena

    cadena[longitud] = '\0';

    // Asigna la cadena al puntero value 1 recibido como argumento
    strcpy(value1, cadena);

    // Lee el N_value2 del archivo binario
    if (fread(N_value2, sizeof(int), 1, file) != 1) {
        perror("GETVALUE Error, error al obtener N_value2 del archivo");
        fclose(file);
        return -1;
    }

    // Lee el value2 (vector de doubles) del archivo binario
    if (fread(V_value2, sizeof(double), *N_value2, file) != *N_value2) {
        perror("GETVALUE Error, error al obtener value2 del archivo");
        fclose(file);
        return -1;
    }

    // Lee el value3 (struct Coord) del archivo binario
    if (fread(value3, sizeof(struct Coord), 1, file) != 1) {
        perror("GETVALUE Error, error al obtener value3 del archivo");
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}


int exist_server(int key){
    char filename[MAX_FILENAME_LENGTH];
    get_filename(key, filename);

    if (exist_file(filename) != 0){
        perror("Error, el archivo no existe");
        return 0;
    }
    printf("El archivo con key %d existe.\n", key);
    return 1; // Exito
}

int delete_key_server(int key){
    char filename[MAX_FILENAME_LENGTH];

    get_filename(key, filename);

    if (exist_server(key) != 1) { // retorna -1 si no existe el archivo
        perror("Error al eliminar el archivo, no existe");
        return -1;
    }

    // Elimina el archivo y su contenido
    if (remove(filename) != 0) {
        perror("Error al eliminar el archivo");
        return -1; // Retorna -1 en caso de error
    }
    return 0;
}



