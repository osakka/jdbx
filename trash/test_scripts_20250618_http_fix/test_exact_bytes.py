#!/usr/bin/env python3
import requests
import json
import urllib3
urllib3.disable_warnings()

# Test exact byte sending
BASE_URL = "https://localhost:5000"

# Login first
login_resp = requests.post(f"{BASE_URL}/api/auth/login",
                          json={"username": "admin", "password": "secure123456789"},
                          verify=False)
token = login_resp.json()["token"]
headers = {"Authorization": f"Bearer {token}"}

# Test 1: Medium document (5KB)
print("Testing 5KB document...")
data = {"key": "x" * 5000}
json_str = json.dumps(data)
print(f"JSON length: {len(json_str)} bytes")

try:
    resp = requests.post(f"{BASE_URL}/api/documents", 
                        headers=headers,
                        json=data,
                        verify=False,
                        timeout=5)
    print(f"Response: {resp.status_code}")
    print(f"Body: {resp.text}")
except Exception as e:
    print(f"Error: {e}")

# Test 2: Check what curl is actually sending
print("\nChecking Content-Length header...")
import subprocess
result = subprocess.run([
    "curl", "-s", "-k", "-X", "POST",
    f"{BASE_URL}/api/documents",
    "-H", f"Authorization: Bearer {token}",
    "-H", "Content-Type: application/json",
    "-d", json_str,
    "-w", "\\nContent-Length: %{size_request}\\nHTTP: %{http_code}\\n",
    "-v"
], capture_output=True, text=True)
print("STDERR:", result.stderr)
print("STDOUT:", result.stdout)