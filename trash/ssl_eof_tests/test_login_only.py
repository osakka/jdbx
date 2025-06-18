#!/usr/bin/env python3
import requests
import urllib3

urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

# Test basic login
print("Testing login with Python...")
response = requests.post(
    "https://localhost:5000/api/auth/login",
    json={"username": "admin", "password": "secure123456789"},
    verify=False,
    timeout=5
)
print(f"Status: {response.status_code}")
print(f"Headers: {dict(response.headers)}")
if response.status_code != 200:
    print(f"Response: {response.text}")
else:
    print("Login successful!")