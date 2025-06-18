#!/usr/bin/env python3
"""
Test delete operations to identify the crash
"""

import requests
import json
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

def main():
    print("Delete Operation Test")
    print("=" * 50)
    
    if not login():
        print("❌ Login failed")
        return
    print("✅ Login successful")
    
    # Create a document
    doc_data = {
        "title": "Delete test doc",
        "content": "This will be deleted",
        "test": True
    }
    
    response = session.post(f"{BASE_URL}/api/documents", json=doc_data)
    if response.status_code != 201:
        print(f"❌ Failed to create document: {response.status_code}")
        return
    
    doc_id = response.json()["uuid"]
    print(f"✅ Created document: {doc_id}")
    
    # Verify it exists
    response = session.get(f"{BASE_URL}/api/documents/{doc_id}")
    if response.status_code == 200:
        print("✅ Document exists")
    else:
        print(f"❌ Cannot retrieve document: {response.status_code}")
    
    # Try to delete it
    print("\nAttempting delete...")
    try:
        response = session.delete(f"{BASE_URL}/api/documents/{doc_id}", timeout=5)
        print(f"Delete response: {response.status_code}")
        if response.text:
            print(f"Response body: {response.text[:200]}")
    except Exception as e:
        print(f"❌ Delete failed with error: {e}")
    
    # Check if it still exists
    print("\nChecking if document still exists...")
    try:
        response = session.get(f"{BASE_URL}/api/documents/{doc_id}", timeout=5)
        if response.status_code == 404:
            print("✅ Document successfully deleted")
        elif response.status_code == 200:
            print("❌ Document still exists after delete")
        else:
            print(f"❓ Unexpected status: {response.status_code}")
    except Exception as e:
        print(f"❌ Check failed: {e}")
    
    # Test server health
    print("\n🏥 Server health check...")
    try:
        response = session.get(f"{BASE_URL}/api/health", timeout=5)
        if response.status_code == 200:
            print("✅ Server is healthy")
        else:
            print(f"❌ Server returned {response.status_code}")
    except Exception as e:
        print(f"❌ Server not responding: {e}")

if __name__ == "__main__":
    main()