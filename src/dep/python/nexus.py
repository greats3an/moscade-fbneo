'''
MOSCade / ggposrv3 Websocket to UDP / TCP reverse proxy

by mos9527 2021,licensed under GPL-2.0
'''
from argparse import ArgumentParser
from select import select
from socketserver import BaseRequestHandler, ThreadingMixIn , TCPServer
from threading import Thread
from time import sleep, time_ns
import socket,shutil,traceback,struct,os
import logging
import websocket

import coloredlogs
coloredlogs.install(level=logging.DEBUG,fmt='[%(asctime)s] [%(levelname)s] %(message)s',datefmt='%H:%M:%S')
logging.basicConfig(handlers=[logging.FileHandler('nexus.log'), logging.StreamHandler()])

# pip install websocket-client coloredlogs

# Request
RQST_JOIN            = b'\xff\xfe'
RQST_SYMM_PROTO      = b'\xff\xfc'
RQST_CONE_PROTO      = b'\xff\xfa'
RQST_PROXY           = b'\xff\xf0'
# Responses
RESP_OK              = b'\x00\x00'
RESP_REJCT           = b'\x00\x01'
RESP_SYNC            = b'\x00\x02'
RESP_SYMM_PROTO      = b'\x00\x03'
RESP_CONE_PROTO      = b'\xff\x05'
RESP_PROXY           = b'\xff\x0f'
# P2P
P2P_PACKET           = b'\x7f\x0f'
P2P_PING             = b'\x7f\x00'
P2P_PONG             = b'\x7f\x01'
# Protocols
PROTOCOL_CONE        = 'PROTOCOL_CONE'
PROTOCOL_SYMM        = 'PROTOCOL_SYMM'

class UDPForwarder(Thread):
    def __init__(self,listen_address , listen_port , host_address , host_port , quark):        
        self.shutdown_ = False

        self.listen_address = (listen_address,int(listen_port))
        self.host_address = (host_address,int(host_port))
        self.remote_address = None
        self.fd = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
        self.fd.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.fd.bind(self.listen_address)        
        
        self.quark = quark        
        self.proto = PROTOCOL_SYMM
        
        self.logger = logging.getLogger('UDP-STUN')

        super().__init__()
        self.daemon = True
    
    def receive_resp(self,data=None):
        if not data:
            data , addr = self.fd.recvfrom(64)
        return data[:2],data[2:]

    def upgrade_proto(self,proto):
        if proto == PROTOCOL_CONE:
            self.fd.sendto(RQST_CONE_PROTO,self.host_address)
        elif proto == PROTOCOL_SYMM:
            self.fd.sendto(RQST_SYMM_PROTO,self.host_address)
        else:
            raise Exception("Invalid Protocol %s" % proto)

    def send_symm(self,data):
        self.fd.sendto(RQST_PROXY + data,self.host_address)
        return 1

    def send_cone(self,data):
        self.fd.sendto(P2P_PACKET + data,self.remote_address)
        return 1

    def run(self):               
        self.logger.debug('STUN Traversal Server : %s:%s' % self.host_address)        
        self.fd.sendto(RQST_JOIN + self.quark.encode(),self.host_address)     
        resp , data = self.receive_resp()                   
        assert resp==RESP_OK,'Bad response %s' % resp
        self.logger.info('STUN Connected : udp/%s:%s <-> %s:%s' % (*self.listen_address,*self.host_address)) 
        resp , data = self.receive_resp()
        assert resp==RESP_SYNC,'Bad response %s' % resp
        self.logger.debug('SYNC Packet : %s' % data)
        self.remote_address = ((socket.inet_ntoa(data[:4]),struct.unpack('<H',data[4:])[0]))
        self.logger.info('Protip : You can press Alt + W in the emulator to bring up the network status window.')
        self.logger.info('Protip : [ SYMM ] Indicates the server is handling the NAT traversal, which can cause performance degradation depending on where you live.')
        self.logger.info('         [ P2P / CONE ] However,means that you have established a DIRECT conenction with your opponent. Which should yield the best experience.')
        self.logger.info('         Otherwise, a VPN service may be used to imporve it even further.')        
        # Begin forwarding        
        forwardee = None
        packets_u,packets_d = 0,0
        tick0 = time_ns()
        pings,pongs = 0,0
        # Proitize SYMM Proto first since it has the best avaibilty. 
        # If a direct peer-to-peer connection is fesabile, connection will corospondingly 
        # change to mode of that
        self.upgrade_proto(PROTOCOL_SYMM)
        while not self.shutdown_:
            data , addr = self.fd.recvfrom(64)                   
            if addr == forwardee or (addr != self.host_address and addr != self.remote_address):
                # Supposingly coming from localhost,which is the target we're trying to proxy
                forwardee = addr
                if self.proto == PROTOCOL_SYMM:                
                    packets_u += self.send_symm(data)
                elif self.proto == PROTOCOL_CONE:
                    packets_u += self.send_cone(data)
            elif forwardee:
                # Remote packet, either from server's proxy or the other peer                
                resp, data_ = self.receive_resp(data)
                if resp == RESP_PROXY or resp == P2P_PACKET:
                    # Packet transfer
                    self.fd.sendto(data_,forwardee)                                
                elif resp == RESP_SYMM_PROTO:                    
                    self.proto = PROTOCOL_SYMM
                elif resp == RESP_CONE_PROTO:
                    self.proto = PROTOCOL_CONE                    
                elif resp == P2P_PING:
                    self.fd.sendto(P2P_PONG,self.remote_address)
                elif resp == P2P_PONG:                    
                    pongs += 1        
                    # When pong/ping ration reaches beyond 50%, consider
                    # A P2P connection is feasbile. Try to switch protocols
                    if pings / pongs > 0.5:
                        self.upgrade_proto(PROTOCOL_CONE)
                elif resp == RESP_REJCT:
                    pass
                else:
                    self.logger.warn('PROXY Unexpected packet : 0x%s' % resp.hex()) 
                packets_d += 1
            if time_ns() - tick0 >= 1e9:
                # Attempt to establish a CONE Protocol by sending ping-pongs every 1s
                # Once received the corrsponding PONG, the consensus
                # is made and P2P connection can be made
                if self.proto == PROTOCOL_SYMM:
                    self.fd.sendto(P2P_PING,self.remote_address)
                    pings += 1
                cols = shutil.get_terminal_size().columns
                status = "\033[30;107m UDP "+ ('\033[103m SYMM ' if self.proto == PROTOCOL_SYMM else'\033[42m P2P / CONE ') + "\033[46;97m      ↑ %3d pkt/s        ↓ %3d pkt/s" % (packets_u,packets_d)
                print(status.ljust(cols,' '),end='\r')
                packets_u,packets_d = 0,0
                tick0 = time_ns()

class TCPServer(ThreadingMixIn,TCPServer):
    def __init__(self, listen_address , listen_port , host_address , host_port):
        self.ws_uri = 'ws://%s:%s/ggpo' % (host_address,host_port)
        super().__init__((listen_address,int(listen_port)),TCPHandler,True)     
        self.logger = logging.getLogger('TCP-WS')           
        self.logger.info('READY : tcp/%s:%s <-> %s' % (*self.server_address,self.ws_uri))

class TCPHandler(BaseRequestHandler):
    def __init__(self, request, client_address, server) -> None:
        self.shutdown_ = False
        super().__init__(request, client_address, server)        

    def handle(self) -> None:        
        self.server : TCPServer        
        self.ws = websocket.WebSocket()        
        self.ws.connect(self.server.ws_uri)         
        self.request.setblocking(False)                
        self.server.logger.info('INIT Party %s:%s <-> %s' % (*self.client_address,self.server.ws_uri))
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
    logging.critical('Shutting down')    
    os.kill(os.getpid(), 9)

if __name__ == '__main__':
    argparse = ArgumentParser(description='GGPOSRV3 / MOSCade proxy')
    argparse.add_argument('--host',help='GGPOSRV3 Host',default='localhost:7000')
    argparse.add_argument('--quark',help='GGPOSRV3 Quark',default='[default]')
    argparse.add_argument('--tcp-port',help='TCP listening port',default=8000,type=int)
    argparse.add_argument('--udp-port',help='UDP listening port',default=9000,type=int)

    logging.info('GGPO Nexus Proxy starting up')    
    args = argparse.parse_args()    
    try:    
        host,port = args.host.replace('/','').split(':')           
        addr = socket.gethostbyname(host)
        srv_u = UDPForwarder('',args.udp_port,addr,port,args.quark)        
        srv_u.start()        
        srv = TCPServer('',args.tcp_port,addr,port)    
        srv.serve_forever()            
    except Exception as e: 
        logging.error(e)       
        traceback.print_stack()        
        suicide()
