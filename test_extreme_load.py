#!/usr/bin/env python3
"""
Extreme load test - maximum stress to verify crash protection
"""

import requests
import json
import urllib3
import time
import threading
from concurrent.futures import ThreadPoolExecutor, as_completed

# Disable SSL warnings
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

SERVER_URL = "https://localhost:5000"
ADMIN_USER = "admin"
ADMIN_PASS = "secure123456789"

# Global token storage
tokens = []
tokens_lock = threading.Lock()

def login_flood(thread_id):
    """Flood server with login requests"""
    results = []
    for i in range(20):
        try:
            response = requests.post(
                f"{SERVER_URL}/api/auth/login",
                json={"username": ADMIN_USER, "password": ADMIN_PASS},
                verify=False,
                timeout=2
            )
            if response.status_code == 200:
                token = response.json()['token']
                with tokens_lock:
                    tokens.append(token)
                results.append('SUCCESS')
            else:
                results.append(f'FAIL_{response.status_code}')
        except Exception as e:
            results.append('ERROR')
    return results

def authenticated_flood(thread_id, token):
    """Flood server with authenticated requests"""
    results = []
    for i in range(30):
        try:
            # Mix of different endpoints
            endpoints = [
                '/api/libraries',
                '/api/collections',
                '/api/health',
                '/api/libraries/default/collections',
            ]
            endpoint = endpoints[i % len(endpoints)]
            
            response = requests.get(
                f"{SERVER_URL}{endpoint}",
                headers={"Authorization": f"Bearer {token}"},
                verify=False,
                timeout=2
            )
            results.append('SUCCESS' if response.status_code == 200 else f'FAIL_{response.status_code}')
        except Exception as e:
            results.append('ERROR')
    return results

def main():
    print("🔥 EXTREME LOAD TEST - Maximum Stress")
    print("="*60)
    print("This test will:")
    print("1. Flood the server with login requests")
    print("2. Use those tokens to flood authenticated endpoints")
    print("3. Mix different types of requests")
    print("4. Apply maximum concurrent load")
    print("="*60)
    
    # Phase 1: Login flood
    print("\n🌊 PHASE 1: Login Flood (200 concurrent logins)")
    start = time.time()
    
    with ThreadPoolExecutor(max_workers=10) as executor:
        futures = [executor.submit(login_flood, i) for i in range(10)]
        login_results = []
        for future in as_completed(futures):
            login_results.extend(future.result())
    
    phase1_duration = time.time() - start
    success_count = login_results.count('SUCCESS')
    print(f"- Duration: {phase1_duration:.2f}s")
    print(f"- Successful logins: {success_count}/200")
    print(f"- Tokens collected: {len(tokens)}")
    
    if len(tokens) == 0:
        print("❌ No tokens obtained, cannot continue")
        return
    
    # Phase 2: Authenticated request flood
    print(f"\n🌊 PHASE 2: Authenticated Request Flood (300 requests using {min(10, len(tokens))} tokens)")
    start = time.time()
    
    # Use up to 10 tokens for variety
    test_tokens = tokens[:min(10, len(tokens))]
    
    with ThreadPoolExecutor(max_workers=10) as executor:
        futures = []
        for i in range(10):
            token = test_tokens[i % len(test_tokens)]
            futures.append(executor.submit(authenticated_flood, i, token))
        
        auth_results = []
        for future in as_completed(futures):
            auth_results.extend(future.result())
    
    phase2_duration = time.time() - start
    auth_success = auth_results.count('SUCCESS')
    print(f"- Duration: {phase2_duration:.2f}s")
    print(f"- Successful requests: {auth_success}/300")
    print(f"- Error rate: {(300-auth_success)/300*100:.1f}%")
    
    # Check for memory corruption
    print("\n🔍 Checking for memory corruption...")
    import subprocess
    log_check = subprocess.run(
        ['tail', '-n', '1000', '/opt/jdbx/build/var/jdbxd.log'], 
        capture_output=True, text=True
    )
    
    corruption_count = log_check.stdout.count('Corrupted JSON type')
    segfault_count = log_check.stdout.count('Segmentation')
    rate_limit_count = log_check.stdout.count('Rate limiting')
    
    print(f"- JSON corruption errors: {corruption_count}")
    print(f"- Segmentation faults: {segfault_count}")
    print(f"- Rate limit activations: {rate_limit_count}")
    
    # Final health check
    print("\n🏥 Final Server Health Check...")
    time.sleep(1)
    
    try:
        response = requests.get(f"{SERVER_URL}/api/health", verify=False, timeout=5)
        if response.status_code == 200:
            print("✅ SERVER SURVIVED THE EXTREME LOAD TEST!")
        else:
            print(f"❌ Server unhealthy: {response.status_code}")
    except Exception as e:
        print(f"❌ Server crashed or not responding: {e}")
    
    # Process check
    status = subprocess.run(['./build/jdbx_runtime.sh', 'status'], 
                          capture_output=True, text=True)
    print(f"\nProcess status: {status.stdout.strip()}")
    
    # Summary
    print("\n" + "="*60)
    print("EXTREME LOAD TEST SUMMARY")
    print(f"Total requests: {200 + 300} in {phase1_duration + phase2_duration:.2f}s")
    print(f"Requests/second: {500/(phase1_duration + phase2_duration):.2f}")
    print(f"Memory corruption: {'DETECTED' if corruption_count > 0 else 'NONE'}")
    print(f"Server status: {'ALIVE' if 'running' in status.stdout else 'DEAD'}")
    print("="*60)

if __name__ == "__main__":
    import os
    os.chdir('/opt/jdbx')
    main()