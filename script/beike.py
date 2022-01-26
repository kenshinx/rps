#! /usr/bin/env python

import sys
import json
import logging
import optparse
import time
import requests
import schedule
import threading
from datetime import datetime, timedelta
from pymongo import MongoReplicaSetClient, MongoClient

MONGO_HOST = "10.160.151.227:7528, 10.160.121.95:7528"
MONGO_USERNAME = "mongo"
MONGO_PASSWORD = "70d05ef8690900a4"
MONGO_REPLICA_SET = "7528"


SOCKS5_API = "http://getip.beikeruanjian.com/getip/?user_id=20220107122456050715&token=1HN2EVg0GWx8tca9&server_id=16607&num=200&protocol=2&format=json&jsonipport=1&jsonexpiretime=1&jsoncity=1&jsonisp=1&dr=0&province=1&city=1&citycode="
HTTP_API = "http://getip.beikeruanjian.com/getip/?user_id=20220107122456050715&token=1HN2EVg0GWx8tca9&server_id=16607&num=200&protocol=1&format=json&jsonipport=1&jsonexpiretime=1&jsoncity=1&jsonisp=1&dr=0&province=1&city=1&citycode="
HTTPS_API = "http://getip.beikeruanjian.com/getip/?user_id=20220107122456050715&token=1HN2EVg0GWx8tca9&server_id=16607&num=200&protocol=1&format=json&jsonipport=1&jsonexpiretime=1&jsoncity=1&jsonisp=1&dr=0&province=1&city=1&citycode="


class BeikeProxy(threading.Thread):
    def __init__(self, dbname):
        threading.Thread.__init__(self)
        self.dbname = dbname
        self.conn = self.createConn()

    @property
    def db(self):
        if self.dbname == "rps":
            return self.conn.rps
        elif self.dbname == "rps_test":
            return self.conn.rps_test
        else:
            raise Exception("Invalid DataBase <%s>" % self.dbname)

    def createConn(self):
        addr = "mongodb://%s:%s@%s" % (MONGO_USERNAME,
                                       MONGO_PASSWORD, MONGO_HOST)
        conn = MongoClient(addr, replicaSet=MONGO_REPLICA_SET)
        return conn

    def insertProxy(self, r, proto):
        if proto == "socks5":
            collection = self.db.socks5
        elif proto == "http_tunnel":
            collection = self.db.http_tunnel
        elif proto == "http":
            collection = self.db.http
        else:
            raise Exception("Error proxy protocol '%s'", proto)

	
        expire_date = datetime.strptime(r["expire_time"], "%Y-%m-%d %H:%M:%S")
        # expire in one minute.
        if expire_date < (datetime.now() + timedelta(seconds=30)):
            print "%s:%d will expire in half minute" % (r["ip"], int(r["port"]))
            return

        filter = {
                "host": r["ip"], 
                "port": r["port"], 
                "proto": proto
                }

        set = {
                "username": None, 
                "password": None, 
                "weight": 0,
                "enable": 1, 
                "expire_date": expire_date, 
                "country":"China",
                "province":r.get("province"), 
                "city":r.get("city"), 
                "isp": r.get("isp")
                }

        setOnInsert = {
                "success": 0, 
                "failure": 0,
                "source": "beike", 
                "insert_date": datetime.now()
                }
	
        collection.update_one(
            filter, {"$set": set, "$setOnInsert": setOnInsert}, upsert=True)

    def _addProxy(self, api, proto):
        try:
            resp = requests.get(api)
        except Exception, e:
            logging.error("beike %s api http request error: %s", proto, str(e))
            return
        try:
            records = resp.json()["data"]
        except Exception, e:
            logging.error("beike %s api json decode error: %s, resp content: %s", proto, str(
                e), resp.content)
            return
        logging.info("get %d %s proxy records from beike API." %
                     (len(records), proto))

        for r in records:
            self.insertProxy(r, proto)

    def addNewProxy(self):
        self._addProxy(SOCKS5_API, "socks5")
        time.sleep(30)
        self._addProxy(HTTP_API, "http")
        time.sleep(30)
        self._addProxy(HTTPS_API, "http_tunnel")

    def delExpireProxy(self):
        # expire in half minute.
        expire_date = datetime.now() + timedelta(seconds=30)
        socks5 = self.db.socks5.delete_many(
            {"source": "beike", "expire_date": {"$lt": expire_date}})
        http = self.db.http.delete_many(
            {"source": "beike", "expire_date": {"$lt": expire_date}})
        http_tunnel = self.db.http_tunnel.delete_many(
            {"source": "beike", "expire_date": {"$lt": expire_date}})
        logging.info("delete [s5: %d, http: %d, https: %d] expire records from mongo."
                     % (socks5.deleted_count, http.deleted_count, http_tunnel.deleted_count))

    def run(self):
        self.delExpireProxy()
        self.addNewProxy()


def run_as_thread(db):
    bp = beikeProxy(db)
    bp.start()


def main():
    parser = optparse.OptionParser()
    parser.add_option("-d", "--database", action="store", dest="db", type="string",
                      default="rps_test", help="[rps|rps_test]")
    parser.add_option("-e", "--every", action="store", dest="every", type="int", default=5,
                      help="run check every %default minutes")
    options, args = parser.parse_args()

    logging.basicConfig(level=logging.INFO,
                        format="%(asctime)s %(levelname)s: %(message)s")

    schedule.every(options.every).seconds.do(run_as_thread, options.db).run()
    while True:
        schedule.run_pending()
        time.sleep(1)


if __name__ == "__main__":
    main()
