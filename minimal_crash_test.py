#!/usr/bin/env python3
"""
Minimal test to reproduce server crash
"""

import requests
import json
import threading
import urllib3

urllib3.disable_warnings()

BASE_URL = "https://localhost:5000"

def create_doc(thread_id, token):
    """Create a document in a thread"""
    session = requests.Session()
    session.verify = False
    session.headers["Authorization"] = f"Bearer {token}"
    
    try:
        doc = {
            "title": f"Thread {thread_id} doc",
            "thread_id": thread_id
        }
        response = session.post(f"{BASE_URL}/api/documents", json=doc, timeout=5)
        print(f"Thread {thread_id}: {response.status_code}")
        return response.status_code == 201
    except Exception as e:
        print(f"Thread {thread_id}: Error - {e}")
        return False

def main():
    print("Minimal Crash Test")
    print("=" * 50)
    
    # Login
    session = requests.Session()
    session.verify = False
    
    response = session.post(f"{BASE_URL}/api/auth/login", json={
        "username": "admin",
        "password": "secure123456789"
    })
    
    if response.status_code != 200:
        print("❌ Login failed")
        return
    
    token = response.json()["token"]
    print("✅ Login successful")
    
    # Test server is healthy
    session.headers["Authorization"] = f"Bearer {token}"
    response = session.get(f"{BASE_URL}/api/health")
    print(f"Initial health: {response.status_code}")
    
    # Launch 20 threads to create documents simultaneously
    print("\nLaunching 20 concurrent document creates...")
    threads = []
    for i in range(20):
        t = threading.Thread(target=create_doc, args=(i, token))
        threads.append(t)
        t.start()
    
    # Wait for threads
    for t in threads:
        t.join()
    
    # Check server health
    print("\nChecking server health...")
    try:
        response = session.get(f"{BASE_URL}/api/health", timeout=5)
        if response.status_code == 200:
            print("✅ Server is still healthy")
        else:
            print(f"❌ Server returned {response.status_code}")
    except Exception as e:
        print(f"❌ Server crashed: {e}")

if __name__ == "__main__":
    main()