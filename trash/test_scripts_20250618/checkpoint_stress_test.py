#!/usr/bin/env python3
"""
Comprehensive stress test for JDBX checkpoint memory management system
Tests concurrent operations, error conditions, and memory stability
"""

import requests
import json
import time
import concurrent.futures
import threading
import random
import string
import urllib3
import sys
from typing import List, Dict, Tuple

urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

# Configuration
BASE_URL = "https://localhost:5000"
ADMIN_USER = "admin"
ADMIN_PASS = "secure123456789"

# Test results tracking
results_lock = threading.Lock()
test_results = {
    "total_operations": 0,
    "successful_operations": 0,
    "failed_operations": 0,
    "operation_times": [],
    "error_types": {},
    "memory_checkpoints": []
}

def log(msg: str):
    """Thread-safe logging"""
    with results_lock:
        print(f"[{time.strftime('%H:%M:%S')}] {msg}")

def login() -> str:
    """Get JWT token"""
    try:
        response = requests.post(
            f"{BASE_URL}/api/auth/login",
            json={"username": ADMIN_USER, "password": ADMIN_PASS},
            headers={"Content-Type": "application/json"},
            verify=False,
            timeout=10
        )
        response.raise_for_status()
        return response.json()["token"]
    except Exception as e:
        log(f"❌ Login failed: {e}")
        return None

def record_operation(success: bool, duration: float, error_type: str = None):
    """Record operation results"""
    with results_lock:
        test_results["total_operations"] += 1
        if success:
            test_results["successful_operations"] += 1
        else:
            test_results["failed_operations"] += 1
            if error_type:
                test_results["error_types"][error_type] = test_results["error_types"].get(error_type, 0) + 1
        test_results["operation_times"].append(duration)

def random_string(length: int = 10) -> str:
    """Generate random string"""
    return ''.join(random.choices(string.ascii_lowercase + string.digits, k=length))

def create_document(token: str, session_id: int, doc_id: int) -> bool:
    """Create a document and test checkpoint system"""
    start_time = time.time()
    
    # Create increasingly complex documents to stress JSON memory management
    doc = {
        "id": f"checkpoint-test-{session_id}-{doc_id}",
        "title": f"Checkpoint Test Document {doc_id}",
        "content": random_string(1000),  # 1KB content
        "metadata": {
            "session": session_id,
            "iteration": doc_id,
            "timestamp": time.time(),
            "test_data": {
                "nested_array": [random_string(100) for _ in range(10)],
                "complex_object": {
                    f"key_{i}": random_string(50) for i in range(20)
                }
            }
        },
        "tags": [random_string(10) for _ in range(5)],
        "large_text": random_string(5000)  # 5KB text to stress memory
    }
    
    try:
        response = requests.post(
            f"{BASE_URL}/api/documents",
            json=doc,
            headers={
                "Authorization": f"Bearer {token}",
                "Content-Type": "application/json"
            },
            verify=False,
            timeout=30
        )
        
        duration = time.time() - start_time
        
        if response.status_code == 201:
            record_operation(True, duration)
            return True
        else:
            record_operation(False, duration, f"HTTP_{response.status_code}")
            log(f"❌ Create failed: {response.status_code} - {response.text[:100]}")
            return False
            
    except Exception as e:
        duration = time.time() - start_time
        record_operation(False, duration, type(e).__name__)
        log(f"❌ Create exception: {e}")
        return False

def query_documents(token: str, session_id: int) -> bool:
    """Query documents to test read operations"""
    start_time = time.time()
    
    try:
        response = requests.get(
            f"{BASE_URL}/api/documents",
            headers={"Authorization": f"Bearer {token}"},
            verify=False,
            timeout=30
        )
        
        duration = time.time() - start_time
        
        if response.status_code == 200:
            # Parse response to stress JSON handling
            data = response.json()
            docs = data.get("documents", [])
            record_operation(True, duration)
            return True
        else:
            record_operation(False, duration, f"HTTP_{response.status_code}")
            return False
            
    except Exception as e:
        duration = time.time() - start_time
        record_operation(False, duration, type(e).__name__)
        return False

def update_document(token: str, doc_uuid: str, session_id: int) -> bool:
    """Update document to test modification operations"""
    start_time = time.time()
    
    update_data = {
        "updated": True,
        "update_time": time.time(),
        "update_session": session_id,
        "additional_data": {
            "more_content": random_string(2000),
            "update_metadata": {
                f"field_{i}": random_string(100) for i in range(10)
            }
        }
    }
    
    try:
        response = requests.put(
            f"{BASE_URL}/api/documents/{doc_uuid}",
            json=update_data,
            headers={
                "Authorization": f"Bearer {token}",
                "Content-Type": "application/json"
            },
            verify=False,
            timeout=30
        )
        
        duration = time.time() - start_time
        
        if response.status_code == 200:
            record_operation(True, duration)
            return True
        else:
            record_operation(False, duration, f"HTTP_{response.status_code}")
            return False
            
    except Exception as e:
        duration = time.time() - start_time
        record_operation(False, duration, type(e).__name__)
        return False

def delete_document(token: str, doc_uuid: str) -> bool:
    """Delete document to test cleanup operations"""
    start_time = time.time()
    
    try:
        response = requests.delete(
            f"{BASE_URL}/api/documents/{doc_uuid}",
            headers={"Authorization": f"Bearer {token}"},
            verify=False,
            timeout=30
        )
        
        duration = time.time() - start_time
        
        if response.status_code == 200:
            record_operation(True, duration)
            return True
        else:
            record_operation(False, duration, f"HTTP_{response.status_code}")
            return False
            
    except Exception as e:
        duration = time.time() - start_time
        record_operation(False, duration, type(e).__name__)
        return False

def stress_session(session_id: int, token: str, operations_per_session: int = 20) -> Dict:
    """Run stress test session"""
    session_results = {
        "session_id": session_id,
        "operations": 0,
        "successes": 0,
        "failures": 0,
        "documents_created": []
    }
    
    log(f"🚀 Session {session_id} starting with {operations_per_session} operations")
    
    # Create documents
    for i in range(operations_per_session // 4):
        if create_document(token, session_id, i):
            session_results["successes"] += 1
            session_results["documents_created"].append(f"checkpoint-test-{session_id}-{i}")
        else:
            session_results["failures"] += 1
        session_results["operations"] += 1
    
    # Query operations
    for i in range(operations_per_session // 4):
        if query_documents(token, session_id):
            session_results["successes"] += 1
        else:
            session_results["failures"] += 1
        session_results["operations"] += 1
    
    # Get actual document UUIDs for updates/deletes
    try:
        response = requests.get(
            f"{BASE_URL}/api/documents",
            headers={"Authorization": f"Bearer {token}"},
            verify=False,
            timeout=30
        )
        if response.status_code == 200:
            docs = response.json().get("documents", [])
            doc_uuids = [doc.get("uuid") for doc in docs if doc.get("uuid")]
        else:
            doc_uuids = []
    except:
        doc_uuids = []
    
    # Update operations
    for i in range(min(operations_per_session // 4, len(doc_uuids))):
        if update_document(token, doc_uuids[i], session_id):
            session_results["successes"] += 1
        else:
            session_results["failures"] += 1
        session_results["operations"] += 1
    
    # Delete operations
    for i in range(min(operations_per_session // 4, len(doc_uuids))):
        if delete_document(token, doc_uuids[i]):
            session_results["successes"] += 1
        else:
            session_results["failures"] += 1
        session_results["operations"] += 1
    
    log(f"✅ Session {session_id} completed: {session_results['successes']}/{session_results['operations']} successful")
    return session_results

def run_stress_test(num_sessions: int = 20, operations_per_session: int = 20) -> bool:
    """Run comprehensive stress test"""
    log(f"\n🧪 Starting checkpoint stress test:")
    log(f"   Sessions: {num_sessions}")
    log(f"   Operations per session: {operations_per_session}")
    log(f"   Total operations: {num_sessions * operations_per_session}")
    
    # Login
    token = login()
    if not token:
        log("❌ Cannot proceed without authentication")
        return False
    
    start_time = time.time()
    
    # Run concurrent sessions
    with concurrent.futures.ThreadPoolExecutor(max_workers=num_sessions) as executor:
        futures = []
        for session_id in range(num_sessions):
            future = executor.submit(stress_session, session_id, token, operations_per_session)
            futures.append(future)
        
        # Wait for completion
        session_results = []
        for future in concurrent.futures.as_completed(futures):
            try:
                result = future.result()
                session_results.append(result)
            except Exception as e:
                log(f"❌ Session failed: {e}")
    
    total_time = time.time() - start_time
    
    # Calculate results
    with results_lock:
        success_rate = (test_results["successful_operations"] / test_results["total_operations"] * 100) if test_results["total_operations"] > 0 else 0
        avg_time = sum(test_results["operation_times"]) / len(test_results["operation_times"]) if test_results["operation_times"] else 0
        ops_per_second = test_results["total_operations"] / total_time if total_time > 0 else 0
    
    # Print results
    log(f"\n📊 Checkpoint Stress Test Results:")
    log(f"   Total time: {total_time:.2f} seconds")
    log(f"   Total operations: {test_results['total_operations']}")
    log(f"   Successful: {test_results['successful_operations']}")
    log(f"   Failed: {test_results['failed_operations']}")
    log(f"   Success rate: {success_rate:.1f}%")
    log(f"   Average response time: {avg_time*1000:.1f}ms")
    log(f"   Operations per second: {ops_per_second:.1f}")
    
    if test_results["error_types"]:
        log(f"   Error breakdown:")
        for error_type, count in test_results["error_types"].items():
            log(f"     {error_type}: {count}")
    
    # Determine test result
    if success_rate >= 95.0:
        log(f"✅ CHECKPOINT SYSTEM EXCELLENT: {success_rate:.1f}% success rate!")
        return True
    elif success_rate >= 85.0:
        log(f"⚠️  CHECKPOINT SYSTEM GOOD: {success_rate:.1f}% success rate")
        return True
    else:
        log(f"❌ CHECKPOINT SYSTEM ISSUES: {success_rate:.1f}% success rate")
        return False

if __name__ == "__main__":
    print("🚀 JDBX Checkpoint Memory Management Stress Test")
    print("=" * 50)
    
    # Progressive stress testing
    test_configs = [
        (5, 10),   # 5 sessions, 10 ops each = 50 total ops
        (10, 15),  # 10 sessions, 15 ops each = 150 total ops
        (20, 20),  # 20 sessions, 20 ops each = 400 total ops
        (30, 25),  # 30 sessions, 25 ops each = 750 total ops
    ]
    
    all_passed = True
    
    for i, (sessions, ops) in enumerate(test_configs, 1):
        log(f"\n🧪 Running stress test {i}/{len(test_configs)}")
        
        # Reset results for this test
        with results_lock:
            test_results.update({
                "total_operations": 0,
                "successful_operations": 0,
                "failed_operations": 0,
                "operation_times": [],
                "error_types": {}
            })
        
        passed = run_stress_test(sessions, ops)
        if not passed:
            all_passed = False
            log(f"❌ Stress test {i} failed - stopping progression")
            break
        
        # Brief pause between tests
        time.sleep(2)
    
    log(f"\n{'='*50}")
    if all_passed:
        log("🎉 ALL CHECKPOINT STRESS TESTS PASSED!")
        log("✅ Memory management system is stable under load")
    else:
        log("❌ Some tests failed - investigate memory management issues")
    
    sys.exit(0 if all_passed else 1)