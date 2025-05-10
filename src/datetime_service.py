from spyne import Application, rpc, ServiceBase, Unicode
from spyne.protocol.soap import Soap11
from spyne.server.wsgi import WsgiApplication
from datetime import datetime

class DateTimeService(ServiceBase):
    """
    Servicio SOAP que proporciona la fecha y hora actual.
    Este servicio se utiliza para registrar el momento en que se realizan
    las operaciones en el sistema.
    """
    @rpc(_returns=Unicode)
    def get_datetime(ctx):
        """
        Método RPC que devuelve la fecha y hora actual.
        
        Parámetros:
            ctx: contexto de la llamada RPC (automáticamente proporcionado por Spyne)
        Retorna:
            Una cadena con la fecha y hora en formato "DD/MM/YYYY HH:MM:SS"
        """
        now = datetime.now()
        return now.strftime("%d/%m/%Y %H:%M:%S")

# Crear la aplicación SOAP con el servicio DateTimeService
application = Application(
    [DateTimeService],                 # Servicios disponibles
    'spyne.examples.datetime',         # Namespace del servicio
    in_protocol=Soap11(validator='lxml'),  # Protocolo de entrada
    out_protocol=Soap11()              # Protocolo de salida
)

# Punto de entrada para ejecutar el servidor directamente
if __name__ == '__main__':
    from wsgiref.simple_server import make_server
    
    # Crear la aplicación WSGI
    wsgi_app = WsgiApplication(application)
    
    # Crear y configurar el servidor HTTP
    server = make_server('127.0.0.1', 8000, wsgi_app)
    print("SOAP DateTimeService running on http://127.0.0.1:8000")
    
    # Iniciar el servidor (bucle infinito)
    server.serve_forever()
