'''
MOSCade / ggposrv3 Websocket to UDP / TCP reverse proxy

by mos9527 2021,licensed under GPL-2.0
'''
from argparse import ArgumentParser
from select import select
from socketserver import BaseRequestHandler, ThreadingMixIn , TCPServer
from struct import pack, unpack
from threading import Lock, Thread
from time import sleep, time_ns
import logging,socket,sys,traceback

import websocket,os
# pip install websocket-client

class UDPServer(Thread):
    def __init__(self,listen_address , listen_port , ws_uri):
        self.shutdown_ = False

        self.server_address = (listen_address,int(listen_port))
        self.ws_uri = ws_uri

        self.fd = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
        self.fd.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.fd.bind(self.server_address)
        self.logger = logging.getLogger('UDPServer')        
        
        self.conns = dict()

        self.ws = websocket.WebSocket()
        def ws_run():
            self.ws.connect(self.ws_uri)            
            print('** READY : udp/%s:%s <-> %s' % (*self.server_address,self.ws_uri))
            while not self.shutdown_:
                self.on_ws_recv(self.ws.recv())
        self.ws_thread = Thread(target=ws_run)
        self.ws_thread.daemon = True
        self.ws_thread.start()

        super().__init__(daemon=True)
        
    def on_ws_recv(self,buf):        
        port,data = unpack('H',(buf[:2]))[0],buf[2:]
        for port_ in self.conns:
            if port != port_:                            
                self.fd.sendto(data,('127.0.0.1',port_))

    def run(self):                
        while not self.shutdown_:                     
            # new connection w/ data to be sent
            data, addr = self.fd.recvfrom(64)                                                
            self.ws.send_binary(pack('H', addr[1]) + data)                    
            self.conns[addr[1]] = True
        

class TCPServer(ThreadingMixIn,TCPServer):
    def __init__(self, listen_address , listen_port , ws_uri):
        self.ws_uri = ws_uri
        super().__init__((listen_address,int(listen_port)),TCPHandler,True)        
        print('** READY : tcp/%s:%s <-> %s' % (*self.server_address,self.ws_uri))

class TCPHandler(BaseRequestHandler):
    def __init__(self, request, client_address, server) -> None:
        self.shutdown_ = False
        super().__init__(request, client_address, server)        

    def handle(self) -> None:        
        self.server : TCPServer        
        self.ws = websocket.WebSocket()        
        self.ws.connect(self.server.ws_uri)         
        self.request.setblocking(False)                
        # Maintain this connection whilst both sockets are alive
        buf = bytearray()
        while not self.shutdown_:            
            s1,s2 = self.request,self.ws.sock
            r,w,_ = select([s1,s2],[],[])
            try:
                if s1 in r:                
                    buf_ = s1.recv(4096)                    
                    self.ws.send_binary(buf_)
                if s2 in r:                
                    buf_ = self.ws.recv()
                    buf += buf_
                if buf:                
                    buf = buf[s1.send(buf):]            
            except Exception as e:
                print(traceback.format_exc())                
                self.ws.close()
                if self.request : self.request.close()        
                return suicide()   
            sleep(0.001)        
        suicide()                

is_shutting_down = False
def suicide():
    global is_shutting_down
    if is_shutting_down:
        return
    is_shutting_down = True
    logging.fatal('Shutting down')
    srv_u.shutdown_ = True
    srv.shutdown_ = True
    os.kill(os.getpid(), 9)

if __name__ == '__main__':
    argparse = ArgumentParser(description='GGPOSRV3 / MOSCade proxy')
    argparse.add_argument('--tcp-host',help='GGPOSRV3 /ggpo Host')
    argparse.add_argument('--udp-host',help='GGPOSRV3 /nexus Host')
    argparse.add_argument('--tcp-port',help='TCP listening port',default=8000,type=int)
    argparse.add_argument('--udp-port',help='UDP listening port',default=9000,type=int)

    print('*** GGPO Nexus Proxy starting up ***')
    print('*** Args',*sys.argv)    
    args = argparse.parse_args()
    print('*** Python Version : %s' % sys.version)
    print('*** HOST 1',args.tcp_host)
    print('*** HOST 2',args.udp_host)    
    srv = TCPServer('127.0.0.1',args.tcp_port,args.tcp_host)
    srv_u = UDPServer('127.0.0.1',args.udp_port,args.udp_host)
    
    try:
        srv_u.start()
        srv.serve_forever()    
    except Exception:
        pass
    finally:             
        srv_u.shutdown_ = True
        suicide()
