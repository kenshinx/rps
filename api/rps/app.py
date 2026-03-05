# -*- coding: utf-8 -*-

import os
from flask import Flask, request, jsonify

from .api import api
from .log import LogConfig
from .config import ProductionConfig, DevelopmentConfig
from .extensions import mongo


DEPLOY_MODE_KEY = "RPS_WEB_DEPLOY_MODE"

DEFAULT_BLUEPRINTS = (
    api,
)


def createAPP(app_name=None, mode=None, blueprints=None):
    if mode is None:
        mode = os.getenv(DEPLOY_MODE_KEY, 'DEV')
    if mode == "DEV":
        config = DevelopmentConfig
    elif mode == "PROD":
        config = ProductionConfig
    else:
        raise Exception("Invalid Deploy Mode")

    if app_name is None:
        app_name = config.PROJECT

    if blueprints is None:
        blueprints = DEFAULT_BLUEPRINTS

    app = Flask(app_name)

    configAPP(app, config)
    configLOG(app)
    configBlueprints(app, blueprints)
    configDB(app)

    return app


def configAPP(app, config):
	app.config.from_object(config)

def configLOG(app):
    conf = app.config
    LogConfig(conf['LOG_LEVEL'], conf['LOG_FMT'], conf['LOG_DATE_FMT'],
              conf.get('LOG_PATH', None), conf['LOG_STDOUT'], conf.get('LOG_SYSLOG', None))

def configBlueprints(app, blueprints):
    for blueprint in blueprints:
        app.register_blueprint(blueprint)

    api_key = os.environ.get("RPS_API_KEY", "")
    if api_key:
        @app.before_request
        def check_api_key():
            if request.path == "/":
                return None
            if request.headers.get("X-RPS-API-Key") != api_key:
                return jsonify(status="UNAUTHORIZED", error="Invalid or missing API key"), 401

def configDB(app):
    mongo.init_app(app)




