#!/usr/bin/env python

import re
import sys
import time
import logging
import asyncore
import optparse
from datetime import datetime

import schedule
from pymongo import MongoReplicaSetClient, MongoClient


from async_s5 import AsyncSocks5Client
from async_http import AsyncHTTPClient
from async_http_tunnel import AsyncHTTPTunnelClient


from conf import (MONGO_USERNAME, MONGO_PASSWORD, 
                MONGO_HOST, MONGO_REPLICA_SET,
                LOG_LEVEL, LOG_FORMAT)


class BaseCheck(object):
    

    HTTP_PATTERN = re.compile("^HTTP\/1\.\d ([0-9]{3}) .*")
    
    HTTP_HOST = "www.baidu.com"
    HTTP_PORT = 80
    HTTP_PAYLOAD = "domain=.baidu.com"

    WHOIS_HOST = "133.130.126.119"
    WHOIS_PORT = 43
    WHOIS_PAYLOAD = "google.com"
    
    def __init__(self, db, client, payload_proto, concurrency):
        self.dbname = db
        self.client =client
        self.payload_proto = payload_proto
        self.concurrency = concurrency
        self.conn = self.createConn()

    @property
    def db(self):
        if self.dbname == "rps":
            return self.conn.rps
        elif self.dbname == "rps_test":
            return self.conn.rps_test
        else:
            raise Exception("Invalid DataBase <%s>" %self.dbname)

    def createConn(self):
        addr = "mongodb://%s:%s@%s" %(MONGO_USERNAME, MONGO_PASSWORD, MONGO_HOST)
        conn = MongoClient(addr, replicaSet = MONGO_REPLICA_SET)
        return conn


    def getProxyList(self):
        raise NotImplemetedError()

    def updateProxy(self):
        raise NotImplemetedError()

    def checkHTTPResp(self, resp):
        if not resp or resp == " ":
            return False

        try:
            code = self.HTTP_PATTERN.findall(resp)[0]
        except Exception, e:
            return False

        if code != "200":
            return False

        return self.HTTP_PAYLOAD in resp


    def checkWhoisResp(self, resp, proxy_ip):
        if not resp or resp == " ":
            return False

        return (resp == "%s:%s:%s" %(self.WHOIS_PAYLOAD, proxy_ip, self.WHOIS_PAYLOAD))


    def check(self, client, host, port):
        resp = client.buffer.getvalue()
        client.buffer.close()

        if self.payload_proto == "http":
            is_ok = self.checkHTTPResp(resp)
        elif self.payload_proto == "whois":
            is_ok = self.checkWhoisResp(resp, host)
        else:
            raise Exception("unsupport payload protocol '%s'" %self.payload_proto)

        return is_ok

    def afterLoop(self, clients):
        for c in clients:
            if self.check(c, c.host, c.port):
                print "%s:%d is ok" %(c.host, c.port)
                self.updateProxy(c.host, c.port, True)
            else:
                print "%s:%d is bad" %(c.host, c.port)
                self.updateProxy(c.host, c.port, False)

            

    def run(self):
        clients = []
        socket_map = {}
        for r in self.getProxyList():
            try:
                if self.payload_proto == "whois":

                    clients.append(self.client(r["host"], r["port"], 
                        self.WHOIS_HOST, self.WHOIS_PORT, "whois", socket_map))
                else:
                    clients.append(self.client(r["host"], r["port"], 
                        self.HTTP_HOST, self.HTTP_PORT, "http", socket_map))
            except Exception, e:
                continue

            if len(clients) >= self.concurrency:
                asyncore.loop(timeout=5, use_poll=True, map=socket_map)
                self.afterLoop(clients)

                
                clients = []
                socket_map = {}

        if len(clients) < self.concurrency:
            asyncore.loop(timeout=5, use_poll=True, map=socket_map)
            self.afterLoop(clients)   


class HTTPProxyCheck(BaseCheck):
    
    def __init__(self, db, payload_proto, concurrency):
        BaseCheck.__init__(self, db, AsyncHTTPClient, "http", concurrency)

    def getProxyList(self):
        for r in self.db.http.find():
            yield r

    def updateProxy(self, host, port, enable):
        self.db.http.update({"host":host, "port":port}, 
            {"$set":{"enable":int(enable), "last_check":datetime.now()}})

class HTTPTunnelProxyCheck(BaseCheck):

    def __init__(self, db, payload_proto, concurrency):
        BaseCheck.__init__(self, db, AsyncHTTPTunnelClient, payload_proto, concurrency)

    def getProxyList(self):
        for r in self.db.http_tunnel.find({"source":{"$in":["nd", "nf"]}}):
            yield r

    def updateProxy(self, host, port, enable):
        self.db.http_tunnel.update({"host":host, "port":port}, 
            {"$set":{"enable":int(enable), "last_check":datetime.now()}})

class Socks5ProxyCheck(BaseCheck):

    def __init__(self, db, payload_proto, concurrency):
        BaseCheck.__init__(self, db, AsyncSocks5Client, payload_proto, concurrency)

    def getProxyList(self):
        for r in self.db.socks5.find({"source":{"$in":["nd", "nf"]}}):
            yield r
                
    def updateProxy(self, host, port, enable):
        self.db.socks5.update({"host":host, "port":port}, 
            {"$set":{"enable":int(enable), "last_check":datetime.now()}})


def usage():
    return "Usage: %s <http|http_tunnel|socks5> [options]" %(sys.argv[0])


if __name__ == "__main__":
    parser = optparse.OptionParser(usage= usage())
    parser.add_option("-d", "--database", action="store", dest="db", type="string", 
                    default="rps_test", help="[rps|rps_test]")
    parser.add_option("-c", "--concurrency", action="store", dest="concurrency", type="int", 
                    default=1000)
    parser.add_option("-e", "--every", action="store", dest="every", type="int", default=30,
                    help="run check every %default minutes")
    parser.add_option("-p", "--payload_proto", action="store", dest="payload_proto", type="string", 
                    default="whois", help="payload protocol default be whois for http_tunnel \
                    and socks5 proxy, http for http proxy")
    options, args = parser.parse_args()

    logging.basicConfig(level=LOG_LEVEL, format=LOG_FORMAT)

    if len(sys.argv) < 2:
        print usage()
        sys.exit(1)

    protocol = sys.argv[1]
    
    if protocol == "http_tunnel":
        checker = HTTPTunnelProxyCheck(options.db, options.payload_proto, options.concurrency)
    elif protocol == "http":
        checker = HTTPProxyCheck(options.db, options.payload_proto, options.concurrency)
    elif protocol == "socks5":
        checker = Socks5ProxyCheck(options.db, options.payload_proto, options.concurrency)
    else:
        print usage()
        sys.exit(1)

    schedule.every(options.every).minutes.do(checker.run).run()
    while True:
        schedule.run_pending()
        time.sleep(1)
