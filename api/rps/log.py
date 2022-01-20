# -*- coding: utf-8 -*-

import os
import sys
import logging
import logging.handlers

class LogConfig:
    
    def __init__(self, level, format, datefmt, path=None, stdout = False, syslog = ()):

        logging.root.setLevel(level)

        if path is not None:
            self.setFileHandler(path, format, datefmt)

        if stdout:
            self.setStreamHandler(format, datefmt)

        if syslog:
            self.setSYSLogHandler(format, datefmt, syslog)


    def setFileHandler(self, path, fmt, datefmt):
        file = logging.handlers.TimedRotatingFileHandler(path, interval=24, backupCount=30)
        formatter = logging.Formatter(fmt=fmt, datefmt=datefmt)
        file.setFormatter(formatter)
        logging.getLogger('').addHandler(file)

    def setStreamHandler(self, fmt, datefmt):
        formatter = logging.Formatter(fmt=fmt, datefmt=datefmt)
        stream = logging.StreamHandler()
        stream.setFormatter(formatter)
        logging.getLogger('').addHandler(stream)

    def setSYSLogHandler(self, fmt, datefmt, address):
        syslog = logging.handlers.SysLogHandler(address=address)
        formatter = logging.Formatter(fmt=fmt, datefmt=datefmt)
        syslog.setFormatter(formatter)
        logging.getLogger('').addHandler(syslog)
