// Inclusión de bibliotecas necesarias
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <dirent.h>  // Para trabajar con directorios
#include "../inc/lines.h"
#include "../inc/files.h"
#include "../fecha_hora.h"

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

// Función para listar archivos recursivamente
int list_files_recursively(int sc_local, const char *base_dir, const char *current_dir) {
    DIR *dir;
    struct dirent *entry;
    char path[MAX_LINE * 2];
    char relative_path[MAX_LINE * 2];
    struct stat statbuf;

    // Construir la ruta completa
    snprintf(path, sizeof(path), "%s/%s", base_dir, current_dir);
    
    dir = opendir(path);
    if (dir == NULL) {
        perror("Error al abrir directorio");
        return -1;
    }
    
    while ((entry = readdir(dir)) != NULL) {
        // Ignorar . y ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        
        // Construir ruta completa del archivo/directorio actual
        snprintf(path, sizeof(path), "%s/%s/%s", base_dir, current_dir, entry->d_name);
        
        // Construir ruta relativa para enviar al cliente
        if (strlen(current_dir) > 0) {
            snprintf(relative_path, sizeof(relative_path), "%s/%s", current_dir, entry->d_name);
        } else {
            snprintf(relative_path, sizeof(relative_path), "%s", entry->d_name);
        }
        
        if (stat(path, &statbuf) == -1) {
            perror("Error obteniendo estado del archivo");
            continue;
        }
        
        if (S_ISDIR(statbuf.st_mode)) {
            // Es un directorio, explorar recursivamente
            list_files_recursively(sc_local, base_dir, relative_path);
        } else {
            // Es un archivo, enviarlo al cliente
            if (sendMessage(sc_local, relative_path, strlen(relative_path) + 1) < 0) {
                perror("Error enviando archivo");
                closedir(dir);
                return -2;
            }
        }
    }
    
    closedir(dir);
    return 0;
}

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


    
    // Variables para el socket
    char op[MAX_LINE];
    char datetime[MAX_LINE]; // <-- Añadido para la fecha/hora
    char user[MAX_LINE];
    // char client_ip[INET_ADDRSTRLEN];
    int32_t port;
    char path[MAX_LINE];
    char description[MAX_LINE];
    int32_t count;
    char user2[MAX_LINE];

    readLine(sc_local, op, MAX_LINE);
    // Leer la cadena de fecha/hora tras el código de operación
    readLine(sc_local, datetime, MAX_LINE);
    // Si quieres, puedes imprimirla:
    // printf("Fecha/hora recibida: %s\n", datetime);


    readLine(sc_local, user, MAX_LINE);
    printf("s > OPERATION %s FROM %s\n", op, user);
    
    

    if (strcmp(op, "REGISTER") == 0) {
        // Comprobar si el usuario ya existe
        if (exist_user(user) != 0) {
            // Crear el usuario
            create_user(user);
            status = 0;
        }
        else{
            status = 1;
        }

    }
    else if (strcmp(op, "UNREGISTER") == 0) {
        // Comprobar si el usuario existe
        if (exist_user(user) == 0) {
            // Eliminar el usuario
            delete_user(user);
            status = 0;
        }
        else{
            status = 1;
        }
    }
    else if(strcmp(op, "CONNECT") == 0){
        // Leer la IP y el puerto del cliente
        printf("s > CONNECT %s\n", user);
        // readLine(sc_local, client_ip, MAX_LINE  );
        // printf("IP: %s\n", client_ip);
        recvMessage(sc_local, (char *)&port, sizeof(int32_t));
        port = ntohl(port);
        // Comprobar si el usuario ya existe
        if (exist_user(user) == 0){
            // Comprobar si el usuario ya está conectado
            if (user_connected(user) == 0){
                status = 2;
            }
            else{
                // Conectar al usuario
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
        // Comprobar si el usuario existe
        if (exist_user(user) == 0){
            // Comprobar si el usuario está conectado
            if (user_connected(user) != 0){
                status = 2;
            }
            else{
                // Desconectar al usuario
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
        // Leer la ruta y la descripción del contenido
        readLine(sc_local, path, MAX_LINE);
        readLine(sc_local, description, MAX_LINE);
        // Comprobar si el usuario existe
        if (exist_user(user) == 0){
            // Comprobar si el usuario está conectado
            if (user_connected(user) != 0){
                status = 2;
            }
            else{
                // Comprobar si el contenido ya existe
                if (exist_content(user, path) != 0){
                    // Crear el contenido
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
        // Leer la ruta del contenido
        readLine(sc_local, path, MAX_LINE);
        // Comprobar si el usuario existe
        if (exist_user(user) == 0){
            // Comprobar si el usuario está conectado
            if (user_connected(user) != 0){
                status = 2;
            }
            else{
                // Comprobar si el contenido existe
                if (exist_content(path, user) == 0){
                    // Eliminar el contenido
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
        // Comprobar si el usuario existe
        if (exist_user(user) == 0){
            // Comprobar si el usuario está conectado
            if (user_connected(user) != 0){
                status = 2;
            }
            else{
                // Contar archivos en la carpeta connect
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

    else if (strcmp(op, "LIST_CONTENT") == 0){
        // Leer el nombre del usuario
        readLine(sc_local, user2, MAX_LINE);
        // Comprobar si el usuario existe
        if (exist_user(user) == 0){
            // Comprobar si el usuario está conectado
            if (user_connected(user) != 0){
                status = 2;
            }
            else{
                // Comprobar si el usuario existe
                if (exist_user(user2) == 0){
                    // Crear la ruta correctamente usando snprintf
                    char user_dir[MAX_LINE * 2];
                    snprintf(user_dir, sizeof(user_dir), "users/%s", user2);
                    
                    // Contar archivos en la carpeta del usuario
                    count = count_files(user_dir);
                    if (count > 0){
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
    // Realizar conexión  con servidor RPC
    char *host;
    host = getenv("LOG_RPC_IP");
    CLIENT *clnt;
	enum clnt_stat retval_1;
	int result_1;
	cadena obtener_tiempo_servidor_1_user = user;
	cadena obtener_tiempo_servidor_1_op = op;
    if (strcmp(op, "PUBLISH") == 0 || strcmp(op, "DELETE") == 0){
        // Crear un buffer temporal para la concatenación
        char temp_buffer[MAX_LINE * 2]; // Buffer con espacio suficiente
        strcpy(temp_buffer, op);        // Copiar op al buffer
        strcat(temp_buffer, " ");       // Agregar espacio
        strcat(temp_buffer, path);      // Concatenar path al buffer
        obtener_tiempo_servidor_1_op = temp_buffer; // Asignar el resultado
    }
	cadena obtener_tiempo_servidor_1_tiempo = datetime;

    // Crear el cliente RPC
	clnt = clnt_create (host, OBTENER_TIEMPO, OBTENER_TIEMPO_VERS, "udp");
	if (clnt == NULL) {
		clnt_pcreateerror (host);
		exit (1);
	}
    // Llamar a la función remota
	retval_1 = obtener_tiempo_servidor_1(obtener_tiempo_servidor_1_user, obtener_tiempo_servidor_1_op, obtener_tiempo_servidor_1_tiempo, &result_1, clnt);
	if (retval_1 != RPC_SUCCESS) {
		clnt_perror (clnt, "call failed");
	}
    
	clnt_destroy (clnt);

    // Enviar el resultado de la operación al cliente
    status = htonl(status);
    if (sendMessage(sc_local, (char *)&status, sizeof(int32_t)) < 0) {
        perror("Error enviando mensaje de respuesta");
        return -2;
    }
    if (status == 0){
        if (strcmp(op, "LIST_USERS") == 0){
            count = htonl(count);
            // Enviar el número de usuarios conectados
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
                    if (sendMessage(sc_local, entry->d_name, strlen(entry->d_name) + 1) < 0) {
                        perror("Error enviando IP");
                        closedir(dir);
                        return -2;
                    }
                    if (sendMessage(sc_local, ip, strlen(ip) + 1) < 0) {
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
        if (strcmp(op, "LIST_CONTENT") == 0){
            count = htonl(count);
            if (sendMessage(sc_local, (char *)&count, sizeof(int32_t)) < 0) {
                perror("Error enviando mensaje de respuesta");
                return -2;
            }
            
            // Buscar archivos en la carpeta del usuario especificado
            char user_dir[MAX_LINE * 2];
            snprintf(user_dir, sizeof(user_dir), "users/%s", user2);
            
            // Usar la función recursiva para listar archivos
            if (list_files_recursively(sc_local, "users", user2) < 0) {
                perror("Error listando archivos recursivamente");
                return -2;
            }
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

    // Verificar si existe el directorio users
    if (exist_dir("users") != 0) {
        // Crear el directorio users si no existe
        if (mkdir("users", 0777) != 0) {
            perror("Error al crear el directorio users");
            return -1;
        }
    }
    // Verificar si existe el directorio connect
    if (exist_dir("connect") != 0) {
        // Crear el directorio connect si no existe
        if (mkdir("connect", 0777) != 0) {
            perror("Error al crear el directorio connect");
            return -1;
        }
    }
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
    // Driver code
    char* get_public_ip(){
        int fd;
        struct ifreq ifr;
        
        // replace with your interface name
        // or ask user to input
        
        char iface[] = "eth0";
        
        fd = socket(AF_INET, SOCK_DGRAM, 0);

        //Type of address to retrieve - IPv4 IP address
        ifr.ifr_addr.sa_family = AF_INET;

        //Copy the interface name in the ifreq structure
        strncpy(ifr.ifr_name , iface , IFNAMSIZ-1);

        ioctl(fd, SIOCGIFADDR, &ifr);

        close(fd);

        //display result
        // printf("%s - %s\n" , iface , inet_ntoa(( (struct sockaddr_in *)&ifr.ifr_addr )->sin_addr) );


        return inet_ntoa(( (struct sockaddr_in *)&ifr.ifr_addr )->sin_addr);
    }

    char* ip_server = get_public_ip();
    size = sizeof(client_addr);
    printf("s > init server %s:%d\n",
        ip_server, ntohs(server_addr.sin_port));

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

