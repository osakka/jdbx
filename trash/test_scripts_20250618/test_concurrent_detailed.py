#!/usr/bin/env python3
"""Detailed test for concurrent request failures"""

import concurrent.futures
import requests
import json
import time
import urllib3

# Disable SSL warnings
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

BASE_URL = "https://localhost:5000"

def login():
    """Get auth token"""
    try:
        response = requests.post(
            f"{BASE_URL}/api/auth/login",
            json={"username": "admin", "password": "secure123456789"},
            verify=False,
            timeout=10
        )
        if response.status_code == 200:
            return response.json()["token"]
    except Exception as e:
        print(f"Login failed: {e}")
    return None

def test_single_request(token, request_id):
    """Test a single document creation"""
    start_time = time.time()
    
    try:
        headers = {"Authorization": f"Bearer {token}"}
        doc = {
            "name": f"Concurrent Test {request_id}",
            "content": f"Test content for request {request_id}"
        }
        
        response = requests.post(
            f"{BASE_URL}/api/documents",
            json=doc,
            headers=headers,
            verify=False,
            timeout=10
        )
        
        elapsed = time.time() - start_time
        
        if response.status_code == 201:
            doc_id = response.json()["uuid"]
            # Clean up
            requests.delete(f"{BASE_URL}/api/documents/{doc_id}", headers=headers, verify=False)
            return f"✅ Request {request_id}: SUCCESS ({elapsed:.2f}s)"
        else:
            return f"❌ Request {request_id}: HTTP {response.status_code} ({elapsed:.2f}s) - {response.text[:100]}"
            
    except requests.exceptions.Timeout:
        elapsed = time.time() - start_time
        return f"⏱️  Request {request_id}: TIMEOUT ({elapsed:.2f}s)"
    except requests.exceptions.ConnectionError as e:
        elapsed = time.time() - start_time
        return f"🔌 Request {request_id}: CONNECTION ERROR ({elapsed:.2f}s) - {str(e)[:100]}"
    except Exception as e:
        elapsed = time.time() - start_time
        return f"❌ Request {request_id}: ERROR ({elapsed:.2f}s) - {str(e)[:100]}"

def test_concurrent_requests(num_workers, num_requests):
    """Test concurrent requests with detailed logging"""
    print(f"\nTesting {num_requests} concurrent requests with {num_workers} workers...")
    
    # Get token
    token = login()
    if not token:
        print("❌ Failed to get auth token")
        return
    
    print("✅ Got auth token")
    
    # Test requests
    start_time = time.time()
    
    with concurrent.futures.ThreadPoolExecutor(max_workers=num_workers) as executor:
        futures = [executor.submit(test_single_request, token, i) for i in range(num_requests)]
        
        # Process results as they complete
        for future in concurrent.futures.as_completed(futures):
            result = future.result()
            print(f"  {result}")
    
    total_time = time.time() - start_time
    print(f"\nTotal time: {total_time:.2f}s")

def test_sequential_requests(num_requests):
    """Test sequential requests for comparison"""
    print(f"\nTesting {num_requests} sequential requests...")
    
    # Get token
    token = login()
    if not token:
        print("❌ Failed to get auth token")
        return
    
    print("✅ Got auth token")
    
    # Test requests
    start_time = time.time()
    
    for i in range(num_requests):
        result = test_single_request(token, i)
        print(f"  {result}")
    
    total_time = time.time() - start_time
    print(f"\nTotal time: {total_time:.2f}s")

if __name__ == "__main__":
    print("Concurrent Request Failure Investigation")
    print("=" * 50)
    
    # Test sequential first to establish baseline
    test_sequential_requests(5)
    
    # Test with different concurrency levels
    test_concurrent_requests(2, 4)
    test_concurrent_requests(5, 10)
    test_concurrent_requests(10, 10)