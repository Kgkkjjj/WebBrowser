#!/usr/bin/env python3
"""Combined backend using Flask and FastAPI for OpenB."""
import os
import json
import logging
from logging.handlers import RotatingFileHandler
import subprocess
import sys
import importlib.util

REQUIRED = ["requests", "flask", "fastapi", "uvicorn"]


def ensure_deps() -> None:
    missing = [p for p in REQUIRED if importlib.util.find_spec(p) is None]
    if missing:
        try:
            subprocess.check_call([sys.executable, "-m", "pip", "install", "--quiet", "--user", *missing])
        except Exception as exc:  # pragma: no cover - best effort install
            print(f"Failed to install packages: {exc}")


ensure_deps()

import requests
from flask import Flask, send_from_directory
from fastapi import FastAPI, Response
from fastapi.middleware.wsgi import WSGIMiddleware
import uvicorn

PORT = int(os.environ.get('OPENB_PORT', '8080'))
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DATA_DIR = os.path.join(BASE_DIR, 'data')
LOG_DIR = os.path.join(os.path.expanduser('~'), '.cache', 'openb')
os.makedirs(LOG_DIR, exist_ok=True)
LOG_FILE = os.path.join(LOG_DIR, 'server.log')

logger = logging.getLogger('openb.server')
handler = RotatingFileHandler(LOG_FILE, maxBytes=1024*1024, backupCount=2)
handler.setFormatter(logging.Formatter('%(asctime)s %(levelname)s %(message)s'))
logger.addHandler(handler)
logger.setLevel(logging.INFO)

flask_app = Flask(__name__)

@flask_app.route('/')
def home():
    return send_from_directory(DATA_DIR, 'home.html')

@flask_app.route('/<path:fname>')
def pages(fname):
    return send_from_directory(DATA_DIR, fname)

api = FastAPI()

tab_count = 1

@api.get('/search')
def api_search(q: str = ''):
    api_url = 'https://api.duckduckgo.com/?q=%s&format=json&no_redirect=1&no_html=1' % requests.utils.quote(q)
    try:
        resp = requests.get(api_url, timeout=5)
        resp.raise_for_status()
        return Response(resp.content, media_type='application/json')
    except Exception as e:
        logger.error('search request failed: %s', e)
        return Response(json.dumps({'error': 'search-failed'}), status_code=502, media_type='application/json')

@api.get('/status')
def api_status():
    return {'tabs': tab_count, 'status': 'ok'}

@api.get('/update_tabs')
def api_update_tabs(count: int = 1):
    global tab_count
    try:
        tab_count = int(count)
    except ValueError:
        tab_count = 1
    return {'ok': True}

@api.get('/log')
def api_log():
    try:
        with open(LOG_FILE, 'r') as f:
            data = f.read()
    except FileNotFoundError:
        data = ''
    return Response(data, media_type='text/plain')

app = FastAPI()
app.mount('/', WSGIMiddleware(flask_app))
for route in api.router.routes:
    app.router.routes.append(route)

if __name__ == '__main__':
    try:
        logger.info('Serving at http://localhost:%d/', PORT)
        uvicorn.run(app, host='0.0.0.0', port=PORT, log_level='warning')
    except Exception as e:
        logger.error('Server error: %s', e)
        print(f'Server error: {e}')
        raise SystemExit(1)
