// Inclusión de bibliotecas necesarias
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>  // Para trabajar con directorios
#include "../inc/lines.h"
#include "../inc/files.h"

// Constantes
#define MAX_LINE 256

// Estructura para pasar datos al hilo
struct thread_data {
    int socket;
    char client_ip[INET_ADDRSTRLEN];
};

// Variables globales para sincronización entre hilos
pthread_mutex_t sync_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t sync_cond = PTHREAD_COND_INITIALIZER;
pthread_attr_t attr_thread;
int sync_copied = 0;

// Función que maneja las peticiones de los clientes
int tratar_petición(void *arg)
{
    int32_t status;
    struct thread_data *my_data = (struct thread_data *)arg;

    // Sincronización inicial del hilo
    pthread_mutex_lock(&sync_mutex);
    int sc_local = my_data->socket;
    char client_ip_local[INET_ADDRSTRLEN];
    strcpy(client_ip_local, my_data->client_ip);
    sync_copied = 1;
    pthread_cond_signal(&sync_cond);
    pthread_mutex_unlock(&sync_mutex);

    // printf("Tratando petición desde %s\n", client_ip_local);
    
    // Variables para el socket
    char op[MAX_LINE];
    char user[MAX_LINE];
    int32_t port;
    char path[MAX_LINE];
    char description[MAX_LINE];
    int32_t count;

    readLine(sc_local, op, MAX_LINE);
    readLine(sc_local, user, MAX_LINE);
    printf("s > OPERATION %s FROM %s\n", op, user);
    
    if (strcmp(op, "REGISTER") == 0) {

        if (exist_user(user) != 0) {
            create_user(user);
            status = 0;
        }
        else{
            status = 1;
        }

    }
    else if (strcmp(op, "UNREGISTER") == 0) {
        if (exist_user(user) == 0) {
            delete_user(user);
            status = 0;
        }
        else{
            status = 1;
        }
    }
    else if(strcmp(op, "CONNECT") == 0){
        recvMessage(sc_local, (char *)&port, sizeof(int32_t));
        port = ntohl(port);
        if (exist_user(user) == 0){
            if (user_connected(user) == 0){
                status = 2;
            }
            else{
                if (connect_user(user, client_ip_local, port) == 0){
                    status = 0;
                }
                else{
                    status = 3;
                }
            }
        }
        else{
            status = 1;
        }
    }
    else if (strcmp(op, "DISCONNECT") == 0){
        if (exist_user(user) == 0){
            if (user_connected(user) != 0){
                status = 2;
            }
            else{
                if (disconnect_user(user) == 0){
                    status = 0;
                }
                else{
                    status = 3;
                }
            }
        }
        else{
            status = 1;
        }
    }

    else if (strcmp(op, "PUBLISH") == 0){
        readLine(sc_local, path, MAX_LINE);
        readLine(sc_local, description, MAX_LINE);
        if (exist_user(user) == 0){
            if (user_connected(user) != 0){
                status = 2;
            }
            else{
                if (exist_content(user, path) != 0){
                    if (create_content(path, user) == 0){
                        status = 0;
                    }
                    else{
                        status = 4;
                    }
                }
                else{
                    status = 3;
                }
            }
        }
        else{
            status = 1;
        }
    }

    else if (strcmp(op, "DELETE") == 0){
        readLine(sc_local, path, MAX_LINE);
        if (exist_user(user) == 0){
            if (user_connected(user) != 0){
                status = 2;
            }
            else{
                if (exist_content(path, user) == 0){
                    if (delete_content(path, user) == 0){
                        status = 0;
                    }
                    else{
                        status = 4;
                    }
                }
                else{
                    status = 3;
                }
            }
        }
        else{
            status = 1;
        }
    }
    else if (strcmp(op, "LIST_USERS") == 0){
        if (exist_user(user) == 0){
            
            if (user_connected(user) != 0){
                status = 2;
            }
            else{
                printf("s > LIST_USERS %s\n", user);
                count = count_files("connect");
                if (count > 0){

                    status = 0;
                }
                else{
                    status = 3;
                }
            }
        }
        else{
            status = 1;
        }
    }

    else{
        status = -1;
    }
    

    // Enviar el resultado de la operación al cliente
    status = htonl(status);
    if (sendMessage(sc_local, (char *)&status, sizeof(int32_t)) < 0) {
        perror("Error enviando mensaje de respuesta");
        return -2;
    }
    if (status == 0){
        if (strcmp(op, "LIST_USERS") == 0){
            count = htonl(count);
            if (sendMessage(sc_local, (char *)&count, sizeof(int32_t)) < 0) {
                perror("Error enviando mensaje de respuesta");
                return -2;
            }
            
            // Buscar archivos en la carpeta connect
            DIR *dir;
            struct dirent *entry;
            char ip[MAX_LINE];
            int port;
            
            // Abrir el directorio connect
            dir = opendir("connect");
            if (dir == NULL) {
                perror("Error al abrir directorio connect");
                return -2;
            }
            
            // Leer cada entrada del directorio
            while ((entry = readdir(dir)) != NULL) {
                // Ignorar . y ..
                if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                    continue;
                if (get_user_information( entry->d_name, ip, &port) == 0) {
                    // Enviar la IP y el puerto al cliente
                    if (sendMessage(sc_local, ip, sizeof(ip)) < 0) {
                        perror("Error enviando IP");
                        closedir(dir);
                        return -2;
                    }
                    port = htonl(port);
                    if (sendMessage(sc_local, (char *)&port, sizeof(int32_t)) < 0) {
                        perror("Error enviando puerto");
                        closedir(dir);
                        return -2;
                    }
                } else {
                    perror("Error al obtener información del usuario");
                    closedir(dir);
                }
            }
            
            // Cerrar el directorio
            closedir(dir);
        }
    }

    close(sc_local);
    return 0;
}

// Función principal
int main(int argc, char *argv[])
{
    pthread_t thid;

    // Variables para el socket
    int sd;                     // Descriptor del socket
    int val;                    // Valor para opciones del socket
    int err;                    // Control de errores

    // Crear socket TCP
    sd =  socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sd < 0) {
        perror("Error in socket");
        exit(1);
    }

    val = 1;
    err = setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *) &val,
            sizeof(int));
    if (err < 0) {
        perror("Error in opction");
        exit(1);
    }

    // Verificar argumentos de línea de comandos
    if (argc != 3 || strcmp(argv[1], "-p") != 0) {
        printf("Usage: server -p <port>\n");
        exit(0);
    }

    int port = atoi(argv[2]);
    if (port <= 0) {
        printf("Error: El puerto debe ser un número válido.\n");
        exit(1);
    }

    // Configuración de la dirección del servidor
    struct sockaddr_in server_addr, client_addr;
    socklen_t size;
    int sc;

    // Inicializar y configurar la dirección del servidor
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    printf("Puerto: %d\n", port);
    server_addr.sin_port = htons(port);
    bind(sd, (const struct sockaddr *)&server_addr,
    sizeof(server_addr));
    
    listen(sd, SOMAXCONN);
    size = sizeof(client_addr);
    printf("s > init server %s:%d\n",
        inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));

    // Bucle principal del servidor
    while (1)
    {
        printf("s>\n");
        
        // Aceptar nueva conexión
        sc = accept(sd, (struct sockaddr *)&client_addr,
        (socklen_t *)&size);

        if(sc < 0) {
            perror("Error en accept");
            exit(1);
        }

        // Preparar datos para el hilo
        struct thread_data *data = malloc(sizeof(struct thread_data));
        data->socket = sc;
        inet_ntop(AF_INET, &(client_addr.sin_addr), data->client_ip, INET_ADDRSTRLEN);

        // Crear nuevo hilo para manejar la petición
        if (pthread_create(&thid, &attr_thread, (void *)tratar_petición, (void *)data) == -1) {
            perror("Error al crear el thread");
            free(data);
            return -1;
        }
        else{
            pthread_mutex_lock(&sync_mutex) ;
            while (sync_copied == 0) {
                pthread_cond_wait(&sync_cond, &sync_mutex) ;
            }
            sync_copied = 0 ;
            pthread_mutex_unlock(&sync_mutex) ;
            free(data);
        }

    }
}

