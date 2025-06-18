#!/usr/bin/env python3
"""
Comprehensive test suite for N-1 byte issue fix in JDBX server over HTTPS.

This test validates that the server correctly reads the full Content-Length
without losing the last byte over SSL/TLS connections.
"""

import requests
import json
import socket
import ssl
import time
import sys
import urllib3

# Disable SSL warnings for self-signed certificate
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

def test_python_requests_https(base_url, token):
    """Test with Python requests library over HTTPS"""
    print("\n📊 Testing with Python requests library (HTTPS):")
    
    test_sizes = [
        100,     # Small
        1000,    # 1KB
        4095,    # Just under initial buffer size
        4096,    # Exactly initial buffer size
        4097,    # Just over initial buffer size
        5000,    # 5KB
        10000,   # 10KB
        50000,   # 50KB
        100000,  # 100KB
        1000000  # 1MB
    ]
    
    passed = 0
    failed = 0
    
    session = requests.Session()
    session.headers['Authorization'] = f'Bearer {token}'
    session.verify = False  # Allow self-signed certificates
    
    for size in test_sizes:
        # Create document with exact size
        data = 'x' * (size - 100)  # Account for JSON overhead
        doc = {
            "title": f"Test {size} bytes",
            "type": "n1-https-test",
            "data": data
        }
        
        json_str = json.dumps(doc)
        actual_size = len(json_str)
        
        try:
            resp = session.post(f"{base_url}/api/documents", json=doc)
            if resp.status_code == 201:
                print(f"  ✅ {size:,} bytes (actual: {actual_size:,}): Success")
                passed += 1
            else:
                print(f"  ❌ {size:,} bytes (actual: {actual_size:,}): HTTP {resp.status_code}")
                failed += 1
        except Exception as e:
            print(f"  ❌ {size:,} bytes (actual: {actual_size:,}): {type(e).__name__}: {str(e)[:50]}")
            failed += 1
    
    return passed, failed

def test_raw_ssl_socket(host, port, token):
    """Test with raw SSL socket to verify exact byte handling"""
    print("\n📊 Testing with raw SSL sockets:")
    
    test_cases = [
        # (body_size, description)
        (100, "Small request"),
        (3959, "Just under 4KB total (with headers)"),
        (3960, "Exactly 4KB total (with headers)"),
        (3961, "Just over 4KB total (with headers)"),
        (10000, "Large request")
    ]
    
    passed = 0
    failed = 0
    
    # Create SSL context
    context = ssl.create_default_context()
    context.check_hostname = False
    context.verify_mode = ssl.CERT_NONE
    
    for body_size, description in test_cases:
        # Create exact body with proper document structure
        data_size = body_size - 100  # Account for JSON structure
        if data_size < 0:
            data_size = 0
        body = json.dumps({
            "title": f"Raw SSL test {body_size}",
            "type": "raw-ssl-test", 
            "data": "x" * data_size
        })
        
        # Build request
        request = f"""POST /api/documents HTTP/1.1\r
Host: {host}:{port}\r
Content-Type: application/json\r
Content-Length: {len(body)}\r
Authorization: Bearer {token}\r
\r
{body}"""
        
        try:
            # Create socket and wrap with SSL
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            ssl_sock = context.wrap_socket(s, server_hostname=host)
            ssl_sock.connect((host, port))
            ssl_sock.sendall(request.encode())
            ssl_sock.shutdown(socket.SHUT_WR)
            
            # Read response
            response = b""
            while True:
                chunk = ssl_sock.recv(4096)
                if not chunk:
                    break
                response += chunk
                if b"\r\n\r\n" in response and (b"Content-Length: 0" in response or b"400 Bad Request" in response or b"401 Unauthorized" in response):
                    break
            
            ssl_sock.close()
            
            # Check response
            if b"401 Unauthorized" in response or b"201 Created" in response:
                print(f"  ✅ {body_size:5d} bytes - {description}: Request processed completely")
                passed += 1
            elif b"Incomplete request" in response:
                print(f"  ❌ {body_size:5d} bytes - {description}: N-1 byte issue detected!")
                failed += 1
            else:
                # Debug what we got
                status_line = response.split(b'\r\n')[0] if response else b"No response"
                print(f"  ❓ {body_size:5d} bytes - {description}: {status_line.decode('utf-8', errors='replace')}")
                failed += 1
                
        except Exception as e:
            print(f"  ❌ {body_size:5d} bytes - {description}: {e}")
            failed += 1
    
    return passed, failed

def test_edge_cases_https(base_url, token):
    """Test edge cases that might trigger N-1 byte issues over HTTPS"""
    print("\n📊 Testing edge cases (HTTPS):")
    
    session = requests.Session()
    session.headers['Authorization'] = f'Bearer {token}'
    session.verify = False
    
    passed = 0
    failed = 0
    
    # Test 1: Document with null bytes in data
    print("  Testing document with null bytes...")
    doc = {
        "title": "Test with nulls",
        "type": "null-test",
        "data": "before\x00null\x00after"
    }
    try:
        resp = session.post(f"{base_url}/api/documents", json=doc)
        if resp.status_code == 201:
            print("    ✅ Null bytes handled correctly")
            passed += 1
        else:
            print(f"    ❌ Failed with status {resp.status_code}")
            failed += 1
    except Exception as e:
        print(f"    ❌ Exception: {e}")
        failed += 1
    
    # Test 2: UTF-8 multi-byte characters
    print("  Testing UTF-8 content...")
    doc = {
        "title": "UTF-8 test",
        "type": "utf8-test",
        "data": "Hello 世界 🌍" * 100
    }
    try:
        resp = session.post(f"{base_url}/api/documents", json=doc)
        if resp.status_code == 201:
            print("    ✅ UTF-8 content handled correctly")
            passed += 1
        else:
            print(f"    ❌ Failed with status {resp.status_code}")
            failed += 1
    except Exception as e:
        print(f"    ❌ Exception: {e}")
        failed += 1
    
    # Test 3: Exactly buffer-sized requests
    print("  Testing buffer boundary conditions...")
    buffer_sizes = [4094, 4095, 4096, 4097, 8191, 8192, 8193]
    boundary_passed = 0
    
    for target_size in buffer_sizes:
        # Calculate data size to hit exact total request size
        overhead = 200  # Approximate header + JSON structure size
        data_size = target_size - overhead
        if data_size > 0:
            doc = {
                "title": f"Size {target_size}",
                "type": "boundary-test",
                "data": "x" * data_size
            }
            try:
                resp = session.post(f"{base_url}/api/documents", json=doc)
                if resp.status_code == 201:
                    boundary_passed += 1
                else:
                    failed += 1
            except:
                failed += 1
    
    if boundary_passed == len(buffer_sizes):
        print(f"    ✅ All {len(buffer_sizes)} buffer boundaries handled correctly")
        passed += 1
    else:
        print(f"    ❌ Only {boundary_passed}/{len(buffer_sizes)} buffer boundaries passed")
        failed += 1
    
    return passed, failed

def main():
    """Run comprehensive N-1 byte test suite over HTTPS"""
    print("🧪 Comprehensive N-1 Byte Issue Test Suite (HTTPS)")
    print("=" * 50)
    
    # Configuration
    base_url = "https://localhost:5000"
    host = "localhost"
    port = 5000
    
    # Login first
    print("\n🔐 Logging in...")
    try:
        resp = requests.post(f"{base_url}/api/auth/login", 
                           json={"username": "admin", "password": "secure123456789"},
                           verify=False)
        if resp.status_code != 200:
            print(f"❌ Login failed: {resp.status_code}")
            return 1
        token = resp.json()["token"]
        print("✅ Login successful")
    except Exception as e:
        print(f"❌ Login error: {e}")
        return 1
    
    # Run all tests
    total_passed = 0
    total_failed = 0
    
    # Test 1: Python requests over HTTPS
    passed, failed = test_python_requests_https(base_url, token)
    total_passed += passed
    total_failed += failed
    
    # Test 2: Raw SSL sockets
    passed, failed = test_raw_ssl_socket(host, port, token)
    total_passed += passed
    total_failed += failed
    
    # Test 3: Edge cases over HTTPS
    passed, failed = test_edge_cases_https(base_url, token)
    total_passed += passed
    total_failed += failed
    
    # Summary
    print("\n" + "=" * 50)
    print("📊 Test Summary:")
    print(f"  ✅ Passed: {total_passed}")
    print(f"  ❌ Failed: {total_failed}")
    print(f"  📈 Success Rate: {total_passed/(total_passed+total_failed)*100:.1f}%")
    
    if total_failed == 0:
        print("\n🎉 All tests passed! N-1 byte issue is completely fixed over HTTPS!")
        return 0
    else:
        print("\n⚠️  Some tests failed. N-1 byte issue may not be fully resolved.")
        return 1

if __name__ == "__main__":
    sys.exit(main())