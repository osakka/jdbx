#!/usr/bin/env python3
"""
Test session reuse attack - using same token repeatedly
"""

import requests
import json
import urllib3
import time
from concurrent.futures import ThreadPoolExecutor, as_completed

# Disable SSL warnings
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

SERVER_URL = "https://localhost:5000"
ADMIN_USER = "admin"
ADMIN_PASS = "secure123456789"

def main():
    print("Session Reuse Attack Test")
    print("="*50)
    
    # First, get a valid token
    print("\n1. Getting authentication token...")
    response = requests.post(
        f"{SERVER_URL}/api/auth/login",
        json={"username": ADMIN_USER, "password": ADMIN_PASS},
        verify=False
    )
    
    if response.status_code != 200:
        print(f"❌ Login failed: {response.status_code}")
        return
        
    token = response.json()['token']
    print(f"✅ Got token: {token[:30]}...")
    
    # Now hammer the server with authenticated requests using the same token
    print("\n2. Hammering server with authenticated requests...")
    
    def make_authenticated_request(request_id):
        try:
            start = time.time()
            response = requests.get(
                f"{SERVER_URL}/api/libraries",
                headers={"Authorization": f"Bearer {token}"},
                verify=False,
                timeout=5
            )
            duration = time.time() - start
            
            return {
                'id': request_id,
                'status': response.status_code,
                'duration': duration,
                'success': response.status_code == 200
            }
        except Exception as e:
            return {
                'id': request_id,
                'status': 'ERROR',
                'error': str(e)[:50],
                'success': False
            }
    
    # Make rapid requests
    num_requests = 50
    start_time = time.time()
    
    with ThreadPoolExecutor(max_workers=10) as executor:
        futures = [executor.submit(make_authenticated_request, i) for i in range(num_requests)]
        results = [future.result() for future in as_completed(futures)]
    
    duration = time.time() - start_time
    
    # Analyze results
    successful = sum(1 for r in results if r['success'])
    print(f"\n📊 Results:")
    print(f"- Total requests: {num_requests}")
    print(f"- Successful: {successful}")
    print(f"- Failed: {num_requests - successful}")
    print(f"- Duration: {duration:.2f}s")
    print(f"- Requests/second: {num_requests/duration:.2f}")
    
    # Check logs for rate limiting
    print("\n3. Checking rate limiting...")
    import subprocess
    log_check = subprocess.run(
        ['tail', '-n', '500', '/opt/jdbx/build/var/jdbxd.log'], 
        capture_output=True, text=True
    )
    
    rate_limit_count = log_check.stdout.count('Rate limiting: Skipping session update')
    update_count = log_check.stdout.count('Extended session')
    corruption_count = log_check.stdout.count('Corrupted JSON type')
    
    print(f"- Session updates performed: {update_count}")
    print(f"- Session updates rate-limited: {rate_limit_count}")
    print(f"- JSON corruption errors: {corruption_count}")
    
    if rate_limit_count > 0:
        print("✅ Rate limiting is working!")
        
        # Show some examples
        for line in log_check.stdout.split('\n'):
            if 'Rate limiting:' in line:
                print(f"   Example: {line.strip()}")
                break
    
    # Check server health
    print("\n4. Server health check...")
    try:
        response = requests.get(f"{SERVER_URL}/api/health", verify=False, timeout=2)
        if response.status_code == 200:
            print("✅ Server is healthy")
        else:
            print(f"❌ Server unhealthy: {response.status_code}")
    except Exception as e:
        print(f"❌ Server not responding: {e}")
    
    # Check process status
    status = subprocess.run(['./build/jdbx_runtime.sh', 'status'], 
                          capture_output=True, text=True)
    print(f"\nServer process: {status.stdout.strip()}")

if __name__ == "__main__":
    import os
    os.chdir('/opt/jdbx')
    main()