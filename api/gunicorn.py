# Gunicorn configuration file.

bind = '0.0.0.0:9897'

daemon = False

workers = 5
worker_class = 'sync'
worker_connections = 1000
threads = 10

#restart after reach max_requests. bypass the memory continuous climbing up issues.
max_requests = 5000

timeout = 300

# raw_env = 'RPS_WEB_DEPLOY_MODE=PROD'

proc_name = 'rps'

limit_request_line = 0

accesslog = 'logs/rps.access.log'

access_log_format = '%({X-Real-IP}i)s %(l)s %(u)s %(t)s "%(r)s" %(s)s %(b)s "%(f)s" "%(a)s" %(L)ss'

errorlog = 'logs/rps.error.log'

