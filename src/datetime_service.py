from spyne import Application, rpc, ServiceBase, Unicode
from spyne.protocol.soap import Soap11
from spyne.server.wsgi import WsgiApplication
from datetime import datetime

class DateTimeService(ServiceBase):
    @rpc(_returns=Unicode)
    def get_datetime(ctx):
        now = datetime.now()
        return now.strftime("%d/%m/%Y %H:%M:%S")

application = Application([DateTimeService], 'spyne.examples.datetime',
                          in_protocol=Soap11(validator='lxml'),
                          out_protocol=Soap11())

if __name__ == '__main__':
    from wsgiref.simple_server import make_server
    wsgi_app = WsgiApplication(application)
    server = make_server('127.0.0.1', 8000, wsgi_app)
    print("SOAP DateTimeService running on http://127.0.0.1:8000")
    server.serve_forever()
