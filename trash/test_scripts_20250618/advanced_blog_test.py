#!/usr/bin/env python3
"""
🚀 ADVANCED BLOG APPLICATION TEST

Comprehensive real-world application testing for JDBX v6.5.5 featuring:
- Critical Security Excellence validation
- HTTP Keep-Alive performance optimization
- Developer Experience enhancement validation
- Multi-user authentication workflows
- Concurrent operations testing
- Edge case error handling

This test validates the complete developer experience from login to complex
multi-user blog operations, ensuring all recent improvements work together.
"""

import requests
import json
import time
import threading
from concurrent.futures import ThreadPoolExecutor, as_completed
import urllib3
import sys
from datetime import datetime

# Disable SSL warnings for self-signed certificates
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

BASE_URL = "https://localhost:5000"
ADMIN_USER = "admin"
ADMIN_PASS = "secure123456789"

class BlogTestSuite:
    def __init__(self):
        self.session = requests.Session()
        self.session.verify = False  # Self-signed cert
        self.admin_token = None
        self.users = {}  # Store user tokens
        self.articles = []  # Store created articles
        self.performance_metrics = {
            'request_times': [],
            'keep_alive_benefits': [],
            'security_validations': 0,
            'developer_ux_wins': 0
        }
    
    def log(self, message, level="INFO"):
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        print(f"[{timestamp}] [{level}] {message}")
    
    def test_security_excellence(self):
        """🔒 Test Critical Security Excellence"""
        self.log("=== TESTING CRITICAL SECURITY EXCELLENCE ===", "TEST")
        
        # Test 1: Unauthenticated document creation should fail
        self.log("Testing unauthenticated document creation (should fail)")
        try:
            response = self.session.post(f"{BASE_URL}/api/documents", 
                                       json={"title": "Unauthorized Test", "content": "Should fail"},
                                       timeout=10)
            if response.status_code == 401:
                self.log("✅ SECURITY: Unauthenticated document creation properly blocked", "PASS")
                self.performance_metrics['security_validations'] += 1
            else:
                self.log(f"❌ SECURITY VULNERABILITY: Got {response.status_code}, expected 401", "FAIL")
                return False
        except Exception as e:
            self.log(f"❌ SECURITY TEST ERROR: {e}", "FAIL")
            return False
        
        # Test 2: Unauthenticated library access should fail
        self.log("Testing unauthenticated library access (should fail)")
        try:
            response = self.session.get(f"{BASE_URL}/api/libraries", timeout=10)
            if response.status_code == 401:
                self.log("✅ SECURITY: Unauthenticated library access properly blocked", "PASS")
                self.performance_metrics['security_validations'] += 1
            else:
                self.log(f"❌ SECURITY VULNERABILITY: Got {response.status_code}, expected 401", "FAIL")
                return False
        except Exception as e:
            self.log(f"❌ SECURITY TEST ERROR: {e}", "FAIL")
            return False
        
        # Test 3: Health endpoint should work (essential endpoint)
        self.log("Testing health endpoint accessibility (should work)")
        try:
            response = self.session.get(f"{BASE_URL}/api/health", timeout=10)
            if response.status_code == 200:
                self.log("✅ SECURITY: Health endpoint accessible as expected", "PASS")
            else:
                self.log(f"⚠️ WARNING: Health endpoint returned {response.status_code}", "WARN")
        except Exception as e:
            self.log(f"⚠️ WARNING: Health endpoint test failed: {e}", "WARN")
        
        return True
    
    def test_admin_authentication(self):
        """Test admin login and token validation"""
        self.log("=== TESTING ADMIN AUTHENTICATION ===", "TEST")
        
        start_time = time.time()
        try:
            response = self.session.post(f"{BASE_URL}/api/auth/login", 
                                       json={"username": ADMIN_USER, "password": ADMIN_PASS},
                                       timeout=10)
            request_time = time.time() - start_time
            self.performance_metrics['request_times'].append(request_time)
            
            if response.status_code == 200:
                data = response.json()
                self.admin_token = data.get('token')
                self.log(f"✅ ADMIN LOGIN: Success in {request_time:.3f}s", "PASS")
                self.log(f"   User ID: {data.get('user_id')}")
                self.log(f"   Library: {data.get('library')}")
                return True
            else:
                self.log(f"❌ ADMIN LOGIN FAILED: {response.status_code} - {response.text}", "FAIL")
                return False
        except Exception as e:
            self.log(f"❌ ADMIN LOGIN ERROR: {e}", "FAIL")
            return False
    
    def test_developer_experience_enhancement(self):
        """🚀 Test Developer Experience Enhancement"""
        self.log("=== TESTING DEVELOPER EXPERIENCE ENHANCEMENT ===", "TEST")
        
        if not self.admin_token:
            self.log("❌ No admin token available for developer UX test", "FAIL")
            return False
        
        headers = {"Authorization": f"Bearer {self.admin_token}", "Content-Type": "application/json"}
        
        # Test 1: Document creation without type/owner (should auto-populate)
        self.log("Testing document auto-population (missing type/owner)")
        start_time = time.time()
        try:
            response = self.session.post(f"{BASE_URL}/api/documents",
                                       json={
                                           "title": "Developer UX Test Article",
                                           "content": "Testing auto-population of type and owner fields",
                                           "category": "test"
                                       },
                                       headers=headers,
                                       timeout=10)
            request_time = time.time() - start_time
            self.performance_metrics['request_times'].append(request_time)
            
            if response.status_code == 200:
                data = response.json()
                if data.get('type') == 'document' and data.get('owner') == 'user':
                    self.log(f"✅ DEVELOPER UX: Auto-population working in {request_time:.3f}s", "PASS")
                    self.log(f"   Auto-populated type: {data.get('type')}")
                    self.log(f"   Auto-populated owner: {data.get('owner')}")
                    self.log(f"   Document UUID: {data.get('uuid')}")
                    self.articles.append(data)
                    self.performance_metrics['developer_ux_wins'] += 1
                    return True
                else:
                    self.log(f"❌ DEVELOPER UX: Auto-population failed - type:{data.get('type')}, owner:{data.get('owner')}", "FAIL")
                    return False
            else:
                self.log(f"❌ DEVELOPER UX: Document creation failed - {response.status_code}", "FAIL")
                self.log(f"   Response: {response.text}")
                return False
        except Exception as e:
            self.log(f"❌ DEVELOPER UX ERROR: {e}", "FAIL")
            return False
    
    def test_http_keep_alive_performance(self):
        """🚀 Test HTTP Keep-Alive Performance Excellence"""
        self.log("=== TESTING HTTP KEEP-ALIVE PERFORMANCE ===", "TEST")
        
        if not self.admin_token:
            self.log("❌ No admin token available for keep-alive test", "FAIL")
            return False
        
        headers = {"Authorization": f"Bearer {self.admin_token}", "Content-Type": "application/json"}
        
        # Test multiple sequential requests on same connection
        self.log("Testing HTTP Keep-Alive with sequential requests")
        total_time = 0
        successful_requests = 0
        
        for i in range(5):
            start_time = time.time()
            try:
                response = self.session.post(f"{BASE_URL}/api/documents",
                                           json={
                                               "title": f"Keep-Alive Test Article {i+1}",
                                               "content": f"Testing HTTP Keep-Alive performance - request {i+1}",
                                               "series": "keep-alive-test"
                                           },
                                           headers=headers,
                                           timeout=10)
                request_time = time.time() - start_time
                total_time += request_time
                self.performance_metrics['request_times'].append(request_time)
                
                if response.status_code == 200:
                    successful_requests += 1
                    data = response.json()
                    self.articles.append(data)
                    self.log(f"   Request {i+1}: {request_time:.3f}s - UUID: {data.get('uuid')}")
                    
                    # Track keep-alive benefits (faster after first request)
                    if i > 0:
                        self.performance_metrics['keep_alive_benefits'].append(request_time)
                else:
                    self.log(f"   Request {i+1} failed: {response.status_code}")
                    
            except Exception as e:
                self.log(f"   Request {i+1} error: {e}")
        
        if successful_requests == 5:
            avg_time = total_time / 5
            self.log(f"✅ KEEP-ALIVE: All 5 requests successful, avg time: {avg_time:.3f}s", "PASS")
            
            # Check if later requests were faster (keep-alive benefit)
            if len(self.performance_metrics['keep_alive_benefits']) >= 3:
                avg_keep_alive = sum(self.performance_metrics['keep_alive_benefits']) / len(self.performance_metrics['keep_alive_benefits'])
                first_request = self.performance_metrics['request_times'][-5]  # First of the 5 requests
                if avg_keep_alive < first_request:
                    improvement = ((first_request - avg_keep_alive) / first_request) * 100
                    self.log(f"✅ PERFORMANCE: Keep-Alive improvement detected: {improvement:.1f}% faster", "PASS")
            return True
        else:
            self.log(f"❌ KEEP-ALIVE: Only {successful_requests}/5 requests successful", "FAIL")
            return False
    
    def test_multi_user_blog_scenario(self):
        """Test realistic multi-user blog scenario"""
        self.log("=== TESTING MULTI-USER BLOG SCENARIO ===", "TEST")
        
        # For now, we'll simulate multiple operations with admin user
        # In a real scenario, we'd create multiple users
        
        if not self.admin_token or not self.articles:
            self.log("❌ Prerequisites not met for multi-user test", "FAIL")
            return False
        
        headers = {"Authorization": f"Bearer {self.admin_token}", "Content-Type": "application/json"}
        
        # Test 1: Create a blog post with complex structure
        self.log("Creating complex blog post")
        start_time = time.time()
        try:
            complex_post = {
                "title": "Advanced JDBX Blog System",
                "content": "This is a comprehensive test of the JDBX blog system featuring security, performance, and developer experience enhancements.",
                "author": "JDBX Team",
                "tags": ["jdbx", "database", "security", "performance"],
                "metadata": {
                    "featured": True,
                    "category": "technology",
                    "read_time": 5
                },
                "status": "published"
            }
            
            response = self.session.post(f"{BASE_URL}/api/documents",
                                       json=complex_post,
                                       headers=headers,
                                       timeout=10)
            request_time = time.time() - start_time
            self.performance_metrics['request_times'].append(request_time)
            
            if response.status_code == 200:
                data = response.json()
                self.log(f"✅ COMPLEX POST: Created in {request_time:.3f}s", "PASS")
                self.log(f"   UUID: {data.get('uuid')}")
                self.log(f"   Auto-populated fields: type={data.get('type')}, owner={data.get('owner')}")
                self.articles.append(data)
                return True
            else:
                self.log(f"❌ COMPLEX POST: Failed - {response.status_code}", "FAIL")
                return False
        except Exception as e:
            self.log(f"❌ COMPLEX POST ERROR: {e}", "FAIL")
            return False
    
    def test_concurrent_operations(self):
        """Test concurrent blog operations"""
        self.log("=== TESTING CONCURRENT OPERATIONS ===", "TEST")
        
        if not self.admin_token:
            self.log("❌ No admin token available for concurrent test", "FAIL")
            return False
        
        headers = {"Authorization": f"Bearer {self.admin_token}", "Content-Type": "application/json"}
        
        def create_article(article_id):
            """Create a single article"""
            try:
                start_time = time.time()
                response = self.session.post(f"{BASE_URL}/api/documents",
                                           json={
                                               "title": f"Concurrent Article {article_id}",
                                               "content": f"Testing concurrent creation - article {article_id}",
                                               "thread_id": article_id
                                           },
                                           headers=headers,
                                           timeout=15)
                request_time = time.time() - start_time
                
                if response.status_code == 200:
                    data = response.json()
                    return {"success": True, "time": request_time, "uuid": data.get('uuid'), "id": article_id}
                else:
                    return {"success": False, "time": request_time, "error": response.status_code, "id": article_id}
            except Exception as e:
                return {"success": False, "time": 0, "error": str(e), "id": article_id}
        
        # Create 10 articles concurrently
        self.log("Creating 10 articles concurrently")
        with ThreadPoolExecutor(max_workers=5) as executor:
            futures = [executor.submit(create_article, i) for i in range(1, 11)]
            results = [future.result() for future in as_completed(futures)]
        
        successful = sum(1 for r in results if r['success'])
        total_time = sum(r['time'] for r in results if r['success'])
        avg_time = total_time / max(successful, 1)
        
        # Add times to metrics
        for r in results:
            if r['success']:
                self.performance_metrics['request_times'].append(r['time'])
        
        if successful >= 8:  # Allow some tolerance
            self.log(f"✅ CONCURRENT: {successful}/10 articles created, avg time: {avg_time:.3f}s", "PASS")
            for r in results:
                if r['success']:
                    self.log(f"   Article {r['id']}: {r['time']:.3f}s - UUID: {r['uuid']}")
                else:
                    self.log(f"   Article {r['id']}: FAILED - {r['error']}")
            return True
        else:
            self.log(f"❌ CONCURRENT: Only {successful}/10 articles created successfully", "FAIL")
            return False
    
    def generate_performance_report(self):
        """Generate comprehensive performance report"""
        self.log("=== PERFORMANCE REPORT ===", "REPORT")
        
        if not self.performance_metrics['request_times']:
            self.log("No performance data collected", "WARN")
            return
        
        times = self.performance_metrics['request_times']
        avg_time = sum(times) / len(times)
        min_time = min(times)
        max_time = max(times)
        
        self.log(f"📊 REQUEST PERFORMANCE:")
        self.log(f"   Total requests: {len(times)}")
        self.log(f"   Average time: {avg_time:.3f}s")
        self.log(f"   Fastest: {min_time:.3f}s")
        self.log(f"   Slowest: {max_time:.3f}s")
        
        self.log(f"🔒 SECURITY VALIDATIONS: {self.performance_metrics['security_validations']}")
        self.log(f"🚀 DEVELOPER UX WINS: {self.performance_metrics['developer_ux_wins']}")
        self.log(f"📄 ARTICLES CREATED: {len(self.articles)}")
        
        if self.performance_metrics['keep_alive_benefits']:
            ka_times = self.performance_metrics['keep_alive_benefits']
            avg_ka = sum(ka_times) / len(ka_times)
            self.log(f"⚡ KEEP-ALIVE BENEFIT: Avg {avg_ka:.3f}s for reused connections")
    
    def run_comprehensive_test(self):
        """Run complete test suite"""
        self.log("🚀 STARTING ADVANCED BLOG APPLICATION TEST", "START")
        self.log("Testing JDBX v6.5.5 with Security + Performance + Developer UX", "START")
        
        test_results = []
        
        # Test 1: Critical Security Excellence
        test_results.append(self.test_security_excellence())
        
        # Test 2: Admin Authentication  
        test_results.append(self.test_admin_authentication())
        
        # Test 3: Developer Experience Enhancement
        test_results.append(self.test_developer_experience_enhancement())
        
        # Test 4: HTTP Keep-Alive Performance
        test_results.append(self.test_http_keep_alive_performance())
        
        # Test 5: Multi-user Blog Scenario
        test_results.append(self.test_multi_user_blog_scenario())
        
        # Test 6: Concurrent Operations
        test_results.append(self.test_concurrent_operations())
        
        # Generate report
        self.generate_performance_report()
        
        # Final results
        passed = sum(test_results)
        total = len(test_results)
        
        self.log("=== FINAL RESULTS ===", "RESULT")
        if passed == total:
            self.log(f"🎉 ALL TESTS PASSED: {passed}/{total}", "SUCCESS")
            self.log("✅ JDBX v6.5.5 - Critical Security + Performance + Developer UX - EXCELLENT", "SUCCESS")
        else:
            self.log(f"⚠️ PARTIAL SUCCESS: {passed}/{total} tests passed", "PARTIAL")
        
        return passed == total

if __name__ == "__main__":
    test_suite = BlogTestSuite()
    success = test_suite.run_comprehensive_test()
    sys.exit(0 if success else 1)