#!/usr/bin/env python3
"""Test concurrent operations on JDBX server"""

import requests
import json
import time
import concurrent.futures
import threading
import random
import string
import urllib3
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

# Server configuration
BASE_URL = "https://localhost:5000"
ADMIN_USER = "admin"
ADMIN_PASS = "secure123456789"

# Global variables for tracking results
results_lock = threading.Lock()
successful_ops = 0
failed_ops = 0
operation_times = []

def login():
    """Login and get JWT token"""
    response = requests.post(
        f"{BASE_URL}/api/auth/login",
        json={"username": ADMIN_USER, "password": ADMIN_PASS},
        headers={"Content-Type": "application/json"},
        verify=False
    )
    response.raise_for_status()
    return response.json()["token"]

def random_string(length=10):
    """Generate random string"""
    return ''.join(random.choices(string.ascii_lowercase + string.digits, k=length))

def perform_crud_operations(session_id, token):
    """Perform CRUD operations"""
    global successful_ops, failed_ops
    
    headers = {
        "Authorization": f"Bearer {token}",
        "Content-Type": "application/json"
    }
    
    ops_in_session = 0
    session_start = time.time()
    
    try:
        # Create operations
        for i in range(5):
            doc_id = f"test-doc-{session_id}-{i}-{random_string(6)}"
            doc = {
                "title": f"Test Document {i}",
                "content": f"Content for test document {i} in session {session_id}",
                "tags": ["test", f"session-{session_id}"],
                "metadata": {
                    "created_by": f"session-{session_id}",
                    "iteration": i
                }
            }
            
            response = requests.post(
                f"{BASE_URL}/api/documents",
                json=doc,
                headers=headers,
                verify=False
            )
            
            if response.status_code == 201:
                with results_lock:
                    successful_ops += 1
                ops_in_session += 1
            else:
                with results_lock:
                    failed_ops += 1
                print(f"Session {session_id}: Create failed - {response.status_code}: {response.text}")
        
        # Read operations
        response = requests.get(
            f"{BASE_URL}/api/documents",
            headers=headers,
            verify=False
        )
        
        if response.status_code == 200:
            with results_lock:
                successful_ops += 1
            ops_in_session += 1
            docs = response.json().get("documents", [])
            
            # Update some documents
            for doc in docs[:3]:
                if "uuid" in doc:
                    doc["updated"] = True
                    doc["update_time"] = time.time()
                    
                    response = requests.put(
                        f"{BASE_URL}/api/documents/{doc['uuid']}",
                        json=doc,
                        headers=headers,
                        verify=False
                    )
                    
                    if response.status_code == 200:
                        with results_lock:
                            successful_ops += 1
                        ops_in_session += 1
                    else:
                        with results_lock:
                            failed_ops += 1
                        print(f"Session {session_id}: Update failed - {response.status_code}")
        
        # Delete operation
        if docs and len(docs) > 0:
            doc_to_delete = docs[0]
            if "uuid" in doc_to_delete:
                response = requests.delete(
                    f"{BASE_URL}/api/documents/{doc_to_delete['uuid']}",
                    headers=headers,
                    verify=False
                )
                
                if response.status_code == 200:
                    with results_lock:
                        successful_ops += 1
                    ops_in_session += 1
                else:
                    with results_lock:
                        failed_ops += 1
                    print(f"Session {session_id}: Delete failed - {response.status_code}")
        
    except Exception as e:
        print(f"Session {session_id} error: {str(e)}")
        with results_lock:
            failed_ops += 1
    
    session_time = time.time() - session_start
    with results_lock:
        operation_times.append(session_time)
    
    return ops_in_session

def stress_test(num_sessions=10):
    """Run concurrent stress test"""
    global successful_ops, failed_ops, operation_times
    
    # Reset counters
    successful_ops = 0
    failed_ops = 0
    operation_times = []
    
    print(f"\n🚀 Starting stress test with {num_sessions} concurrent sessions...")
    
    # Login once to get token
    try:
        token = login()
        print("✅ Login successful")
    except Exception as e:
        print(f"❌ Login failed: {e}")
        return
    
    # Run concurrent operations
    start_time = time.time()
    
    with concurrent.futures.ThreadPoolExecutor(max_workers=num_sessions) as executor:
        futures = []
        for i in range(num_sessions):
            future = executor.submit(perform_crud_operations, i, token)
            futures.append(future)
        
        # Wait for all to complete
        concurrent.futures.wait(futures)
    
    total_time = time.time() - start_time
    
    # Print results
    print(f"\n📊 Stress Test Results:")
    print(f"   Total time: {total_time:.2f} seconds")
    print(f"   Successful operations: {successful_ops}")
    print(f"   Failed operations: {failed_ops}")
    print(f"   Success rate: {(successful_ops/(successful_ops+failed_ops)*100):.1f}%" if (successful_ops+failed_ops) > 0 else "N/A")
    print(f"   Operations per second: {successful_ops/total_time:.1f}")
    print(f"   Average session time: {sum(operation_times)/len(operation_times):.2f}s" if operation_times else "N/A")
    
    return successful_ops, failed_ops

if __name__ == "__main__":
    print("JDBX Concurrent Operations Test")
    print("================================")
    
    # Test with increasing loads
    for sessions in [5, 10, 20, 50]:
        successful, failed = stress_test(sessions)
        
        if failed > 0:
            print(f"\n⚠️  Warning: {failed} operations failed!")
            if failed > successful * 0.1:  # More than 10% failure rate
                print("❌ High failure rate detected!")
                break
        else:
            print(f"✅ All operations successful!")
        
        time.sleep(2)  # Brief pause between tests
    
    print("\n✅ Test completed!")