#!/usr/bin/env python3
"""
Focused test for HTTP parsing fix - validates that requests with headers
and body in separate TCP packets are handled correctly
"""

import json
import socket
import ssl
import time

# Test configuration
SERVER_HOST = "localhost"
SERVER_PORT = 5000

def create_ssl_socket():
    """Create an SSL socket with self-signed certificate acceptance"""
    context = ssl.create_default_context()
    context.check_hostname = False
    context.verify_mode = ssl.CERT_NONE
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    ssl_sock = context.wrap_socket(sock, server_hostname=SERVER_HOST)
    ssl_sock.connect((SERVER_HOST, SERVER_PORT))
    return ssl_sock

def test_split_request():
    """Test HTTP request with headers and body sent separately"""
    print("="*60)
    print("HTTP PARSING TEST: Headers and Body in Separate Packets")
    print("="*60)
    
    # Login request data
    login_data = json.dumps({
        "username": "admin",
        "password": "secure123456789"
    })
    
    # Construct HTTP request
    request_headers = (
        f"POST /api/auth/login HTTP/1.1\r\n"
        f"Host: {SERVER_HOST}:{SERVER_PORT}\r\n"
        f"Content-Type: application/json\r\n"
        f"Content-Length: {len(login_data)}\r\n"
        f"Connection: keep-alive\r\n"
        f"\r\n"
    )
    
    print("\nTest 1: Send headers and body together (baseline)")
    print("-"*60)
    
    # Test 1: Send headers and body together (baseline)
    try:
        sock = create_ssl_socket()
        full_request = request_headers + login_data
        sock.sendall(full_request.encode())
        
        # Read response
        response = sock.recv(4096).decode()
        if "200 OK" in response and "token" in response:
            print("✅ PASS: Received JWT token when sent together")
            token_start = response.find('"token":"') + 9
            token_end = response.find('"', token_start)
            token = response[token_start:token_end]
            print(f"   Token: {token[:30]}...")
        else:
            print("❌ FAIL: No valid response")
            print(f"   Response: {response[:200]}")
        sock.close()
    except Exception as e:
        print(f"❌ ERROR: {e}")
    
    time.sleep(0.5)  # Small delay between tests
    
    print("\nTest 2: Send headers first, then body (split request)")
    print("-"*60)
    
    # Test 2: Send headers first, then body separately
    try:
        sock = create_ssl_socket()
        
        # Send headers first
        sock.sendall(request_headers.encode())
        print("📤 Sent headers only")
        
        # Small delay to ensure packets are separate
        time.sleep(0.1)
        
        # Send body separately
        sock.sendall(login_data.encode())
        print("📤 Sent body separately")
        
        # Read response
        response = sock.recv(4096).decode()
        if "200 OK" in response and "token" in response:
            print("✅ PASS: Received JWT token with split request!")
            print("   🎯 HTTP PARSING FIX VERIFIED!")
            token_start = response.find('"token":"') + 9
            token_end = response.find('"', token_start)
            token = response[token_start:token_end]
            print(f"   Token: {token[:30]}...")
        else:
            print("❌ FAIL: Split request failed")
            print(f"   Response: {response[:200]}")
        sock.close()
    except Exception as e:
        print(f"❌ ERROR: {e}")
    
    time.sleep(0.5)
    
    print("\nTest 3: Multiple requests on same connection (keep-alive)")
    print("-"*60)
    
    # Test 3: Multiple requests on same connection
    try:
        sock = create_ssl_socket()
        success_count = 0
        
        for i in range(3):
            # Send login request
            full_request = request_headers + login_data
            sock.sendall(full_request.encode())
            
            # Read response
            response = sock.recv(4096).decode()
            if "200 OK" in response and "token" in response:
                success_count += 1
                print(f"   Request {i+1}: ✅ Success")
            else:
                print(f"   Request {i+1}: ❌ Failed")
        
        if success_count == 3:
            print("✅ PASS: Keep-alive working with multiple requests")
        else:
            print(f"❌ FAIL: Only {success_count}/3 requests succeeded")
        
        sock.close()
    except Exception as e:
        print(f"❌ ERROR: {e}")
    
    print("\n" + "="*60)
    print("HTTP PARSING TEST COMPLETE")
    print("="*60)

if __name__ == "__main__":
    test_split_request()