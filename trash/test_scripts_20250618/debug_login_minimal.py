#!/usr/bin/env python3
"""Minimal login test"""

import requests
import urllib3

urllib3.disable_warnings()

# Force HTTP/1.1 and disable keep-alive
s = requests.Session()
s.verify = False
s.headers.update({'Connection': 'close'})  # Force new connection each time

# Test 1: Simple login
print("Testing login...")
try:
    r = s.post('https://localhost:5000/api/auth/login', 
               json={'username': 'admin', 'password': 'secure123456789'})
    print(f"Status: {r.status_code}")
    print(f"Response: {r.text}")
    if r.status_code == 200:
        print("SUCCESS!")
    else:
        print("FAILED!")
        # Let's see the raw request
        print(f"Request URL: {r.request.url}")
        print(f"Request method: {r.request.method}")
        print(f"Request headers: {dict(r.request.headers)}")
        print(f"Request body: {r.request.body}")
        
except Exception as e:
    print(f"Exception: {e}")