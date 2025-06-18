#!/usr/bin/env python3
"""
Test large document handling with dynamic buffers
"""

import requests
import json
import urllib3
import time

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

def test_document_size(size_kb, description):
    """Test creating a document of given size"""
    print(f"\n🧪 Testing {description} ({size_kb}KB)...")
    
    # Generate content of specified size
    content = "x" * (size_kb * 1024)
    
    doc = {
        "title": f"Large document {description}",
        "content": content,
        "size_kb": size_kb,
        "timestamp": time.time()
    }
    
    try:
        start_time = time.time()
        response = session.post(f"{BASE_URL}/api/documents", json=doc, timeout=30)
        elapsed = time.time() - start_time
        
        if response.status_code == 201:
            doc_id = response.json()["uuid"]
            print(f"✅ Created successfully in {elapsed:.2f}s")
            
            # Try to retrieve it
            start_time = time.time()
            get_response = session.get(f"{BASE_URL}/api/documents/{doc_id}", timeout=30)
            elapsed = time.time() - start_time
            
            if get_response.status_code == 200:
                retrieved_doc = get_response.json()
                if len(retrieved_doc.get("content", "")) == len(content):
                    print(f"✅ Retrieved successfully in {elapsed:.2f}s")
                else:
                    print(f"⚠️ Retrieved but content size mismatch")
            else:
                print(f"❌ Failed to retrieve: {get_response.status_code}")
            
            # Clean up
            session.delete(f"{BASE_URL}/api/documents/{doc_id}")
            
        elif response.status_code == 413:
            print(f"❌ Rejected as too large (413)")
        else:
            print(f"❌ Failed with status {response.status_code}: {response.text[:100]}")
            
    except Exception as e:
        print(f"❌ Error: {e}")

def main():
    print("Large Document Test Suite")
    print("=" * 50)
    
    if not login():
        print("❌ Login failed")
        return
    print("✅ Login successful")
    
    # Test various document sizes
    test_sizes = [
        (1, "1KB - Tiny"),
        (10, "10KB - Small"),
        (100, "100KB - Medium"),
        (500, "500KB - Large"),
        (1024, "1MB - Very Large"),
        (5120, "5MB - Huge"),
        (10240, "10MB - Maximum"),
        (15360, "15MB - Over Limit"),
    ]
    
    for size_kb, description in test_sizes:
        test_document_size(size_kb, description)
    
    # Final health check
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