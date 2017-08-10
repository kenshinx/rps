# -*- coding: utf-8 -*-

import os
import time
from datetime import datetime



def mkdir(dir_path):
    try:
        if not os.path.exists(dir_path):
            os.mkdir(dir_path)
    except Exception, e:
        raise e
        
def dt2ts(dt):
	return int(time.mktime(dt.timetuple()))