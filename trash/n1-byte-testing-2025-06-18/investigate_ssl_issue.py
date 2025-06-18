#!/usr/bin/env python3
"""
Investigate the SSL N-1 byte issue with Python requests
"""

import socket
import ssl
import json

def test_raw_ssl_request():
    """Test with raw SSL socket to see if issue is in requests library"""
    
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
        
        # Create a simple JSON payload
        body = json.dumps({"username": "admin", "password": "secure123456789"})
        
        # Build HTTP request
        request = f"POST /api/auth/login HTTP/1.1\r\n"
        request += f"Host: localhost:5000\r\n"
        request += f"Content-Type: application/json\r\n"
        request += f"Content-Length: {len(body)}\r\n"
        request += f"Connection: close\r\n"
        request += f"\r\n"
        request += body
        
        print(f"📤 Sending request ({len(request)} bytes):")
        print(f"Body length: {len(body)}")
        print(f"Content-Length header: {len(body)}")
        
        # Send all data
        sent = ssl_sock.sendall(request.encode())
        print(f"✅ Sent all data successfully")
        
        # Properly shutdown SSL
        ssl_sock.shutdown(socket.SHUT_WR)
        
        # Read response
        response = b""
        while True:
            data = ssl_sock.recv(4096)
            if not data:
                break
            response += data
        
        print(f"📥 Response received:")
        print(response.decode('utf-8', errors='ignore'))
        
    finally:
        ssl_sock.close()

def test_with_proper_close():
    """Test if proper SSL shutdown fixes the issue"""
    import requests
    from requests.adapters import HTTPAdapter
    
    class ProperCloseAdapter(HTTPAdapter):
        def send(self, request, **kwargs):
            response = super().send(request, **kwargs)
            # Try to properly close the SSL connection
            if hasattr(response.raw, '_original_response'):
                if hasattr(response.raw._original_response, '_ssl'):
                    try:
                        response.raw._original_response._ssl.shutdown(socket.SHUT_WR)
                    except:
                        pass
            return response
    
    session = requests.Session()
    session.mount('https://', ProperCloseAdapter())
    session.verify = False
    
    response = session.post("https://localhost:5000/api/auth/login", 
                           json={"username": "admin", "password": "secure123456789"})
    print(f"Custom adapter response: {response.status_code}")

if __name__ == "__main__":
    print("🔬 Investigating SSL N-1 byte issue\n")
    
    print("Test 1: Raw SSL socket")
    print("-" * 40)
    test_raw_ssl_request()
    
    print("\n\nTest 2: Requests with proper close adapter")
    print("-" * 40)
    try:
        test_with_proper_close()
    except Exception as e:
        print(f"Error: {e}")