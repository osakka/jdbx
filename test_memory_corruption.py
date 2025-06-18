#!/usr/bin/env python3
"""
Test to reproduce memory corruption during concurrent operations
"""

import requests
import json
import urllib3
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed

# Disable SSL warnings
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

SERVER_URL = "https://localhost:5000"
ADMIN_USER = "admin"
ADMIN_PASS = "secure123456789"

def stress_test_login(thread_id, iterations=5):
    """Repeatedly login to stress test memory handling"""
    results = []
    
    for i in range(iterations):
        try:
            response = requests.post(
                f"{SERVER_URL}/api/auth/login",
                json={"username": ADMIN_USER, "password": ADMIN_PASS},
                verify=False,
                timeout=5
            )
            
            if response.status_code == 200:
                results.append((thread_id, i, "SUCCESS", response.json().get('token', 'NO_TOKEN')[:20]))
            else:
                results.append((thread_id, i, "FAIL", f"HTTP {response.status_code}"))
        except Exception as e:
            results.append((thread_id, i, "ERROR", str(e)[:50]))
            break
    
    return results

def main():
    print("Memory Corruption Stress Test")
    print("="*60)
    
    # First, start the server fresh
    print("Starting server...")
    import subprocess
    subprocess.run(['./build/jdbx_runtime.sh', 'stop'], capture_output=True)
    time.sleep(1)
    
    env = {
        'JDBX_BOOTSTRAP_ADMIN_USER': 'admin',
        'JDBX_BOOTSTRAP_ADMIN_PASS': 'secure123456789'
    }
    result = subprocess.run(['./build/jdbx_runtime.sh', 'start'], 
                          capture_output=True, text=True, env={**os.environ, **env})
    print("Server start result:", result.stdout)
    time.sleep(2)
    
    # Test 1: Sequential logins (baseline)
    print("\nTest 1: Sequential logins (5 iterations)")
    print("-"*60)
    
    results = stress_test_login(0, 5)
    for thread_id, iteration, status, details in results:
        print(f"Sequential {iteration}: {status} - {details}")
    
    # Test 2: Concurrent logins
    print("\nTest 2: Concurrent logins (5 threads x 3 iterations)")
    print("-"*60)
    
    with ThreadPoolExecutor(max_workers=5) as executor:
        futures = [executor.submit(stress_test_login, i, 3) for i in range(5)]
        
        for future in as_completed(futures):
            results = future.result()
            for thread_id, iteration, status, details in results:
                print(f"Thread {thread_id}, Iter {iteration}: {status} - {details}")
    
    # Check if server is still running
    time.sleep(1)
    status_result = subprocess.run(['./build/jdbx_runtime.sh', 'status'], 
                                 capture_output=True, text=True)
    print("\n" + "-"*60)
    print("Server status after test:", status_result.stdout.strip())
    
    # Check for memory corruption in logs
    print("\nChecking for memory corruption...")
    log_check = subprocess.run(['tail', '-n', '100', '/opt/jdbx/build/var/jdbxd.log'], 
                             capture_output=True, text=True)
    
    corruption_count = log_check.stdout.count('Corrupted JSON type')
    if corruption_count > 0:
        print(f"❌ MEMORY CORRUPTION DETECTED: {corruption_count} instances")
        # Show some examples
        for line in log_check.stdout.split('\n'):
            if 'Corrupted JSON type' in line:
                print(f"  {line}")
                break
    else:
        print("✅ No memory corruption detected")
    
    print("\n" + "="*60)
    print("Test complete")

if __name__ == "__main__":
    import os
    main()