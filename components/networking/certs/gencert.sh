#!/bin/bash
 
openssl req -newkey rsa:2048 -nodes -keyout privkey.pem -x509 -days 3650 -out cert.pem -subj "/CN=Arctic Tracker"

