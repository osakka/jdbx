#!/usr/bin/env python3
"""Test chunked sending to diagnose the issue"""

import socket
import ssl
import json
import time

def test_chunked(size, chunk_size=1024):
    """Test sending in chunks"""
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
            
            # Test document creation with chunked sending
            sock2 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            ssl_sock2 = context.wrap_socket(sock2, server_hostname='localhost')
            ssl_sock2.connect(('localhost', 5000))
            
            # Create document
            content = "x" * size
            doc = {"name": f"Test {size}", "content": content}
            body = json.dumps(doc)
            
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
            
            print(f"\nTesting {size} byte content (chunked, {chunk_size} byte chunks):")
            print(f"  Total request: {len(request_bytes)} bytes")
            print(f"  Body length: {len(body)} bytes")
            
            # Send in chunks
            sent = 0
            chunk_count = 0
            while sent < len(request_bytes):
                to_send = min(chunk_size, len(request_bytes) - sent)
                chunk_data = request_bytes[sent:sent+to_send]
                n = ssl_sock2.send(chunk_data)
                sent += n
                chunk_count += 1
                print(f"  Chunk {chunk_count}: sent {n} bytes (total: {sent}/{len(request_bytes)})")
                
                # Small delay between chunks
                if sent < len(request_bytes):
                    time.sleep(0.01)
            
            print("  All chunks sent, reading response...")
            
            # Read response
            response2 = b""
            start_time = time.time()
            while time.time() - start_time < 5:
                try:
                    chunk = ssl_sock2.recv(4096)
                    if not chunk:
                        break
                    response2 += chunk
                except ssl.SSLWantReadError:
                    time.sleep(0.1)
                    continue
                except:
                    break
            
            if response2:
                print(f"  Response: {len(response2)} bytes")
                if b"201" in response2:
                    print("  ✅ SUCCESS")
                else:
                    print("  ❌ FAILED")
                    # Check what we got
                    if b"HTTP" in response2:
                        status_line = response2.split(b"\r\n")[0]
                        print(f"  Status: {status_line}")
                        # Print full response if it's an error
                        if b"400" in status_line or b"500" in status_line:
                            print(f"  Full response:\n{response2.decode()}")
            else:
                print("  ❌ No response received")
                
            ssl_sock2.close()
            
    except Exception as e:
        print(f"Error: {e}")
    finally:
        try:
            ssl_sock.close()
        except:
            pass

if __name__ == "__main__":
    # Test the problematic size with different chunk sizes
    test_chunked(3584, chunk_size=4096)  # Send all at once
    test_chunked(3584, chunk_size=2048)  # Two chunks
    test_chunked(3584, chunk_size=1024)  # Multiple chunks
    test_chunked(3584, chunk_size=512)   # Many small chunks