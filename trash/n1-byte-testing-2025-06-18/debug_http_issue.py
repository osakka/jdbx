#!/usr/bin/env python3
"""
Debug HTTP issues without SSL
"""

import socket
import json

def send_exact_request(size):
    """Send exact HTTP request with proper content length"""
    
    # First get a token
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('localhost', 5000))
    
    login_body = '{"username":"admin","password":"secure123456789"}'
    login_req = f"POST /api/auth/login HTTP/1.1\r\n"
    login_req += f"Host: localhost:5000\r\n"
    login_req += f"Content-Type: application/json\r\n"
    login_req += f"Content-Length: {len(login_body)}\r\n"
    login_req += f"Connection: close\r\n"
    login_req += f"\r\n"
    login_req += login_body
    
    print(f"Login request length: {len(login_req)} bytes")
    print(f"Login body length: {len(login_body)} bytes")
    
    sock.sendall(login_req.encode())
    
    response = b""
    while True:
        data = sock.recv(4096)
        if not data:
            break
        response += data
    
    sock.close()
    
    # Parse token
    resp_parts = response.split(b'\r\n\r\n')
    if len(resp_parts) > 1:
        token = json.loads(resp_parts[1])['token']
        print(f"Got token: {token[:20]}...")
    else:
        print("Failed to get token")
        return
    
    # Now send document
    doc = {"title": f"Test {size} bytes", "type": "test", "data": "x" * size}
    body = json.dumps(doc)
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('localhost', 5000))
    
    req = f"POST /api/documents HTTP/1.1\r\n"
    req += f"Host: localhost:5000\r\n"
    req += f"Authorization: Bearer {token}\r\n"
    req += f"Content-Type: application/json\r\n"
    req += f"Content-Length: {len(body)}\r\n"
    req += f"Connection: close\r\n"
    req += f"\r\n"
    req += body
    
    print(f"\nDocument request:")
    print(f"  Total request length: {len(req)} bytes")
    print(f"  Body length: {len(body)} bytes")
    print(f"  Content-Length header: {len(body)}")
    
    # Send in chunks to debug
    sent = sock.send(req.encode())
    print(f"  Sent: {sent} bytes")
    
    # Try to read response
    sock.settimeout(5)
    try:
        response = b""
        while True:
            data = sock.recv(4096)
            if not data:
                break
            response += data
        
        if response:
            status_line = response.split(b'\r\n')[0]
            print(f"  Response: {status_line}")
            if b'400' in status_line:
                body = response.split(b'\r\n\r\n')[1]
                print(f"  Error: {body}")
        else:
            print("  No response received")
            
    except socket.timeout:
        print("  Timeout waiting for response")
    except Exception as e:
        print(f"  Error: {e}")
    
    sock.close()

def test_with_raw_socket():
    """Test with raw socket to ensure exact byte count"""
    print("🔍 Testing with raw socket (no HTTP library)\n")
    
    # Test small document
    send_exact_request(100)
    
    print("\n" + "-" * 50 + "\n")
    
    # Test medium document  
    send_exact_request(2000)

if __name__ == "__main__":
    test_with_raw_socket()