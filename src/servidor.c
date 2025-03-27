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
#include "../inc/lines.h"
#include "../inc/files.h"

// Constantes
#define MAX_LINE 256

// Variables globales para sincronización entre hilos
pthread_mutex_t sync_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t sync_cond = PTHREAD_COND_INITIALIZER;
pthread_attr_t attr_thread;
int sync_copied = 0;

// Función que maneja las peticiones de los clientes
int tratar_petición ( void * sc )
{
    int32_t status;

    // Sincronización inicial del hilo
    pthread_mutex_lock(&sync_mutex);
    int sc_local = (*(int *) sc);
    sync_copied = 1;
    pthread_cond_signal(&sync_cond);
    pthread_mutex_unlock(&sync_mutex);

    printf("Tratando petición\n");
    
    // Variables para el socket
    char op[MAX_LINE];
    char user[MAX_LINE];

    readLine(sc_local, op, MAX_LINE);
    printf("Operación: %s\n", op);
    
    if (strcmp(op, "REGISTER") == 0) {
        readLine(sc_local, user, MAX_LINE);
        printf("Usuario: %s\n", user);
        if (exist_user(user) != 0) {
            create_user(user);
            status = 0;
        }
        else{
            status = 1;
        }

    }

    else{
        status = 2;
    }

    // Enviar el resultado de la operación al cliente
    status = htonl(status);
    if (sendMessage(sc_local, (char *)&status, sizeof(int32_t)) < 0) {
        perror("Error enviando mensaje de respuesta");
        return -2;
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
    if (argc != 2) {
        printf("Usage: server <port>\n");
        exit(0);
    }

    // Configuración de la dirección del servidor
    struct sockaddr_in server_addr, client_addr;
    socklen_t size;
    int sc;

    // Inicializar y configurar la dirección del servidor
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    printf("Puerto: %d\n", atoi(argv[1]));
    server_addr.sin_port = htons(atoi(argv[1]));
    bind(sd, (const struct sockaddr *)&server_addr,
    sizeof(server_addr));
    listen(sd, SOMAXCONN);
    size = sizeof(client_addr);
    printf("Servidor en marcha\n");

    // Bucle principal del servidor
    while (1)
    {
        printf("esperando conexion\n");
        
        // Aceptar nueva conexión
        sc = accept(sd, (struct sockaddr *)&client_addr,
        (socklen_t *)&size);

        if(sc < 0) {
            perror("Error en accept");
            exit(1);
        }
        // Crear nuevo hilo para manejar la petición
        if (pthread_create(&thid, &attr_thread, (void *)tratar_petición, (void *)&sc) == -1) {
            perror("Error al crear el thread");
            return -1;
        }
        else{
            pthread_mutex_lock(&sync_mutex) ;
            while (sync_copied == 0) {
                pthread_cond_wait(&sync_cond, &sync_mutex) ;
            }
            sync_copied = 0 ;
            pthread_mutex_unlock(&sync_mutex) ;
        }

    }
}

