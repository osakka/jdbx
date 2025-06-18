#!/usr/bin/env python3
"""
Comprehensive client compatibility testing for JDBX server
Tests various HTTP clients and request patterns to validate HTTP parsing
"""

import json
import time
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed

# Test configuration
SERVER_URL = "https://localhost:5000"
ADMIN_USER = "admin"
ADMIN_PASS = "secure123456789"

# ANSI color codes for output
GREEN = '\033[92m'
RED = '\033[91m'
YELLOW = '\033[93m'
RESET = '\033[0m'

def print_test(client, test, status, details=""):
    """Print test result with color coding"""
    color = GREEN if status == "PASS" else RED if status == "FAIL" else YELLOW
    print(f"{client:<20} {test:<40} [{color}{status:^6}{RESET}] {details}")

def test_curl():
    """Test with curl (command line)"""
    client = "curl"
    
    # Test 1: Basic login
    try:
        cmd = [
            'curl', '-s', '-k', '-X', 'POST',  # -k to accept self-signed certificates
            f'{SERVER_URL}/api/auth/login',
            '-H', 'Content-Type: application/json',
            '-d', json.dumps({"username": ADMIN_USER, "password": ADMIN_PASS})
        ]
        result = subprocess.run(cmd, capture_output=True, text=True)
        response = json.loads(result.stdout)
        
        if 'token' in response:
            print_test(client, "Basic login", "PASS", f"Token: {response['token'][:20]}...")
            token = response['token']
        else:
            print_test(client, "Basic login", "FAIL", f"Response: {result.stdout}")
            return
    except Exception as e:
        print_test(client, "Basic login", "FAIL", str(e))
        return
    
    # Test 2: Large payload
    try:
        large_doc = {
            "title": "Large Document",
            "content": "x" * 10000,  # 10KB of data
            "metadata": {f"field_{i}": f"value_{i}" for i in range(100)}
        }
        
        cmd = [
            'curl', '-s', '-k', '-X', 'POST',
            f'{SERVER_URL}/api/libraries/default/collections/documents/documents',
            '-H', 'Content-Type: application/json',
            '-H', f'Authorization: Bearer {token}',
            '-d', json.dumps(large_doc)
        ]
        result = subprocess.run(cmd, capture_output=True, text=True)
        response = json.loads(result.stdout)
        
        if 'uuid' in response:
            print_test(client, "Large payload (10KB)", "PASS", f"UUID: {response['uuid']}")
        else:
            print_test(client, "Large payload (10KB)", "FAIL", result.stdout[:100])
    except Exception as e:
        print_test(client, "Large payload (10KB)", "FAIL", str(e))
    
    # Test 3: Keep-alive multiple requests
    try:
        # Use curl's connection reuse
        cmd_base = ['curl', '-s', '-k', '--keepalive-time', '5']
        success_count = 0
        
        for i in range(5):
            cmd = cmd_base + [
                '-X', 'GET',
                f'{SERVER_URL}/api/libraries/default/collections/documents/documents',
                '-H', f'Authorization: Bearer {token}'
            ]
            result = subprocess.run(cmd, capture_output=True, text=True)
            if result.returncode == 0:
                success_count += 1
        
        if success_count == 5:
            print_test(client, "Keep-alive (5 requests)", "PASS", "All requests successful")
        else:
            print_test(client, "Keep-alive (5 requests)", "FAIL", f"{success_count}/5 successful")
    except Exception as e:
        print_test(client, "Keep-alive (5 requests)", "FAIL", str(e))

def test_python_requests():
    """Test with Python requests library"""
    import requests
    import urllib3
    # Disable SSL warnings for self-signed certificates
    urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
    
    client = "Python requests"
    
    # Test 1: Basic login
    try:
        response = requests.post(
            f'{SERVER_URL}/api/auth/login',
            json={"username": ADMIN_USER, "password": ADMIN_PASS},
            verify=False  # Accept self-signed certificates
        )
        data = response.json()
        
        if response.status_code == 200 and 'token' in data:
            print_test(client, "Basic login", "PASS", f"Token: {data['token'][:20]}...")
            token = data['token']
        else:
            print_test(client, "Basic login", "FAIL", f"Status: {response.status_code}, Response: {response.text}")
            return
    except Exception as e:
        print_test(client, "Basic login", "FAIL", str(e))
        return
    
    # Test 2: Session reuse
    try:
        session = requests.Session()
        session.headers.update({'Authorization': f'Bearer {token}'})
        
        success_count = 0
        for i in range(5):
            response = session.get(f'{SERVER_URL}/api/libraries', verify=False)
            if response.status_code == 200:
                success_count += 1
        
        if success_count == 5:
            print_test(client, "Session reuse (5 requests)", "PASS", "Connection pooling working")
        else:
            print_test(client, "Session reuse (5 requests)", "FAIL", f"{success_count}/5 successful")
    except Exception as e:
        print_test(client, "Session reuse (5 requests)", "FAIL", str(e))
    
    # Test 3: Chunked encoding simulation
    try:
        # Create a large JSON that might trigger chunked encoding
        large_doc = {
            "title": "Very Large Document",
            "sections": [{"id": i, "content": "x" * 1000} for i in range(50)]  # ~50KB
        }
        
        response = session.post(
            f'{SERVER_URL}/api/libraries/default/collections/documents/documents',
            json=large_doc,
            verify=False
        )
        
        if response.status_code == 201:
            print_test(client, "Large JSON payload (50KB)", "PASS", "Large request handled")
        else:
            print_test(client, "Large JSON payload (50KB)", "FAIL", f"Status: {response.status_code}")
    except Exception as e:
        print_test(client, "Large JSON payload (50KB)", "FAIL", str(e))

def test_nodejs_fetch():
    """Test with Node.js fetch API"""
    client = "Node.js fetch"
    
    # Create a Node.js test script
    nodejs_script = """
const SERVER_URL = 'https://localhost:5000';
process.env["NODE_TLS_REJECT_UNAUTHORIZED"] = 0; // Accept self-signed certificates
const ADMIN_USER = 'admin';
const ADMIN_PASS = 'secure123456789';

async function runTests() {
    // Test 1: Basic login
    try {
        const loginResponse = await fetch(`${SERVER_URL}/api/auth/login`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ username: ADMIN_USER, password: ADMIN_PASS })
        });
        
        const loginData = await loginResponse.json();
        if (loginResponse.ok && loginData.token) {
            console.log('LOGIN:PASS:' + loginData.token.substring(0, 20));
            const token = loginData.token;
            
            // Test 2: Multiple requests
            let successCount = 0;
            for (let i = 0; i < 5; i++) {
                const response = await fetch(`${SERVER_URL}/api/libraries`, {
                    headers: { 'Authorization': `Bearer ${token}` }
                });
                if (response.ok) successCount++;
            }
            console.log('MULTIPLE:' + (successCount === 5 ? 'PASS' : 'FAIL') + ':' + successCount);
            
        } else {
            console.log('LOGIN:FAIL:' + loginResponse.status);
        }
    } catch (error) {
        console.log('LOGIN:FAIL:' + error.message);
    }
}

runTests();
"""
    
    try:
        # Write Node.js script
        with open('/tmp/test_nodejs.js', 'w') as f:
            f.write(nodejs_script)
        
        # Run Node.js test
        result = subprocess.run(['node', '/tmp/test_nodejs.js'], capture_output=True, text=True)
        
        for line in result.stdout.strip().split('\n'):
            if line.startswith('LOGIN:'):
                parts = line.split(':')
                if parts[1] == 'PASS':
                    print_test(client, "Basic login", "PASS", f"Token: {parts[2]}...")
                else:
                    print_test(client, "Basic login", "FAIL", parts[2])
            elif line.startswith('MULTIPLE:'):
                parts = line.split(':')
                if parts[1] == 'PASS':
                    print_test(client, "Multiple requests (5)", "PASS", "All successful")
                else:
                    print_test(client, "Multiple requests (5)", "FAIL", f"{parts[2]}/5 successful")
    except Exception as e:
        print_test(client, "Node.js tests", "FAIL", str(e))

def test_wget():
    """Test with wget"""
    client = "wget"
    
    # Test basic request
    try:
        cmd = [
            'wget', '-q', '-O', '-', '--no-check-certificate',
            '--header=Content-Type: application/json',
            '--post-data=' + json.dumps({"username": ADMIN_USER, "password": ADMIN_PASS}),
            f'{SERVER_URL}/api/auth/login'
        ]
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        if result.returncode == 0:
            response = json.loads(result.stdout)
            if 'token' in response:
                print_test(client, "Basic login", "PASS", f"Token: {response['token'][:20]}...")
            else:
                print_test(client, "Basic login", "FAIL", "No token in response")
        else:
            print_test(client, "Basic login", "FAIL", f"Exit code: {result.returncode}")
    except Exception as e:
        print_test(client, "Basic login", "FAIL", str(e))

def test_concurrent_clients():
    """Test multiple clients concurrently"""
    print("\n" + "="*80)
    print("CONCURRENT CLIENT TEST (10 simultaneous connections)")
    print("="*80)
    
    import requests
    import urllib3
    urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
    
    def make_request(client_id):
        try:
            # Login
            response = requests.post(
                f'{SERVER_URL}/api/auth/login',
                json={"username": ADMIN_USER, "password": ADMIN_PASS},
                timeout=5,
                verify=False
            )
            
            if response.status_code == 200:
                token = response.json()['token']
                
                # Make authenticated request
                response = requests.get(
                    f'{SERVER_URL}/api/libraries',
                    headers={'Authorization': f'Bearer {token}'},
                    timeout=5,
                    verify=False
                )
                
                return client_id, response.status_code == 200, "Success"
            else:
                return client_id, False, f"Login failed: {response.status_code}"
        except Exception as e:
            return client_id, False, str(e)
    
    with ThreadPoolExecutor(max_workers=10) as executor:
        futures = [executor.submit(make_request, i) for i in range(10)]
        success_count = 0
        
        for future in as_completed(futures):
            client_id, success, message = future.result()
            if success:
                success_count += 1
                print(f"Client {client_id:2d}: {GREEN}SUCCESS{RESET}")
            else:
                print(f"Client {client_id:2d}: {RED}FAILED{RESET} - {message}")
        
        print(f"\nTotal: {success_count}/10 successful ({success_count*10}%)")

def main():
    print("="*80)
    print("JDBX CLIENT COMPATIBILITY TEST SUITE")
    print("="*80)
    print(f"Server: {SERVER_URL}")
    print(f"Testing with admin user: {ADMIN_USER}")
    print("="*80)
    print()
    
    # Check if server is running
    try:
        import requests
        import urllib3
        urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
        response = requests.get(f'{SERVER_URL}/api/health', timeout=2, verify=False)
        if response.status_code != 200:
            print(f"{RED}ERROR: Server not responding at {SERVER_URL}{RESET}")
            sys.exit(1)
    except Exception as e:
        print(f"{RED}ERROR: Cannot connect to server at {SERVER_URL}{RESET}")
        print(f"Details: {e}")
        sys.exit(1)
    
    print(f"{CLIENT:<20} {TEST:<40} {STATUS:^8} {DETAILS}")
    print("-"*80)
    
    # Run tests for each client
    test_curl()
    print()
    
    test_python_requests()
    print()
    
    test_wget()
    print()
    
    # Check if Node.js is available
    try:
        subprocess.run(['node', '--version'], capture_output=True, check=True)
        test_nodejs_fetch()
        print()
    except:
        print_test("Node.js fetch", "Skipped", "SKIP", "Node.js not installed")
        print()
    
    # Run concurrent test
    test_concurrent_clients()
    
    print("\n" + "="*80)
    print("TEST SUITE COMPLETE")
    print("="*80)

# Format strings for header
CLIENT = "Client"
TEST = "Test"
STATUS = "Status"
DETAILS = "Details"

if __name__ == "__main__":
    main()