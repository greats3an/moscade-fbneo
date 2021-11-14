'''
MOSCade / ggposrv3 Websocket to UDP / TCP reverse proxy

by mos9527 2021,licensed under GPL-2.0
'''
from argparse import ArgumentParser
from select import select
from socketserver import BaseRequestHandler, ThreadingMixIn , TCPServer
import struct
from threading import Thread
from time import sleep, time_ns
import socket,sys,traceback

import websocket,os
# pip install websocket-client

RESP_OK = b'\x00\x00\x00\x00'
RESP_SYNC = b'\xff\xff\xff\xff'

class UDPForwarder(Thread):
    def __init__(self,listen_address , listen_port , host_address , host_port , quark):        
        self.shutdown_ = False

        self.listen_address = (listen_address,int(listen_port))
        self.host_address = (host_address,int(host_port))

        self.fd = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
        self.fd.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.fd.bind(self.listen_address)        
        
        self.quark = quark
        self.conns = dict()              
        super().__init__()
        self.daemon = True
        
    def run(self):               
        print('[UDP] Linking to server : %s:%s' % self.host_address)        
        self.fd.sendto(self.quark.encode(),self.host_address)        
        data , addr = self.fd.recvfrom(256)
        assert data.startswith(RESP_OK),'Bad response %s' % data
        print('[UDP] Linked to server : udp/%s:%s <-> %s:%s' % (*self.listen_address,*self.host_address))     
        data , addr = self.fd.recvfrom(256)
        assert data.startswith(RESP_SYNC),'Bad response %s' % data        
        remote = ((socket.inet_ntoa(data[4:8]),struct.unpack('<H',data[8:])[0]))
        print('[UDP] Forwarding to address : %s:%d' % remote)        
        print('[NOTE] Protip : Press Alt + W in the emulator to bring up the network status window.')        
        forwardee = None        
        packets_u,packets_d = 0,0
        tick0 = time_ns()
        while not self.shutdown_:
            data , addr = self.fd.recvfrom(64)                   
            if addr != remote:
                forwardee = forwardee or addr # Only update for the first time
                self.fd.sendto(data,remote)
                packets_u += 1
            elif forwardee:
                self.fd.sendto(data,forwardee)    
                packets_d += 1
            if time_ns() - tick0 >= 1e9:
                print('[UDP] ↑ %3d pkt/s ↓ %3d pkt/s' % (packets_u,packets_d),end='\r')
                packets_u,packets_d = 0,0
                tick0 = time_ns()

class TCPServer(ThreadingMixIn,TCPServer):
    def __init__(self, listen_address , listen_port , host_address , host_port):
        self.ws_uri = 'ws://%s:%s/ggpo' % (host_address,host_port)
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
        print('[TCP] INIT Party %s:%s <-> %s' % (*self.client_address,self.server.ws_uri))
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

def suicide():
    print('[ERROR] Shutting down')
    os.kill(os.getpid(), 9)

if __name__ == '__main__':
    argparse = ArgumentParser(description='GGPOSRV3 / MOSCade proxy')
    argparse.add_argument('--host',help='GGPOSRV3 Host',default='localhost:7000')
    argparse.add_argument('--quark',help='GGPOSRV3 Quark',default='[default]')
    argparse.add_argument('--tcp-port',help='TCP listening port',default=8000,type=int)
    argparse.add_argument('--udp-port',help='UDP listening port',default=9000,type=int)

    print('[INFO] GGPO Nexus Proxy starting up')
    print('*** Args',*sys.argv)        
    args = argparse.parse_args()
    print('*** Python Version : %s' % sys.version)    
    try:    
        host,port = args.host.replace('/','').split(':')           
        addr = socket.gethostbyname(host)
        srv_u = UDPForwarder('',args.udp_port,addr,port,args.quark)        
        srv_u.start()        
        srv = TCPServer('',args.tcp_port,addr,port)    
        srv.serve_forever()            
    except Exception as e:
        print('[ERROR]',e)
        traceback.print_stack()        
        input() or suicide()
