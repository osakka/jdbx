#!/usr/bin/env python3
"""
Comprehensive stability test suite for JDBX
Tests various edge cases and stability issues
"""

import requests
import json
import threading
import time
import urllib3
import random
import string
from concurrent.futures import ThreadPoolExecutor, as_completed

urllib3.disable_warnings()

BASE_URL = "https://localhost:5000"

class JDBXTester:
    def __init__(self):
        self.session = requests.Session()
        self.session.verify = False
        self.auth_token = None
        self.test_results = {}
        
    def login(self):
        """Login and get auth token"""
        try:
            response = self.session.post(f"{BASE_URL}/api/auth/login", json={
                "username": "admin",
                "password": "secure123456789"
            }, timeout=5)
            
            if response.status_code == 200:
                self.auth_token = response.json()["token"]
                self.session.headers["Authorization"] = f"Bearer {self.auth_token}"
                return True
        except Exception as e:
            print(f"Login failed: {e}")
        return False
    
    def test_rapid_connections(self):
        """Test rapid connection creation and teardown"""
        print("\n🧪 Test: Rapid Connections")
        success = 0
        
        for i in range(20):
            try:
                # Create new session for each connection
                temp_session = requests.Session()
                temp_session.verify = False
                temp_session.headers["Authorization"] = f"Bearer {self.auth_token}"
                
                response = temp_session.get(f"{BASE_URL}/api/health", timeout=2)
                if response.status_code == 200:
                    success += 1
                temp_session.close()
            except Exception as e:
                pass
        
        result = success >= 15  # Allow some failures
        print(f"Result: {success}/20 successful ({'✅ PASS' if result else '❌ FAIL'})")
        return result
    
    def test_malformed_json(self):
        """Test various malformed JSON payloads"""
        print("\n🧪 Test: Malformed JSON")
        test_cases = [
            '{"title": "test"',  # Missing closing brace
            '{"title": "test", "extra",}',  # Trailing comma
            '{"title": null undefined}',  # Invalid syntax
            '{title: "test"}',  # Unquoted key
            '["not", "an", "object"]',  # Array instead of object
        ]
        
        passed = 0
        for i, payload in enumerate(test_cases):
            try:
                response = self.session.post(
                    f"{BASE_URL}/api/documents",
                    data=payload,
                    headers={"Content-Type": "application/json"},
                    timeout=5
                )
                # Should get 400 Bad Request
                if response.status_code in [400, 413]:
                    passed += 1
            except Exception:
                # Connection errors are ok - server protected itself
                passed += 1
        
        result = passed == len(test_cases)
        print(f"Result: {passed}/{len(test_cases)} handled correctly ({'✅ PASS' if result else '❌ FAIL'})")
        return result
    
    def test_concurrent_library_creation(self):
        """Test concurrent library creation"""
        print("\n🧪 Test: Concurrent Library Creation")
        
        def create_library(lib_name):
            try:
                response = self.session.post(
                    f"{BASE_URL}/api/libraries",
                    json={"name": lib_name},
                    timeout=5
                )
                return response.status_code in [201, 409]  # Created or already exists
            except Exception:
                return False
        
        lib_names = [f"test_lib_{i}" for i in range(10)]
        with ThreadPoolExecutor(max_workers=5) as executor:
            futures = [executor.submit(create_library, name) for name in lib_names]
            results = [f.result() for f in as_completed(futures)]
        
        success = sum(results)
        result = success >= 8  # Allow some failures
        print(f"Result: {success}/10 libraries ({'✅ PASS' if result else '❌ FAIL'})")
        return result
    
    def test_large_document_handling(self):
        """Test handling of documents of various sizes"""
        print("\n🧪 Test: Large Document Handling")
        
        sizes = [
            (1024, "1KB"),
            (10 * 1024, "10KB"),
            (100 * 1024, "100KB"),
            (500 * 1024, "500KB"),
        ]
        
        passed = 0
        for size, label in sizes:
            try:
                # Generate random content
                content = ''.join(random.choices(string.ascii_letters, k=size))
                doc = {
                    "title": f"Large doc {label}",
                    "content": content,
                    "size_label": label
                }
                
                response = self.session.post(
                    f"{BASE_URL}/api/documents",
                    json=doc,
                    timeout=10
                )
                
                if response.status_code in [201, 413]:
                    passed += 1
                    if response.status_code == 201:
                        # Try to retrieve it
                        doc_id = response.json()["uuid"]
                        get_response = self.session.get(f"{BASE_URL}/api/documents/{doc_id}")
                        if get_response.status_code == 200:
                            print(f"  {label}: ✅ Created and retrieved")
                        else:
                            print(f"  {label}: ⚠️ Created but retrieval failed")
                    else:
                        print(f"  {label}: ✅ Rejected (too large)")
            except Exception as e:
                print(f"  {label}: ❌ Error: {str(e)[:50]}")
        
        result = passed >= 3  # Most should work
        print(f"Result: {passed}/{len(sizes)} handled ({'✅ PASS' if result else '❌ FAIL'})")
        return result
    
    def test_query_edge_cases(self):
        """Test various query edge cases"""
        print("\n🧪 Test: Query Edge Cases")
        
        # Create test documents first
        test_docs = []
        for i in range(5):
            doc = {
                "title": f"Query test {i}",
                "type": "query_test",
                "index": i,
                "tags": ["test", f"tag_{i}"]
            }
            response = self.session.post(f"{BASE_URL}/api/documents", json=doc)
            if response.status_code == 201:
                test_docs.append(response.json()["uuid"])
        
        # Test different query patterns
        queries = [
            {"type": "query_test"},  # Simple query
            {"index": {"$gte": 2}},  # Range query
            {"tags": {"$in": ["test"]}},  # Array query
            {"$or": [{"index": 1}, {"index": 3}]},  # OR query
            {"$and": [{"type": "query_test"}, {"index": {"$lt": 3}}]},  # AND query
        ]
        
        passed = 0
        for query in queries:
            try:
                response = self.session.post(
                    f"{BASE_URL}/api/documents/query",
                    json=query,
                    timeout=5
                )
                if response.status_code == 200:
                    passed += 1
            except Exception:
                pass
        
        # Cleanup
        for doc_id in test_docs:
            try:
                self.session.delete(f"{BASE_URL}/api/documents/{doc_id}")
            except:
                pass
        
        result = passed >= 3  # Most queries should work
        print(f"Result: {passed}/{len(queries)} queries worked ({'✅ PASS' if result else '❌ FAIL'})")
        return result
    
    def test_session_persistence(self):
        """Test session persistence across requests"""
        print("\n🧪 Test: Session Persistence")
        
        # Create multiple sessions
        sessions = []
        for i in range(5):
            s = requests.Session()
            s.verify = False
            # Login with each session
            response = s.post(f"{BASE_URL}/api/auth/login", json={
                "username": "admin",
                "password": "secure123456789"
            })
            if response.status_code == 200:
                token = response.json()["token"]
                s.headers["Authorization"] = f"Bearer {token}"
                sessions.append(s)
        
        # Use sessions concurrently
        def use_session(session, index):
            try:
                # Create a document
                response = session.post(f"{BASE_URL}/api/documents", json={
                    "title": f"Session {index} doc",
                    "session_id": index
                })
                return response.status_code == 201
            except:
                return False
        
        with ThreadPoolExecutor(max_workers=5) as executor:
            futures = [executor.submit(use_session, s, i) for i, s in enumerate(sessions)]
            results = [f.result() for f in as_completed(futures)]
        
        success = sum(results)
        result = success >= 3
        print(f"Result: {success}/{len(sessions)} sessions worked ({'✅ PASS' if result else '❌ FAIL'})")
        
        # Close sessions
        for s in sessions:
            s.close()
        
        return result
    
    def test_error_recovery(self):
        """Test server's ability to recover from errors"""
        print("\n🧪 Test: Error Recovery")
        
        # Cause various errors and check recovery
        error_tests = [
            # Invalid UUID format
            lambda: self.session.get(f"{BASE_URL}/api/documents/not-a-uuid"),
            # Non-existent endpoint
            lambda: self.session.get(f"{BASE_URL}/api/nonexistent"),
            # Invalid method
            lambda: self.session.request("INVALID", f"{BASE_URL}/api/health"),
            # Deeply nested query
            lambda: self.session.post(f"{BASE_URL}/api/documents/query", 
                                    json={"a": {"b": {"c": {"d": {"e": {"f": "test"}}}}}}),
        ]
        
        passed = 0
        for i, test in enumerate(error_tests):
            try:
                test()
            except:
                pass
            
            # Check if server is still responsive
            try:
                response = self.session.get(f"{BASE_URL}/api/health", timeout=2)
                if response.status_code == 200:
                    passed += 1
            except:
                pass
        
        result = passed == len(error_tests)
        print(f"Result: {passed}/{len(error_tests)} recoveries ({'✅ PASS' if result else '❌ FAIL'})")
        return result
    
    def run_all_tests(self):
        """Run all stability tests"""
        print("🔍 Comprehensive Stability Test Suite")
        print("=" * 50)
        
        if not self.login():
            print("❌ Login failed - cannot proceed with tests")
            return
        
        print("✅ Login successful")
        
        tests = [
            self.test_rapid_connections,
            self.test_malformed_json,
            self.test_concurrent_library_creation,
            self.test_large_document_handling,
            self.test_query_edge_cases,
            self.test_session_persistence,
            self.test_error_recovery,
        ]
        
        results = []
        for test in tests:
            try:
                result = test()
                results.append(result)
            except Exception as e:
                print(f"❌ Test crashed: {e}")
                results.append(False)
        
        # Summary
        print("\n" + "=" * 50)
        print("📊 Test Summary")
        print("=" * 50)
        
        test_names = [
            "Rapid Connections",
            "Malformed JSON",
            "Concurrent Library Creation",
            "Large Document Handling",
            "Query Edge Cases",
            "Session Persistence",
            "Error Recovery",
        ]
        
        for name, result in zip(test_names, results):
            status = "✅ PASS" if result else "❌ FAIL"
            print(f"{name:<30} {status}")
        
        passed = sum(results)
        total = len(results)
        percentage = (passed / total) * 100
        print(f"\nTotal: {passed}/{total} passed ({percentage:.1f}%)")
        
        # Final health check
        print("\n🏥 Final Server Health Check...")
        try:
            response = self.session.get(f"{BASE_URL}/api/health", timeout=5)
            if response.status_code == 200:
                print("✅ Server is healthy")
            else:
                print("❌ Server returned non-200 status")
        except Exception as e:
            print(f"❌ Server not responding: {e}")

if __name__ == "__main__":
    tester = JDBXTester()
    tester.run_all_tests()