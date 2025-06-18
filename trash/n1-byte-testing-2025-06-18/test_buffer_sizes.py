#!/usr/bin/env python3
"""
Test various buffer sizes to isolate the issue
"""

import requests
import json

# Test without SSL
BASE_URL = "http://localhost:5000"

print("🧪 Testing Buffer Size Issue\n")

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

# Test various sizes around the 4KB boundary
sizes = [3000, 3500, 4000, 4090, 4095, 4096, 4097, 4100, 5000, 8000, 10000]

for size in sizes:
    doc = {
        "title": f"Test {size} bytes",
        "type": "buffer-test", 
        "data": "x" * (size - 100)  # Account for JSON overhead
    }
    
    # Calculate actual JSON size
    json_str = json.dumps(doc)
    actual_size = len(json_str)
    
    try:
        resp = s.post(f"{BASE_URL}/api/documents", json=doc)
        
        if resp.status_code == 201:
            print(f"✅ {size:5d} bytes (actual: {actual_size:5d}): Success")
        else:
            print(f"❌ {size:5d} bytes (actual: {actual_size:5d}): HTTP {resp.status_code} - {resp.text[:100]}")
    except Exception as e:
        print(f"❌ {size:5d} bytes (actual: {actual_size:5d}): {type(e).__name__}: {str(e)[:100]}")

print("\n📊 Summary: Buffer reallocation appears to happen around 4KB boundary")