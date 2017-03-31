#! /usr/bin/env python

import re
import socket
import optparse

HTTP_PROXY_HOST = "dev1"
HTTP_PROXY_PORT = 8889
HTTP_PROXY_HOST = "localhost"
HTTP_PROXY_PORT = 9891
HTTP_PROXY_UNAME = "rps"
HTTP_PROXY_PASSWD = "secret"


class HTTPTunnelPorxy(object):

    pattern = re.compile("^HTTP\/1\.\d ([0-9]{3}) .*")

    def __init__(self, proxy_host, proxy_port, proxy_uname, proxy_passwd):
        self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            self.s.connect((proxy_host, proxy_port))
        except:
            print "can't connect porxy: %s:%d" %(proxy_host, proxy_port)
            exit(1);
        
        self.uname = proxy_uname;
        self.passwd = proxy_passwd;
        
    def handshake(self, host, port):
        payload = "CONNECT %s:%d HTTP/1.1\r\n" %(host, port)
        payload = payload + "HOST: %s\r\n" %host
        payload = payload + "User-agent: RPS/HTTP PROXY\r\n"
        payload = payload + "\r\n\r\n"

        print "---------------------------------------------"
        print "send:\n"
        print payload

        self.s.sendall(payload)

        data = self.s.recv(1024)
        print "recv: %d character\n" %len(data)
        print data

        data = data.strip()
        try:
            code = self.pattern.findall(data)[0]
        except Exception, e:
            print "invalid http response"
            return False


        if code == "200":
            print "handshake success"
            return True
        elif code == "407":
            return self.doAuth(host, port)
        else:
            print "invalid http response code"
            return False

    def doAuth(self, host, port):

        credential = "%s:%s" %(self.uname, self.passwd)
        credential = credential.encode("base64")
        credential = "Basic %s" %credential

        print credential

        payload = "CONNECT %s:%d HTTP/1.1\r\n" %(host, port)
        payload = payload + "HOST: %s\r\n" %host
        payload = payload + "User-agent: RPS/HTTP PROXY\r\n"
        payload = payload + "Proxy-Authorization: %s\r\n" %credential 
        payload = payload + "\r\n\r\n"

        print "---------------------------------------------"
        print "send:\n"
        print payload

        self.s.sendall(payload)

        data = self.s.recv(1024)
        print "recv: %d character\n" %len(data)
        print data

        data = data.strip()
        try:
            code = self.pattern.findall(data)[0]
        except Exception, e:
            print "invalid http response"
            return False

        if code == "200":
            print "http authenticate success"
            return True
        elif code == "407":
            print "http authenticate fail"
            return False
        else:
            print "invalid http response code"
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
        print "recv: %d character\n" %len(data)
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
        print "recv: %d character\n" %len(data)
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
    proxy = HTTPTunnelPorxy(HTTP_PROXY_HOST, HTTP_PROXY_PORT, 
            HTTP_PROXY_UNAME, HTTP_PROXY_PASSWD)

    proxy.doHTTPRequest("www.google.com", 80)
    #proxy.doHTTPSRequest("www.google.com", 80)
    #proxy.doWhoisRequest("whois.godaddy.com", 43, "kenshinx.me")

    

        



if __name__ == "__main__":
    main()
