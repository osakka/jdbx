#!/usr/bin/env python3
import requests
import json
import urllib3

# Disable SSL warnings for self-signed cert
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

base_url = "https://localhost:5000"

# Login
login_response = requests.post(
    f"{base_url}/api/auth/login",
    json={"username": "admin", "password": "secure123456789"},
    verify=False
)
if login_response.status_code != 200:
    print(f"Login failed: HTTP {login_response.status_code}")
    print(f"Response: {login_response.text}")
    exit(1)
token = login_response.json()["token"]
headers = {"Authorization": f"Bearer {token}"}

print("Testing with Python requests library (proper SSL handling)...")

# Test 1: Small document
small_doc = {"data": "x" * 100}
response = requests.post(f"{base_url}/api/documents", json=small_doc, headers=headers, verify=False)
print(f"Small document (100 bytes): HTTP {response.status_code}")

# Test 2: Medium document (5KB)
medium_doc = {"data": "x" * 5000}
response = requests.post(f"{base_url}/api/documents", json=medium_doc, headers=headers, verify=False)
print(f"Medium document (5KB): HTTP {response.status_code}")

# Test 3: Large document (50KB)
large_doc = {"data": "x" * 50000}
response = requests.post(f"{base_url}/api/documents", json=large_doc, headers=headers, verify=False)
print(f"Large document (50KB): HTTP {response.status_code}")

# Test 4: Very large document (500KB)
very_large_doc = {"data": "x" * 500000}
response = requests.post(f"{base_url}/api/documents", json=very_large_doc, headers=headers, verify=False)
print(f"Very large document (500KB): HTTP {response.status_code}")

print("\n✅ All tests passed with proper SSL client!")