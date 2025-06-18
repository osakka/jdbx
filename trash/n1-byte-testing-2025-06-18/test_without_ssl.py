#!/usr/bin/env python3
"""
Test JDBX without SSL to find real bugs
"""

import requests
import json
import time
from concurrent.futures import ThreadPoolExecutor

# No SSL warnings needed since we're using HTTP
BASE_URL = "http://localhost:5000"

class JDBXTester:
    def __init__(self):
        self.session = requests.Session()
        self.token = None
    
    def login(self):
        """Login to get token"""
        response = self.session.post(f"{BASE_URL}/api/auth/login", 
                                   json={"username": "admin", "password": "secure123456789"})
        if response.status_code == 200:
            self.token = response.json()["token"]
            self.session.headers["Authorization"] = f"Bearer {self.token}"
            return True
        return False
    
    def test_large_documents(self):
        """Test various document sizes"""
        print("\n📦 Testing Large Documents (no SSL):")
        
        sizes = [1000, 5000, 10000, 50000, 100000, 500000, 1000000]
        results = []
        
        for size in sizes:
            data = "x" * size
            doc = {
                "title": f"Large doc {size} bytes",
                "type": "large-test",
                "data": data
            }
            
            try:
                response = self.session.post(f"{BASE_URL}/api/documents", json=doc)
                
                if response.status_code == 201:
                    result = response.json()
                    print(f"   ✅ {size:,} bytes: Created {result['uuid']}")
                    results.append((size, True, None))
                else:
                    print(f"   ❌ {size:,} bytes: HTTP {response.status_code} - {response.text[:100]}")
                    results.append((size, False, response.status_code))
            except Exception as e:
                print(f"   ❌ {size:,} bytes: Error - {str(e)[:100]}")
                results.append((size, False, str(e)))
        
        return results
    
    def test_query_limits(self):
        """Test query result limits"""
        print("\n🔍 Testing Query Limits:")
        
        # Create 100 documents
        print("   Creating 100 test documents...")
        for i in range(100):
            doc = {
                "title": f"Query test {i:03d}",
                "type": "query-limit-test",
                "index": i
            }
            self.session.post(f"{BASE_URL}/api/documents", json=doc)
        
        # Query without limit
        response = self.session.post(f"{BASE_URL}/api/documents/query", 
                                   json={"type": "query-limit-test"})
        
        if response.status_code == 200:
            docs = response.json().get("documents", [])
            print(f"   Query returned {len(docs)} documents (created 100)")
            
            if len(docs) < 100:
                print(f"   ⚠️  Query has implicit limit of {len(docs)}")
            else:
                print("   ✅ No implicit query limit found")
            
            # Test explicit limit
            response = self.session.post(f"{BASE_URL}/api/documents/query", 
                                       json={"type": "query-limit-test", "limit": 10})
            limited_docs = response.json().get("documents", [])
            print(f"   With limit=10: returned {len(limited_docs)} documents")
        
        return len(docs) == 100
    
    def test_concurrent_updates(self):
        """Test concurrent updates to same document"""
        print("\n🔀 Testing Concurrent Updates:")
        
        # Create a document
        response = self.session.post(f"{BASE_URL}/api/documents", 
                                   json={"title": "Concurrent update test", "type": "update-test", "counter": 0})
        doc_id = response.json()["uuid"]
        
        # Try to update it 50 times concurrently
        def update_doc(i):
            try:
                response = self.session.put(f"{BASE_URL}/api/documents/{doc_id}", 
                                          json={"counter": i, "last_update": f"Update {i}"})
                return response.status_code == 200
            except:
                return False
        
        with ThreadPoolExecutor(max_workers=10) as executor:
            results = list(executor.map(update_doc, range(50)))
        
        success_count = sum(results)
        print(f"   Concurrent updates: {success_count}/50 succeeded")
        
        # Check final state
        response = self.session.get(f"{BASE_URL}/api/documents/{doc_id}")
        if response.status_code == 200:
            final_doc = response.json()
            print(f"   Final counter value: {final_doc.get('counter', 'N/A')}")
        
        return success_count == 50
    
    def test_special_characters(self):
        """Test documents with special characters"""
        print("\n🔤 Testing Special Characters:")
        
        test_cases = [
            ("Unicode", {"title": "Unicode test 中文 émojis 🚀", "type": "unicode-test"}),
            ("Quotes", {"title": 'Test with "quotes" and \'apostrophes\'', "type": "quote-test"}),
            ("Newlines", {"title": "Test with\nnewlines\rand\ttabs", "type": "newline-test"}),
            ("HTML", {"title": "<script>alert('xss')</script>", "type": "html-test"}),
            ("JSON chars", {"title": '{"nested": "json"}', "type": "json-test"}),
            ("Null bytes", {"title": "Test\x00with\x00nulls", "type": "null-test"}),
        ]
        
        results = []
        for name, doc in test_cases:
            try:
                response = self.session.post(f"{BASE_URL}/api/documents", json=doc)
                if response.status_code == 201:
                    # Try to retrieve it
                    doc_id = response.json()["uuid"]
                    response = self.session.get(f"{BASE_URL}/api/documents/{doc_id}")
                    if response.status_code == 200:
                        retrieved = response.json()
                        if retrieved.get("title") == doc["title"]:
                            print(f"   ✅ {name}: Stored and retrieved correctly")
                            results.append(True)
                        else:
                            print(f"   ❌ {name}: Title corrupted")
                            print(f"      Original: {repr(doc['title'])}")
                            print(f"      Retrieved: {repr(retrieved.get('title'))}")
                            results.append(False)
                else:
                    print(f"   ❌ {name}: Failed to create - {response.status_code}")
                    results.append(False)
            except Exception as e:
                print(f"   ❌ {name}: Error - {e}")
                results.append(False)
        
        return all(results)
    
    def test_missing_endpoints(self):
        """Test various API endpoints"""
        print("\n🌐 Testing API Endpoints:")
        
        endpoints = [
            ("GET", "/api/status"),
            ("GET", "/api/metrics"),
            ("GET", "/api/config"),
            ("GET", "/api/users"),
            ("GET", "/api/roles"),
        ]
        
        results = []
        for method, path in endpoints:
            try:
                response = self.session.request(method, f"{BASE_URL}{path}")
                if response.status_code == 404:
                    print(f"   ❌ {path}: Not implemented (404)")
                    results.append(False)
                elif response.status_code == 200:
                    print(f"   ✅ {path}: Working")
                    results.append(True)
                else:
                    print(f"   ⚠️  {path}: HTTP {response.status_code}")
                    results.append(True)  # Not 404, so it exists
            except Exception as e:
                print(f"   ❌ {path}: Error - {e}")
                results.append(False)
        
        return results

def main():
    print("🧪 JDBX Testing WITHOUT SSL")
    print("=" * 50)
    
    tester = JDBXTester()
    
    # Login
    print("\n🔐 Authenticating...")
    if not tester.login():
        print("   ❌ Authentication failed")
        return
    print("   ✅ Authenticated successfully")
    
    # Run all tests
    print("\n" + "=" * 50)
    print("RUNNING TESTS")
    print("=" * 50)
    
    test_results = {
        "Large Documents": tester.test_large_documents(),
        "Query Limits": tester.test_query_limits(),
        "Concurrent Updates": tester.test_concurrent_updates(),
        "Special Characters": tester.test_special_characters(),
        "API Endpoints": tester.test_missing_endpoints()
    }
    
    # Summary
    print("\n" + "=" * 50)
    print("📊 REAL BUGS FOUND (without SSL issues)")
    print("=" * 50)
    
    # Analyze large document results
    large_doc_results = test_results["Large Documents"]
    max_working_size = 0
    for size, success, error in large_doc_results:
        if success:
            max_working_size = size
        else:
            print(f"\n1. Document size limit: ~{max_working_size:,} bytes")
            print(f"   Documents > {max_working_size:,} bytes fail with: {error}")
            break
    
    if not test_results["Query Limits"]:
        print("\n2. Query results have implicit limit")
    
    if not test_results["Concurrent Updates"]:
        print("\n3. Concurrent updates have race conditions")
    
    if not test_results["Special Characters"]:
        print("\n4. Special character handling issues")
    
    endpoint_results = test_results["API Endpoints"]
    if not all(endpoint_results):
        print("\n5. Missing API endpoints:")
        endpoints = ["/api/status", "/api/metrics", "/api/config", "/api/users", "/api/roles"]
        for i, implemented in enumerate(endpoint_results):
            if not implemented:
                print(f"   - {endpoints[i]}")

if __name__ == "__main__":
    main()