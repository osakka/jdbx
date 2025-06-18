#!/usr/bin/env python3
"""
Test large payload handling with buffer overflow fix
"""

import requests
import json
import urllib3

# Disable SSL warnings
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

SERVER_URL = "https://localhost:5000"

print("Large Payload Testing")
print("="*50)

# First login
print("\n1. Login...")
response = requests.post(
    f"{SERVER_URL}/api/auth/login",
    json={"username": "admin", "password": "secure123456789"},
    verify=False
)

if response.status_code == 200:
    token = response.json()['token']
    print("✅ Login successful")
else:
    print(f"❌ Login failed: {response.status_code}")
    exit(1)

headers = {"Authorization": f"Bearer {token}"}

# Test different payload sizes
test_sizes = [
    (1, "1KB"),
    (3, "3KB"), 
    (4, "4KB (near limit)"),
    (5, "5KB (over limit)"),
    (10, "10KB"),
    (50, "50KB")
]

for size_kb, label in test_sizes:
    print(f"\n2. Testing {label} payload...")
    
    # Create payload of specified size
    content = "x" * (size_kb * 1024)
    doc = {
        "title": f"Large Document {label}",
        "content": content,
        "size_test": label
    }
    
    try:
        response = requests.post(
            f"{SERVER_URL}/api/libraries/default/collections/documents/documents",
            json=doc,
            headers=headers,
            verify=False,
            timeout=10
        )
        
        if response.status_code == 201:
            print(f"✅ {label}: Created successfully")
        elif response.status_code == 413:
            print(f"⚠️  {label}: Request too large (413) - Expected for large payloads")
        else:
            print(f"❌ {label}: Failed with status {response.status_code}")
            print(f"   Response: {response.text[:100]}")
    except Exception as e:
        print(f"❌ {label}: Error - {str(e)[:100]}")

# Check server status
print("\n3. Checking server health...")
try:
    response = requests.get(f"{SERVER_URL}/api/health", verify=False, timeout=5)
    if response.status_code == 200:
        print("✅ Server still healthy after large payload tests")
    else:
        print(f"❌ Server unhealthy: {response.status_code}")
except Exception as e:
    print(f"❌ Server not responding: {e}")

print("\n" + "="*50)
print("Large payload testing complete")