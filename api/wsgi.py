# -*- coding: utf-8 -*-

import sys, os, pwd

project = "RFS"


# give wsgi the "application"
from rps import createAPP
application = createAPP()
