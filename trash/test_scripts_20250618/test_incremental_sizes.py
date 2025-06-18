#!/usr/bin/env python3
"""
Test with incrementally larger payloads to find exact failure point
"""

import requests
import json
import urllib3
import time

urllib3.disable_warnings()

BASE_URL = "https://localhost:5000"

def test_size(size_bytes, description):
    """Test a specific payload size"""
    print(f"\n🧪 Testing {description} ({size_bytes} bytes)...", end="", flush=True)
    
    session = requests.Session()
    session.verify = False
    
    try:
        # Login first
        response = session.post(f"{BASE_URL}/api/auth/login", json={
            "username": "admin",
            "password": "secure123456789"
        }, timeout=10)
        
        if response.status_code != 200:
            print(f" ❌ Login failed")
            return False
            
        token = response.json()["token"]
        session.headers["Authorization"] = f"Bearer {token}"
        
        # Create content of exact size
        # Account for JSON overhead
        json_overhead = len(json.dumps({"title": "Test", "content": ""}))
        content_size = max(1, size_bytes - json_overhead)
        content = "x" * content_size
        
        doc = {
            "title": "Test",
            "content": content
        }
        
        # Verify actual size
        actual_size = len(json.dumps(doc))
        
        response = session.post(f"{BASE_URL}/api/documents", json=doc, timeout=30)
        
        if response.status_code == 201:
            print(f" ✅ Success (actual: {actual_size} bytes)")
            # Clean up
            doc_id = response.json()["uuid"]
            session.delete(f"{BASE_URL}/api/documents/{doc_id}")
            return True
        elif response.status_code == 413:
            print(f" ⚠️ Too large (413)")
            return True  # Server handled it properly
        else:
            print(f" ❌ Failed: {response.status_code}")
            return False
            
    except requests.exceptions.ConnectionError as e:
        print(f" ❌ Connection error: {str(e)[:50]}...")
        return False
    except Exception as e:
        print(f" ❌ Error: {type(e).__name__}: {str(e)[:50]}...")
        return False

def main():
    print("Incremental Payload Size Testing")
    print("=" * 50)
    
    # Test sizes from 1KB to 10KB in 1KB increments
    test_sizes = [
        (1024, "1KB"),
        (2048, "2KB"),
        (3072, "3KB"),
        (4096, "4KB"),
        (5120, "5KB"),
        (6144, "6KB"),
        (7168, "7KB"),
        (8192, "8KB"),
        (9216, "9KB"),
        (10240, "10KB"),
    ]
    
    last_success = 0
    first_failure = None
    
    for size, desc in test_sizes:
        success = test_size(size, desc)
        if success:
            last_success = size
        elif first_failure is None:
            first_failure = size
            # Test in between to narrow down
            print(f"\n📍 Narrowing down between {last_success} and {first_failure} bytes...")
            
            # Binary search for exact failure point
            low = last_success
            high = first_failure
            
            while high - low > 100:
                mid = (low + high) // 2
                if test_size(mid, f"{mid} bytes"):
                    low = mid
                else:
                    high = mid
            
            print(f"\n🎯 Failure occurs between {low} and {high} bytes")
            break
    
    # Final health check
    print("\n🏥 Server health check...", end="", flush=True)
    try:
        session = requests.Session()
        session.verify = False
        response = session.get(f"{BASE_URL}/api/health", timeout=5)
        if response.status_code == 200:
            print(" ✅ Server is healthy")
        else:
            print(f" ❌ Server returned {response.status_code}")
    except Exception as e:
        print(f" ❌ Server not responding: {e}")

if __name__ == "__main__":
    main()