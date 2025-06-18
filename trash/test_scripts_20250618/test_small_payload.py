#!/usr/bin/env python3
"""Test small payload to verify basic functionality"""

import socket
import ssl
import json

def test_payload(size):
    """Test specific payload size"""
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
        
        # Login first
        login_body = '{"username":"admin","password":"secure123456789"}'
        login_request = (
            "POST /api/auth/login HTTP/1.1\r\n"
            "Host: localhost:5000\r\n"
            "Content-Type: application/json\r\n"
            f"Content-Length: {len(login_body)}\r\n"
            "Connection: close\r\n"
            "\r\n"
            f"{login_body}"
        )
        
        ssl_sock.sendall(login_request.encode())
        
        # Read login response
        response = b""
        while True:
            try:
                chunk = ssl_sock.recv(4096)
                if not chunk:
                    break
                response += chunk
            except:
                break
                
        # Extract token
        if b"200 OK" in response:
            body_start = response.find(b"\r\n\r\n") + 4
            body = response[body_start:].decode()
            token = json.loads(body)["token"]
            ssl_sock.close()
            
            # Test document creation
            sock2 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            ssl_sock2 = context.wrap_socket(sock2, server_hostname='localhost')
            ssl_sock2.connect(('localhost', 5000))
            
            # Create small document
            content = "x" * size
            doc = {"name": f"Test {size}", "content": content}
            body = json.dumps(doc)
            
            # Build request manually to ensure Content-Length is correct
            headers = (
                "POST /api/documents HTTP/1.1\r\n"
                "Host: localhost:5000\r\n"
                f"Authorization: Bearer {token}\r\n"
                "Content-Type: application/json\r\n"
                f"Content-Length: {len(body)}\r\n"
                "Connection: close\r\n"
                "\r\n"
            )
            
            request = headers + body
            request_bytes = request.encode('utf-8')
            
            print(f"\nTesting {size} byte content:")
            print(f"  Body: {len(body)} bytes")
            print(f"  Headers: {len(headers)} chars")
            print(f"  Total request: {len(request_bytes)} bytes")
            print(f"  Content-Length header: {len(body)}")
            
            # Send all at once
            sent = ssl_sock2.send(request_bytes)
            print(f"  Sent: {sent} bytes")
            
            # Read response
            response2 = b""
            while True:
                try:
                    chunk = ssl_sock2.recv(4096)
                    if not chunk:
                        break
                    response2 += chunk
                except:
                    break
            
            print(f"  Response: {len(response2)} bytes")
            if b"201" in response2:
                print("  ✅ SUCCESS")
            else:
                print("  ❌ FAILED")
                if response2:
                    print(f"  Response: {response2[:200]}")
                    
            ssl_sock2.close()
            
    except Exception as e:
        print(f"Error: {e}")
    finally:
        try:
            ssl_sock.close()
        except:
            pass

if __name__ == "__main__":
    # Test different sizes
    for size in [100, 1000, 2000, 3000, 3500, 3584]:
        test_payload(size)