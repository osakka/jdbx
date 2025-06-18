#!/usr/bin/env python3
"""
Test concurrent updates in isolation
"""

import requests
import json
import threading
import time
import urllib3

urllib3.disable_warnings()

BASE_URL = "https://localhost:5000"
session = requests.Session()
session.verify = False

def login():
    """Login and get auth token"""
    response = session.post(f"{BASE_URL}/api/auth/login", json={
        "username": "admin",
        "password": "secure123456789"
    })
    if response.status_code == 200:
        token = response.json()["token"]
        session.headers["Authorization"] = f"Bearer {token}"
        return True
    return False

def update_document(doc_id, thread_id, results):
    """Update a document from a thread"""
    try:
        # First get the document
        get_response = session.get(f"{BASE_URL}/api/documents/{doc_id}")
        if get_response.status_code != 200:
            results[thread_id] = {
                "status": get_response.status_code,
                "response": f"Failed to get document: {get_response.text[:100]}"
            }
            return
            
        doc = get_response.json()
        
        # Update it
        doc["updated_by"] = f"thread_{thread_id}"
        doc["timestamp"] = time.time()
        doc["random_data"] = f"test_{thread_id}_{time.time()}"
        
        response = session.put(
            f"{BASE_URL}/api/documents/{doc_id}",
            json=doc,
            timeout=5
        )
        
        results[thread_id] = {
            "status": response.status_code,
            "response": response.text[:100] if response.text else "No response"
        }
    except Exception as e:
        results[thread_id] = {
            "status": "error",
            "response": str(e)
        }

def main():
    print("Concurrent Updates Test")
    print("=" * 50)
    
    # Login
    if not login():
        print("❌ Login failed")
        return
    print("✅ Login successful")
    
    # Ensure library exists
    response = session.post(f"{BASE_URL}/api/libraries", json={"name": "default"})
    if response.status_code in [201, 409]:  # Created or already exists
        print("✅ Library 'default' ready")
    
    # Create a test document
    doc_data = {
        "title": "Concurrent Test Doc",
        "content": "Initial content",
        "version": 1
    }
    
    response = session.post(
        f"{BASE_URL}/api/documents",
        json=doc_data
    )
    
    if response.status_code != 201:
        print(f"❌ Failed to create document: {response.status_code}")
        return
    
    doc_id = response.json()["uuid"]
    print(f"✅ Created document: {doc_id}")
    
    # Launch concurrent updates
    print("\nLaunching 10 concurrent updates...")
    threads = []
    results = {}
    
    for i in range(10):
        t = threading.Thread(target=update_document, args=(doc_id, i, results))
        threads.append(t)
        t.start()
    
    # Wait for all threads
    for t in threads:
        t.join()
    
    # Check results
    print("\nResults:")
    success_count = 0
    for thread_id, result in sorted(results.items()):
        status = result["status"]
        if status == 200:
            success_count += 1
            print(f"Thread {thread_id}: ✅ {status}")
        else:
            print(f"Thread {thread_id}: ❌ {status} - {result['response']}")
    
    print(f"\nTotal: {success_count}/10 successful updates")
    
    # Verify document is still accessible
    response = session.get(f"{BASE_URL}/api/documents/{doc_id}")
    if response.status_code == 200:
        print("✅ Document still accessible after concurrent updates")
    else:
        print("❌ Document not accessible after concurrent updates")
    
    # Cleanup
    session.delete(f"{BASE_URL}/api/documents/{doc_id}")

if __name__ == "__main__":
    main()