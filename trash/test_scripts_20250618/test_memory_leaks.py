#!/usr/bin/env python3
"""
Test script to verify memory leak fixes in database.c
Tests multiple operations that previously leaked memory from skiplist_search
"""

import requests
import json
import time

BASE_URL = "http://localhost:5000"

def login():
    """Login and get JWT token"""
    response = requests.post(f"{BASE_URL}/api/auth/login", json={
        "username": "admin",
        "password": "secure123456789"
    })
    return response.json().get("token")

def test_document_operations(token):
    """Test document CRUD operations that use skiplist_search"""
    headers = {"Authorization": f"Bearer {token}"}
    
    # Create multiple documents
    print("Creating documents...")
    doc_ids = []
    for i in range(10):
        doc = {
            "name": f"Test Doc {i}",
            "value": f"Value {i}",
            "type": "test"
        }
        response = requests.post(f"{BASE_URL}/api/documents", 
                               json=doc, headers=headers)
        if response.status_code == 200:
            doc_ids.append(response.json()["uuid"])
            print(f"  Created document {i+1}/10")
    
    # Update documents multiple times (tests db_update_document leak)
    print("\nUpdating documents...")
    for i, doc_id in enumerate(doc_ids):
        for j in range(5):  # Update each doc 5 times
            update = {"value": f"Updated Value {i}-{j}"}
            response = requests.put(f"{BASE_URL}/api/documents/{doc_id}",
                                  json=update, headers=headers)
            if response.status_code == 200:
                print(f"  Updated document {i+1}/10, iteration {j+1}/5")
    
    # Get documents (tests storage_get_document)
    print("\nReading documents...")
    for i, doc_id in enumerate(doc_ids):
        response = requests.get(f"{BASE_URL}/api/documents/{doc_id}",
                               headers=headers)
        if response.status_code == 200:
            print(f"  Read document {i+1}/10")
    
    # Delete documents (tests db_delete_document leak)
    print("\nDeleting documents...")
    for i, doc_id in enumerate(doc_ids):
        response = requests.delete(f"{BASE_URL}/api/documents/{doc_id}",
                                  headers=headers)
        if response.status_code == 200:
            print(f"  Deleted document {i+1}/10")

def main():
    print("Memory Leak Test for database.c skiplist_search fixes")
    print("=" * 50)
    
    # Login
    print("Logging in...")
    token = login()
    if not token:
        print("Failed to login!")
        return
    
    print("Login successful!")
    
    # Run multiple iterations to amplify any memory leaks
    for iteration in range(3):
        print(f"\n--- Iteration {iteration + 1}/3 ---")
        test_document_operations(token)
        time.sleep(1)  # Brief pause between iterations
    
    print("\n" + "=" * 50)
    print("Test completed!")
    print("Check server memory usage to verify no significant growth.")
    print("Previously, this would have leaked memory on each skiplist_search call.")

if __name__ == "__main__":
    main()