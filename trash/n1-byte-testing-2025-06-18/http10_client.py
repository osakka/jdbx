#!/usr/bin/env python3
"""
JDBX client using HTTP/1.0 to avoid chunked encoding and EOF issues
"""

import socket
import ssl
import json
import base64

def http10_request(host, port, method, path, headers=None, body=None):
    """Make HTTP/1.0 request with SSL"""
    
    # Create socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    
    # Wrap with SSL
    context = ssl.create_default_context()
    context.check_hostname = False
    context.verify_mode = ssl.CERT_NONE
    
    ssl_sock = context.wrap_socket(sock, server_hostname=host)
    
    try:
        # Connect
        ssl_sock.connect((host, port))
        
        # Build request
        request = f"{method} {path} HTTP/1.0\r\n"
        request += f"Host: {host}:{port}\r\n"
        
        if headers:
            for key, value in headers.items():
                request += f"{key}: {value}\r\n"
        
        if body:
            request += f"Content-Length: {len(body)}\r\n"
            request += f"Content-Type: application/json\r\n"
        
        request += "\r\n"
        
        if body:
            request += body
        
        # Send request
        ssl_sock.sendall(request.encode())
        
        # Read response
        response = b""
        while True:
            data = ssl_sock.recv(4096)
            if not data:
                break
            response += data
        
        # Parse response
        parts = response.decode('utf-8', errors='ignore').split('\r\n\r\n', 1)
        headers = parts[0]
        body = parts[1] if len(parts) > 1 else ""
        
        # Get status code
        status_line = headers.split('\r\n')[0]
        status_code = int(status_line.split()[1])
        
        return status_code, body
        
    finally:
        ssl_sock.close()

class JDBXHTTP10Client:
    def __init__(self, host="localhost", port=5000):
        self.host = host
        self.port = port
        self.token = None
    
    def login(self, username, password):
        """Login using HTTP/1.0"""
        body = json.dumps({"username": username, "password": password})
        
        status, response = http10_request(
            self.host, self.port,
            "POST", "/api/auth/login",
            body=body
        )
        
        if status == 200:
            data = json.loads(response)
            self.token = data["token"]
            print(f"   ✅ Login successful (HTTP/1.0)")
            return True
        else:
            print(f"   ❌ Login failed: {status} - {response}")
            return False
    
    def create_document(self, doc):
        """Create document using HTTP/1.0"""
        if not self.token:
            return None
        
        body = json.dumps(doc)
        headers = {"Authorization": f"Bearer {self.token}"}
        
        status, response = http10_request(
            self.host, self.port,
            "POST", "/api/documents",
            headers=headers,
            body=body
        )
        
        if status == 201:
            return json.loads(response)
        else:
            print(f"   Create failed: {status} - {response}")
            return None
    
    def query_documents(self, query):
        """Query documents using HTTP/1.0"""
        if not self.token:
            return None
        
        body = json.dumps(query)
        headers = {"Authorization": f"Bearer {self.token}"}
        
        status, response = http10_request(
            self.host, self.port,
            "POST", "/api/documents/query",
            headers=headers,
            body=body
        )
        
        if status == 200:
            return json.loads(response)
        else:
            print(f"   Query failed: {status} - {response}")
            return None

def main():
    """Test with HTTP/1.0"""
    print("🧪 JDBX HTTP/1.0 Client Test\n")
    
    client = JDBXHTTP10Client()
    
    # Test 1: Login
    print("1. Testing login with HTTP/1.0...")
    if not client.login("admin", "secure123456789"):
        return
    
    # Test 2: Create
    print("\n2. Creating document...")
    doc = client.create_document({
        "title": "HTTP/1.0 Test",
        "type": "test",
        "description": "Created with HTTP/1.0"
    })
    
    if doc:
        print(f"   ✅ Created: {doc['uuid']}")
    
    # Test 3: Query
    print("\n3. Querying documents...")
    result = client.query_documents({"type": "test"})
    
    if result:
        docs = result.get("documents", [])
        print(f"   ✅ Found {len(docs)} documents")
        for doc in docs[:3]:
            print(f"      - {doc.get('title', 'Untitled')}")

if __name__ == "__main__":
    main()