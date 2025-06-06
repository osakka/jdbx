#!/usr/bin/env python3
"""
JSON Database Client Example
===========================

A simple Python client that demonstrates how to interact with the JSON database server API.
This example shows the following operations:
- Authentication (login/register)
- Collection management (create/list)
- Document operations (create/read/update/delete)
- Basic error handling
"""

import requests
import json
import sys

# Server configuration
BASE_URL = "http://localhost:5000"
AUTH_TOKEN = None


def print_section(title):
    """Print a section title."""
    print("\n" + "=" * 50)
    print(f" {title}")
    print("=" * 50)


def authenticate(username="admin", password="admin", register=False):
    """Authenticate with the server."""
    global AUTH_TOKEN
    
    print_section("Authentication")
    
    endpoint = "/api/auth/register" if register else "/api/auth/login"
    url = BASE_URL + endpoint
    
    data = {
        "username": username,
        "password": password
    }
    
    try:
        response = requests.post(url, json=data)
        if response.status_code == 200 or response.status_code == 201:
            result = response.json()
            AUTH_TOKEN = result.get("token")
            print(f"Authentication successful!")
            print(f"User ID: {result.get('user_id')}")
            print(f"Username: {result.get('username')}")
            if AUTH_TOKEN:
                print(f"Token received: {AUTH_TOKEN[:10]}...")
            else:
                print("Warning: No token received")
            return True
        else:
            print(f"Authentication failed: {response.status_code}")
            print(response.text)
            return False
    except Exception as e:
        print(f"Error during authentication: {str(e)}")
        return False


def get_headers():
    """Get headers with authorization token."""
    headers = {"Content-Type": "application/json"}
    if AUTH_TOKEN:
        headers["Authorization"] = f"Bearer {AUTH_TOKEN}"
    return headers


def create_collection(name):
    """Create a new collection."""
    print_section(f"Creating Collection '{name}'")
    
    url = BASE_URL + "/api/collections"
    
    data = {
        "name": name
    }
    
    try:
        response = requests.post(url, headers=get_headers(), json=data)
        if response.status_code == 201:
            result = response.json()
            print(f"Collection created: {result}")
            return True
        else:
            print(f"Failed to create collection: {response.status_code}")
            print(response.text)
            return False
    except Exception as e:
        print(f"Error creating collection: {str(e)}")
        return False


def list_collections():
    """List all collections."""
    print_section("Listing Collections")
    
    url = BASE_URL + "/api/collections"
    
    try:
        response = requests.get(url, headers=get_headers())
        if response.status_code == 200:
            result = response.json()
            collections = result.get("collections", [])
            print(f"Found {len(collections)} collections:")
            for idx, collection in enumerate(collections, 1):
                print(f"  {idx}. {collection}")
            return collections
        else:
            print(f"Failed to list collections: {response.status_code}")
            print(response.text)
            return []
    except Exception as e:
        print(f"Error listing collections: {str(e)}")
        return []


def create_document(collection_name, document):
    """Create a new document in a collection."""
    print_section(f"Creating Document in '{collection_name}'")
    
    url = f"{BASE_URL}/api/collections/{collection_name}/documents"
    
    try:
        response = requests.post(url, headers=get_headers(), json=document)
        if response.status_code == 201:
            result = response.json()
            print(f"Document created with ID: {result.get('_id')}")
            return result.get('_id')
        else:
            print(f"Failed to create document: {response.status_code}")
            print(response.text)
            return None
    except Exception as e:
        print(f"Error creating document: {str(e)}")
        return None


def get_document(collection_name, document_id):
    """Get a document by ID."""
    print_section(f"Getting Document from '{collection_name}'")
    
    url = f"{BASE_URL}/api/collections/{collection_name}/documents/{document_id}"
    
    try:
        response = requests.get(url, headers=get_headers())
        if response.status_code == 200:
            document = response.json()
            print(f"Document retrieved:")
            print(json.dumps(document, indent=2))
            return document
        else:
            print(f"Failed to get document: {response.status_code}")
            print(response.text)
            return None
    except Exception as e:
        print(f"Error getting document: {str(e)}")
        return None


def update_document(collection_name, document_id, document):
    """Update a document by ID."""
    print_section(f"Updating Document in '{collection_name}'")
    
    url = f"{BASE_URL}/api/collections/{collection_name}/documents/{document_id}"
    
    try:
        response = requests.put(url, headers=get_headers(), json=document)
        if response.status_code == 200:
            result = response.json()
            print(f"Document updated: {result}")
            return True
        else:
            print(f"Failed to update document: {response.status_code}")
            print(response.text)
            return False
    except Exception as e:
        print(f"Error updating document: {str(e)}")
        return False


def query_documents(collection_name, query=None):
    """Query documents in a collection."""
    print_section(f"Querying Documents in '{collection_name}'")
    
    url = f"{BASE_URL}/api/collections/{collection_name}/documents"
    
    params = {}
    if query:
        params["query"] = json.dumps(query)
    
    try:
        response = requests.get(url, headers=get_headers(), params=params)
        if response.status_code == 200:
            result = response.json()
            documents = result.get("documents", [])
            print(f"Found {len(documents)} documents:")
            for idx, doc in enumerate(documents, 1):
                print(f"  {idx}. {doc.get('_id')}: {doc}")
            return documents
        else:
            print(f"Failed to query documents: {response.status_code}")
            print(response.text)
            return []
    except Exception as e:
        print(f"Error querying documents: {str(e)}")
        return []


def delete_document(collection_name, document_id):
    """Delete a document by ID."""
    print_section(f"Deleting Document from '{collection_name}'")
    
    url = f"{BASE_URL}/api/collections/{collection_name}/documents/{document_id}"
    
    try:
        response = requests.delete(url, headers=get_headers())
        if response.status_code == 204:
            print(f"Document deleted successfully")
            return True
        else:
            print(f"Failed to delete document: {response.status_code}")
            print(response.text)
            return False
    except Exception as e:
        print(f"Error deleting document: {str(e)}")
        return False


def main():
    """Run the client example."""
    print_section("JSON Database Client Example")
    
    # Check if server is running
    try:
        requests.get(BASE_URL, timeout=2)
    except requests.exceptions.ConnectionError:
        print(f"Error: Cannot connect to server at {BASE_URL}")
        print("Make sure the server is running and the URL is correct.")
        sys.exit(1)
    
    # Try to log in with default credentials
    if not authenticate():
        print("Trying to register with default credentials...")
        if not authenticate(register=True):
            print("Authentication failed. Exiting.")
            sys.exit(1)
    
    # Create a test collection
    collection_name = "test_collection"
    create_collection(collection_name)
    
    # List collections
    list_collections()
    
    # Create a test document
    test_document = {
        "name": "Test Document",
        "value": 42,
        "tags": ["test", "example", "json"],
        "nested": {
            "property": "This is a nested property"
        }
    }
    
    doc_id = create_document(collection_name, test_document)
    if doc_id:
        # Get the document
        document = get_document(collection_name, doc_id)
        
        # Update the document
        if document:
            document["value"] = 100
            document["tags"].append("updated")
            update_document(collection_name, doc_id, document)
        
        # Query documents
        query_documents(collection_name)
        
        # Delete the document
        delete_document(collection_name, doc_id)
        
        # Verify deletion
        query_documents(collection_name)
    
    print_section("Example Completed")


if __name__ == "__main__":
    main()