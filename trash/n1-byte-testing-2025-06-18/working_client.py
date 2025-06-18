#!/usr/bin/env python3
"""
Working JDBX Python client that properly handles OpenSSL 3.x
Based on research findings about SSL_OP_IGNORE_UNEXPECTED_EOF
"""

import requests
from requests.adapters import HTTPAdapter
from urllib3.poolmanager import PoolManager
import ssl
import json
import urllib3

# Disable SSL warnings
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

class OpenSSL3Adapter(HTTPAdapter):
    """Custom adapter for OpenSSL 3.x compatibility"""
    
    def init_poolmanager(self, *args, **kwargs):
        ctx = ssl.create_default_context()
        ctx.check_hostname = False
        ctx.verify_mode = ssl.CERT_NONE
        
        # Set legacy options for OpenSSL 3.x
        ctx.options |= 0x4  # SSL_OP_LEGACY_SERVER_CONNECT
        ctx.set_ciphers('DEFAULT@SECLEVEL=1')
        
        kwargs['ssl_context'] = ctx
        return super().init_poolmanager(*args, **kwargs)

class JDBXClient:
    def __init__(self, base_url="https://localhost:5000"):
        self.base_url = base_url
        self.session = requests.Session()
        
        # Use custom adapter for OpenSSL 3.x
        self.session.mount('https://', OpenSSL3Adapter())
        self.session.verify = False
        
        # Always use Connection: close to avoid EOF issues
        self.session.headers.update({
            'Connection': 'close',
            'Accept': 'application/json'
        })
        
        self.token = None
    
    def login(self, username, password):
        """Authenticate with JDBX"""
        try:
            response = self.session.post(
                f"{self.base_url}/api/auth/login",
                json={"username": username, "password": password},
                timeout=5
            )
            
            if response.status_code == 200:
                data = response.json()
                self.token = data.get("token")
                self.session.headers["Authorization"] = f"Bearer {self.token}"
                return True
            else:
                print(f"Login failed: {response.status_code} - {response.text}")
                return False
        except Exception as e:
            print(f"Login error: {e}")
            return False
    
    def create_document(self, document):
        """Create a document"""
        try:
            response = self.session.post(
                f"{self.base_url}/api/documents",
                json=document,
                timeout=5
            )
            
            if response.status_code == 201:
                return response.json()
            else:
                print(f"Create failed: {response.status_code} - {response.text}")
                return None
        except Exception as e:
            print(f"Create error: {e}")
            return None
    
    def query_documents(self, query):
        """Query documents"""
        try:
            response = self.session.post(
                f"{self.base_url}/api/documents/query",
                json=query,
                timeout=5
            )
            
            if response.status_code == 200:
                return response.json()
            else:
                print(f"Query failed: {response.status_code} - {response.text}")
                return None
        except Exception as e:
            print(f"Query error: {e}")
            return None
    
    def update_document(self, doc_id, updates):
        """Update a document"""
        try:
            response = self.session.put(
                f"{self.base_url}/api/documents/{doc_id}",
                json=updates,
                timeout=5
            )
            
            if response.status_code == 200:
                return response.json()
            else:
                print(f"Update failed: {response.status_code} - {response.text}")
                return None
        except Exception as e:
            print(f"Update error: {e}")
            return None

def main():
    """Test JDBX with OpenSSL 3.x workarounds"""
    print("🧪 JDBX Client with OpenSSL 3.x Workarounds\n")
    
    client = JDBXClient()
    
    # Test 1: Login
    print("1. Testing login...")
    if not client.login("admin", "secure123456789"):
        print("   ❌ Login failed - trying raw request for debugging")
        
        # Debug with raw request
        import subprocess
        result = subprocess.run([
            'curl', '-k', '-s', '-X', 'POST',
            'https://localhost:5000/api/auth/login',
            '-H', 'Content-Type: application/json',
            '-H', 'Connection: close',
            '-d', '{"username":"admin","password":"secure123456789"}'
        ], capture_output=True, text=True)
        
        print(f"   curl result: {result.stdout}")
        return
    
    print("   ✅ Login successful")
    
    # Test 2: Create documents
    print("\n2. Creating documents...")
    
    # Small document
    doc1 = client.create_document({
        "title": "OpenSSL 3.x Test",
        "type": "test",
        "description": "Testing with workarounds"
    })
    
    if doc1:
        print(f"   ✅ Created: {doc1['uuid']}")
    else:
        print("   ❌ Failed to create document")
    
    # Test 3: Query
    print("\n3. Querying documents...")
    result = client.query_documents({"type": "test"})
    
    if result:
        docs = result.get("documents", [])
        print(f"   ✅ Found {len(docs)} documents")
        for doc in docs[:3]:
            print(f"      - {doc.get('title', 'Untitled')} ({doc['uuid']})")
    else:
        print("   ❌ Query failed")
    
    print("\n✅ Testing complete")

if __name__ == "__main__":
    main()