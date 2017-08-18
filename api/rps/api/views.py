# -*- coding: utf-8 -*-

from flask import Blueprint, current_app, jsonify, request
from datetime import datetime

from ..extensions import mongo
from ..utils import dt2ts

api = Blueprint('api', __name__, url_prefix='/api')

DURATION_MAP = {'s': 1, 'm': 60, 'h': 3600, 'd': 86400}

@api.route("/")
def index():
    return ""

def is_banned(ban):
    if ban is None:
        return False

    ts = ban.get("ts", None)
    if ts is None:
        return False

    duration = ban.get("duration", None)
    if duration is None:
        return False

    uint = DURATION_MAP.get(duration[-1], 0)
    if uint == 0:
        return False

    num = duration.split(duration[-1])[0]

    duration = uint * int(num)
    
    return dt2ts(ts) + duration > dt2ts(datetime.now())
    

@api.route("/<tag>/proxy/<any('socks5', 'http', 'http_tunnel'):proto>/")
def proxy(tag, proto):
    if proto == "socks5":
        collection = mongo.db.socks5
    elif proto ==  "http":
        collection = mongo.db.http
    elif proto == "http_tunnel":
        collection = mongo.db.http_tunnel
    else:
        return jsonify(status="BAD", error="Invalid proto %s" %proto)    

    records = []
    for r in collection.find({}, {"_id":0}):
        if not r.has_key("insert_date"):
            continue
        r["insert_date"] = dt2ts(r["insert_date"])

        ban = r.pop("ban", None)
        if ban is not None:
            ban_tag = ban.get(tag, None)
            ban_all = ban.get("all", None)
            if is_banned(ban_tag):
                r["enable"] = 0
                records.append(r)
                continue

            if is_banned(ban_all):
                r["enable"] = 0
                records.append(r)
                continue                

            if not r.get("enable", 0):
                continue
    
        records.append(r)
    return jsonify(records)



@api.route("/proxy/<any('ban', 'unban'):action>/<host>", methods=["POST"])
def ban(action, host):
    collections = []

    data = request.get_json()
    port = data.pop("port", None)
    proto = data.pop("proto", None)
    tag = data.pop("tag", None)

    filter = {"host":host}
    if port is not None:
        filter["port"] = port

    if proto == "socks5":
        collections.append(mongo.db.socks5)
    elif proto ==  "http":
        collections.append(mongo.db.http)
    elif proto == "http_tunnel":
        collections.append(mongo.db.http_tunnel)
    elif proto == None:
        collections = [mongo.db.socks5, mongo.db.http, mongo.db.http_tunnel]
    else:
        return jsonify(status="BAD", error="Invalid proto %s" %proto)

    if tag is None:
        tag = "all";

   
    data["ts"] = datetime.now()
    tag = "ban.%s" %tag
    if action == "ban":
        set = {tag:  data}
    else:
        set = {tag: None}

    for collection in collections:
        collection.update(filter, {"$set":set}, upsert=True)
    
    return jsonify(status = "OK")

