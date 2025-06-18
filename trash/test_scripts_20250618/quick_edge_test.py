#!/usr/bin/env python3
"""
Quick edge case test - find what's hanging
"""

import requests
import json
import urllib3
import time

urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

BASE_URL = "https://localhost:5000"
session = requests.Session()
session.verify = False

# Login
print("Logging in...")
response = session.post(
    f"{BASE_URL}/api/auth/login",
    json={"username": "admin", "password": "secure123456789"}
)
if response.status_code == 200:
    token = response.json()['token']
    session.headers.update({'Authorization': f'Bearer {token}'})
    print("✅ Login successful")
else:
    print("❌ Login failed")
    exit(1)

# Test 1: Empty document
print("\n1. Testing empty document...")
response = session.post(
    f"{BASE_URL}/api/libraries/default/collections/test/documents",
    json={}
)
print(f"Result: {response.status_code} - {response.text[:100]}")

# Test 2: Special characters
print("\n2. Testing special characters...")
doc = {
    "title": "Test \"quotes\" and 'quotes'",
    "emoji": "🚀🔥💥",
    "unicode": "你好世界"
}
response = session.post(
    f"{BASE_URL}/api/libraries/default/collections/test/documents",
    json=doc
)
print(f"Result: {response.status_code}")

if response.status_code == 201:
    doc_id = response.json()['uuid']
    # Read it back
    response = session.get(f"{BASE_URL}/api/documents/{doc_id}")
    if response.status_code == 200:
        retrieved = response.json()
        print(f"Retrieved emoji: {retrieved.get('emoji')}")
        print(f"Retrieved unicode: {retrieved.get('unicode')}")

# Test 3: Large array (but not too large)
print("\n3. Testing array with 100 items...")
doc = {
    "title": "Array Test",
    "items": [f"item_{i}" for i in range(100)]
}
response = session.post(
    f"{BASE_URL}/api/libraries/default/collections/test/documents",
    json=doc
)
print(f"Result: {response.status_code}")

# Test 4: Invalid UUID operations
print("\n4. Testing invalid UUID operations...")
response = session.get(f"{BASE_URL}/api/documents/invalid-uuid-12345")
print(f"Read invalid UUID: {response.status_code}")

response = session.delete(f"{BASE_URL}/api/documents/invalid-uuid-12345")
print(f"Delete invalid UUID: {response.status_code}")

print("\n✅ Quick tests completed!")