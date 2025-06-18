#!/usr/bin/env python3
"""Capture detailed Python request trace"""

import requests
import urllib3
import json
import logging

# Enable detailed logging
logging.basicConfig(level=logging.DEBUG)
urllib3.disable_warnings()

# Create session with detailed logging
s = requests.Session()
s.verify = False

# Test request
print("Making Python request...")
try:
    response = s.post('https://localhost:5000/api/auth/login',
                     json={'username': 'admin', 'password': 'secure123456789'},
                     timeout=10)
    
    print(f"Status: {response.status_code}")
    print(f"Response: {response.text}")
    print(f"Request headers: {dict(response.request.headers)}")
    print(f"Request body: {response.request.body}")
    
except Exception as e:
    print(f"Error: {e}")