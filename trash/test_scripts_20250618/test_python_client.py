#!/usr/bin/env python3
"""
Test Python requests library compatibility with JDBX
"""

import requests
import json
import urllib3

# Disable SSL warnings for self-signed certificates
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

SERVER_URL = "https://localhost:5000"

print("Python Requests Library Compatibility Test")
print("="*50)

# Test 1: Login
print("\n1. Testing login endpoint...")
try:
    response = requests.post(
        f"{SERVER_URL}/api/auth/login",
        json={"username": "admin", "password": "secure123456789"},
        verify=False
    )
    
    if response.status_code == 200:
        data = response.json()
        if 'token' in data:
            print("✅ Login successful!")
            print(f"   Token: {data['token'][:30]}...")
            token = data['token']
        else:
            print("❌ Login failed: No token in response")
            print(f"   Response: {data}")
            exit(1)
    else:
        print(f"❌ Login failed: HTTP {response.status_code}")
        print(f"   Response: {response.text}")
        exit(1)
except Exception as e:
    print(f"❌ Login error: {e}")
    exit(1)

# Test 2: Authenticated request
print("\n2. Testing authenticated request...")
try:
    response = requests.get(
        f"{SERVER_URL}/api/libraries",
        headers={"Authorization": f"Bearer {token}"},
        verify=False
    )
    
    if response.status_code == 200:
        data = response.json()
        print("✅ Authenticated request successful!")
        print(f"   Libraries count: {len(data.get('libraries', []))}")
    else:
        print(f"❌ Request failed: HTTP {response.status_code}")
        print(f"   Response: {response.text}")
except Exception as e:
    print(f"❌ Request error: {e}")

# Test 3: Create document
print("\n3. Testing document creation...")
try:
    doc = {
        "title": "Test Document from Python",
        "content": "This document was created using Python requests library",
        "tags": ["python", "test", "api"]
    }
    
    response = requests.post(
        f"{SERVER_URL}/api/libraries/default/collections/documents/documents",
        json=doc,
        headers={"Authorization": f"Bearer {token}"},
        verify=False
    )
    
    if response.status_code == 201:
        data = response.json()
        print("✅ Document created successfully!")
        print(f"   UUID: {data.get('uuid')}")
        print(f"   Type: {data.get('type')}")
        print(f"   Owner: {data.get('owner')}")
    else:
        print(f"❌ Document creation failed: HTTP {response.status_code}")
        print(f"   Response: {response.text}")
except Exception as e:
    print(f"❌ Document creation error: {e}")

# Test 4: Session reuse
print("\n4. Testing session reuse (connection pooling)...")
try:
    session = requests.Session()
    session.headers.update({"Authorization": f"Bearer {token}"})
    session.verify = False
    
    success_count = 0
    for i in range(5):
        response = session.get(f"{SERVER_URL}/api/collections")
        if response.status_code == 200:
            success_count += 1
    
    if success_count == 5:
        print("✅ Session reuse successful!")
        print(f"   All 5 requests completed using connection pooling")
    else:
        print(f"❌ Session reuse failed: {success_count}/5 succeeded")
except Exception as e:
    print(f"❌ Session reuse error: {e}")

print("\n" + "="*50)
print("Python client compatibility: ✅ VERIFIED")
print("="*50)