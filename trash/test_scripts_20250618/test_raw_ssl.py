#!/usr/bin/env python3
"""Test raw SSL connection to debug issues"""

import ssl
import socket
import time

def test_raw_ssl_request():
    """Test a raw SSL request"""
    # Create socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    
    # Wrap with SSL
    context = ssl.create_default_context()
    context.check_hostname = False
    context.verify_mode = ssl.CERT_NONE
    
    ssl_sock = context.wrap_socket(sock, server_hostname='localhost')
    
    try:
        # Connect
        ssl_sock.connect(('localhost', 5000))
        print("✅ SSL connection established")
        
        # Build a simple request
        request = (
            "GET /api/health HTTP/1.1\r\n"
            "Host: localhost:5000\r\n"
            "User-Agent: test-raw-ssl\r\n"
            "Accept: */*\r\n"
            "Connection: close\r\n"
            "\r\n"
        )
        
        print(f"📤 Sending request ({len(request)} bytes):")
        print(request)
        
        # Send request
        ssl_sock.sendall(request.encode())
        
        # Read response
        response = b""
        while True:
            chunk = ssl_sock.recv(4096)
            if not chunk:
                break
            response += chunk
            # Check if we have complete headers
            if b"\r\n\r\n" in response:
                # Check if we have Content-Length
                headers = response.split(b"\r\n\r\n")[0].decode()
                content_length = 0
                for line in headers.split("\r\n"):
                    if line.lower().startswith("content-length:"):
                        content_length = int(line.split(":")[1].strip())
                        break
                
                # Check if we have complete body
                body_start = response.find(b"\r\n\r\n") + 4
                body_received = len(response) - body_start
                if body_received >= content_length:
                    break
        
        print(f"📥 Received response ({len(response)} bytes):")
        print(response.decode()[:500])
        
        # Test a POST request with body
        print("\n" + "="*50 + "\n")
        
        # Create new connection for POST
        sock2 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        ssl_sock2 = context.wrap_socket(sock2, server_hostname='localhost')
        ssl_sock2.connect(('localhost', 5000))
        
        body = '{"username":"admin","password":"secure123456789"}'
        request2 = (
            "POST /api/auth/login HTTP/1.1\r\n"
            "Host: localhost:5000\r\n"
            "User-Agent: test-raw-ssl\r\n"
            "Accept: */*\r\n"
            "Content-Type: application/json\r\n"
            f"Content-Length: {len(body)}\r\n"
            "Connection: close\r\n"
            "\r\n"
            f"{body}"
        )
        
        print(f"📤 Sending POST request ({len(request2)} bytes):")
        print(f"   Body length: {len(body)}")
        print(f"   Total request: {len(request2)}")
        
        # Send in one go
        ssl_sock2.sendall(request2.encode())
        
        # Read response
        response2 = b""
        start_time = time.time()
        while time.time() - start_time < 5:  # 5 second timeout
            try:
                chunk = ssl_sock2.recv(4096)
                if not chunk:
                    break
                response2 += chunk
            except ssl.SSLWantReadError:
                time.sleep(0.1)
                continue
            except socket.timeout:
                break
        
        print(f"📥 Received POST response ({len(response2)} bytes):")
        if response2:
            print(response2.decode()[:500])
        else:
            print("No response received!")
            
        ssl_sock2.close()
        
    finally:
        ssl_sock.close()

if __name__ == "__main__":
    print("Raw SSL Connection Test")
    print("=" * 50)
    test_raw_ssl_request()