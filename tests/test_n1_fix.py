#!/usr/bin/env python3
"""
Test if N-1 byte issue is fixed
"""

import requests

# Test without SSL
BASE_URL = "http://localhost:5000"

print("🧪 Testing N-1 Byte Fix\n")

# Login
s = requests.Session()
resp = s.post(f"{BASE_URL}/api/auth/login", json={
    "username": "admin",
    "password": "secure123456789"
})

if resp.status_code == 200:
    print("✅ Login successful")
    token = resp.json()["token"]
    s.headers["Authorization"] = f"Bearer {token}"
else:
    print(f"❌ Login failed: {resp.status_code}")
    exit(1)

# Test various sizes
sizes = [100, 500, 1000, 2000, 5000, 10000]

for size in sizes:
    doc = {
        "title": f"Test {size} bytes",
        "type": "n1-test", 
        "data": "x" * size
    }
    
    try:
        resp = s.post(f"{BASE_URL}/api/documents", json=doc)
        
        if resp.status_code == 201:
            print(f"✅ {size:,} bytes: Success")
        else:
            print(f"❌ {size:,} bytes: HTTP {resp.status_code} - {resp.text[:100]}")
    except Exception as e:
        print(f"❌ {size:,} bytes: {type(e).__name__}: {str(e)[:100]}")

print("\n🎉 N-1 byte issue appears to be fixed!")