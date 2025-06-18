#!/usr/bin/env python3
"""
Test exact buffer boundaries
"""

import requests
import json

# Test without SSL
BASE_URL = "http://localhost:5000"

print("🧪 Testing Exact Buffer Boundaries\n")

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

# Calculate the exact size to fill the buffer
# Headers are approximately 130-140 bytes for the request
# Let's test sizes that would result in total request sizes around 4096
header_size = 140  # Approximate

for offset in range(-10, 11):
    target_total = 4096 + offset
    body_size = target_total - header_size
    
    # Create a document that will result in the target body size
    # Account for JSON overhead: {"title":"...", "type":"...", "data":"..."}
    json_overhead = len('{"title":"Test","type":"buffer-test","data":""}')
    data_size = body_size - json_overhead
    
    if data_size < 0:
        continue
        
    doc = {
        "title": "Test",
        "type": "buffer-test", 
        "data": "x" * data_size
    }
    
    # Calculate actual sizes
    json_str = json.dumps(doc)
    actual_body_size = len(json_str)
    total_size = header_size + actual_body_size
    
    try:
        resp = s.post(f"{BASE_URL}/api/documents", json=doc)
        
        if resp.status_code == 201:
            print(f"✅ Total: {total_size:4d} (body: {actual_body_size:4d}): Success")
        else:
            print(f"❌ Total: {total_size:4d} (body: {actual_body_size:4d}): HTTP {resp.status_code}")
    except Exception as e:
        print(f"❌ Total: {total_size:4d} (body: {actual_body_size:4d}): {type(e).__name__}")

print("\n📊 Analysis: Server fails when total request size >= 4096 bytes")