#!/usr/bin/env python3
"""
Comprehensive JDBX testing using HTTP/1.0 to bypass SSL issues
Focus on finding real JDBX bugs
"""

import socket
import ssl
import json
import time
import threading
from concurrent.futures import ThreadPoolExecutor

def http10_request(host, port, method, path, headers=None, body=None):
    """Make HTTP/1.0 request with SSL"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    context = ssl.create_default_context()
    context.check_hostname = False
    context.verify_mode = ssl.CERT_NONE
    ssl_sock = context.wrap_socket(sock, server_hostname=host)
    
    try:
        ssl_sock.connect((host, port))
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
        
        ssl_sock.sendall(request.encode())
        
        response = b""
        while True:
            data = ssl_sock.recv(4096)
            if not data:
                break
            response += data
        
        parts = response.decode('utf-8', errors='ignore').split('\r\n\r\n', 1)
        headers = parts[0]
        body = parts[1] if len(parts) > 1 else ""
        
        status_line = headers.split('\r\n')[0]
        status_code = int(status_line.split()[1])
        
        return status_code, body
        
    finally:
        ssl_sock.close()

class JDBXTester:
    def __init__(self):
        self.host = "localhost"
        self.port = 5000
        self.token = None
        
    def api_call(self, method, path, body=None):
        """Make authenticated API call"""
        headers = {}
        if self.token:
            headers["Authorization"] = f"Bearer {self.token}"
        
        if body:
            body = json.dumps(body)
        
        return http10_request(self.host, self.port, method, path, headers, body)
    
    def login(self):
        """Authenticate"""
        status, response = self.api_call("POST", "/api/auth/login", 
                                        {"username": "admin", "password": "secure123456789"})
        if status == 200:
            self.token = json.loads(response)["token"]
            return True
        return False
    
    def test_large_documents(self):
        """Test large document handling"""
        print("\n📦 Testing Large Documents:")
        
        sizes = [1000, 5000, 10000, 50000, 100000]
        results = []
        
        for size in sizes:
            data = "x" * size
            doc = {
                "title": f"Large doc {size} bytes",
                "type": "large-test",
                "data": data
            }
            
            status, response = self.api_call("POST", "/api/documents", doc)
            
            if status == 201:
                try:
                    result = json.loads(response)
                    print(f"   ✅ {size:,} bytes: Created {result['uuid']}")
                    results.append((size, True))
                except:
                    print(f"   ❌ {size:,} bytes: Invalid JSON response")
                    results.append((size, False))
            else:
                print(f"   ❌ {size:,} bytes: HTTP {status}")
                results.append((size, False))
        
        return results
    
    def test_query_completeness(self):
        """Test if queries return all documents"""
        print("\n🔍 Testing Query Completeness:")
        
        # Create 10 test documents
        created_ids = []
        for i in range(10):
            doc = {
                "title": f"Query test {i}",
                "type": "query-completeness-test",
                "index": i
            }
            status, response = self.api_call("POST", "/api/documents", doc)
            if status == 201:
                created_ids.append(json.loads(response)["uuid"])
        
        print(f"   Created {len(created_ids)} documents")
        
        # Query them back
        status, response = self.api_call("POST", "/api/documents/query", 
                                        {"type": "query-completeness-test"})
        
        if status == 200:
            result = json.loads(response)
            found_docs = result.get("documents", [])
            print(f"   Found {len(found_docs)} documents (expected {len(created_ids)})")
            
            if len(found_docs) == len(created_ids):
                print("   ✅ Query returned all documents")
                return True
            else:
                print("   ❌ Query is missing documents")
                # Show which ones are missing
                found_ids = [doc["uuid"] for doc in found_docs]
                missing = [id for id in created_ids if id not in found_ids]
                print(f"   Missing: {missing[:3]}...")
                return False
        else:
            print(f"   ❌ Query failed: HTTP {status}")
            return False
    
    def test_concurrent_operations(self):
        """Test concurrent document creation"""
        print("\n🔀 Testing Concurrent Operations:")
        
        def create_doc(index):
            doc = {
                "title": f"Concurrent doc {index}",
                "type": "concurrent-test",
                "index": index
            }
            try:
                status, response = self.api_call("POST", "/api/documents", doc)
                return status == 201
            except:
                return False
        
        # Create 20 documents concurrently
        with ThreadPoolExecutor(max_workers=10) as executor:
            results = list(executor.map(create_doc, range(20)))
        
        success_count = sum(results)
        print(f"   Created {success_count}/20 documents successfully")
        
        if success_count == 20:
            print("   ✅ All concurrent creates succeeded")
            return True
        else:
            print(f"   ❌ {20 - success_count} concurrent creates failed")
            return False
    
    def test_api_endpoints(self):
        """Test various API endpoints"""
        print("\n🌐 Testing API Endpoints:")
        
        endpoints = [
            ("GET", "/api/libraries", None),
            ("GET", "/api/collections", None),
            ("GET", "/api/health", None),
            ("GET", "/api/status", None),
        ]
        
        results = []
        for method, path, body in endpoints:
            try:
                status, response = self.api_call(method, path, body)
                
                if status == 200:
                    try:
                        data = json.loads(response)
                        print(f"   ✅ {path}: Valid JSON response")
                        results.append(True)
                    except:
                        print(f"   ❌ {path}: Invalid JSON")
                        print(f"      Response: {response[:100]}...")
                        results.append(False)
                else:
                    print(f"   ❌ {path}: HTTP {status}")
                    results.append(False)
            except Exception as e:
                print(f"   ❌ {path}: Error - {e}")
                results.append(False)
        
        return all(results)
    
    def test_update_operations(self):
        """Test document updates"""
        print("\n✏️ Testing Update Operations:")
        
        # Create a document
        doc = {
            "title": "Update test",
            "type": "update-test",
            "status": "pending"
        }
        status, response = self.api_call("POST", "/api/documents", doc)
        
        if status != 201:
            print("   ❌ Failed to create test document")
            return False
        
        doc_id = json.loads(response)["uuid"]
        
        # Update it
        updates = {
            "status": "completed",
            "priority": "high"
        }
        status, response = self.api_call("PUT", f"/api/documents/{doc_id}", updates)
        
        if status == 200:
            updated = json.loads(response)
            if updated.get("status") == "completed":
                print("   ✅ Update successful")
                return True
            else:
                print("   ❌ Update didn't apply")
                return False
        else:
            print(f"   ❌ Update failed: HTTP {status}")
            return False

def main():
    print("🧪 JDBX Comprehensive Testing (HTTP/1.0)")
    print("=" * 50)
    
    tester = JDBXTester()
    
    # Login first
    print("\n🔐 Authenticating...")
    if not tester.login():
        print("   ❌ Authentication failed")
        return
    print("   ✅ Authenticated")
    
    # Run all tests
    test_results = {
        "Large Documents": tester.test_large_documents(),
        "Query Completeness": tester.test_query_completeness(),
        "Concurrent Operations": tester.test_concurrent_operations(),
        "API Endpoints": tester.test_api_endpoints(),
        "Update Operations": tester.test_update_operations()
    }
    
    # Summary
    print("\n" + "=" * 50)
    print("📊 TEST SUMMARY:")
    print("=" * 50)
    
    for test_name, result in test_results.items():
        status = "✅ PASSED" if result else "❌ FAILED"
        print(f"{test_name}: {status}")
    
    # Issues found
    print("\n🐛 ISSUES FOUND:")
    if not test_results["Query Completeness"]:
        print("1. Queries don't return all matching documents")
    if not all(result for size, result in test_results["Large Documents"] if isinstance(result, tuple) and size > 10000):
        print("2. Large documents (>10KB) have response issues")
    if not test_results["Concurrent Operations"]:
        print("3. Concurrent operations have race conditions")
    if not test_results["API Endpoints"]:
        print("4. Some API endpoints return invalid JSON")

if __name__ == "__main__":
    main()