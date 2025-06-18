#!/usr/bin/env python3
"""
Check the actual error message for large documents
"""

import socket
import ssl
import json

def test_large_doc():
    # Get token
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    context = ssl.create_default_context()
    context.check_hostname = False
    context.verify_mode = ssl.CERT_NONE
    ssl_sock = context.wrap_socket(sock, server_hostname='localhost')
    
    ssl_sock.connect(('localhost', 5000))
    login_body = '{"username":"admin","password":"secure123456789"}'
    login_req = f"POST /api/auth/login HTTP/1.0\r\nHost: localhost:5000\r\nContent-Type: application/json\r\nContent-Length: {len(login_body)}\r\n\r\n{login_body}"
    ssl_sock.sendall(login_req.encode())
    
    response = b""
    while True:
        data = ssl_sock.recv(4096)
        if not data:
            break
        response += data
    
    token = json.loads(response.split(b'\r\n\r\n')[1])['token']
    ssl_sock.close()
    
    # Send 5KB document
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    ssl_sock = context.wrap_socket(sock, server_hostname='localhost')
    ssl_sock.connect(('localhost', 5000))
    
    doc = {"title": "Large test", "type": "test", "data": "x" * 5000}
    body = json.dumps(doc)
    
    req = f"POST /api/documents HTTP/1.0\r\n"
    req += f"Host: localhost:5000\r\n"
    req += f"Authorization: Bearer {token}\r\n"
    req += f"Content-Type: application/json\r\n"
    req += f"Content-Length: {len(body)}\r\n"
    req += f"\r\n"
    req += body
    
    print(f"Sending {len(body)} byte body...")
    ssl_sock.sendall(req.encode())
    
    response = b""
    while True:
        data = ssl_sock.recv(4096)
        if not data:
            break
        response += data
    
    ssl_sock.close()
    
    # Parse response
    parts = response.decode('utf-8').split('\r\n\r\n', 1)
    headers = parts[0]
    body = parts[1] if len(parts) > 1 else ""
    
    print("\nResponse headers:")
    print(headers)
    print("\nResponse body:")
    print(body)
    
    # Check logs for the actual issue
    print("\nChecking logs for errors...")
    import subprocess
    result = subprocess.run(['tail', '-n', '30', '/opt/jdbx/build/var/jdbxd.log'], 
                          capture_output=True, text=True)
    
    for line in result.stdout.split('\n'):
        if 'ERROR' in line or '400' in line or 'Bad Request' in line:
            print(f"LOG: {line}")

if __name__ == "__main__":
    test_large_doc()