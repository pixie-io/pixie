# Copyright 2018- The Pixie Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

import logging
import ssl
import os
from http.server import HTTPServer, BaseHTTPRequestHandler
from sys import exit, version_info

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s %(levelname)s %(message)s',
    handlers=[logging.StreamHandler()])

major, minor = version_info.major, version_info.minor
logging.info(f"{version_info}")
if major < 3 or minor < 10:
    logging.fatal(f"Python version must be 3.10 or greater for this test assertion. Detected {version_info} instead")
    exit(-1)

pid = os.getpid()
logging.info(f"pid={pid}")

file = open("/usr/share/nginx/html/index.html", "r")
PAYLOAD = file.read()


class MyRequestHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-type", "text/html")
        self.send_header("Content-length", len(PAYLOAD))
        self.end_headers()
        self.wfile.write(bytes(PAYLOAD, 'utf-8'))


httpd = HTTPServer(('localhost', 443), MyRequestHandler)

httpd.socket = ssl.wrap_socket(httpd.socket,
                               keyfile="/etc/ssl/server.key",
                               certfile='/etc/ssl/server.crt', server_side=True)

httpd.serve_forever()
