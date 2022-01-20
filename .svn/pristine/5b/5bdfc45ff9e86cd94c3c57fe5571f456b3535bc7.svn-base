#! /usr/bin/env python

import sys
import redis
import json
import optparse
import time
from datetime import datetime
from pymongo import MongoReplicaSetClient, MongoClient

MONGO_HOST     = "dev"
MONGO_USERNAME = "mongo"
MONGO_PASSWORD = "secret"
MONGO_REPLICA_SET = "7000"


def create_conn():
    addr = "mongodb://%s:%s@%s" %(MONGO_USERNAME, MONGO_PASSWORD, MONGO_HOST)
    conn = MongoClient(addr, replicaSet = MONGO_REPLICA_SET)
    return conn


def add(db, host, port, proto="socks5", username=None, password=None, source=None):

    data = {
            "host": host, 
            "port": int(port), 
            "proto": proto,
            "username": username,
            "password": password,
            "source": source,
            "success":0,
            "failure": 0,
            "weight":0,
            "enable":1,
            "insert_date": datetime.now()
        }

    print data
    
    if proto == "socks5":
        collection = db.socks5
    elif proto == "http_tunnel":
        collection = db.http_tunnel
    elif proto == "http":
        collection = db.http
    else:
        raise Exception("Error proxy protocol '%s'", proto)


    filter = {"host": host, "port":int(port), "proto":proto}    
    set = {"username": username, "password": password, 
            "source": source, "weight": 0, "enable":1}
    setOnInsert = {"success":0, "failure": 0, "insert_date": datetime.now()}

    collection.update_one(filter, {"$set": set, "$setOnInsert": setOnInsert}, upsert = True)
    

def usage():
    return "Usage: %s <ip:port> [options] \n\npython %s dev1:1221" %(sys.argv[0], sys.argv[0])
    

def main():
    parser = optparse.OptionParser(usage= usage())
    parser.add_option("-u", "--username", action="store", dest="username", type="string")
    parser.add_option("-p", "--password", action="store", dest="password", type="string")
    parser.add_option("-s", "--source", action="store", dest="source", type="string", default="nf")
    parser.add_option("-f", "--file", action="store", dest="file", type="string")
    parser.add_option("-d", "--database", action="store", dest="db", type="string", 
                    default="rps_test", help="[rps|rps_test], default:%default")
    parser.add_option("", "--proto", action="store", dest="proto", type="string", default="socks5")
    options, args = parser.parse_args()

    conn = create_conn()

    if options.db == "rps_test":
        db = conn.rps_test
    elif options.db == "rps":
        db = conn.rps_test
    else:
        sys.stderr.write("invalid database options.\n")
        sys.exit(1)


    if len(args) == 1:
        host, port = args[0].split(":")
        add(db, host, port, options.proto, options.username, options.password, options.source)

    if options.file is not None:
        f = open(options.file, "r")
        for l in f.readlines():
            line = l.strip()
            if not line:
                continue

            host, port  = line.split(":")
            add(db, host, port, options.proto, options.username, options.password, options.source)
    

if __name__ == "__main__":
    main()
