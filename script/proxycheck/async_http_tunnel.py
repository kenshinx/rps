#/usr/bin/env python

import sys
import re
import asyncore
import logging
import socket
from cStringIO import StringIO

HTTP_TUNNEL_STATE_INIT = 0
HTTP_TUNNEL_STATE_CONNECT_REQ = 1
HTTP_TUNNEL_STATE_CONNECT_RESP = 2
HTTP_TUNNEL_STATE_REQ = 3
HTTP_TUNNEL_STATE_RESP = 4
HTTP_TUNNEL_STATE_PAYLOAD_REQ = 5
HTTP_TUNNEL_STATE_PAYLOAD_RESP = 6
HTTP_TUNNEL_STATE_CLOSED = 7

class HTTPTunnelProtocolError(Exception):
    pass

class HTTPTunnelTimeout(Exception):
    pass


class AsyncHTTPTunnelClient(asyncore.dispatcher):

    MAX_LOOP_COUNT = 720

    WHOIS_PAYLOAD =  "google.com"

    HTTP_PATTERN = re.compile("^HTTP\/1\.\d ([0-9]{3}) .*")


    def __init__(self, host, port, remote_host, remote_port, remote_proto, socket_map):
        asyncore.dispatcher.__init__(self, map = socket_map)
        self.host = host
        self.port = int(port)
        self.remote_host = remote_host
        self.remote_port = remote_port
        self.remote_proto = remote_proto
        self.addr = (self.host, self.port)
        self.socket_map = socket_map
        self.buffer = StringIO()
        self.logger = logging.getLogger(self.host+":"+str(port))
        self.create_socket(socket.AF_INET, socket.SOCK_STREAM)
        self.count = 0
        self.state = HTTP_TUNNEL_STATE_INIT
        self.connect(self.addr)
        

    def handle_connect(self):
        self.state = HTTP_TUNNEL_STATE_CONNECT_REQ

    def handle_close(self):
        self.state = HTTP_TUNNEL_STATE_CLOSED
        self.logger.debug("close")
        self.close()

    def handle_timeout(self):
        self.logger.debug("timeout")
        self.handle_close()

    def handle_error(self):
        t, v, tb = sys.exc_info()
        self.logger.debug("%s", v)
        self.handle_close()
        self.is_ok = False

    def readble(self):
        return self.state in [
                    HTTP_TUNNEL_STATE_CONNECT_RESP,
                    HTTP_TUNNEL_STATE_RESP
                ]

    def writable(self):
        self.logger.debug("current state:%d, count:%d", self.state, self.count)
        self.count +=1
        return self.state in [
                    HTTP_TUNNEL_STATE_INIT,
                    HTTP_TUNNEL_STATE_CONNECT_REQ,
                    HTTP_TUNNEL_STATE_REQ
                ]


    
    def send_connect(self):
        payload = "CONNECT %s:%d HTTP/1.1\r\n" %(self.remote_host, self.remote_port)
        payload = payload + "HOST: %s\r\n" %self.remote_host
        payload = payload + "User-Agent: RPS/HTTP PROXY\r\n"
        payload = payload + "\r\n"

        self.send(payload)

        self.state = HTTP_TUNNEL_STATE_CONNECT_RESP
        self.logger.debug("send connect")

    def parse_connect(self):
        data = self.recv(1024)
        try:
            code = self.HTTP_PATTERN.findall(data)[0]
        except Exception, e:
            self.logger.debug("invalid http connect response: %s", data[:64])
            raise HTTPTunnelProtocolError("invalid http connect response")

        self.logger.debug("receive handhshake response code %s", code)
        
        if code in ["407", "403"]:
            raise HTTPTunnelProtocolError("Need authentication")
        
        if code != "200":
            raise HTTPTunnelProtocolError("Invalid connect response code %s" %code)
        
        self.state = HTTP_TUNNEL_STATE_REQ
        return

        
    @property
    def payload(self):
        if self.remote_proto == "http":
            payload = "GET / HTTP/1.1\r\n"
            payload = payload + "HOST: %s\r\n" %self.remote_host
            payload = payload + "\r\n"
        elif self.remote_proto == "whois":
            payload = self.WHOIS_PAYLOAD
        else:
            raise HTTPTunnelProtocolError("unsupport remote proto %s" %self.remote_proto)
        return payload

    def send_request(self):
        self.send(self.payload)
        self.state = HTTP_TUNNEL_STATE_RESP
        self.logger.debug("send request")

    def parse_response(self):
        data = self.recv(1024)
        data = data.strip()
        self.buffer.write(data)
        self.close()
            

    def handle_read(self):
        if self.state == HTTP_TUNNEL_STATE_CONNECT_RESP:
            self.parse_connect()
        elif self.state == HTTP_TUNNEL_STATE_RESP:
            self.parse_response()

    def handle_write(self):
        if self.state == HTTP_TUNNEL_STATE_CONNECT_REQ:
            self.send_connect()
        elif self.state == HTTP_TUNNEL_STATE_REQ:
            self.send_request()
        
    def timeout(self):
        return self.count > self.MAX_LOOP_COUNT


    def __str__(self):
        return "%s:%d" %(self.host, self.port)





if __name__ == "__main__":
    logging.basicConfig(level = logging.DEBUG, format="%(name)s %(message)s")

    clients = []
    socket_map = {}

    HTTP_HOST = "www.baidu.com"
    HTTP_PORT = 80

    WHOIS_HOST = "36.55.244.17"
    WHOIS_PORT = 43


    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        host, port = line.split(":")
        clients.append(AsyncHTTPTunnelClient(host, port, WHOIS_HOST, WHOIS_PORT, "whois", socket_map))
        #clients.append(AsyncHTTPTunnelClient(host, port, HTTP_HOST, HTTP_PORT, "http", socket_map))
                

    asyncore.loop(timeout=1, use_poll=True, map=socket_map)


    for c in clients:
        print c.buffer.getvalue()
