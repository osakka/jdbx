#!/usr/bin/env python3
"""
Debug CRUD operations
"""

import requests
import json
import urllib3

urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

BASE_URL = "https://localhost:5000"

# Login
response = requests.post(
    f"{BASE_URL}/api/auth/login",
    json={"username": "admin", "password": "secure123456789"},
    verify=False
)
token = response.json()['token']
headers = {"Authorization": f"Bearer {token}"}

# Create
print("Creating document...")
doc = {"title": "Debug Test", "content": "Testing"}
response = requests.post(
    f"{BASE_URL}/api/libraries/default/collections/test/documents",
    json=doc,
    headers=headers,
    verify=False
)
print(f"Create: {response.status_code}")
created = response.json()
doc_id = created['uuid']
print(f"Created doc: {json.dumps(created, indent=2)}")

# Read
print(f"\nReading document {doc_id}...")
response = requests.get(
    f"{BASE_URL}/api/documents/{doc_id}",
    headers=headers,
    verify=False
)
print(f"Read: {response.status_code}")
if response.status_code == 200:
    print(f"Read doc: {json.dumps(response.json(), indent=2)}")

# Update - try to preserve all fields
print(f"\nUpdating document {doc_id}...")
# First get the current document
current_doc = response.json()
# Update specific fields
current_doc['title'] = "Updated Debug Test"
current_doc['content'] = "Updated content"

response = requests.put(
    f"{BASE_URL}/api/documents/{doc_id}",
    json=current_doc,
    headers=headers,
    verify=False
)
print(f"Update: {response.status_code}")
print(f"Update response: {response.text[:200]}")

# Delete
print(f"\nDeleting document {doc_id}...")
response = requests.delete(
    f"{BASE_URL}/api/documents/{doc_id}",
    headers=headers,
    verify=False
)
print(f"Delete: {response.status_code}")
print(f"Delete response: {response.text}")