#!/usr/bin/env python3
"""Simple test for JDBX server checkpoint system"""

import requests
import json
import time
import urllib3
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

# Server configuration
BASE_URL = "https://localhost:5000"
ADMIN_USER = "admin"
ADMIN_PASS = "secure123456789"

def test_basic_operations():
    """Test basic CRUD operations"""
    print("Testing JDBX Checkpoint System")
    print("==============================\n")
    
    # 1. Login
    print("1. Testing login...")
    try:
        response = requests.post(
            f"{BASE_URL}/api/auth/login",
            json={"username": ADMIN_USER, "password": ADMIN_PASS},
            headers={"Content-Type": "application/json"},
            verify=False
        )
        response.raise_for_status()
        token = response.json()["token"]
        print("   ✅ Login successful")
    except Exception as e:
        print(f"   ❌ Login failed: {e}")
        return
    
    headers = {
        "Authorization": f"Bearer {token}",
        "Content-Type": "application/json"
    }
    
    # 2. Create document
    print("\n2. Testing document creation...")
    doc = {
        "title": "Test Document",
        "content": "This is a test document for checkpoint system",
        "tags": ["test", "checkpoint"]
    }
    
    try:
        response = requests.post(
            f"{BASE_URL}/api/documents",
            json=doc,
            headers=headers,
            verify=False
        )
        response.raise_for_status()
        created_doc = response.json()
        doc_id = created_doc.get("uuid", created_doc.get("id"))
        print(f"   ✅ Document created: {doc_id}")
    except Exception as e:
        print(f"   ❌ Create failed: {e}")
        return
    
    # 3. Read document
    print("\n3. Testing document read...")
    try:
        response = requests.get(
            f"{BASE_URL}/api/documents/{doc_id}",
            headers=headers,
            verify=False
        )
        response.raise_for_status()
        print(f"   ✅ Document read successful")
    except Exception as e:
        print(f"   ❌ Read failed: {e}")
    
    # 4. Update document
    print("\n4. Testing document update...")
    doc["content"] = "Updated content"
    doc["updated"] = True
    
    try:
        response = requests.put(
            f"{BASE_URL}/api/documents/{doc_id}",
            json=doc,
            headers=headers,
            verify=False
        )
        response.raise_for_status()
        print(f"   ✅ Document updated")
    except Exception as e:
        print(f"   ❌ Update failed: {e}")
    
    # 5. Delete document
    print("\n5. Testing document delete...")
    try:
        response = requests.delete(
            f"{BASE_URL}/api/documents/{doc_id}",
            headers=headers,
            verify=False
        )
        response.raise_for_status()
        print(f"   ✅ Document deleted")
    except Exception as e:
        print(f"   ❌ Delete failed: {e}")
    
    # 6. List documents
    print("\n6. Testing document list...")
    try:
        response = requests.get(
            f"{BASE_URL}/api/documents",
            headers=headers,
            verify=False
        )
        response.raise_for_status()
        docs = response.json().get("documents", [])
        print(f"   ✅ Listed {len(docs)} documents")
    except Exception as e:
        print(f"   ❌ List failed: {e}")
    
    print("\n✅ All tests completed!")

if __name__ == "__main__":
    test_basic_operations()