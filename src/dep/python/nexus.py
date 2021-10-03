'''
MOSCade / ggposrv3 Websocket to UDP / TCP reverse proxy

by mos9527 2021,licensed under GPL-2.0
'''
from argparse import ArgumentParser
from select import select
from socketserver import BaseRequestHandler, ThreadingMixIn , TCPServer
from struct import pack, unpack
from threading import Lock, Thread
from time import sleep
import logging,socket,sys

import websocket
# pip install websocket-client

class UDPServer(Thread):
    def __init__(self,listen_address , listen_port , ws_uri):
        self.shutdown = False

        self.server_address = (listen_address,int(listen_port))
        self.ws_uri = ws_uri

        self.ws = websocket.WebSocket()
        self.ws.connect(self.ws_uri)

        self.fd = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
        self.fd.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.fd.bind(self.server_address)
        self.logger = logging.getLogger('UDPServer')
        self.addrs = dict()

        self.recv_thread = Thread(target=self.recv_ws,daemon=True)        
        self.recv_lock = Lock()        
        self.recv_thread.start()        
        
        super().__init__(daemon=True)
        print('** READY : udp/%s:%s <-> %s' % (*self.server_address,self.ws_uri))

    def recv_ws(self):
        while not self.shutdown:    
            try:    
                buf = self.ws.recv()
                assert type(buf) != str
                # self.logger.debug('WSOCK %s' % buf.hex(' '))
                baddr, data = buf[:6],buf[6:]                
                with self.recv_lock:
                    for addr in self.addrs:
                        if addr != baddr:                        
                            self.fd.sendto(data,self.bytes2addr(addr))
                            # self.logger.debug('WSOCK %s -> %s' % (baddr,addr))     
            except Exception as e:
                self.logger.fatal("CONNECTION LOST : %s" % e)
                suicide()                

    def addr2bytes(self, addr : tuple):	        	
        '''packing address tuple'''
        host,port = addr
        return socket.inet_aton(host) + pack('H', port)

    def bytes2addr(self, b : bytes):
        '''unpacking address tuple'''
        host,port = b[:4],b[4:]
        return socket.inet_ntoa(host), unpack('H',port)[0]

    def run(self):                
        while not self.shutdown:                     
            # new connection w/ data to be sent                
            data, addr = self.fd.recvfrom(4096)
            # self.logger.debug('UDP %s:%s -> %s' % (*addr,data.hex(' ')))
            baddr = self.addr2bytes(addr)
            self.ws.send_binary(baddr + data)
            with self.recv_lock:
                self.addrs[baddr] = 1 # marking the address where proceeding data will be send to

class TCPServer(ThreadingMixIn,TCPServer):
    def __init__(self, listen_address , listen_port , ws_uri):
        self.ws_uri = ws_uri
        super().__init__((listen_address,int(listen_port)),TCPHandler,True)        
        print('** READY : tcp/%s:%s <-> %s' % (*self.server_address,self.ws_uri))

class TCPHandler(BaseRequestHandler):
    def handle(self) -> None:        
        self.server : TCPServer        
        self.ws = websocket.WebSocket()        
        self.ws.connect(self.server.ws_uri)         
        self.request.setblocking(False)
        self.logger = logging.getLogger('NODE (%s) %s:%s' % (self.server.socket_type.name,*self.client_address))
        self.logger.setLevel(logging.ERROR)        
        self.shutdown_ = False
        # Maintain this connection whilst both sockets are alive
        buf = bytearray()
        while not self.shutdown_:            
            s1,s2 = self.request,self.ws.sock
            r,w,_ = select([s1,s2],[],[])
            try:
                if (s1 in r):                
                    buf_ = s1.recv(4096)                    
                    # self.logger.debug('TCP: %s' % buf_.hex(' '))
                    self.ws.send_binary(buf_)
                if (s2 in r):                
                    buf_ = self.ws.recv()
                    # self.logger.debug('WSOCK %s' % buf_.hex(' '))
                    buf += buf_
                if (buf):                
                    buf = buf[s1.send(buf):]            
            except Exception as e:
                self.logger.fatal("Exception : %s" % e)
                self.ws.close()
                if self.request : self.request.close()        
                break
            sleep(0.01)
        self.logger.fatal("CONNECTION LOST")
        suicide()                

is_shutting_down = False
def suicide():
    global is_shutting_down
    if is_shutting_down:
        return
    is_shutting_down = True
    logging.fatal('Shutting down')
    srv_u.shutdown = True
    srv.shutdown()
    sys.exit(1)

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
        srv_u.shutdown = True
        suicide()
