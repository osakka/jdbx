#!/usr/bin/env python3
"""
Debug large request handling
"""

import socket
import ssl
import json
import time

def send_large_request():
    """Send a large request manually to debug"""
    print("Debug Large Request Handling")
    print("=" * 50)
    
    # Create SSL context
    context = ssl.create_default_context()
    context.check_hostname = False
    context.verify_mode = ssl.CERT_NONE
    
    # First, login to get a token
    print("\n1️⃣ Getting auth token...")
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    ssl_sock = context.wrap_socket(sock)
    ssl_sock.settimeout(10)
    ssl_sock.connect(('localhost', 5000))
    
    login_body = json.dumps({
        "username": "admin",
        "password": "secure123456789"
    })
    
    login_request = f"POST /api/auth/login HTTP/1.1\r\n"
    login_request += f"Host: localhost:5000\r\n"
    login_request += f"Content-Type: application/json\r\n"
    login_request += f"Content-Length: {len(login_body)}\r\n"
    login_request += f"\r\n"
    login_request += login_body
    
    ssl_sock.send(login_request.encode())
    
    # Read response
    response = b""
    while b"\r\n\r\n" not in response:
        response += ssl_sock.recv(1024)
    
    # Parse response
    headers, body = response.split(b"\r\n\r\n", 1)
    print(f"Login response headers:\n{headers.decode()}")
    
    # Get content length
    content_length = 0
    for line in headers.decode().split("\r\n"):
        if line.lower().startswith("content-length:"):
            content_length = int(line.split(":")[1].strip())
    
    # Read remaining body
    while len(body) < content_length:
        body += ssl_sock.recv(1024)
    
    auth_response = json.loads(body.decode())
    token = auth_response.get("token")
    print(f"Got token: {token[:20]}...")
    
    ssl_sock.close()
    
    # Now send a large document
    print("\n2️⃣ Sending 5KB document...")
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    ssl_sock = context.wrap_socket(sock)
    ssl_sock.settimeout(10)
    ssl_sock.connect(('localhost', 5000))
    
    # Create 5KB document
    doc_content = "x" * (5 * 1024)
    doc_body = json.dumps({
        "title": "Test 5KB document",
        "content": doc_content
    })
    
    print(f"Document body size: {len(doc_body)} bytes")
    
    doc_request = f"POST /api/documents HTTP/1.1\r\n"
    doc_request += f"Host: localhost:5000\r\n"
    doc_request += f"Authorization: Bearer {token}\r\n"
    doc_request += f"Content-Type: application/json\r\n"
    doc_request += f"Content-Length: {len(doc_body)}\r\n"
    doc_request += f"\r\n"
    
    # Send headers first
    print("Sending headers...")
    ssl_sock.send(doc_request.encode())
    
    # Send body in chunks
    print("Sending body in 1KB chunks...")
    sent = 0
    chunk_size = 1024
    doc_body_bytes = doc_body.encode()
    
    while sent < len(doc_body_bytes):
        chunk = doc_body_bytes[sent:sent + chunk_size]
        bytes_sent = ssl_sock.send(chunk)
        sent += bytes_sent
        print(f"  Sent chunk: {bytes_sent} bytes (total: {sent}/{len(doc_body_bytes)})")
        time.sleep(0.1)  # Small delay between chunks
    
    print("\nWaiting for response...")
    
    try:
        response = b""
        while True:
            data = ssl_sock.recv(4096)
            if not data:
                break
            response += data
            if b"\r\n\r\n" in response:
                # Check if we have complete response
                headers, body = response.split(b"\r\n\r\n", 1)
                if b"Content-Length:" in headers:
                    for line in headers.split(b"\r\n"):
                        if line.lower().startswith(b"content-length:"):
                            cl = int(line.split(b":")[1].strip())
                            if len(body) >= cl:
                                break
    except socket.timeout:
        print("Timeout waiting for response")
    
    if response:
        print(f"\nResponse received ({len(response)} bytes):")
        print(response[:500].decode('utf-8', errors='ignore'))
    else:
        print("\nNo response received")
    
    ssl_sock.close()

if __name__ == "__main__":
    send_large_request()