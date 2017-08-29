# -*- coding: utf-8 -*-

import os
import datetime
import logging
import sys

from .utils import mkdir


class BaseConfig(object):
    PROJECT = "RPS_WEB"



    SECRET_KEY = 'oiAd}gRN9EsH47UqYQTiZNYDy74vbJ2P'

    # log
    LOG_FMT = '[%(asctime)s <%(name)s>] %(levelname)s: %(message)s'
    LOG_DATE_FMT = "%Y-%m-%d %H:%M:%S"

    PROJECT_ROOT = os.path.abspath(os.path.dirname(os.path.dirname(__file__)))

    # Log - File Handler
    LOG_FOLDER = os.path.join(PROJECT_ROOT, 'logs')
    mkdir(LOG_FOLDER)

    LOG_FILE = "rps_api.log"
    LOG_PATH = os.path.join(LOG_FOLDER, LOG_FILE)

    MONGO_HOST     = "dev"
    MONGO_USERNAME = "mongo"
    MONGO_PASSWORD = "secret"
    MONGO_DBNAME = "rps_test"
    MONGO_AUTH_SOURCE = "admin"


class ProductionConfig(BaseConfig):

    DEBUG = False

    LOG_LEVEL = logging.INFO
    LOG_STDOUT = False

    
    MONGO_HOST     = "dev1"
    MONGO_USERNAME = "mongo"
    MONGO_PASSWORD = "secret"
    MONGO_DBNAME = "rps"

class DevelopmentConfig(BaseConfig):

    DEBUG = True

    LOG_LEVEL = logging.DEBUG
    LOG_STDOUT = True







