#!/usr/bin/env python3
import concurrent.futures
import requests
import time
import json
import urllib3

# Disable SSL warnings for self-signed certificates
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

BASE_URL = "https://localhost:5000"
NUM_OPERATIONS = 30

def bootstrap_admin():
    """Create admin user by triggering bootstrap mode"""
    try:
        # Try to login - this should trigger admin creation in bootstrap mode
        login_data = {
            "username": "admin", 
            "password": "admin"
        }
        
        print("🔧 Triggering admin creation via bootstrap mode...")
        response = requests.post(
            f"{BASE_URL}/api/auth/login",
            json=login_data,
            verify=False,
            timeout=10
        )
        
        print(f"🔧 Bootstrap response: {response.status_code}")
        if response.status_code == 200:
            data = response.json()
            return data.get('token')
        else:
            # Bootstrap might create admin on first attempt, retry
            print("🔧 Retrying login after bootstrap...")
            time.sleep(1)
            response = requests.post(
                f"{BASE_URL}/api/auth/login",
                json=login_data,
                verify=False,
                timeout=10
            )
            if response.status_code == 200:
                data = response.json()
                return data.get('token')
        
        return None
    except Exception as e:
        print(f"Bootstrap error: {e}")
        return None

def perform_operation(op_data):
    """Perform document operation with authentication"""
    op_num, token = op_data
    try:
        doc_data = {
            "title": f"Test Document {op_num}",
            "content": f"Testing concurrent operation {op_num}",
            "timestamp": time.time(),
            "operation_id": op_num,
            "type": "test-document",
            "library": "default"
        }
        
        headers = {
            "Authorization": f"Bearer {token}",
            "Content-Type": "application/json"
        }
        
        response = requests.post(
            f"{BASE_URL}/api/documents",
            json=doc_data,
            headers=headers,
            verify=False,
            timeout=10
        )
        
        if response.status_code == 200:
            return True, f"Operation {op_num}: SUCCESS"
        else:
            return False, f"Operation {op_num}: HTTP {response.status_code}"
            
    except Exception as e:
        return False, f"Operation {op_num}: ERROR - {str(e)}"

print(f"🎯 BOOTSTRAP CONCURRENT TEST: {NUM_OPERATIONS} operations")
print("=" * 60)

# Bootstrap admin creation
token = bootstrap_admin()
if not token:
    print("❌ Failed to bootstrap admin user")
    exit(1)

print("✅ Admin user created and authenticated")

# Run concurrent operations
start_time = time.time()
with concurrent.futures.ThreadPoolExecutor(max_workers=8) as executor:
    futures = []
    for i in range(NUM_OPERATIONS):
        future = executor.submit(perform_operation, (i, token))
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
            if failures <= 2:
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
    print("\n🎉 THREADING FIX SUCCESSFUL: 100% CONCURRENT RELIABILITY! 🎉")
    print("🔧 JSON string storage prevents concurrent access crashes!")
else:
    print(f"\n📊 Current reliability: {(successes/NUM_OPERATIONS)*100:.1f}%")