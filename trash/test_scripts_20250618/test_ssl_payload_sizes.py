#!/usr/bin/env python3
"""
Test SSL payload handling at various sizes
"""

import requests
import json
import urllib3
import time
import socket
import ssl

urllib3.disable_warnings()

BASE_URL = "https://localhost:5000"

def test_raw_ssl_send(size_kb):
    """Test sending data directly over SSL socket"""
    print(f"\n🔌 Testing raw SSL send of {size_kb}KB...")
    
    try:
        # Create SSL context
        context = ssl.create_default_context()
        context.check_hostname = False
        context.verify_mode = ssl.CERT_NONE
        
        # Create socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        ssl_sock = context.wrap_socket(sock)
        ssl_sock.settimeout(10)
        ssl_sock.connect(('localhost', 5000))
        
        # Generate content
        content = "x" * (size_kb * 1024)
        
        # Build HTTP request
        body = json.dumps({
            "username": "admin",
            "password": "secure123456789"
        })
        
        request = f"POST /api/auth/login HTTP/1.1\r\n"
        request += f"Host: localhost:5000\r\n"
        request += f"Content-Type: application/json\r\n"
        request += f"Content-Length: {len(body)}\r\n"
        request += f"Connection: close\r\n"
        request += f"\r\n"
        request += body
        
        # Send in chunks
        chunk_size = 4096
        sent = 0
        request_bytes = request.encode()
        
        while sent < len(request_bytes):
            chunk = request_bytes[sent:sent + chunk_size]
            bytes_sent = ssl_sock.send(chunk)
            sent += bytes_sent
            print(f"  Sent {bytes_sent} bytes (total: {sent}/{len(request_bytes)})")
        
        # Read response
        response = b""
        while True:
            try:
                data = ssl_sock.recv(4096)
                if not data:
                    break
                response += data
            except socket.timeout:
                break
        
        print(f"  Response: {response[:200].decode('utf-8', errors='ignore')}")
        
        ssl_sock.close()
        return True
        
    except Exception as e:
        print(f"  ❌ Error: {e}")
        return False

def test_requests_library(size_kb):
    """Test using requests library"""
    print(f"\n📚 Testing requests library with {size_kb}KB...")
    
    session = requests.Session()
    session.verify = False
    
    try:
        # First login
        response = session.post(f"{BASE_URL}/api/auth/login", json={
            "username": "admin",
            "password": "secure123456789"
        }, timeout=10)
        
        if response.status_code != 200:
            print(f"  ❌ Login failed: {response.status_code}")
            return False
            
        token = response.json()["token"]
        session.headers["Authorization"] = f"Bearer {token}"
        
        # Now send large payload
        content = "x" * (size_kb * 1024)
        doc = {
            "title": f"Test {size_kb}KB",
            "content": content
        }
        
        response = session.post(f"{BASE_URL}/api/documents", json=doc, timeout=30)
        print(f"  Response: {response.status_code}")
        return response.status_code in [201, 413]
        
    except Exception as e:
        print(f"  ❌ Error: {e}")
        return False

def main():
    print("SSL Payload Size Testing")
    print("=" * 50)
    
    # Test various sizes
    sizes = [1, 5, 10, 50, 100, 500, 1024]
    
    print("\n1️⃣ Raw SSL Socket Tests")
    for size in [1]:  # Just test login first
        test_raw_ssl_send(size)
    
    print("\n2️⃣ Requests Library Tests")
    for size in sizes:
        if not test_requests_library(size):
            print(f"  Failed at {size}KB, stopping tests")
            break
    
    # Check server health
    print("\n🏥 Server health check...")
    try:
        session = requests.Session()
        session.verify = False
        response = session.get(f"{BASE_URL}/api/health", timeout=5)
        if response.status_code == 200:
            print("✅ Server is healthy")
        else:
            print(f"❌ Server returned {response.status_code}")
    except Exception as e:
        print(f"❌ Server not responding: {e}")

if __name__ == "__main__":
    main()