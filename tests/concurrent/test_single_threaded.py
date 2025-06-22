#!/usr/bin/env python3
import requests
import time
import urllib3

# Disable SSL warnings for self-signed certificates
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

BASE_URL = "https://localhost:5000"

# First, login to get JWT token
def get_auth_token():
    login_data = {
        "username": "admin",
        "password": "admin"
    }
    
    response = requests.post(
        f"{BASE_URL}/api/auth/login",
        json=login_data,
        verify=False,
        timeout=10
    )
    
    if response.status_code == 200:
        return response.json().get("token")
    else:
        print(f"Login failed: HTTP {response.status_code}")
        return None

def perform_operation(op_num, token):
    try:
        # Create a unique document
        doc_data = {
            "title": f"Sequential Test Document {op_num}",
            "content": f"Testing sequential operation number {op_num}",
            "timestamp": time.time(),
            "operation_id": op_num
        }
        
        headers = {
            "Authorization": f"Bearer {token}"
        }
        
        response = requests.post(
            f"{BASE_URL}/api/collections/test-sequential/documents",
            json=doc_data,
            headers=headers,
            verify=False,
            timeout=10
        )
        
        if response.status_code in [200, 201]:
            return True, f"Operation {op_num}: SUCCESS (HTTP {response.status_code})"
        else:
            return False, f"Operation {op_num}: HTTP {response.status_code}"
            
    except Exception as e:
        return False, f"Operation {op_num}: ERROR - {str(e)}"

print("🔑 Getting authentication token...")
token = get_auth_token()
if not token:
    print("❌ Failed to authenticate!")
    exit(1)

print(f"✅ Got JWT token: {token[:20]}...")
print(f"\n🎯 SEQUENTIAL TEST: 20 operations one at a time")
print("=" * 60)

# Run sequential operations
start_time = time.time()
successes = 0
failures = 0

for i in range(20):
    success, message = perform_operation(i, token)
    if success:
        successes += 1
        print("✅", end="", flush=True)
    else:
        failures += 1
        print("❌", end="", flush=True)
        print(f"\n{message}")
    
    if (i + 1) % 10 == 0:
        print(f" ({i+1}/20)", end="", flush=True)

print()
print("=" * 60)
print(f"🏁 FINAL RESULTS: {successes}/20 successful ({(successes/20)*100:.1f}%)")
print(f"⏱️  Time taken: {time.time() - start_time:.2f} seconds")

# Check if server is still running
try:
    response = requests.get(f"{BASE_URL}/api/health", verify=False, timeout=5)
    if response.status_code == 200:
        print("✅ Server is still running and healthy!")
    else:
        print(f"⚠️  Server returned status {response.status_code}")
except Exception as e:
    print("❌ Server appears to have crashed:", str(e))