#! /usr/bin/env python

import sys
import json
import logging
import optparse
import time
import requests
import schedule
from datetime import datetime
from pymongo import MongoReplicaSetClient, MongoClient

from proxycheck.check import BaseCheck
from proxycheck.async_http_tunnel import AsyncHTTPTunnelClient


API = "http://10.138.107.157:8360/api/proxyip?limit=20000&token=78f75f66-b50a-11e5-b03e-6c4008a93f02"


class NDCheck(BaseCheck):
    def __init__(self, db, payload_proto, concurrency):
        BaseCheck.__init__(self, db, AsyncHTTPTunnelClient, "whois", concurrency)

    def getProxyList(self):
        resp = requests.get(API)
        for r in resp.json()["data"]:
            yield {"host":r["ip"], "port":int(r["port"])}

        

    def updateProxy(self, host, port, enable):
        if not enable:
            return

        data = {
            "host": host, 
            "port": int(port), 
            "proto": "http_tunnel",
            "username": None,
            "password": None,
            "source": "nd",
            "success":0,
            "failure": 0,
            "weight":0,
            "enable":1,
            "insert_date": datetime.now()
        }
        filter = {"host": host, "port":int(port), "proto":"http_tunnel"}    
        set = {"username": None, "password": None, "weight": 0, "enable":1}
        setOnInsert = {"success":0, "failure": 0, "source": "nd", "insert_date": datetime.now()}

        print data

        self.db.http_tunnel.update_one(filter, {"$set": set, "$setOnInsert": setOnInsert}, upsert = True)

    

def usage():
    return "Usage: %s <ip:port> [options] \n\npython %s dev1:1221" %(sys.argv[0], sys.argv[0])
    

def main():
    parser = optparse.OptionParser(usage= usage())
    parser.add_option("-d", "--database", action="store", dest="db", type="string", 
                    default="rps_test", help="[rps|rps_test]")
    parser.add_option("-c", "--concurrency", action="store", dest="concurrency", type="int", 
                    default=1000)
    parser.add_option("-e", "--every", action="store", dest="every", type="int", default=10,
                    help="run check every %default minutes")
    parser.add_option("-p", "--payload_proto", action="store", dest="payload_proto", type="string", 
                    default="whois", help="payload protocol default be whois for http_tunnel \
                    and socks5 proxy, http for http proxy")
    options, args = parser.parse_args()


    logging.basicConfig(level=logging.INFO, format="%(name)s %(message)s")

    
    ndcheck = NDCheck(options.db, options.payload_proto, options.concurrency)

    schedule.every(options.every).minutes.do(ndcheck.run).run()
    while True:
        schedule.run_pending()
        time.sleep(1)

    

if __name__ == "__main__":
    main()
