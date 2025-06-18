#!/usr/bin/env python3
"""
Test authentication attack resilience with rate limiting
"""

import requests
import json
import urllib3
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
import threading

# Disable SSL warnings
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

SERVER_URL = "https://localhost:5000"
ADMIN_USER = "admin"
ADMIN_PASS = "secure123456789"

# Track results
results_lock = threading.Lock()
results = {
    'success': 0,
    'failed': 0,
    'errors': 0,
    'tokens': set()
}

def hammer_auth(thread_id, requests_per_thread=10):
    """Simulate rapid authentication requests"""
    local_results = []
    
    for i in range(requests_per_thread):
        try:
            start_time = time.time()
            response = requests.post(
                f"{SERVER_URL}/api/auth/login",
                json={"username": ADMIN_USER, "password": ADMIN_PASS},
                verify=False,
                timeout=5
            )
            end_time = time.time()
            
            if response.status_code == 200:
                token = response.json().get('token', '')
                local_results.append({
                    'thread': thread_id,
                    'request': i,
                    'status': 'SUCCESS',
                    'time': end_time - start_time,
                    'token': token[:20]
                })
                with results_lock:
                    results['success'] += 1
                    results['tokens'].add(token)
            else:
                local_results.append({
                    'thread': thread_id,
                    'request': i,
                    'status': f'HTTP_{response.status_code}',
                    'time': end_time - start_time
                })
                with results_lock:
                    results['failed'] += 1
                    
        except Exception as e:
            local_results.append({
                'thread': thread_id,
                'request': i,
                'status': 'ERROR',
                'error': str(e)[:50]
            })
            with results_lock:
                results['errors'] += 1
                
        # No delay between requests - maximum stress
        
    return local_results

def main():
    print("Authentication DoS Attack Simulation")
    print("="*60)
    print("WARNING: This is a stress test that may crash unprotected servers!")
    print("="*60)
    
    # Test parameters
    num_threads = 10
    requests_per_thread = 20
    total_requests = num_threads * requests_per_thread
    
    print(f"\nTest Configuration:")
    print(f"- Threads: {num_threads}")
    print(f"- Requests per thread: {requests_per_thread}")
    print(f"- Total requests: {total_requests}")
    print(f"- Rate limiting expected: 1 session update per 30 seconds")
    
    # Check server health before test
    print("\nChecking server health before attack...")
    try:
        response = requests.get(f"{SERVER_URL}/api/health", verify=False, timeout=2)
        if response.status_code == 200:
            print("✅ Server healthy")
        else:
            print(f"❌ Server unhealthy: {response.status_code}")
            return
    except Exception as e:
        print(f"❌ Server not responding: {e}")
        return
    
    # Launch the attack
    print("\n🚀 Launching authentication attack...")
    start_time = time.time()
    
    with ThreadPoolExecutor(max_workers=num_threads) as executor:
        futures = [executor.submit(hammer_auth, i, requests_per_thread) for i in range(num_threads)]
        
        # Wait for all threads to complete
        all_results = []
        for future in as_completed(futures):
            thread_results = future.result()
            all_results.extend(thread_results)
    
    end_time = time.time()
    duration = end_time - start_time
    
    # Analyze results
    print(f"\n⏱️  Attack completed in {duration:.2f} seconds")
    print(f"📊 Requests per second: {total_requests/duration:.2f}")
    print(f"\nResults:")
    print(f"- Successful logins: {results['success']}")
    print(f"- Failed requests: {results['failed']}")
    print(f"- Errors: {results['errors']}")
    print(f"- Unique tokens generated: {len(results['tokens'])}")
    
    # Check for rate limiting evidence
    if results['success'] > 0:
        print("\n🔍 Checking session update logs...")
        import subprocess
        log_check = subprocess.run(
            ['tail', '-n', '200', '/opt/jdbx/build/var/jdbxd.log'], 
            capture_output=True, text=True
        )
        
        rate_limit_count = log_check.stdout.count('Rate limiting: Skipping session update')
        update_count = log_check.stdout.count('Extended session')
        
        print(f"- Session updates performed: {update_count}")
        print(f"- Session updates rate-limited: {rate_limit_count}")
        
        if rate_limit_count > 0:
            print("✅ Rate limiting is working! DoS protection active.")
        else:
            print("⚠️  No rate limiting detected in this test window.")
    
    # Check server health after attack
    print("\n🏥 Checking server health after attack...")
    time.sleep(1)
    
    try:
        response = requests.get(f"{SERVER_URL}/api/health", verify=False, timeout=5)
        if response.status_code == 200:
            print("✅ Server survived the attack!")
        else:
            print(f"❌ Server unhealthy: {response.status_code}")
    except Exception as e:
        print(f"❌ Server crashed or not responding: {e}")
        
    # Check if server process is still running
    process_check = subprocess.run(['./build/jdbx_runtime.sh', 'status'], 
                                 capture_output=True, text=True)
    print(f"\nServer status: {process_check.stdout.strip()}")
    
    print("\n" + "="*60)
    print("Attack simulation complete")

if __name__ == "__main__":
    import subprocess
    import os
    os.chdir('/opt/jdbx')
    main()