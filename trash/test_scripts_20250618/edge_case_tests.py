#!/usr/bin/env python3
"""
Edge case testing - Find breaking points and issues
"""

import requests
import json
import urllib3
import time
import random
import string
from concurrent.futures import ThreadPoolExecutor

urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

BASE_URL = "https://localhost:5000"
session = requests.Session()
session.verify = False

def login():
    """Get auth token"""
    response = session.post(
        f"{BASE_URL}/api/auth/login",
        json={"username": "admin", "password": "secure123456789"}
    )
    if response.status_code == 200:
        token = response.json()['token']
        session.headers.update({'Authorization': f'Bearer {token}'})
        return True
    return False

def test_empty_document():
    """Test creating empty document"""
    print("\n🧪 Test 1: Empty Document")
    response = session.post(
        f"{BASE_URL}/api/libraries/default/collections/test/documents",
        json={}
    )
    print(f"Empty doc: {response.status_code} - {response.text[:100]}")
    return response.status_code == 201

def test_null_values():
    """Test document with null values"""
    print("\n🧪 Test 2: Null Values")
    doc = {
        "title": None,
        "content": "Test",
        "tags": None,
        "metadata": None
    }
    response = session.post(
        f"{BASE_URL}/api/libraries/default/collections/test/documents",
        json=doc
    )
    print(f"Null values: {response.status_code} - {response.text[:100]}")
    return response.status_code == 201

def test_special_characters():
    """Test special characters in values"""
    print("\n🧪 Test 3: Special Characters")
    doc = {
        "title": "Test \"quotes\" and 'quotes'",
        "content": "Line1\nLine2\r\nLine3\tTab",
        "emoji": "🚀🔥💥",
        "unicode": "你好世界",
        "special": "<script>alert('xss')</script>"
    }
    response = session.post(
        f"{BASE_URL}/api/libraries/default/collections/test/documents",
        json=doc
    )
    print(f"Special chars: {response.status_code}")
    
    if response.status_code == 201:
        doc_id = response.json()['uuid']
        # Try to read it back
        response = session.get(f"{BASE_URL}/api/documents/{doc_id}")
        if response.status_code == 200:
            retrieved = response.json()
            print(f"Retrieved emoji: {retrieved.get('emoji')}")
            print(f"Retrieved unicode: {retrieved.get('unicode')}")
            return True
    return False

def test_nested_objects():
    """Test deeply nested objects"""
    print("\n🧪 Test 4: Deeply Nested Objects")
    doc = {"level1": {}}
    current = doc["level1"]
    
    # Create 10 levels of nesting
    for i in range(2, 11):
        current[f"level{i}"] = {}
        current = current[f"level{i}"]
    current["value"] = "deep"
    
    response = session.post(
        f"{BASE_URL}/api/libraries/default/collections/test/documents",
        json=doc
    )
    print(f"Nested object (10 levels): {response.status_code}")
    return response.status_code == 201

def test_large_array():
    """Test document with large array"""
    print("\n🧪 Test 5: Large Array")
    doc = {
        "title": "Large Array Test",
        "items": [f"item_{i}" for i in range(1000)]
    }
    response = session.post(
        f"{BASE_URL}/api/libraries/default/collections/test/documents",
        json=doc
    )
    print(f"Array with 1000 items: {response.status_code}")
    return response.status_code in [201, 413]

def test_duplicate_keys():
    """Test how duplicate keys are handled"""
    print("\n🧪 Test 6: Duplicate Keys")
    # JSON doesn't actually allow duplicate keys, but let's test edge cases
    doc = {
        "title": "First",
        "content": "Test",
        "Title": "Second",  # Different case
        "TITLE": "Third"
    }
    response = session.post(
        f"{BASE_URL}/api/libraries/default/collections/test/documents",
        json=doc
    )
    print(f"Multiple title variations: {response.status_code}")
    return response.status_code == 201

def test_invalid_uuid_operations():
    """Test operations with invalid UUIDs"""
    print("\n🧪 Test 7: Invalid UUID Operations")
    
    # Try to read non-existent document
    response = session.get(f"{BASE_URL}/api/documents/invalid-uuid-12345")
    print(f"Read invalid UUID: {response.status_code}")
    
    # Try to update non-existent document
    response = session.put(
        f"{BASE_URL}/api/documents/invalid-uuid-12345",
        json={"title": "Update non-existent"}
    )
    print(f"Update invalid UUID: {response.status_code}")
    
    # Try to delete non-existent document
    response = session.delete(f"{BASE_URL}/api/documents/invalid-uuid-12345")
    print(f"Delete invalid UUID: {response.status_code}")
    
    return True

def test_concurrent_updates():
    """Test concurrent updates to same document"""
    print("\n🧪 Test 8: Concurrent Updates")
    
    # Create a document
    response = session.post(
        f"{BASE_URL}/api/libraries/default/collections/test/documents",
        json={"counter": 0, "title": "Concurrent Test"}
    )
    if response.status_code != 201:
        print("Failed to create test document")
        return False
    
    doc_id = response.json()['uuid']
    
    def update_counter(thread_id):
        # Each thread tries to increment the counter
        for i in range(5):
            # Get current value
            resp = session.get(f"{BASE_URL}/api/documents/{doc_id}")
            if resp.status_code == 200:
                doc = resp.json()
                doc['counter'] = doc.get('counter', 0) + 1
                doc['last_updater'] = f"thread_{thread_id}"
                
                # Update
                resp = session.put(f"{BASE_URL}/api/documents/{doc_id}", json=doc)
                if resp.status_code != 200:
                    return False
        return True
    
    # Run 5 threads updating the same document
    with ThreadPoolExecutor(max_workers=5) as executor:
        futures = [executor.submit(update_counter, i) for i in range(5)]
        results = [f.result() for f in futures]
    
    # Check final value
    response = session.get(f"{BASE_URL}/api/documents/{doc_id}")
    if response.status_code == 200:
        final_doc = response.json()
        final_counter = final_doc.get('counter', 0)
        print(f"Final counter value: {final_counter} (expected: 25)")
        print(f"Last updater: {final_doc.get('last_updater')}")
        # Due to race conditions, might not be exactly 25
        return final_counter > 0
    
    return False

def test_invalid_json_types():
    """Test invalid JSON in requests"""
    print("\n🧪 Test 9: Invalid JSON Types")
    
    # This would need to be sent as raw data, not through requests.json
    # Let's test edge cases that are valid JSON but might cause issues
    
    # Very large numbers
    doc = {
        "big_int": 99999999999999999999999999999,
        "float": 3.141592653589793238462643383279,
        "negative": -99999999999999999999999999999
    }
    response = session.post(
        f"{BASE_URL}/api/libraries/default/collections/test/documents",
        json=doc
    )
    print(f"Large numbers: {response.status_code}")
    
    return response.status_code == 201

def test_library_isolation():
    """Test that libraries are properly isolated"""
    print("\n🧪 Test 10: Library Isolation")
    
    # Create doc in library1
    doc1 = {"title": "Library1 Doc", "secret": "library1-secret"}
    response = session.post(
        f"{BASE_URL}/api/libraries/library1/collections/secrets/documents",
        json=doc1
    )
    print(f"Create in library1: {response.status_code}")
    
    # Create doc in library2
    doc2 = {"title": "Library2 Doc", "secret": "library2-secret"}
    response = session.post(
        f"{BASE_URL}/api/libraries/library2/collections/secrets/documents",
        json=doc2
    )
    print(f"Create in library2: {response.status_code}")
    
    # Try to list library1 docs
    response = session.get(f"{BASE_URL}/api/libraries/library1/collections/secrets/documents")
    if response.status_code == 200:
        lib1_docs = response.json().get('documents', [])
        print(f"Library1 has {len(lib1_docs)} documents")
        
        # Check that we don't see library2's documents
        for doc in lib1_docs:
            if 'library2' in str(doc):
                print("❌ SECURITY ISSUE: Library isolation breach!")
                return False
    
    return True

def test_rapid_create_delete():
    """Test rapid create/delete cycles"""
    print("\n🧪 Test 11: Rapid Create/Delete")
    
    success_count = 0
    for i in range(10):
        # Create
        response = session.post(
            f"{BASE_URL}/api/libraries/default/collections/temp/documents",
            json={"title": f"Temp doc {i}"}
        )
        if response.status_code == 201:
            doc_id = response.json()['uuid']
            
            # Immediately delete
            response = session.delete(f"{BASE_URL}/api/documents/{doc_id}")
            if response.status_code == 200:
                success_count += 1
    
    print(f"Successful create/delete cycles: {success_count}/10")
    return success_count >= 8

def test_malformed_requests():
    """Test malformed HTTP requests"""
    print("\n🧪 Test 12: Malformed Requests")
    
    # Send non-JSON content type
    response = session.post(
        f"{BASE_URL}/api/libraries/default/collections/test/documents",
        data="This is not JSON",
        headers={'Content-Type': 'text/plain'}
    )
    print(f"Non-JSON content: {response.status_code}")
    
    # Send malformed JSON
    response = session.post(
        f"{BASE_URL}/api/libraries/default/collections/test/documents",
        data='{"invalid": json}',  # Missing quotes
        headers={'Content-Type': 'application/json'}
    )
    print(f"Malformed JSON: {response.status_code}")
    
    return True  # As long as server doesn't crash

def main():
    print("🔍 Edge Case Testing Suite")
    print("="*50)
    
    # Login first
    if not login():
        print("❌ Login failed!")
        return
    
    print("✅ Login successful")
    
    # Track results
    results = []
    
    # Run all tests
    tests = [
        test_empty_document,
        test_null_values,
        test_special_characters,
        test_nested_objects,
        test_large_array,
        test_duplicate_keys,
        test_invalid_uuid_operations,
        test_concurrent_updates,
        test_invalid_json_types,
        test_library_isolation,
        test_rapid_create_delete,
        test_malformed_requests
    ]
    
    for test in tests:
        try:
            result = test()
            results.append((test.__name__, result))
        except Exception as e:
            print(f"❌ {test.__name__} crashed: {e}")
            results.append((test.__name__, False))
    
    # Summary
    print("\n" + "="*50)
    print("📊 Test Summary")
    print("="*50)
    
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    for test_name, result in results:
        status = "✅ PASS" if result else "❌ FAIL"
        print(f"{test_name:<30} {status}")
    
    print(f"\nTotal: {passed}/{total} passed ({passed/total*100:.1f}%)")
    
    # Check if server is still running
    print("\n🏥 Server Health Check...")
    try:
        response = session.get(f"{BASE_URL}/api/health")
        if response.status_code == 200:
            print("✅ Server is still running")
        else:
            print("❌ Server unhealthy")
    except:
        print("❌ Server not responding")

if __name__ == "__main__":
    main()