#!/usr/bin/env python3
import socket
import time

def test_connection_timing():
    print("Testing connection and request timing...")
    
    # Test 1: Connection setup/teardown overhead
    print("\n1. Testing connection setup/teardown (5 requests with new connections):")
    for i in range(5):
        start = time.time()
        
        # Create new connection
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('localhost', 5000))
        
        # Send request
        request = b"GET /api/health HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
        s.send(request)
        
        # Read response
        response = b""
        while True:
            data = s.recv(4096)
            if not data:
                break
            response += data
        
        s.close()
        
        elapsed = (time.time() - start) * 1000
        print(f"   Request {i+1}: {elapsed:.1f}ms (full connection lifecycle)")
    
    # Test 2: Keep-alive performance
    print("\n2. Testing keep-alive (5 requests on same connection):")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('localhost', 5000))
    
    for i in range(5):
        start = time.time()
        
        # Send request
        request = b"GET /api/health HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n"
        s.send(request)
        
        # Read response headers
        headers = b""
        while b"\r\n\r\n" not in headers:
            chunk = s.recv(1)
            if not chunk:
                break
            headers += chunk
        
        # Parse content-length
        content_length = 0
        for line in headers.split(b"\r\n"):
            if line.lower().startswith(b"content-length:"):
                content_length = int(line.split(b":")[1].strip())
                break
        
        # Read body
        body = s.recv(content_length)
        
        elapsed = (time.time() - start) * 1000
        print(f"   Request {i+1}: {elapsed:.1f}ms (reused connection)")
    
    s.close()
    
    # Test 3: Connection idle timeout
    print("\n3. Testing connection idle behavior:")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('localhost', 5000))
    
    # Send first request
    request = b"GET /api/health HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n"
    s.send(request)
    
    # Read response
    headers = b""
    while b"\r\n\r\n" not in headers:
        headers += s.recv(1)
    
    print("   Connection established, waiting 2 seconds...")
    time.sleep(2)
    
    # Try second request
    try:
        start = time.time()
        s.send(request)
        headers = b""
        while b"\r\n\r\n" not in headers:
            headers += s.recv(1)
        elapsed = (time.time() - start) * 1000
        print(f"   Request after 2s idle: {elapsed:.1f}ms")
    except:
        print("   Connection closed by server")
    
    s.close()

if __name__ == "__main__":
    test_connection_timing()