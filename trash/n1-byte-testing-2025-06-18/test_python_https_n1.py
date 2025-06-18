#!/usr/bin/env python3
"""Test N-1 byte fix with Python requests over HTTPS"""

import requests
import json
import urllib3

# Disable SSL warnings for self-signed certificate
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

print("🧪 Testing N-1 Byte Fix with Python requests over HTTPS")
print("=" * 50)

# Configuration
base_url = "https://localhost:5000"

# Login
print("\n🔐 Logging in...")
try:
    resp = requests.post(f"{base_url}/api/auth/login", 
                        json={"username": "admin", "password": "secure123456789"},
                        verify=False)
    if resp.status_code != 200:
        print(f"❌ Login failed: {resp.status_code}")
        exit(1)
    token = resp.json()["token"]
    print("✅ Login successful")
except Exception as e:
    print(f"❌ Login error: {e}")
    exit(1)

# Test various sizes
print("\n📊 Testing various document sizes:")
test_sizes = [100, 1000, 5000, 10000, 50000, 100000]

session = requests.Session()
session.headers['Authorization'] = f'Bearer {token}'
session.verify = False

passed = 0
failed = 0

for size in test_sizes:
    # Create document with exact size
    data = 'x' * (size - 100)  # Account for JSON overhead
    doc = {
        "title": f"Test {size} bytes",
        "type": "https-test",
        "data": data
    }
    
    json_str = json.dumps(doc)
    actual_size = len(json_str)
    
    try:
        resp = session.post(f"{base_url}/api/documents", json=doc)
        if resp.status_code == 201:
            print(f"  ✅ {size:,} bytes (actual: {actual_size:,}): Success")
            passed += 1
        else:
            print(f"  ❌ {size:,} bytes (actual: {actual_size:,}): HTTP {resp.status_code}")
            failed += 1
    except Exception as e:
        print(f"  ❌ {size:,} bytes (actual: {actual_size:,}): {type(e).__name__}: {str(e)[:50]}")
        failed += 1

# Summary
print("\n" + "=" * 50)
print("📊 Test Summary:")
print(f"  ✅ Passed: {passed}")
print(f"  ❌ Failed: {failed}")
print(f"  📈 Success Rate: {passed/(passed+failed)*100:.1f}%")

if failed == 0:
    print("\n🎉 All tests passed! N-1 byte issue is fixed for HTTPS!")
else:
    print("\n⚠️  Some tests failed.")