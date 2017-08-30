#/usr/bin/env python

import sys
import re
import asyncore
import logging
import socket
from cStringIO import StringIO

HTTP_PROXY_STATE_INIT = 0
HTTP_PROXY_STATE_REQ = 1
HTTP_PROXY_STATE_RESP = 2
HTTP_PROXY_STATE_CLOSED = 3

class HTTPProxyProtocolError(Exception):
    pass

class HTTPProxyTimeout(Exception):
    pass


class AsyncHTTPClient(asyncore.dispatcher):

    MAX_LOOP_COUNT = 720
    PATTERN = re.compile("^HTTP\/1\.\d ([0-9]{3}) .*")


    def __init__(self, host, port, remote_host, remote_port, remote_proto, socket_map):
        asyncore.dispatcher.__init__(self, map = socket_map)
        self.host = host
        self.port = int(port)
        self.remote_host = remote_host
        self.remote_port = remote_port
        self.addr = (self.host, self.port)
        self.socket_map = socket_map
        self.buffer = StringIO()
        self.logger = logging.getLogger(self.host+":"+str(port))
        self.create_socket(socket.AF_INET, socket.SOCK_STREAM)
        self.count = 0
        self.state = HTTP_PROXY_STATE_INIT
        self.connect(self.addr)
            
        

    def handle_connect(self):
        self.state = HTTP_PROXY_STATE_REQ

    def handle_close(self):
        self.state = HTTP_PROXY_STATE_CLOSED
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
                    HTTP_PROXY_STATE_RESP
                ]

    def writable(self):
        self.logger.debug("current state:%d, count:%d", self.state, self.count)
        self.count +=1
        return self.state in [
                    HTTP_PROXY_STATE_INIT,
                    HTTP_PROXY_STATE_REQ
                ]

    @property
    def payload(self):
        payload = "GET http://%s/ HTTP/1.1\r\n" %self.remote_host
        payload = payload + "HOST: %s\r\n" %self.remote_host
        payload = payload + "User-Agent: RPS/HTTP PROXY\r\n"
        payload = payload + "Accept: */*\r\n"
        payload = payload + "\r\n"
        return payload

    def send_request(self):
        self.send(self.payload)
        self.state = HTTP_PROXY_STATE_RESP
        self.logger.debug("send request")

    def parse_response(self):
        data = self.recv(1024)
        data = data.strip()

        try:
            data = data.encode("utf-8")
        except:
            raise HTTPProxyProtocolError("Invalid response")

        self.buffer.write(data)
        self.close()
            


    def handle_read(self):
        if self.state == HTTP_PROXY_STATE_RESP:
            self.parse_response()

    def handle_write(self):
        if self.state == HTTP_PROXY_STATE_REQ:
            self.send_request()
        
    def timeout(self):
        return self.count > self.MAX_LOOP_COUNT


    def __str__(self):
        return "%s:%d" %(self.host, self.port)





if __name__ == "__main__":
    logging.basicConfig(level = logging.DEBUG, format="%(name)s %(message)s")

    clients = []
    socket_map = {}

    REMOTE_HOST = "www.baidu.com"
    REMOTE_PORT = 80


    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        host, port = line.split(":")
        clients.append(AsyncHTTPProxyClient(host, port, REMOTE_HOST, REMOTE_PORT, socket_map))
                

    asyncore.loop(timeout=1, use_poll=True, map=socket_map)


    for c in clients:
        print c.buffer.getvalue()
