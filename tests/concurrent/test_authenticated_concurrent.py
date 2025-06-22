#!/usr/bin/env python3
import concurrent.futures
import requests
import time
import json
import urllib3

# Disable SSL warnings for self-signed certificates
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

BASE_URL = "https://localhost:5000"
NUM_OPERATIONS = 50  # Start with 50 to test authentication

def get_auth_token():
    """Get authentication token"""
    try:
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
            data = response.json()
            return data.get('token')
        else:
            print(f"Login failed: {response.status_code} - {response.text}")
            return None
    except Exception as e:
        print(f"Login error: {e}")
        return None

def perform_operation(op_data):
    """Perform authenticated operation"""
    op_num, token = op_data
    try:
        # Create a unique document
        doc_data = {
            "title": f"Concurrent Test Document {op_num}",
            "content": f"Testing concurrent operation number {op_num} with authentication",
            "timestamp": time.time(),
            "operation_id": op_num
        }
        
        headers = {
            "Authorization": f"Bearer {token}",
            "Content-Type": "application/json"
        }
        
        response = requests.post(
            f"{BASE_URL}/api/collections/test-concurrent/documents",
            json=doc_data,
            headers=headers,
            verify=False,
            timeout=10
        )
        
        if response.status_code == 200:
            return True, f"Operation {op_num}: SUCCESS"
        else:
            return False, f"Operation {op_num}: HTTP {response.status_code} - {response.text[:100]}"
            
    except Exception as e:
        return False, f"Operation {op_num}: ERROR - {str(e)}"

print(f"🎯 AUTHENTICATED CONCURRENT TEST: {NUM_OPERATIONS} operations")
print("=" * 60)

# Get authentication token
print("🔐 Getting authentication token...")
auth_token = get_auth_token()
if not auth_token:
    print("❌ Failed to get authentication token")
    exit(1)

print("✅ Authentication successful")

# Run concurrent operations
start_time = time.time()
with concurrent.futures.ThreadPoolExecutor(max_workers=10) as executor:
    futures = []
    for i in range(NUM_OPERATIONS):
        future = executor.submit(perform_operation, (i, auth_token))
        futures.append(future)
    
    # Track results
    successes = 0
    failures = 0
    
    for future in concurrent.futures.as_completed(futures):
        success, message = future.result()
        if success:
            successes += 1
            print("✅", end="", flush=True)
        else:
            failures += 1
            print("❌", end="", flush=True)
            # Print first few failure messages
            if failures <= 3:
                print(f"\n  DEBUG: {message}")
            
        # Print progress every 10 operations
        total = successes + failures
        if total % 10 == 0:
            print(f" ({total}/{NUM_OPERATIONS})", end="", flush=True)

print()
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

if successes == NUM_OPERATIONS:
    print("\n🎉 ACHIEVEMENT UNLOCKED: 100% CONCURRENT OPERATION RELIABILITY! 🎉")
else:
    print(f"\n📊 Current reliability: {(successes/NUM_OPERATIONS)*100:.1f}%")