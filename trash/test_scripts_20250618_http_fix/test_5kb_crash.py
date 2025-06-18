#!/usr/bin/env python3
import requests
import json
import urllib3
urllib3.disable_warnings()

BASE_URL = "https://localhost:5000"

# Login
print("Logging in...")
login_resp = requests.post(f"{BASE_URL}/api/auth/login",
                          json={"username": "admin", "password": "secure123456789"},
                          verify=False,
                          timeout=5)
print(f"Login response: {login_resp.status_code}")
if login_resp.status_code != 200:
    print(f"Login failed: {login_resp.text}")
    exit(1)
    
token = login_resp.json()["token"]
headers = {"Authorization": f"Bearer {token}"}
print(f"Got token: {token[:20]}...")

# Create 5KB content (5000 'A' characters)
content = "A" * 5000
data = {
    "title": "Medium Document",
    "content": content
}

print(f"Content length: {len(content)}")
print(f"JSON length: {len(json.dumps(data))}")

# Send request
try:
    print("\nSending 5KB document...")
    resp = requests.post(f"{BASE_URL}/api/documents", 
                        headers=headers,
                        json=data,
                        verify=False,
                        timeout=5)
    print(f"Response: {resp.status_code}")
    print(f"Body: {resp.text}")
except Exception as e:
    print(f"Error: {e}")