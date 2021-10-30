'''
MOSCade / ggposrv3 Websocket to UDP / TCP reverse proxy

by mos9527 2021,licensed under GPL-2.0
'''
from argparse import ArgumentParser
from select import select
from socketserver import BaseRequestHandler, ThreadingMixIn , TCPServer
from struct import pack, unpack
from threading import Thread
from time import sleep
import socket,sys,traceback

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
        
        self.conns = dict()
        self.pn    = b'\x00'

        self.ws = websocket.WebSocket()
        def ws_run():
            self.ws.connect(self.ws_uri)   
            hello_msg = self.ws.recv()
            header,player_n = hello_msg[:-1],int(hello_msg[-1:])
            assert 'SVHLO_P' == header
            print('[UDP] READY : udp/%s:%s <-> %s as P%d' % (*self.server_address,self.ws_uri,player_n))
            self.pn = player_n.to_bytes(1,'little')
            while not self.shutdown_:
                self.on_ws_recv(self.ws.recv())
        self.ws_thread = Thread(target=ws_run)
        self.ws_thread.daemon = True
        self.ws_thread.start()

        super().__init__(daemon=True)
    
    def from_ident(self,ident):        
        return unpack('H',(ident[:2]))[0],ident[2]
    def to_ident(self,port,pn):
        return pack('H',port) + pn

    def on_ws_recv(self,buf):        
        ident,data = buf[:3],buf[3:]        
        for ident_ in self.conns:
            if ident != ident_:                            
                self.fd.sendto(data,('127.0.0.1',self.from_ident(ident_)[0]))

    def run(self):                
        while not self.shutdown_:                     
            # new connection w/ data to be sent
            data, addr = self.fd.recvfrom(64)          
            ident = self.to_ident(addr[1],self.pn)                                    
            self.ws.send_binary(ident + data)                    
            self.conns[ident] = True
        

class TCPServer(ThreadingMixIn,TCPServer):
    def __init__(self, listen_address , listen_port , ws_uri):
        self.ws_uri = ws_uri
        super().__init__((listen_address,int(listen_port)),TCPHandler,True)        
        print('[TCP] READY : tcp/%s:%s <-> %s' % (*self.server_address,self.ws_uri))

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
    print('[ERROR] Shutting down')
    srv_u.shutdown_ = True
    srv.shutdown_ = True
    os.kill(os.getpid(), 9)

if __name__ == '__main__':
    argparse = ArgumentParser(description='GGPOSRV3 / MOSCade proxy')
    argparse.add_argument('--tcp-host',help='GGPOSRV3 /ggpo Host')
    argparse.add_argument('--udp-host',help='GGPOSRV3 /nexus Host')
    argparse.add_argument('--tcp-port',help='TCP listening port',default=8000,type=int)
    argparse.add_argument('--udp-port',help='UDP listening port',default=9000,type=int)

    print('[INFO] GGPO Nexus Proxy starting up')
    print('*** Args',*sys.argv)    
    args = argparse.parse_args()
    print('*** Python Version : %s' % sys.version)
    
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
