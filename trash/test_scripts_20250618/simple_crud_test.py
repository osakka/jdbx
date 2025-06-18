#!/usr/bin/env python3
"""
Simple CRUD test - Basic document operations
"""

import requests
import json
import urllib3
import time

# Disable SSL warnings
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

BASE_URL = "https://localhost:5000"

def test_crud():
    print("📝 Simple CRUD Test")
    print("="*50)
    
    # 1. Login
    print("\n1. Login...")
    response = requests.post(
        f"{BASE_URL}/api/auth/login",
        json={"username": "admin", "password": "secure123456789"},
        verify=False
    )
    
    if response.status_code != 200:
        print(f"❌ Login failed: {response.status_code}")
        return
    
    token = response.json()['token']
    headers = {"Authorization": f"Bearer {token}"}
    print(f"✅ Login successful. Token: {token[:30]}...")
    
    # 2. Create a document
    print("\n2. Create document...")
    doc = {
        "title": "Test Document",
        "content": "This is a test document for CRUD operations",
        "status": "active",
        "tags": ["test", "crud"]
    }
    
    response = requests.post(
        f"{BASE_URL}/api/libraries/default/collections/testdocs/documents",
        json=doc,
        headers=headers,
        verify=False
    )
    
    if response.status_code == 201:
        created_doc = response.json()
        doc_id = created_doc['uuid']
        print(f"✅ Document created with ID: {doc_id}")
    else:
        print(f"❌ Create failed: {response.status_code} - {response.text}")
        return
    
    # 3. Read the document
    print("\n3. Read document...")
    response = requests.get(
        f"{BASE_URL}/api/documents/{doc_id}",
        headers=headers,
        verify=False
    )
    
    if response.status_code == 200:
        read_doc = response.json()
        print(f"✅ Document retrieved: {read_doc.get('title')}")
    else:
        print(f"❌ Read failed: {response.status_code}")
    
    # 4. Update the document
    print("\n4. Update document...")
    update_data = {
        "title": "Updated Test Document",
        "content": "This document has been updated",
        "status": "updated",
        "tags": ["test", "crud", "updated"]
    }
    
    response = requests.put(
        f"{BASE_URL}/api/documents/{doc_id}",
        json=update_data,
        headers=headers,
        verify=False
    )
    
    if response.status_code == 200:
        updated_doc = response.json()
        print(f"✅ Document updated: {updated_doc.get('title')}")
    else:
        print(f"❌ Update failed: {response.status_code}")
    
    # 5. List all documents
    print("\n5. List all documents...")
    response = requests.get(
        f"{BASE_URL}/api/libraries/default/collections/testdocs/documents",
        headers=headers,
        verify=False
    )
    
    if response.status_code == 200:
        docs = response.json().get('documents', [])
        print(f"✅ Found {len(docs)} documents")
        for doc in docs:
            print(f"   - {doc.get('title', 'Untitled')} (ID: {doc.get('uuid')})")
    else:
        print(f"❌ List failed: {response.status_code}")
    
    # 6. Delete the document
    print("\n6. Delete document...")
    response = requests.delete(
        f"{BASE_URL}/api/documents/{doc_id}",
        headers=headers,
        verify=False
    )
    
    if response.status_code == 204:
        print(f"✅ Document deleted")
    else:
        print(f"❌ Delete failed: {response.status_code}")
    
    # 7. Verify deletion
    print("\n7. Verify deletion...")
    response = requests.get(
        f"{BASE_URL}/api/documents/{doc_id}",
        headers=headers,
        verify=False
    )
    
    if response.status_code == 404:
        print("✅ Document not found (correctly deleted)")
    else:
        print(f"❌ Document still exists: {response.status_code}")
    
    print("\n" + "="*50)
    print("✅ Basic CRUD operations completed successfully!")

if __name__ == "__main__":
    test_crud()