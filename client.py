from enum import Enum

import argparse
import requests
import socket
import os
import threading

import struct
from zeep import Client as ZeepClient

# Variables globales para control del cliente
stop_event = threading.Event()  # Evento para señalizar la detención del hilo de servicio
service_thread_instance = None  # Instancia del hilo de servicio
user_socket = None              # Socket para recepción de solicitudes de otros clientes
user_connected = ''             # Usuario actualmente conectado
list_users = {}                 # Diccionario para almacenar información de otros usuarios

def get_public_ip():
    """
    Obtiene la IP pública del cliente consultando un servicio externo.
    Retorna la IP local si no es posible obtener la IP pública.
    """
    try:
        # Consultar a un servicio que devuelve la IP pública
        response = requests.get('https://api.ipify.org', timeout=5)
        return response.text
    except Exception as e:
        print(f"Error obteniendo IP pública: {e}")
        # Devolver IP local si falla la obtención de la IP pública
        return socket.gethostbyname(socket.gethostname())

def readInt32(sock):
    """
    Lee un entero de 32 bits desde el socket.
    Utiliza big-endian para la interpretación de los bytes.
    Retorna: el entero leído.
    """
    data = sock.recv(4)  # Read 4 bytes from the socket
    if len(data) < 4:
        raise ValueError("Incomplete data received for int32")
    return struct.unpack('!I', data)[0]  # Unpack as a big-endian int

def readString(sock):
    """
    Lee una cadena de caracteres desde el socket hasta encontrar un byte nulo.
    Retorna: la cadena leída.
    """
    a = ''
    while True:
        msg = sock.recv(1)
        if (msg == b'\0'):
            break
        a += msg.decode()
    return(a)

def get_datetime_string():
    """
    Obtiene la fecha y hora actual desde el servicio SOAP.
    Retorna: una cadena con la fecha y hora en formato "DD/MM/YYYY HH:MM:SS".
    """
    try:
        soap_client = ZeepClient('http://127.0.0.1:8000/?wsdl')
        return soap_client.service.get_datetime()
    except Exception as e:
        print(f"Error obteniendo fecha/hora del servicio SOAP: {e}")
        return ""

class client :
    """
    Clase principal del cliente que implementa todas las operaciones del protocolo.
    """

    # ******************** TYPES *********************

    class RC(Enum):
        """
        Códigos de retorno para los métodos del protocolo
        """
        OK = 0
        ERROR = 1
        USER_ERROR = 2

    # ****************** ATTRIBUTES ******************

    _server = None  # Dirección IP del servidor
    _port = -1      # Puerto del servidor

    # ******************** METHODS *******************

    @staticmethod
    def register(user):
        """
        Registra un nuevo usuario en el sistema.
        Parámetros:
            user: nombre de usuario a registrar
        Retorna:
            RC.OK si el registro fue exitoso
            RC.ERROR en caso de error
            RC.USER_ERROR si el usuario es inválido
        """
        if len(user) > 255:
            print("Error: User name is too long")
            return client.RC.USER_ERROR
            
        # Crear socket y conectar al servidor
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_address = (client._server, int(client._port))

        try:
            sock.connect(server_address)
        except socket.error as e:
            print("c > REGISTER FAIL")
            return client.RC.ERROR

        # Enviar código de operación
        message = "REGISTER" + "\0"
        sock.sendall(message.encode())

        # Enviar la fecha/hora obtenida del servicio SOAP
        datetime_str = get_datetime_string() + "\0"
        print(datetime_str)
        sock.sendall(datetime_str.encode())

        # Enviar nombre de usuario
        message = user + "\0"
        sock.sendall(message.encode())

        # Recibir y procesar respuesta del servidor
        status = int(readInt32(sock))

        if status == 0:
            print("c> REGISTER OK")
        elif status == 1:
            print("c> USERNAME IN USE")
        elif status == 2:
            print("c> REGISTER FAIL")
        sock.close()

        return client.RC.ERROR

    @staticmethod
    def unregister(user):
        """
        Da de baja a un usuario registrado en el sistema.
        Parámetros:
            user: nombre del usuario a dar de baja
        Retorna:
            RC.OK si la baja fue exitosa
            RC.ERROR en caso de error
            RC.USER_ERROR si el usuario es inválido
        """
        if len(user) > 255:
            print("Error: User name is too long")
            return client.RC.USER_ERROR
            
        # Crear socket y conectar al servidor
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_address = (client._server, int(client._port))
        try:
            sock.connect(server_address)
        except socket.error as e:
            print("c > UNREGISTER FAIL")
            return client.RC.ERROR

        # Enviar código de operación
        message = "UNREGISTER" + "\0"
        sock.sendall(message.encode())

        # Enviar fecha/hora del servicio SOAP
        datetime_str = get_datetime_string() + "\0"
        sock.sendall(datetime_str.encode())

        # Enviar nombre de usuario
        message = user + "\0"
        sock.sendall(message.encode())

        # Recibir y procesar respuesta del servidor
        status = int(readInt32(sock))

        if status == 0:
            print("c> UNREGISTER OK")
        elif status == 1:
            print("c> USER DOES NOT EXIST")
        elif status == 2:
            print("c> UNREGISTER FAIL")
        sock.close()

        return client.RC.ERROR

    @staticmethod
    def connect(user):
        """
        Conecta un usuario al sistema para poder utilizar sus servicios.
        Crea un socket para recibir peticiones de otros clientes.
        
        Parámetros:
            user: nombre de usuario a conectar
        Retorna:
            RC.OK si la conexión fue exitosa
            RC.ERROR en caso de error
            RC.USER_ERROR si el usuario es inválido
        """
        global user_connected
        if len(user) > 255:
            print("Error: User name is too long")
            return client.RC.USER_ERROR
        


        # Crear socket para escuchar conexiones entrantes de otros clientes
        user_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        user_socket.bind(('', 0))  # Usar puerto aleatorio
        port = user_socket.getsockname()[1]

        # Crear un hilo para manejar el socket de servicio
        def service_thread():
            """
            Función que ejecuta el hilo de servicio para atender peticiones 
            de otros clientes (principalmente para la operación GET_FILE)
            """
            user_socket.listen(1)  # Escuchar una conexión entrante
            while not stop_event.is_set():
                try:
                    sock, addr = user_socket.accept()
                    message = readString(sock)
                    if message != 'GET_FILE':
                        # Si no es una petición de archivo, devolver error
                        message = struct.pack('!I', 2)
                        sock.sendall(message)
                    else:
                        # Procesar petición de archivo
                        path = readString(sock)     
                        full_path = os.path.join(os.getcwd(), path)
                        if not os.path.exists(full_path):
                            print(f"El archivo {full_path} no existe.")
                            message = struct.pack('!I', 1)  # Archivo no existe
                            sock.sendall(message)
                            print(f"Datos recibidos: {full_path.decode()}")
                        else:
                            # El archivo existe, enviarlo
                            message = struct.pack('!I', 0)  # OK
                            sock.sendall(message)
                            size = os.path.getsize(full_path)
                            message = struct.pack('!I', size)  # Tamaño del archivo
                            sock.sendall(message)
                            try: 
                                # Abrir el archivo en modo binario
                                with open(full_path, 'rb') as file:                            
                                    # Leer el contenido
                                    file = file.read(size)
                                    # Enviar el contenido del archivo
                                    sock.sendall(file)
                                
                            except Exception as e:
                                print(f"Error while sending file: {e}")
                                sock.close()

                    sock.close()
                except socket.timeout:
                    continue  # Manejar timeout sin bloquear
                except Exception as e:
                    print(f"Error en el hilo de servicio: {e}")
                    break



        # Conectar al servidor para registrar la conexión
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_address = (client._server, int(client._port))
        try:
            sock.connect(server_address)
        except socket.error as e:
            print("c > CONNECT FAIL")
            return client.RC.ERROR

        # Enviar código de operación
        message = "CONNECT" + "\0"
        sock.sendall(message.encode())

        # Enviar fecha/hora
        datetime_str = get_datetime_string() + "\0"
        sock.sendall(datetime_str.encode())

        # Enviar nombre de usuario
        message = user + "\0"
        sock.sendall(message.encode())

        # Enviar IP pública
        # message = get_public_ip() + "\0"
        # sock.sendall(message.encode())
        
        # Enviar puerto local donde se escuchan peticiones
        message = struct.pack('!I', port)
        sock.sendall(message)

        # Recibir y procesar respuesta del servidor
        status = int(readInt32(sock))

        if status == 0:
                    # Si ya hay un usuario conectado y es diferente al actual, desconectarlo    
            if user_connected and user_connected != user:
                client.disconnect(user_connected)  # Desconectar al usuario anterior si es diferente
            
            user_connected = user

            # Iniciar el hilo de servicio
            thread = threading.Thread(target=service_thread, daemon=True)
            thread.start()
            print("c > CONNECT OK")
        elif status == 1:
            print("c > CONNECT FAIL , USER DOES NOT EXIST")
        elif status == 2:
            print("c > USER ALREADY CONNECTED")
        elif status == 3:
            print("c > CONNECT FAIL")
        sock.close()

        return client.RC.ERROR

    @staticmethod
    def disconnect(user):
        """
        Desconecta un usuario del sistema.
        Detiene el hilo de servicio y cierra el socket.
        
        Parámetros:
            user: nombre de usuario a desconectar
        Retorna:
            RC.OK si la desconexión fue exitosa
            RC.ERROR en caso de error
            RC.USER_ERROR si el usuario es inválido
        """
        global user_socket, service_thread_instance, stop_event
        
        if len(user) > 255:
            print("Error: User name is too long")
            return client.RC.USER_ERROR
            
        # Señalizar al hilo que debe detenerse
        stop_event.set()
        if service_thread_instance:
            service_thread_instance.join()  # Esperar a que el hilo termine
            service_thread_instance = None

        # Cerrar el socket de escucha
        if user_socket:
            user_socket.close()
            user_socket = None

        # Conectar al servidor para registrar la desconexión
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_address = (client._server, int(client._port))
        try:
            sock.connect(server_address)
        except socket.error as e:
            print("c > DISCONNECT FAIL")
            return client.RC.ERROR

        # Enviar código de operación
        message = "DISCONNECT" + "\0"
        sock.sendall(message.encode())

        # Enviar fecha/hora
        datetime_str = get_datetime_string() + "\0"
        sock.sendall(datetime_str.encode())

        # Enviar nombre de usuario
        message = user + "\0"
        sock.sendall(message.encode())

        # Recibir y procesar respuesta del servidor
        status = int(readInt32(sock))

        if status == 0:
            print("c > DISCONNECT OK")
        elif status == 1:
            print("c > DISCONNECT FAIL , USER DOES NOT EXIST")
        elif status == 2:
            print("c > DISCONNECT FAIL , USER NOT CONNECTED")
        elif status == 3:
            print("c > DISCONNECT FAIL")
        sock.close()

        return client.RC.ERROR

    @staticmethod
    def publish(fileName, description):
        """
        Publica un archivo para compartirlo con otros usuarios.
        
        Parámetros:
            fileName: nombre del archivo a publicar
            description: descripción del archivo
        Retorna:
            RC.OK si la publicación fue exitosa
            RC.ERROR en caso de error
            RC.USER_ERROR si los parámetros son inválidos
        """
        if len(fileName) > 255:
            print("c > PUBLISH FAIL")
            return client.RC.USER_ERROR
        if len(description) > 255:
            print("c > PUBLISH FAIL")
            return client.RC.USER_ERROR
            
        # Crear socket y conectar al servidor
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_address = (client._server, int(client._port))
        try:
            sock.connect(server_address)
        except socket.error as e:
            print("c > PUBLISH FAIL")
            return client.RC.ERROR

        # Enviar código de operación
        message = "PUBLISH" + "\0"
        sock.sendall(message.encode())

        # Enviar fecha/hora
        datetime_str = get_datetime_string() + "\0"
        sock.sendall(datetime_str.encode())

        # Enviar nombre de usuario
        message = user_connected + "\0"
        sock.sendall(message.encode())

        # Enviar nombre del archivo
        message = fileName + "\0"
        sock.sendall(message.encode())

        # Enviar descripción del archivo
        message = description + "\0"
        sock.sendall(message.encode())

        # Recibir y procesar respuesta del servidor
        status = int(readInt32(sock))

        if status == 0:
            print("c> PUBLISH OK OK")
        elif status == 1:
            print("c> PUBLISH FAIL , USER DOES NOT EXIST")
        elif status == 2:
            print("c> PUBLISH FAIL , USER NOT CONNECTED")
        elif status == 3:
            print("c> PUBLISH FAIL , CONTENT ALREADY PUBLISHED")
        elif status == 4:
            print("c> PUBLISH FAIL")
        sock.close()

        return client.RC.ERROR

    @staticmethod
    def delete(fileName):
        """
        Elimina un archivo publicado.
        
        Parámetros:
            fileName: nombre del archivo a eliminar
        Retorna:
            RC.OK si la eliminación fue exitosa
            RC.ERROR en caso de error
            RC.USER_ERROR si el nombre de archivo es inválido
        """
        if len(fileName) > 255:
            print("Error: description is too long")
            return client.RC.USER_ERROR
            
        # Crear socket y conectar al servidor
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_address = (client._server, int(client._port))
        try:
            sock.connect(server_address)
        except socket.error as e:
            print("c > DELETE FAIL")
            return client.RC.ERROR

        # Enviar código de operación
        message = "DELETE" + "\0"
        sock.sendall(message.encode())

        # Enviar fecha/hora
        datetime_str = get_datetime_string() + "\0"
        sock.sendall(datetime_str.encode())

        # Enviar nombre de usuario
        message = user_connected + "\0"
        sock.sendall(message.encode())

        # Enviar nombre del archivo
        message = fileName + "\0"
        sock.sendall(message.encode())

        # Recibir y procesar respuesta del servidor
        status = int(readInt32(sock))
        if status == 0:
            print("c> DELETE OK")
        elif status == 1:
            print("c> DELETE FAIL , USER DOES NOT EXIST")
        elif status == 2:
            print("c> DELETE FAIL , USER NOT CONNECTED")
        elif status == 3:
            print("c> DELETE FAIL , CONTENT NOT PUBLISHED")
        elif status == 4:
            print("c> DELETE FAIL")
        sock.close()

        return client.RC.ERROR

    @staticmethod
    def listusers():
        """
        Lista los usuarios conectados en el sistema.
        Actualiza la variable global list_users con la información recibida.
        
        Retorna:
            RC.OK si el listado fue exitoso
            RC.ERROR en caso de error
        """
        # Crear socket y conectar al servidor
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_address = (client._server, int(client._port))
        try:
            sock.connect(server_address)
        except socket.error as e:
            print("c > LIST_USERS FAIL")
            return client.RC.ERROR
        
        # Enviar código de operación
        message = "LIST_USERS" + "\0"
        sock.sendall(message.encode())

        # Enviar fecha/hora
        datetime_str = get_datetime_string() + "\0"
        sock.sendall(datetime_str.encode())

        # Enviar nombre de usuario
        message = user_connected + "\0"
        sock.sendall(message.encode())

        # Recibir y procesar respuesta del servidor
        status = int(readInt32(sock))
        if status == 0:
            print("c > LIST_USERS OK")
            # Leer el número de usuarios
            num_users = int(readInt32(sock))
            for i in range(num_users):
                # Leer información de cada usuario: nombre, IP y puerto
                user_name = readString(sock)
                user, extension = os.path.splitext(user_name)
                ip = readString(sock)
                port = int(readInt32(sock))
                # Almacenar información en el diccionario global
                list_users[user] = [ip, port]
                print(f"{user} {ip} {port}")

        elif status == 1:
            print("c > LIST_USERS FAIL , USER DOES NOT EXIST")
        elif status == 2:
            print("c > LIST_USERS FAIL , USER NOT CONNECTED")
        elif status == 3:
            print("c > LIST_USERS FAIL")
        sock.close()

        return client.RC.ERROR

    @staticmethod
    def listcontent(user):
        """
        Lista los archivos publicados por un usuario específico.
        
        Parámetros:
            user: nombre del usuario cuyos archivos se quieren listar
        Retorna:
            RC.OK si el listado fue exitoso
            RC.ERROR en caso de error
        """
        # Crear socket y conectar al servidor
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_address = (client._server, int(client._port))
        try:
            sock.connect(server_address)
        except socket.error as e:
            print("c > LIST_CONTENT OK")
            return client.RC.ERROR
        
        # Enviar código de operación
        message = "LIST_CONTENT" + "\0"
        sock.sendall(message.encode())

        # Enviar fecha/hora
        datetime_str = get_datetime_string() + "\0"
        sock.sendall(datetime_str.encode())

        # Enviar nombre del usuario que realiza la petición
        message = user_connected + "\0"
        sock.sendall(message.encode())

        # Enviar nombre del usuario cuyos archivos se listarán
        message = user + "\0"
        sock.sendall(message.encode())

        # Recibir y procesar respuesta del servidor
        status = int(readInt32(sock))
        if status == 0:
            print("c > LIST_CONTENT OK")
            # Leer el número de archivos
            num_files = int(readInt32(sock))
            for i in range(num_files):
                # Leer y mostrar el nombre de cada archivo
                file = readString(sock)
                print(file)

        elif status == 1:
            print("c> LIST_CONTENT FAIL , USER DOES NOT EXIST")
        elif status == 2:
            print("c> LIST_CONTENT FAIL , USER NOT CONNECTED")
        elif status == 3:
            print("c> LIST_CONTENT FAIL,  REMOTE USER DOES NOT EXIST")
        elif status == 4:
            print("c> LIST_CONTENT FAIL")
        sock.close()

        return client.RC.ERROR

    @staticmethod
    def getfile(user, remote_FileName, local_FileName):
        """
        Obtiene un archivo de otro usuario.
        
        Parámetros:
            user: nombre del usuario propietario del archivo
            remote_FileName: nombre del archivo a obtener
            local_FileName: nombre con el que se guardará el archivo localmente
        Retorna:
            RC.OK si la descarga fue exitosa
            RC.ERROR en caso de error
        """
        # Actualizar lista de usuarios conectados para obtener IP y puerto
        client.listusers()
        ip = list_users[user][0]
        port = list_users[user][1]

        # Crear socket y conectar al usuario propietario del archivo
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_address = (ip, int(port))
        try:
            sock.connect(server_address)
        except socket.error as e:
            print("c > GET_FILE FAIL")
            return client.RC.ERROR
            
        # Enviar código de operación
        message = "GET_FILE" + "\0"
        sock.sendall(message.encode())

        # Enviar nombre del archivo remoto
        message = remote_FileName + "\0"
        sock.sendall(message.encode())       
        
        # Recibir y procesar respuesta
        status = int(readInt32(sock))
        if status == 0:
            print("c > GET_FILE OK")
            # Leer tamaño del archivo
            size = int(readInt32(sock))
            
            # Crear directorio para el archivo local si es necesario
            output_dir = os.path.dirname(local_FileName)
            if output_dir and not os.path.exists(output_dir):
                os.makedirs(output_dir)
                
            # Recibir y guardar el archivo
            with open(local_FileName, 'wb') as f:
                file = sock.recv(size)
                # Escribir los datos en el archivo local
                f.write(file)

        elif status == 1:
            print("c> GET_FILE FAIL , FILE NOT EXIST")
        elif status == 2:
            print("c> GET_FILE FAIL")
        sock.close()

        return client.RC.ERROR

    @staticmethod
    def shell():
        """
        Intérprete de comandos para el cliente.
        Lee comandos del usuario y llama a las funciones correspondientes.
        """
        while (True):
            try:
                command = input("c> ")
                line = command.split(" ")

                if (len(line) > 0):
                    line[0] = line[0].upper()

                    if (line[0]=="REGISTER"):
                        if (len(line) == 2):
                            client.register(line[1])
                        else:
                            print("Syntax error. Usage: REGISTER <userName>")

                    elif(line[0]=="UNREGISTER"):
                        if (len(line) == 2):
                            client.unregister(line[1])
                        else:
                            print("Syntax error. Usage: UNREGISTER <userName>")

                    elif(line[0]=="CONNECT"):
                        if (len(line) == 2):
                            client.connect(line[1])
                        else:
                            print("Syntax error. Usage: CONNECT <userName>")
                    
                    elif(line[0]=="PUBLISH"):
                        if (len(line) >= 3):
                            #  Eliminar las dos primeras palabras y unir el resto como descripción
                            description = ' '.join(line[2:])
                            client.publish(line[1], description)
                        else:
                            print("Syntax error. Usage: PUBLISH <fileName> <description>")

                    elif(line[0]=="DELETE"):
                        if (len(line) == 2):
                            client.delete(line[1])
                        else:
                            print("Syntax error. Usage: DELETE <fileName>")

                    elif(line[0]=="LIST_USERS"):
                        if (len(line) == 1):
                            client.listusers()
                        else:
                            print("Syntax error. Use: LIST_USERS")

                    elif(line[0]=="LIST_CONTENT"):
                        if (len(line) == 2):
                            client.listcontent(line[1])
                        else:
                            print("Syntax error. Usage: LIST_CONTENT <userName>")

                    elif(line[0]=="DISCONNECT"):
                        if (len(line) == 2):
                            client.disconnect(line[1])
                        else:
                            print("Syntax error. Usage: DISCONNECT <userName>")

                    elif(line[0]=="GET_FILE"):
                        if (len(line) == 4):
                            client.getfile(line[1], line[2], line[3])
                        else:
                            print("Syntax error. Usage: GET_FILE <userName> <remote_fileName> <local_fileName>")

                    elif(line[0]=="QUIT"):
                        if (len(line) == 1):
                            if user_connected:
                                client.disconnect(user_connected)  # Desconectar al usuario antes de salir
                            break
                        else:
                            print("Syntax error. Use: QUIT")

                    else:
                        print("Error: command " + line[0] + " not valid.")

            except KeyboardInterrupt:
                # Capturar Ctrl+C para una salida limpia
                if user_connected:
                    print()
                    client.disconnect(user_connected)  # Desconectar al usuario antes de salir
                break
                
            except Exception as e:
                print("Exception: " + str(e))

    @staticmethod
    def usage():
        """
        Muestra la forma de uso del programa.
        """
        print("Usage: python3 client.py -s <server> -p <port>")

    @staticmethod
    def parseArguments(argv):
        """
        Analiza los argumentos de ejecución del programa.
        
        Parámetros:
            argv: argumentos de la línea de comandos
        Retorna:
            True si los argumentos son válidos, False en caso contrario
        """
        parser = argparse.ArgumentParser()
        parser.add_argument('-s', type=str, required=True, help='Server IP')
        parser.add_argument('-p', type=int, required=True, help='Server Port')
        args = parser.parse_args()

        if (args.s is None):
            parser.error("Usage: python3 client.py -s <server> -p <port>")
            return False

        if ((args.p < 1024) or (args.p > 65535)):
            parser.error("Error: Port must be in the range 1024 <= port <= 65535");
            return False;
        
        client._server = args.s
        client._port = args.p

        return True

    @staticmethod
    def main(argv):
        """
        Función principal del cliente.
        
        Parámetros:
            argv: argumentos de la línea de comandos
        """
        if (not client.parseArguments(argv)):
            client.usage()
            return

        client.shell()
        print("+++ FINISHED +++")

if __name__=="__main__":
    client.main([])