from enum import Enum

import argparse

import socket
import os
import threading

import struct
from zeep import Client as ZeepClient

stop_event = threading.Event()
service_thread_instance = None
user_socket = None
user_connected = ''
list_users = {}

def readInt32(sock):
    """
    Reads a 32-bit integer from the socket.
    """
    data = sock.recv(4)  # Read 4 bytes from the socket
    if len(data) < 4:
        raise ValueError("Incomplete data received for int32")
    return struct.unpack('!I', data)[0]  # Unpack as a big-endian int

def readString(sock):
    a = ''
    while True:
        msg = sock.recv(1)
        if (msg == b'\0'):
            break
        a += msg.decode()
    return(a)

# Añadir función para obtener la fecha/hora del servicio SOAP
def get_datetime_string():
    try:
        soap_client = ZeepClient('http://127.0.0.1:8000/?wsdl')
        return soap_client.service.get_datetime()
    except Exception as e:
        print(f"Error obteniendo fecha/hora del servicio SOAP: {e}")
        return ""

class client :

    # ******************** TYPES *********************

    # *

    # * @brief Return codes for the protocol methods

    class RC(Enum) :

        OK = 0

        ERROR = 1

        USER_ERROR = 2

    # ****************** ATTRIBUTES ******************

    _server = None

    _port = -1

    # ******************** METHODS *******************

    @staticmethod
    def register(user) :
        if len(user) > 255:
            print("Error: User name is too long")
            return client.RC.USER_ERROR
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_address = (client._server, int(client._port))
        try:
            sock.connect(server_address)
        except socket.error as e:
            print("c > PUBLISH FAIL")
            return client.RC.ERROR

        message = "REGISTER" + "\0"
        sock.sendall(message.encode())

        # Enviar la fecha/hora tras el código de operación
        datetime_str = get_datetime_string() + "\0"
        print(datetime_str)
        sock.sendall(datetime_str.encode())

        message = user + "\0"
        sock.sendall(message.encode())

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
    def unregister(user) :
        if len(user) > 255:
            print("Error: User name is too long")
            return client.RC.USER_ERROR
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_address = (client._server, int(client._port))
        try:
            sock.connect(server_address)
        except socket.error as e:
            print("c > PUBLISH FAIL")
            return client.RC.ERROR

        message = "UNREGISTER" + "\0"
        sock.sendall(message.encode())

        datetime_str = get_datetime_string() + "\0"
        sock.sendall(datetime_str.encode())

        message = user + "\0"
        sock.sendall(message.encode())

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
        global user_connected
        if len(user) > 255:
            print("Error: User name is too long")
            return client.RC.USER_ERROR
            
        if user_connected and user_connected != user:
            client.disconnect(user_connected)  # Disconnect the previous user if different
        
        user_connected = user

        # Crear socket para escuchar conexiones entrantes de otros clientes
        user_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        user_socket.bind(('', 0))  # Usar puerto aleatorio
        port = user_socket.getsockname()[1]

        # Crear un hilo para manejar el socket de servicio
        def service_thread():
            user_socket.listen(1)  # Escuchar una conexión entrante
            while not stop_event.is_set():
                try:
                    sock, addr = user_socket.accept()
                    message = readString(sock)
                    print(message)
                    if message != 'GET_FILE':
                        print("ups")
                        message = struct.pack('!I', 1)
                        sock.sendall(message)
                    else:
                        path = readString(sock)     
                        full_path = os.path.join(os.getcwd(), path)
                        if not os.path.exists(full_path):
                            message = struct.pack('!I', 1)
                            sock.sendall(message)
                            print(f"Datos recibidos: {full_path.decode()}")
                        else:
                           
                            message = struct.pack('!I', 0)
                            sock.sendall(message)
                            size = os.path.getsize(full_path)
                            message = struct.pack('!I', size)
                            sock.sendall(message)
                            try: 
                                # Open the file in binary mode
                                with open(full_path, 'rb') as file:                            
                                    # Read the first byte
                                    file = file.read(size)
                                    print(file.decode())
                                    # Send the file content
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

        thread = threading.Thread(target=service_thread, daemon=True)
        thread.start()

        # Conectar al servidor
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_address = (client._server, int(client._port))
        try:
            sock.connect(server_address)
        except socket.error as e:
            print("c > PUBLISH FAIL")
            return client.RC.ERROR

        message = "CONNECT" + "\0"
        sock.sendall(message.encode())

        datetime_str = get_datetime_string() + "\0"
        sock.sendall(datetime_str.encode())

        message = user + "\0"
        sock.sendall(message.encode())

        message = struct.pack('!I', port)
        sock.sendall(message)

        status = int(readInt32(sock))

        if status == 0:
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
    def disconnect(user) :
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

        # Conectar al servidor
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_address = (client._server, int(client._port))
        try:
            sock.connect(server_address)
        except socket.error as e:
            print("c > PUBLISH FAIL")
            return client.RC.ERROR

        message = "DISCONNECT" + "\0"
        sock.sendall(message.encode())

        datetime_str = get_datetime_string() + "\0"
        sock.sendall(datetime_str.encode())

        message = user + "\0"
        sock.sendall(message.encode())

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
    def publish(fileName,  description) :
        if len(fileName) > 255:
            print("Error: file name is too long")
            return client.RC.USER_ERROR
        if len(description) > 255:
            print("Error: description is too long")
            return client.RC.USER_ERROR
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_address = (client._server, int(client._port))
        try:
            sock.connect(server_address)
        except socket.error as e:
            print("c > PUBLISH FAIL")
            return client.RC.ERROR

        message = "PUBLISH" + "\0"
        sock.sendall(message.encode())

        datetime_str = get_datetime_string() + "\0"
        sock.sendall(datetime_str.encode())

        message = user_connected + "\0"
        sock.sendall(message.encode())

        message = fileName + "\0"
        sock.sendall(message.encode())

        message = description + "\0"
        sock.sendall(message.encode())

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
    def delete(fileName) :
        if len(fileName) > 255:
            print("Error: description is too long")
            return client.RC.USER_ERROR
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_address = (client._server, int(client._port))
        try:
            sock.connect(server_address)
        except socket.error as e:
            print("c > “DELETE” FAIL")
            return client.RC.ERROR

        message = "DELETE" + "\0"
        sock.sendall(message.encode())

        datetime_str = get_datetime_string() + "\0"
        sock.sendall(datetime_str.encode())

        message = user_connected + "\0"
        sock.sendall(message.encode())

        message = fileName + "\0"
        sock.sendall(message.encode())

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
    def listusers() :
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_address = (client._server, int(client._port))
        try:
            sock.connect(server_address)
        except socket.error as e:
            print("c > LIST_USERS FAIL")
            return client.RC.ERROR
        
        message = "LIST_USERS" + "\0"
        sock.sendall(message.encode())

        datetime_str = get_datetime_string() + "\0"
        sock.sendall(datetime_str.encode())

        message = user_connected + "\0"
        sock.sendall(message.encode())

        status = int(readInt32(sock))
        if status == 0:
            print("c > LIST_USERS OK")
            # Read the number of users
            num_users = int(readInt32(sock))
            for i in range(num_users):
                # Read the user name
                user_name = readString(sock)
                user, extension = os.path.splitext(user_name)
                ip = readString(sock)
                port = int(readInt32(sock))
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
    def listcontent(user) :
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_address = (client._server, int(client._port))
        try:
            sock.connect(server_address)
        except socket.error as e:
            print("c > LIST_CONTENT OK")
            return client.RC.ERROR
        
        message = "LIST_CONTENT" + "\0"
        sock.sendall(message.encode())

        datetime_str = get_datetime_string() + "\0"
        sock.sendall(datetime_str.encode())

        message = user_connected + "\0"
        sock.sendall(message.encode())

        message = user + "\0"
        sock.sendall(message.encode())

        status = int(readInt32(sock))
        if status == 0:
            print("c > LIST_CONTENT FAIL")
            # Read the number of users
            num_files = int(readInt32(sock))
            for i in range(num_files):
                # Read the user name
                file = readString(sock)
                file, extension = os.path.splitext(file)
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
    def getfile(user,  remote_FileName,  local_FileName) :
        client.listusers()
        ip = list_users[user][0]
        port = list_users[user][1]

        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_address = (ip, int(port))
        try:
            sock.connect(server_address)
        except socket.error as e:
            print("c > GET_FILE FAIL")
            return client.RC.ERROR
        message = "GET_FILE" + "\0"
        sock.sendall(message.encode())

        datetime_str = get_datetime_string() + "\0"
        sock.sendall(datetime_str.encode())

        message = remote_FileName + "\0"
        sock.sendall(message.encode())       
        status = int(readInt32(sock))
        if status == 0:
            print("c > GET_FILE OK")
            size = int(readInt32(sock))
            output_dir = os.path.dirname(local_FileName)
            if output_dir and not os.path.exists(output_dir):
                os.makedirs(output_dir)
            with open(local_FileName, 'wb') as f:
                file = sock.recv(size)
                # Write the byte to the file
                f.write(file)

        elif status == 1:
            print("c> GET_FILE FAIL , FILE NOT EXIST")
        elif status == 2:
            print("c> GET_FILE FAIL")
        sock.close()

        return client.RC.ERROR

    # *

    # **

    # * @brief Command interpreter for the client. It calls the protocol functions.

    @staticmethod
    def shell():

        while (True) :

            try :

                command = input("c> ")

                line = command.split(" ")

                if (len(line) > 0):

                    line[0] = line[0].upper()

                    if (line[0]=="REGISTER") :

                        if (len(line) == 2) :

                            client.register(line[1])

                        else :

                            print("Syntax error. Usage: REGISTER <userName>")

                    elif(line[0]=="UNREGISTER") :

                        if (len(line) == 2) :

                            client.unregister(line[1])

                        else :

                            print("Syntax error. Usage: UNREGISTER <userName>")

                    elif(line[0]=="CONNECT") :

                        if (len(line) == 2) :

                            client.connect(line[1])

                        else :

                            print("Syntax error. Usage: CONNECT <userName>")
                    
                    elif(line[0]=="PUBLISH") :

                        if (len(line) >= 3) :

                            #  Remove first two words

                            description = ' '.join(line[2:])

                            client.publish(line[1], description)

                        else :

                            print("Syntax error. Usage: PUBLISH <fileName> <description>")

                    elif(line[0]=="DELETE") :

                        if (len(line) == 2) :

                            client.delete(line[1])

                        else :

                            print("Syntax error. Usage: DELETE <fileName>")

                    elif(line[0]=="LIST_USERS") :

                        if (len(line) == 1) :

                            client.listusers()

                        else :

                            print("Syntax error. Use: LIST_USERS")

                    elif(line[0]=="LIST_CONTENT") :

                        if (len(line) == 2) :

                            client.listcontent(line[1])

                        else :

                            print("Syntax error. Usage: LIST_CONTENT <userName>")

                    elif(line[0]=="DISCONNECT") :

                        if (len(line) == 2) :

                            client.disconnect(line[1])

                        else :

                            print("Syntax error. Usage: DISCONNECT <userName>")

                    elif(line[0]=="GET_FILE") :

                        if (len(line) == 4) :

                            client.getfile(line[1], line[2], line[3])

                        else :

                            print("Syntax error. Usage: GET_FILE <userName> <remote_fileName> <local_fileName>")

                    elif(line[0]=="QUIT") :

                        if (len(line) == 1) :
                            client.disconnect(user_connected)  # Disconnect the user before quitting
                            break

                        else :

                            print("Syntax error. Use: QUIT")

                    else :

                        print("Error: command " + line[0] + " not valid.")

            except Exception as e:

                print("Exception: " + str(e))

    # *

    # * @brief Prints program usage

    @staticmethod
    def usage() :
        print("Usage: python3 client.py -s <server> -p <port>")

    # *

    # * @brief Parses program execution arguments

    @staticmethod
    def parseArguments(argv) :
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

    # ******************** MAIN *********************

    @staticmethod
    def main(argv) :
        if (not client.parseArguments(argv)) :
            client.usage()
            return

        client.shell()
        print("+++ FINISHED +++")

if __name__=="__main__":
    client.main([])