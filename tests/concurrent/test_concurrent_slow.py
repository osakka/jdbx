#!/usr/bin/env python3
import concurrent.futures
import requests
import time
import json
import urllib3

# Disable SSL warnings for self-signed certificates
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

BASE_URL = "https://localhost:5000"
NUM_OPERATIONS = 10  # Fewer operations
MAX_WORKERS = 5      # Fewer concurrent workers

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
            "title": f"Concurrent Test Document {op_num}",
            "content": f"Testing concurrent operation number {op_num}",
            "timestamp": time.time(),
            "operation_id": op_num
        }
        
        headers = {
            "Authorization": f"Bearer {token}"
        }
        
        response = requests.post(
            f"{BASE_URL}/api/collections/test-concurrent-slow/documents",
            json=doc_data,
            headers=headers,
            verify=False,
            timeout=10
        )
        
        if response.status_code in [200, 201]:
            return True, f"Operation {op_num}: SUCCESS (HTTP {response.status_code})"
        else:
            return False, f"Operation {op_num}: HTTP {response.status_code} - {response.text[:100]}"
            
    except Exception as e:
        return False, f"Operation {op_num}: ERROR - {str(e)}"

print("🔑 Getting authentication token...")
token = get_auth_token()
if not token:
    print("❌ Failed to authenticate!")
    exit(1)

print(f"✅ Got JWT token: {token[:20]}...")
print(f"\n🎯 SLOW CONCURRENT TEST: {NUM_OPERATIONS} operations with {MAX_WORKERS} workers")
print("=" * 60)

# Run concurrent operations
start_time = time.time()
with concurrent.futures.ThreadPoolExecutor(max_workers=MAX_WORKERS) as executor:
    futures = []
    for i in range(NUM_OPERATIONS):
        future = executor.submit(perform_operation, i, token)
        futures.append(future)
        time.sleep(0.1)  # Small delay between submissions
    
    # Track results
    successes = 0
    failures = 0
    
    for future in concurrent.futures.as_completed(futures):
        success, message = future.result()
        if success:
            successes += 1
            print(f"✅ {message}")
        else:
            failures += 1
            print(f"❌ {message}")

print("=" * 60)
print(f"🏁 FINAL RESULTS: {successes}/{NUM_OPERATIONS} successful ({(successes/NUM_OPERATIONS)*100:.1f}%)")
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