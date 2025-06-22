#!/usr/bin/env python3
import concurrent.futures
import requests
import time
import json
import urllib3

# Disable SSL warnings for self-signed certificates
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

BASE_URL = "https://localhost:5000"
NUM_OPERATIONS = 50

def perform_operation(op_num):
    """Perform direct document creation (allowed in bootstrap mode)"""
    try:
        # Create a unique document
        doc_data = {
            "title": f"Test Document {op_num}",
            "content": f"Testing document storage operation {op_num}",
            "timestamp": time.time(),
            "operation_id": op_num,
            "type": "test-document",
            "library": "default",
            "collection": "test-docs"
        }
        
        response = requests.post(
            f"{BASE_URL}/api/documents",
            json=doc_data,
            verify=False,
            timeout=10
        )
        
        if response.status_code == 200:
            return True, f"Operation {op_num}: SUCCESS"
        else:
            return False, f"Operation {op_num}: HTTP {response.status_code} - {response.text[:100]}"
            
    except Exception as e:
        return False, f"Operation {op_num}: ERROR - {str(e)}"

print(f"🎯 DOCUMENT STORAGE CONCURRENT TEST: {NUM_OPERATIONS} operations")
print("=" * 60)

# Run concurrent operations
start_time = time.time()
with concurrent.futures.ThreadPoolExecutor(max_workers=10) as executor:
    futures = []
    for i in range(NUM_OPERATIONS):
        future = executor.submit(perform_operation, i)
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
            # Print first few failure messages for debugging
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
    print("🔧 JSON threading fix successful - no more concurrent access crashes!")
else:
    print(f"\n📊 Current reliability: {(successes/NUM_OPERATIONS)*100:.1f}%")