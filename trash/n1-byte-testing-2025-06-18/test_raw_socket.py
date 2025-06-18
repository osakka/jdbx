#!/usr/bin/env python3
"""
Test with raw socket to eliminate client library issues
"""

import socket
import time

def test_raw_request(size):
    """Send a raw HTTP request with exact byte count"""
    
    # Create test data
    data = "x" * size
    body = f'{{"test": "{data}"}}'
    
    # Build HTTP request
    request = f"""POST /api/documents HTTP/1.1\r
Host: localhost:5000\r
Content-Type: application/json\r
Content-Length: {len(body)}\r
Authorization: Bearer dummy\r
\r
{body}"""
    
    print(f"\n📊 Testing {size} bytes (body length: {len(body)})")
    print(f"Request length: {len(request)} bytes")
    
    try:
        # Connect and send
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('localhost', 5000))
        
        # Send all bytes
        sent = s.sendall(request.encode())
        print(f"✅ Sent all bytes successfully")
        
        # Read response
        response = b""
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            response += chunk
            if b"\r\n\r\n" in response:
                # Check if we have complete response
                header_end = response.find(b"\r\n\r\n")
                headers = response[:header_end].decode()
                if "Content-Length: 0" in headers or "400 Bad Request" in headers:
                    break
        
        print(f"Response: {response.decode()[:200]}...")
        
        s.close()
        
    except Exception as e:
        print(f"❌ Error: {e}")

# Test various sizes
sizes = [100, 1000, 3000, 3900, 3950, 4000]
for size in sizes:
    test_raw_request(size)
    time.sleep(0.1)