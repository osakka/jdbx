#!/usr/bin/env python3
"""
Stress test to find crash conditions
"""

import requests
import json
import threading
import time
import urllib3
from concurrent.futures import ThreadPoolExecutor, as_completed

urllib3.disable_warnings()

BASE_URL = "https://localhost:5000"

def create_session():
    """Create authenticated session"""
    session = requests.Session()
    session.verify = False
    
    response = session.post(f"{BASE_URL}/api/auth/login", json={
        "username": "admin",
        "password": "secure123456789"
    })
    
    if response.status_code == 200:
        token = response.json()["token"]
        session.headers["Authorization"] = f"Bearer {token}"
        return session
    return None

def stress_test_creates(session, thread_id):
    """Stress test document creation"""
    for i in range(10):
        try:
            doc = {
                "title": f"Stress doc {thread_id}-{i}",
                "thread": thread_id,
                "index": i,
                "data": "x" * 1000  # 1KB of data
            }
            response = session.post(f"{BASE_URL}/api/documents", json=doc, timeout=2)
            if response.status_code != 201:
                print(f"Thread {thread_id}: Create failed with {response.status_code}")
        except Exception as e:
            print(f"Thread {thread_id}: Error - {str(e)[:50]}")
            return False
    return True

def stress_test_queries(session, thread_id):
    """Stress test queries"""
    # Note: Query endpoint doesn't exist, this will generate 404s
    for i in range(10):
        try:
            # Try different endpoints that might work
            endpoints = [
                f"{BASE_URL}/api/documents",  # List all
                f"{BASE_URL}/api/libraries",  # List libraries
                f"{BASE_URL}/api/health",     # Health check
            ]
            
            for endpoint in endpoints:
                response = session.get(endpoint, timeout=2)
                if response.status_code >= 500:
                    print(f"Thread {thread_id}: Server error {response.status_code} on {endpoint}")
        except Exception as e:
            print(f"Thread {thread_id}: Query error - {str(e)[:50]}")
            return False
    return True

def stress_test_mixed(session, thread_id):
    """Mixed operations"""
    doc_ids = []
    
    # Create some docs
    for i in range(5):
        try:
            doc = {"title": f"Mixed {thread_id}-{i}", "test": True}
            response = session.post(f"{BASE_URL}/api/documents", json=doc, timeout=2)
            if response.status_code == 201:
                doc_ids.append(response.json()["uuid"])
        except:
            pass
    
    # Update them
    for doc_id in doc_ids:
        try:
            response = session.get(f"{BASE_URL}/api/documents/{doc_id}", timeout=2)
            if response.status_code == 200:
                doc = response.json()
                doc["updated"] = True
                session.put(f"{BASE_URL}/api/documents/{doc_id}", json=doc, timeout=2)
        except:
            pass
    
    # Delete them
    for doc_id in doc_ids:
        try:
            session.delete(f"{BASE_URL}/api/documents/{doc_id}", timeout=2)
        except:
            pass
    
    return True

def main():
    print("Stress Test - Looking for Crash Conditions")
    print("=" * 50)
    
    # Create initial session for testing
    session = create_session()
    if not session:
        print("❌ Failed to create initial session")
        return
    
    print("✅ Initial login successful")
    
    # Test 1: Many concurrent sessions
    print("\n🧪 Test 1: Creating 20 concurrent sessions...")
    sessions = []
    with ThreadPoolExecutor(max_workers=20) as executor:
        futures = [executor.submit(create_session) for _ in range(20)]
        for future in as_completed(futures):
            s = future.result()
            if s:
                sessions.append(s)
    
    print(f"Created {len(sessions)}/20 sessions")
    
    # Test 2: Concurrent document creation
    print("\n🧪 Test 2: Concurrent document creation...")
    with ThreadPoolExecutor(max_workers=10) as executor:
        futures = [executor.submit(stress_test_creates, sessions[i % len(sessions)], i) 
                  for i in range(10)]
        results = [f.result() for f in as_completed(futures)]
    
    print(f"Document creation: {sum(results)}/10 threads succeeded")
    
    # Check server health
    try:
        response = session.get(f"{BASE_URL}/api/health", timeout=2)
        print(f"Server health: {response.status_code}")
    except Exception as e:
        print(f"❌ Server not responding: {e}")
        return
    
    # Test 3: Concurrent queries
    print("\n🧪 Test 3: Concurrent queries...")
    with ThreadPoolExecutor(max_workers=10) as executor:
        futures = [executor.submit(stress_test_queries, sessions[i % len(sessions)], i) 
                  for i in range(10)]
        results = [f.result() for f in as_completed(futures)]
    
    print(f"Query test: {sum(results)}/10 threads succeeded")
    
    # Test 4: Mixed operations
    print("\n🧪 Test 4: Mixed operations...")
    with ThreadPoolExecutor(max_workers=10) as executor:
        futures = [executor.submit(stress_test_mixed, sessions[i % len(sessions)], i) 
                  for i in range(10)]
        results = [f.result() for f in as_completed(futures)]
    
    print(f"Mixed operations: {sum(results)}/10 threads succeeded")
    
    # Final health check
    print("\n🏥 Final health check...")
    try:
        response = session.get(f"{BASE_URL}/api/health", timeout=5)
        if response.status_code == 200:
            print("✅ Server survived stress test!")
        else:
            print(f"❌ Server returned {response.status_code}")
    except Exception as e:
        print(f"❌ Server crashed: {e}")
    
    # Close all sessions
    for s in sessions:
        s.close()

if __name__ == "__main__":
    main()