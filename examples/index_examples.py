#!/usr/bin/env python3

import requests
import json
import sys
from datetime import datetime

# Configuration
SERVER_URL = "http://localhost:5000"
USERNAME = "admin"
PASSWORD = "admin123"
TEST_COLLECTION = "python_index_test"

class IndexTest:
    def __init__(self):
        self.token = None
    
    def authenticate(self):
        """Authenticate and get a JWT token"""
        print("Authenticating...")
        response = requests.post(
            f"{SERVER_URL}/api/auth/login",
            json={"username": USERNAME, "password": PASSWORD}
        )
        
        if response.status_code != 200:
            print(f"Authentication failed: {response.text}")
            sys.exit(1)
        
        self.token = response.json()["token"]
        print("Authentication successful")
    
    def get_headers(self):
        """Get headers with authentication token"""
        return {
            "Authorization": f"Bearer {self.token}",
            "Content-Type": "application/json"
        }
    
    def create_collection(self):
        """Create a test collection"""
        print(f"Creating collection: {TEST_COLLECTION}")
        response = requests.post(
            f"{SERVER_URL}/api/collections",
            headers=self.get_headers(),
            json={"name": TEST_COLLECTION}
        )
        
        if response.status_code not in [200, 201]:
            # Check if it's because collection already exists
            if "already exists" in response.text:
                print("Collection already exists, continuing...")
            else:
                print(f"Failed to create collection: {response.text}")
                sys.exit(1)
        else:
            print("Collection created successfully")
    
    def add_test_data(self):
        """Add test documents to the collection"""
        print("Adding test documents...")
        
        # Sample test documents
        documents = [
            {
                "name": "John Doe",
                "email": "john.doe@example.com",
                "age": 30,
                "status": "active",
                "role": "admin",
                "created_at": datetime.now().isoformat()
            },
            {
                "name": "Jane Smith",
                "email": "jane.smith@example.com",
                "age": 25,
                "status": "active",
                "role": "user",
                "created_at": datetime.now().isoformat()
            },
            {
                "name": "Bob Johnson",
                "email": "bob.johnson@example.com",
                "age": 42,
                "status": "inactive",
                "role": "user",
                "created_at": datetime.now().isoformat()
            },
            {
                "name": "Alice Brown",
                "email": "alice.brown@example.com",
                "age": 35,
                "status": "active",
                "role": "admin",
                "created_at": datetime.now().isoformat()
            },
            {
                "name": "Charlie Wilson",
                "email": "charlie.wilson@example.com",
                "age": 28,
                "status": "pending",
                "role": "user",
                "created_at": datetime.now().isoformat()
            }
        ]
        
        # Add each document
        for doc in documents:
            response = requests.post(
                f"{SERVER_URL}/api/collections/{TEST_COLLECTION}",
                headers=self.get_headers(),
                json=doc
            )
            
            if response.status_code not in [200, 201]:
                print(f"Failed to add document: {response.text}")
            
        print("Added 5 test documents")
    
    def create_indexes(self):
        """Create indexes on the collection"""
        print("Creating indexes...")
        
        # Define indexes to create
        indexes = [
            {"name": "email_idx", "field": "email", "type": "unique"},
            {"name": "age_idx", "field": "age", "type": "non_unique"},
            {"name": "status_idx", "field": "status", "type": "non_unique"},
            {"name": "role_idx", "field": "role", "type": "non_unique"}
        ]
        
        # Create each index
        for idx in indexes:
            response = requests.post(
                f"{SERVER_URL}/api/indexes/{TEST_COLLECTION}",
                headers=self.get_headers(),
                json=idx
            )
            
            if response.status_code not in [200, 201]:
                print(f"Failed to create {idx['name']} index: {response.text}")
            else:
                print(f"Created {idx['name']} index successfully")
    
    def list_indexes(self):
        """List all indexes for the collection"""
        print("\nListing indexes...")
        
        response = requests.get(
            f"{SERVER_URL}/api/indexes/{TEST_COLLECTION}",
            headers=self.get_headers()
        )
        
        if response.status_code != 200:
            print(f"Failed to list indexes: {response.text}")
        else:
            print(json.dumps(response.json(), indent=2))
    
    def query_by_index(self):
        """Perform queries using indexes"""
        print("\nQuerying by email index...")
        
        # Query by email
        response = requests.post(
            f"{SERVER_URL}/api/indexes/query/{TEST_COLLECTION}",
            headers=self.get_headers(),
            json={
                "field": "email",
                "value": "john.doe@example.com"
            }
        )
        
        if response.status_code != 200:
            print(f"Failed to query by email: {response.text}")
        else:
            print(json.dumps(response.json(), indent=2))
        
        print("\nQuerying by age index...")
        
        # Query by age
        response = requests.post(
            f"{SERVER_URL}/api/indexes/query/{TEST_COLLECTION}",
            headers=self.get_headers(),
            json={
                "field": "age",
                "value": 30
            }
        )
        
        if response.status_code != 200:
            print(f"Failed to query by age: {response.text}")
        else:
            print(json.dumps(response.json(), indent=2))
    
    def compound_query(self):
        """Perform a compound query"""
        print("\nPerforming compound query (role=admin AND status=active)...")
        
        response = requests.post(
            f"{SERVER_URL}/api/indexes/compound/{TEST_COLLECTION}",
            headers=self.get_headers(),
            json={
                "operation": "AND",
                "queries": [
                    {
                        "field": "role",
                        "value": "admin"
                    },
                    {
                        "field": "status",
                        "value": "active"
                    }
                ]
            }
        )
        
        if response.status_code != 200:
            print(f"Failed to perform compound query: {response.text}")
        else:
            print(json.dumps(response.json(), indent=2))
    
    def get_index_stats(self):
        """Get statistics for an index"""
        print("\nGetting statistics for email index...")
        
        response = requests.get(
            f"{SERVER_URL}/api/indexes/stats/{TEST_COLLECTION}/email_idx",
            headers=self.get_headers()
        )
        
        if response.status_code != 200:
            print(f"Failed to get index statistics: {response.text}")
        else:
            print(json.dumps(response.json(), indent=2))
    
    def rebuild_index(self):
        """Rebuild an index"""
        print("\nRebuilding email index...")
        
        response = requests.post(
            f"{SERVER_URL}/api/indexes/rebuild/{TEST_COLLECTION}/email_idx",
            headers=self.get_headers()
        )
        
        if response.status_code != 200:
            print(f"Failed to rebuild index: {response.text}")
        else:
            print(json.dumps(response.json(), indent=2))

    def run_tests(self):
        """Run all tests"""
        self.authenticate()
        self.create_collection()
        self.add_test_data()
        self.create_indexes()
        self.list_indexes()
        self.query_by_index()
        self.compound_query()
        self.get_index_stats()
        self.rebuild_index()
        print("\nAll tests completed successfully!")

if __name__ == "__main__":
    test = IndexTest()
    test.run_tests()