#/usr/bin/env python

import sys
import asyncore
import logging
import socket
import struct
from cStringIO import StringIO

S5_STATE_INIT = 0
S5_STATE_HANDSHAKE_REQ = 1
S5_STATE_HANDSHAKE_RESP = 2
S5_STATE_AUTH_REQ = 3
S5_STATE_AUTH_RESP = 4
S5_STATE_REQ = 5
S5_STATE_RESP = 6
S5_STATE_PAYLOAD_REQ = 7
S5_STATE_PAYLOAD_RESP = 8
S5_STATE_CLOSED = 9


SOCKS5_VERSION = 5
SOCKS5_AUTH_VERSION = 1
SOCKS5_AUTH_NONE = 0
SOCKS5_AUTH_PASSWD = 2
SOCKS5_CMD_CONNECT = 1
SOCKS5_RSV = 0
SOCKS5_ATYP_IPV4 = 1
SOCKS5_ATYP_DOMAIN = 3

SOCKS5_REPLY_CODE = [
        "succcded",
        "general SOCKS server failure",
        "connection not allowed by ruleset",
        "Network unreachable",
        "Host unreachable",
        "Connection refused",
        "TTL expired",
        "Command not supported",
        "Address type not supported",
        "to X'FF' unassigned",
]

class Socks5TimeOut(Exception):
    pass

class Socks5ProtocolError(Exception):
    pass

class Socks5BadVersion(Socks5ProtocolError):
    def __init__(self, version):
        Socks5ProtocolError.__init__(self, "Bad version <%d>" % version)

class Socks5AuthFail(Socks5ProtocolError):
    def __init__(self):
        Socks5ProtocolError.__init__(self, "Auth faild.")

class Socks5UnsupportAuth(Socks5ProtocolError):
    def __init__(self, method):
        Socks5ProtocolError.__init__(self, "Unsupport auth method <%d>" % method)

class Socks5InvalidRespCode(Socks5ProtocolError):
    def __init__(self, code):
        if code in range(len(SOCKS5_REPLY_CODE)):
            Socks5ProtocolError.__init__(self, SOCKS5_REPLY_CODE[code])
        else:
            Socks5ProtocolError.__init__(self, "Invalid response code <%d>", code)


class AsyncSocks5Client(asyncore.dispatcher):

    MAX_LOOP_COUNT = 720

    HTTP_PAYLOAD = "baidu.com"
    WHOIS_PAYLOAD = "google.com"

    def __init__(self, host, port, remote_host, remote_port, remote_proto, socket_map, uname=None, passwd=None):
        asyncore.dispatcher.__init__(self, map = socket_map)
        self.host = host
        self.port = int(port)
        self.remote_host = remote_host
        self.remote_port = remote_port
        self.remote_proto = remote_proto
        self.socket_map = socket_map
        self.uname = uname
        self.passwd = passwd
        self.buffer = StringIO()
        self.create_socket(socket.AF_INET, socket.SOCK_STREAM)
        self.addr = (self.host, self.port)
        self.logger = logging.getLogger(self.host+":"+str(port))
        self.state = S5_STATE_INIT
        self.count = 0
        self.connect(self.addr)


    @property
    def payload(self):
        if self.remote_proto == "whois":
            return self.WHOIS_PAYLOAD
        elif self.remote_proto == "http":
            return "GET / HTTP/1.1\r\nHOST:%s\r\n\r\n" %(self.remote_host)
        else:
            raise Exception("Unsupport remote protocol")

    def handle_connect(self):
        self.state = S5_STATE_HANDSHAKE_REQ
        return

    def handle_close(self):
        self.close()
        self.state = S5_STATE_CLOSED
        self.logger.debug("close")

    def writable(self):
        self.logger.debug("current state:%d, count:%d", self.state, self.count)
        self.count +=1
        return self.state in [
                              S5_STATE_INIT,
                              S5_STATE_HANDSHAKE_REQ, 
                              S5_STATE_AUTH_REQ,
                              S5_STATE_REQ, 
                              S5_STATE_PAYLOAD_REQ]

    def readable(self):
        return True

    def timeout(self):
        return self.count > self.MAX_LOOP_COUNT

    def handle_write(self):
        if self.state == S5_STATE_HANDSHAKE_REQ:
            self.send_handshake()
        elif self.state == S5_STATE_AUTH_REQ:
            self.send_auth(self.uname, self.passwd)
        elif self.state == S5_STATE_REQ:
            self.send_req(self.remote_host, self.remote_port)
        elif self.state == S5_STATE_PAYLOAD_REQ:
            self.send_payload()

        

    def handle_read(self):
        if self.state == S5_STATE_HANDSHAKE_RESP:
            self.parse_handshake()
        elif self.state == S5_STATE_AUTH_RESP:
            self.parse_auth()
        elif self.state == S5_STATE_RESP:
            self.parse_resp()
        elif self.state == S5_STATE_PAYLOAD_RESP:
            self.parse_payload()
            
        
        #self.read_buffer.write(data)

    def handle_timeout(self):
        self.logger.debug("timeout")
        self.handle_close()

    def handle_error(self):
        t, v, tb = sys.exc_info()
        self.logger.debug("%s", v)
        self.handle_close()
        self.is_ok = False


    def send_handshake(self):
        if self.uname is None:
            buf = "\x05\x01\x00"
        else:
            buf = "\x05\x02\x00\x02"
        self.send(buf)

        self.logger.debug("send handshake")

        self.state = S5_STATE_HANDSHAKE_RESP
        


    def parse_handshake(self):
        data = self.recv(2)
        if len(data) != 2:
            raise Socks5ProtocolError("invalid handshake resp, len: %d" %len(data))
        
        version, method = struct.unpack("2B", data)
        if version != SOCKS5_VERSION:
            raise Socks5BadVersion(version)
        if method != SOCKS5_AUTH_NONE and method != SOCKS5_AUTH_PASSWD:
            raise Socks5UnsupportAuth(method)

        if method == SOCKS5_AUTH_PASSWD:
            self.state = S5_STATE_AUTH_REQ
        else:
            self.state = S5_STATE_REQ

    def send_auth(self, uname, passwd):
        buf = chr(SOCKS5_AUTH_VERSION) + chr(len(uname)) + uname + chr(len(passwd)) + passwd

        self.send(buf)
        
        self.logger.debug("send auth")
        self.state = S5_STATE_AUTH_RESP

    def parse_auth(self):
        data = self.recv(2)
        if len(data) != 2:
            raise Socks5ProtocolError("invalid auth resp, len: %d" %len(data))

        version, status = struct.unpack("2B", data)

        if version != SOCKS5_AUTH_VERSION and version != SOCKS5_VERSION:
            raise Socks5AuthFail()

        if status == 0:
            self.state = S5_STATE_REQ
        else:
            raise Socks5AuthFail()
        
        pass
        

    def send_req(self, host, port):
        buf = struct.pack("3B", SOCKS5_VERSION, SOCKS5_CMD_CONNECT, SOCKS5_RSV)
        if self.is_ip(host):
            buf += chr(SOCKS5_ATYP_IPV4) + socket.inet_aton(host)
        else:
            buf += chr(SOCKS5_ATYP_DOMAIN) + chr(len(host)) + host

        buf += struct.pack(">H", port)

        self.send(buf)
        
        self.logger.debug("send req")
        
        self.state = S5_STATE_RESP
        

    def parse_resp(self):
        data = self.recv(4)
        if len(data) != 4:
            raise Socks5ProtocolError("invalid resp, len: %d" %len(data))

        version, reply_code, _, addr_type = struct.unpack("4B", data)

        if version != SOCKS5_VERSION:
            raise Socks5BadVersion(version)

        if reply_code != 0:
            raise Socks5InvalidRespCode(reply_code)

        if addr_type == SOCKS5_ATYP_IPV4:
            host = socket.inet_ntoa(self.recv(4))
        elif addr_type == SOCKS5_ATYP_DOMAIN:
            hlen = ord(self.recv(1))
            host = self.recv(hlen)
        else:
            raise Socks5ProtocolError("invalid address tyep <%d>" % addr_type)

        port = struct.unpack(">H", self.recv(2))[0]

        self.logger.debug("version: %d, reply_code:%d, addr_type: %d, host: %s, port: %d"\
                %(version, reply_code, addr_type, host, port))

        self.state = S5_STATE_PAYLOAD_REQ

    def send_payload(self):
        self.send(self.payload)
        self.state = S5_STATE_PAYLOAD_RESP
    

    def parse_payload(self):
        data = self.recv(1024)
        data = data.strip()
        self.buffer.write(data)
        self.close()
        

    def print_hex(self, buf):
        print " ".join(hex(ord(n)) for n in buf)

    def is_ip(self, addr):
        try:
            socket.inet_aton(addr)
            return True
        except:
            return False
        
        
        


if __name__ == "__main__":
    logging.basicConfig(level = logging.DEBUG, format="%(name)s %(message)s")

    clients = []
    socket_map = {}

    HTTP_HOST = "www.baidu.com"
    HTTP_PORT = 80

    WHOIS_HOST = "133.130.126.119"
    WHOIS_PORT = 43

    UNAME = ""
    PASSWD = ""
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        host, port = line.split(":")
        clients.append(AsyncSocks5Client(host, port, WHOIS_HOST, WHOIS_PORT, "whois", socket_map, UNAME, PASSWD))
        #clients.append(AsyncSocks5Client(host, port, HTTP_HOST, HTTP_PORT, "http", socket_map, UNAME, PASSWD))
                

    asyncore.loop(timeout=1, use_poll=True, map=socket_map)

    for c in clients:
        print c.buffer.getvalue()
    
    

    


    
    
