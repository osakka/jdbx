#!/usr/bin/env python3
import requests
import urllib3
urllib3.disable_warnings()

BASE_URL = "https://localhost:5000"

# Test 1: Login
print("1. Testing login...")
s = requests.Session()
s.verify = False

resp = s.post(f"{BASE_URL}/api/auth/login", json={
    "username": "admin",
    "password": "secure123456789"
})
print(f"   Status: {resp.status_code}")
if resp.status_code == 200:
    token = resp.json()["token"]
    s.headers["Authorization"] = f"Bearer {token}"
    print(f"   ✅ Got token")
else:
    print(f"   ❌ Failed: {resp.text}")
    exit(1)

# Test 2: Create small document
print("\n2. Creating small document...")
resp = s.post(f"{BASE_URL}/api/documents", json={
    "title": "Small test",
    "type": "test"
})
print(f"   Status: {resp.status_code}")
print(f"   Response: {resp.text[:200]}")

# Test 3: Create medium document (1KB)
print("\n3. Creating 1KB document...")
resp = s.post(f"{BASE_URL}/api/documents", json={
    "title": "Medium test",
    "type": "test",
    "data": "x" * 1000
})
print(f"   Status: {resp.status_code}")
print(f"   Response: {resp.text[:200]}")

# Test 4: Query documents
print("\n4. Querying documents...")
resp = s.post(f"{BASE_URL}/api/documents/query", json={
    "type": "test"
})
print(f"   Status: {resp.status_code}")
print(f"   Response: {resp.text[:500]}")