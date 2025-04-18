#!/bin/bash

openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -sha256 -days 3650 -nodes -subj "/C=CA/ST=Ontario/L=Toronto/O=Localhosty.com/OU=Localhosty.com/CN=www.localhosty.com"
