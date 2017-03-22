#! /usr/bin/env python

import socket
import optparse

HTTP_PROXY_HOST = "dev1"
HTTP_PROXY_PORT = 8889
HTTP_PROXY_HOST = "localhost"

class HTTPTunnelPorxy(object):

    def __init__(self, proxy_host, proxy_port):
        self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            self.s.connect((proxy_host, proxy_port))
        except:
            print "can't connect porxy: %s:%d" %(proxy_host, proxy_port)
            exit(1);
        
    def handshake(self, host, port):
        payload = "CONNECT %s:%d HTTP/1.1\r\n" %(host, port)
        payload = payload + "HOST: %s\r\n" %host
        payload = payload + "User-agent: RPS/HTTP PROXY\r\n"
        payload = payload + "\r\n\r\n"

        print "---------------------------------------------"
        print "send:\n"
        print payload

        self.s.sendall(payload)

        data = self.s.recv(512)
        print "recv: \n"
        print data

        data = data.strip()
        if data == "HTTP/1.0 200 Connection established":
            print "handshake success"
            return True
        else:
            print "handshake failed"
            return False

    def doHTTPRequest(self, host, port):
        if not self.handshake(host, port):
            return

        payload = "GET / HTTP/1.1\r\n"
        payload = payload + "HOST: %s\r\n" %host
        payload = payload + "\r\n\r\n"

        print "---------------------------------------------"
        print "send: \n"
        print payload

        self.s.sendall(payload)

        data = self.s.recv(1024)
        print "recv: \n"
        print data

   
    def doHTTPSRequest(self, host, port):
        if not self.handshake(host, port):
            return
        
        payload = "GET https://%s HTTP/1.1\r\n" %host
        payload = payload + "HOST: %s\r\n" %host
        payload = payload + "\r\n\r\n"

        print "---------------------------------------------"
        print "send: \n"
        print payload

        self.s.sendall(payload)

        data = self.s.recv(1024)
        print "recv: \n"
        print data

   
    def doWhoisRequest(self, host, port, query):
        if not self.handshake(host, port):
            return

        payload = "%s\r\n" %query
        
        print "---------------------------------------------"
        print "send: \n"
        print payload

        self.s.sendall(payload)

        data = self.s.recv(1024)
        print "recv: \n"
        print data

        
        


def main():
    proxy = HTTPTunnelPorxy(HTTP_PROXY_HOST, HTTP_PROXY_PORT)


    proxy.doHTTPRequest("www.google.com", 80)
    #proxy.doHTTPSRequest("www.google.com", 80)
    #proxy.doWhoisRequest("whois.godaddy.com", 43, "kenshinx.me")

    

        



if __name__ == "__main__":
    main()
