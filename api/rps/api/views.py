# -*- coding: utf-8 -*-

from flask import Blueprint, current_app, jsonify

from ..extensions import mongo
from ..utils import dt2ts

api = Blueprint('api', __name__, url_prefix='/api')

@api.route("/")
def index():
	return ""


def _get_records(collection):
	records = []
	for r in collection.find({"enable":1}, {"_id":0}):
		r["insert_date"] = dt2ts(r["insert_date"])
		records.append(r)
	return records

@api.route("/proxy/socks5")
def socks5():
	records = _get_records(mongo.db.socks5)
	return jsonify(records)


@api.route("/proxy/http_tunnel")
def http_tunnel():
	records = _get_records(mongo.db.http_tunnel)
	return jsonify(records)

@api.route("/proxy/http")
def http():
	records = _get_records(mongo.db.http)
	return jsonify(records)