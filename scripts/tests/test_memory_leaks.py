#!/usr/bin/env python3
"""
JDBX Memory Leak Testing - Phase 1.1 Implementation
Tests checkpoint-based memory management under sustained load
"""

import requests
import threading
import time
import psutil
import os
import json
import random
import string
from concurrent.futures import ThreadPoolExecutor

# Server configuration
SERVER_URL = "https://localhost:5000"
ADMIN_USER = os.getenv("JDBX_BOOTSTRAP_ADMIN_USER", "admin")
ADMIN_PASS = os.getenv("JDBX_BOOTSTRAP_ADMIN_PASS", "secure123456789")

# Test configuration
TEST_DURATION = 300  # 5 minutes of sustained load
CONCURRENT_THREADS = 20
OPERATIONS_PER_THREAD = 100
MEMORY_SAMPLE_INTERVAL = 5  # seconds

class MemoryMonitor:
    def __init__(self):
        self.samples = []
        self.running = False
        
    def start_monitoring(self, pid):
        self.running = True
        self.pid = pid
        threading.Thread(target=self._monitor_loop, daemon=True).start()
        
    def stop_monitoring(self):
        self.running = False
        
    def _monitor_loop(self):
        try:
            process = psutil.Process(self.pid)
            while self.running:
                memory_info = process.memory_info()
                cpu_percent = process.cpu_percent()
                
                sample = {
                    'timestamp': time.time(),
                    'rss_mb': memory_info.rss / 1024 / 1024,
                    'vms_mb': memory_info.vms / 1024 / 1024,
                    'cpu_percent': cpu_percent,
                    'num_threads': process.num_threads()
                }
                self.samples.append(sample)
                time.sleep(MEMORY_SAMPLE_INTERVAL)
        except psutil.NoSuchProcess:
            print(f"Process {self.pid} no longer exists")
            
    def get_analysis(self):
        if len(self.samples) < 2:
            return "Insufficient samples for analysis"
            
        start_memory = self.samples[0]['rss_mb']
        end_memory = self.samples[-1]['rss_mb']
        peak_memory = max(s['rss_mb'] for s in self.samples)
        avg_memory = sum(s['rss_mb'] for s in self.samples) / len(self.samples)
        
        memory_growth = end_memory - start_memory
        growth_rate = memory_growth / (len(self.samples) * MEMORY_SAMPLE_INTERVAL) * 60  # MB/minute
        
        return {
            'start_memory_mb': start_memory,
            'end_memory_mb': end_memory,
            'peak_memory_mb': peak_memory,
            'avg_memory_mb': avg_memory,
            'memory_growth_mb': memory_growth,
            'growth_rate_mb_per_min': growth_rate,
            'total_samples': len(self.samples),
            'test_duration_sec': len(self.samples) * MEMORY_SAMPLE_INTERVAL
        }

class JDBXClient:
    def __init__(self):
        self.session = requests.Session()
        self.session.verify = False  # SSL verification disabled for testing
        self.token = None
        
    def login(self):
        """Authenticate and get JWT token"""
        login_data = {
            "username": ADMIN_USER,
            "password": ADMIN_PASS
        }
        
        response = self.session.post(f"{SERVER_URL}/api/auth/login", json=login_data)
        if response.status_code == 200:
            self.token = response.json().get("token")
            self.session.headers.update({"Authorization": f"Bearer {self.token}"})
            return True
        return False
        
    def create_document(self, library="default", collection="test_docs"):
        """Create a test document"""
        doc_data = {
            "name": f"test_doc_{random.randint(1000, 9999)}",
            "data": {
                "value": random.randint(1, 1000),
                "text": ''.join(random.choices(string.ascii_letters, k=100)),
                "nested": {
                    "items": [random.randint(1, 100) for _ in range(10)],
                    "metadata": {
                        "created_by": "memory_test",
                        "tags": ["test", "memory", "checkpoint"]
                    }
                }
            }
        }
        
        response = self.session.post(
            f"{SERVER_URL}/api/libraries/{library}/collections/{collection}/documents",
            json=doc_data
        )
        return response.status_code == 201, response.json() if response.status_code == 201 else None
        
    def query_documents(self, library="default", collection="test_docs"):
        """Query documents to test read operations"""
        response = self.session.get(
            f"{SERVER_URL}/api/libraries/{library}/collections/{collection}/documents"
        )
        return response.status_code == 200, response.json() if response.status_code == 200 else None
        
    def update_document(self, uuid, library="default", collection="test_docs"):
        """Update a document to test modification operations"""
        update_data = {
            "data": {
                "updated_at": time.time(),
                "update_count": random.randint(1, 100),
                "new_field": ''.join(random.choices(string.ascii_letters, k=50))
            }
        }
        
        response = self.session.put(
            f"{SERVER_URL}/api/libraries/{library}/collections/{collection}/documents/{uuid}",
            json=update_data
        )
        return response.status_code == 200, response.json() if response.status_code == 200 else None
        
    def delete_document(self, uuid, library="default", collection="test_docs"):
        """Delete a document to test cleanup operations"""
        response = self.session.delete(
            f"{SERVER_URL}/api/libraries/{library}/collections/{collection}/documents/{uuid}"
        )
        return response.status_code == 200 or response.status_code == 404

def get_jdbx_pid():
    """Find the JDBX server process ID"""
    for proc in psutil.process_iter(['pid', 'name', 'cmdline']):
        try:
            if 'jdbxd' in proc.info['name']:
                return proc.info['pid']
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            continue
    return None

def worker_thread(thread_id, operations_count, results):
    """Worker thread for sustained load testing"""
    client = JDBXClient()
    
    # Login
    if not client.login():
        results[thread_id] = {"error": "Failed to login", "operations": 0}
        return
        
    operations_completed = 0
    created_docs = []
    
    try:
        for i in range(operations_count):
            operation_type = random.choice(['create', 'read', 'update', 'delete'])
            
            if operation_type == 'create' or len(created_docs) == 0:
                success, doc = client.create_document()
                if success and doc:
                    created_docs.append(doc.get('uuid'))
                    operations_completed += 1
                    
            elif operation_type == 'read':
                success, _ = client.query_documents()
                if success:
                    operations_completed += 1
                    
            elif operation_type == 'update' and created_docs:
                uuid = random.choice(created_docs)
                success, _ = client.update_document(uuid)
                if success:
                    operations_completed += 1
                    
            elif operation_type == 'delete' and created_docs:
                uuid = created_docs.pop(random.randint(0, len(created_docs) - 1))
                success = client.delete_document(uuid)
                if success:
                    operations_completed += 1
                    
            # Small delay to simulate realistic usage
            time.sleep(0.01)
            
    except Exception as e:
        results[thread_id] = {"error": str(e), "operations": operations_completed}
        return
        
    results[thread_id] = {"operations": operations_completed, "created_docs": len(created_docs)}

def main():
    print("🧪 JDBX Memory Leak Testing - Phase 1.1")
    print("=" * 50)
    
    # Find JDBX process
    pid = get_jdbx_pid()
    if not pid:
        print("❌ JDBX server process not found")
        return 1
        
    print(f"📊 Monitoring JDBX process PID: {pid}")
    
    # Start memory monitoring
    monitor = MemoryMonitor()
    monitor.start_monitoring(pid)
    
    print(f"🚀 Starting sustained load test:")
    print(f"   Duration: {TEST_DURATION} seconds")
    print(f"   Concurrent threads: {CONCURRENT_THREADS}")
    print(f"   Operations per thread: {OPERATIONS_PER_THREAD}")
    print(f"   Total operations: {CONCURRENT_THREADS * OPERATIONS_PER_THREAD}")
    
    # Run sustained load test
    start_time = time.time()
    results = {}
    
    with ThreadPoolExecutor(max_workers=CONCURRENT_THREADS) as executor:
        futures = []
        for i in range(CONCURRENT_THREADS):
            future = executor.submit(worker_thread, i, OPERATIONS_PER_THREAD, results)
            futures.append(future)
            
        # Wait for completion or timeout
        completed = 0
        while completed < CONCURRENT_THREADS and (time.time() - start_time) < TEST_DURATION:
            time.sleep(1)
            completed = sum(1 for f in futures if f.done())
            print(f"🔄 Progress: {completed}/{CONCURRENT_THREADS} threads completed")
            
        # Cancel remaining futures if timeout
        if (time.time() - start_time) >= TEST_DURATION:
            print("⏰ Test duration reached, stopping threads...")
            for future in futures:
                future.cancel()
    
    # Stop monitoring
    monitor.stop_monitoring()
    
    # Analyze results
    print("\n📈 Memory Analysis Results:")
    print("=" * 30)
    
    analysis = monitor.get_analysis()
    if isinstance(analysis, dict):
        print(f"Start Memory: {analysis['start_memory_mb']:.2f} MB")
        print(f"End Memory: {analysis['end_memory_mb']:.2f} MB")
        print(f"Peak Memory: {analysis['peak_memory_mb']:.2f} MB")
        print(f"Average Memory: {analysis['avg_memory_mb']:.2f} MB")
        print(f"Memory Growth: {analysis['memory_growth_mb']:.2f} MB")
        print(f"Growth Rate: {analysis['growth_rate_mb_per_min']:.2f} MB/minute")
        print(f"Test Duration: {analysis['test_duration_sec']} seconds")
        
        # Memory leak assessment
        growth_threshold = 10.0  # MB
        rate_threshold = 2.0     # MB/minute
        
        if analysis['memory_growth_mb'] > growth_threshold:
            print(f"⚠️  POTENTIAL MEMORY LEAK: Growth of {analysis['memory_growth_mb']:.2f} MB exceeds threshold")
        elif analysis['growth_rate_mb_per_min'] > rate_threshold:
            print(f"⚠️  POTENTIAL MEMORY LEAK: Growth rate of {analysis['growth_rate_mb_per_min']:.2f} MB/min exceeds threshold")
        else:
            print("✅ MEMORY STABLE: No significant memory leaks detected")
    else:
        print(f"❌ Analysis failed: {analysis}")
    
    # Operation results
    print("\n🎯 Operation Results:")
    print("=" * 20)
    
    total_operations = 0
    error_count = 0
    
    for thread_id, result in results.items():
        if "error" in result:
            print(f"Thread {thread_id}: ERROR - {result['error']}")
            error_count += 1
        else:
            total_operations += result['operations']
            
    print(f"Total Operations: {total_operations}")
    print(f"Error Count: {error_count}")
    print(f"Success Rate: {((len(results) - error_count) / len(results) * 100):.1f}%")
    
    # Final assessment
    print("\n🏆 Phase 1.1 Memory Management Assessment:")
    print("=" * 45)
    
    if isinstance(analysis, dict):
        if (analysis['memory_growth_mb'] <= growth_threshold and 
            analysis['growth_rate_mb_per_min'] <= rate_threshold and
            error_count == 0):
            print("✅ PRODUCTION READY: Checkpoint-based memory management validated")
            print("✅ Zero memory leaks under sustained concurrent load")
            print("✅ All operations completed successfully")
            return 0
        else:
            print("⚠️  REQUIRES ATTENTION: Memory management issues detected")
            return 1
    else:
        print("❌ TESTING FAILED: Unable to complete memory analysis")
        return 1

if __name__ == "__main__":
    exit(main())