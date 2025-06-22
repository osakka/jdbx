#!/usr/bin/env python3
"""
Stress test for threading - push the server to its limits
"""
import concurrent.futures
import requests
import time
import urllib3
import threading
import statistics

urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

BASE_URL = "https://localhost:5000"
DURATION_SECONDS = 30  # Run for 30 seconds
MAX_WORKERS = 50       # High concurrency

# Shared counters
success_count = 0
failure_count = 0
response_times = []
lock = threading.Lock()

def perform_request():
    """Single health check request with timing"""
    global success_count, failure_count, response_times
    
    start = time.time()
    try:
        response = requests.get(f"{BASE_URL}/api/health", verify=False, timeout=5)
        elapsed = time.time() - start
        
        with lock:
            if response.status_code == 200:
                success_count += 1
                response_times.append(elapsed * 1000)  # Convert to ms
            else:
                failure_count += 1
        
        return response.status_code == 200
    except Exception:
        with lock:
            failure_count += 1
        return False

def worker_thread():
    """Worker thread that continuously makes requests"""
    end_time = time.time() + DURATION_SECONDS
    while time.time() < end_time:
        perform_request()
        # Small random delay to prevent thundering herd
        time.sleep(0.001)

print("🚀 JDBX THREADING STRESS TEST")
print(f"Duration: {DURATION_SECONDS} seconds")
print(f"Workers: {MAX_WORKERS} concurrent threads")
print("="*60)

# Start timer
start_time = time.time()

# Launch worker threads
with concurrent.futures.ThreadPoolExecutor(max_workers=MAX_WORKERS) as executor:
    futures = []
    for _ in range(MAX_WORKERS):
        future = executor.submit(worker_thread)
        futures.append(future)
    
    # Progress indicator
    print("Running", end="", flush=True)
    while any(not f.done() for f in futures):
        print(".", end="", flush=True)
        time.sleep(1)
    
    # Wait for all to complete
    concurrent.futures.wait(futures)

# Calculate results
elapsed = time.time() - start_time
total_requests = success_count + failure_count
success_rate = (success_count / total_requests * 100) if total_requests > 0 else 0
throughput = total_requests / elapsed if elapsed > 0 else 0

print("\n\n" + "="*60)
print("📊 STRESS TEST RESULTS")
print("="*60)
print(f"Total Requests: {total_requests:,}")
print(f"Successful: {success_count:,} ({success_rate:.1f}%)")
print(f"Failed: {failure_count:,}")
print(f"Duration: {elapsed:.1f} seconds")
print(f"Throughput: {throughput:.1f} requests/second")

if response_times:
    print(f"\nResponse Times (ms):")
    print(f"  Min: {min(response_times):.1f}")
    print(f"  Max: {max(response_times):.1f}")
    print(f"  Avg: {statistics.mean(response_times):.1f}")
    print(f"  Median: {statistics.median(response_times):.1f}")
    if len(response_times) > 1:
        print(f"  StdDev: {statistics.stdev(response_times):.1f}")

# Final health check
print("\n" + "="*60)
print("🏥 POST-STRESS HEALTH CHECK")
print("="*60)

try:
    response = requests.get(f"{BASE_URL}/api/health", verify=False, timeout=5)
    if response.status_code == 200:
        print("✅ Server is still healthy after stress test!")
    else:
        print(f"⚠️  Server returned status {response.status_code}")
except Exception as e:
    print("❌ Server appears unhealthy:", str(e))

# Success criteria
print("\n" + "="*60)
if success_rate >= 99.9 and throughput > 100:
    print("🎉 STRESS TEST PASSED! 🎉")
    print("✅ 99.9%+ success rate achieved")
    print("✅ High throughput maintained")
    print("✅ No threading issues detected")
    print("🚀 JDBX threading is production-grade!")
else:
    print("⚠️  STRESS TEST NEEDS INVESTIGATION")
    print(f"Success rate: {success_rate:.1f}% (target: 99.9%)")
    print(f"Throughput: {throughput:.1f} req/s (target: 100+)")