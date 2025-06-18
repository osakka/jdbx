#!/usr/bin/env python3
"""
Proper JDBX Python client that handles SSL correctly
"""

import http.client
import json
import ssl
import socket

class JDBXClient:
    def __init__(self, host="localhost", port=5000):
        self.host = host
        self.port = port
        self.token = None
        
    def _make_request(self, method, path, headers=None, body=None):
        """Make a proper HTTPS request with correct SSL handling"""
        # Create SSL context
        context = ssl.create_default_context()
        context.check_hostname = False
        context.verify_mode = ssl.CERT_NONE
        
        # Create connection
        conn = http.client.HTTPSConnection(self.host, self.port, context=context)
        
        # Prepare headers
        if headers is None:
            headers = {}
        
        if body is not None:
            headers["Content-Type"] = "application/json"
            headers["Content-Length"] = str(len(body))
            
        if self.token:
            headers["Authorization"] = f"Bearer {self.token}"
            
        headers["Connection"] = "close"
        
        try:
            # Send request
            conn.request(method, path, body, headers)
            
            # Get response
            response = conn.getresponse()
            data = response.read()
            
            return response.status, data.decode('utf-8')
        finally:
            conn.close()
    
    def login(self, username, password):
        """Authenticate and store token"""
        body = json.dumps({"username": username, "password": password})
        status, response = self._make_request("POST", "/api/auth/login", body=body)
        
        if status == 200:
            data = json.loads(response)
            self.token = data.get("token")
            return True
        else:
            print(f"Login failed: {status} - {response}")
            return False
    
    def create_document(self, doc):
        """Create a document"""
        body = json.dumps(doc)
        status, response = self._make_request("POST", "/api/documents", body=body)
        
        if status == 201:
            return json.loads(response)
        else:
            print(f"Create failed: {status} - {response}")
            return None
    
    def query_documents(self, query):
        """Query documents"""
        body = json.dumps(query)
        status, response = self._make_request("POST", "/api/documents/query", body=body)
        
        if status == 200:
            return json.loads(response)
        else:
            print(f"Query failed: {status} - {response}")
            return None

def main():
    print("🎯 Proper JDBX Client Test\n")
    
    client = JDBXClient()
    
    # Test 1: Login
    print("1. Login test...")
    if client.login("admin", "secure123456789"):
        print("   ✅ Login successful")
    else:
        print("   ❌ Login failed")
        return
    
    # Test 2: Create documents
    print("\n2. Creating documents...")
    
    # Small doc
    doc1 = client.create_document({
        "title": "Proper client test",
        "type": "test",
        "description": "Created with proper SSL handling"
    })
    if doc1:
        print(f"   ✅ Created: {doc1['uuid']}")
    
    # Large doc (10KB)
    doc2 = client.create_document({
        "title": "Large document test",
        "type": "test",
        "data": "x" * 10000
    })
    if doc2:
        print(f"   ✅ Created large doc: {doc2['uuid']}")
    
    # Test 3: Query
    print("\n3. Querying documents...")
    result = client.query_documents({"type": "test"})
    if result:
        docs = result.get("documents", [])
        print(f"   ✅ Found {len(docs)} documents")
        for doc in docs[:5]:  # Show first 5
            print(f"      - {doc['title']} ({doc['uuid']})")

if __name__ == "__main__":
    main()