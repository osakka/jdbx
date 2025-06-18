#!/usr/bin/env python3
"""Test to diagnose 4KB+ payload issues"""

import socket
import ssl
import time
import json

def test_exact_size(size):
    """Test exact payload size with raw SSL"""
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
        
        # First login to get token
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
            
            # Now test large payload
            sock2 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            ssl_sock2 = context.wrap_socket(sock2, server_hostname='localhost')
            ssl_sock2.connect(('localhost', 5000))
            
            # Create document with exact size
            content = "x" * size
            doc = {"name": f"Test {size}", "content": content}
            body = json.dumps(doc)
            
            request = (
                "POST /api/documents HTTP/1.1\r\n"
                "Host: localhost:5000\r\n"
                f"Authorization: Bearer {token}\r\n"
                "Content-Type: application/json\r\n"
                f"Content-Length: {len(body)}\r\n"
                "Connection: close\r\n"
                "\r\n"
                f"{body}"
            )
            
            print(f"📤 Sending {size} byte payload (total request: {len(request)} bytes)")
            print(f"   Headers length: {len(request) - len(body)} bytes")
            print(f"   Body length: {len(body)} bytes")
            
            # Send in chunks to see where it fails
            request_bytes = request.encode()
            sent = 0
            chunk_size = 1024
            
            while sent < len(request_bytes):
                try:
                    to_send = min(chunk_size, len(request_bytes) - sent)
                    n = ssl_sock2.send(request_bytes[sent:sent+to_send])
                    sent += n
                    print(f"   Sent chunk: {n} bytes (total sent: {sent}/{len(request_bytes)})")
                    time.sleep(0.01)  # Small delay between chunks
                except Exception as e:
                    print(f"   ❌ Send failed at {sent} bytes: {e}")
                    return False
            
            print("   ✅ All data sent, waiting for response...")
            
            # Read response
            response2 = b""
            start_time = time.time()
            while time.time() - start_time < 5:
                try:
                    chunk = ssl_sock2.recv(4096)
                    if not chunk:
                        break
                    response2 += chunk
                    if b"\r\n\r\n" in response2:
                        # Check if we have complete response
                        headers_end = response2.find(b"\r\n\r\n")
                        headers = response2[:headers_end].decode()
                        if "Content-Length:" in headers:
                            for line in headers.split("\r\n"):
                                if line.startswith("Content-Length:"):
                                    content_len = int(line.split(":")[1].strip())
                                    body_start = headers_end + 4
                                    if len(response2) >= body_start + content_len:
                                        break
                except ssl.SSLWantReadError:
                    time.sleep(0.1)
                    continue
                except Exception as e:
                    print(f"   ❌ Read failed: {e}")
                    break
            
            if response2:
                print(f"   📥 Received {len(response2)} bytes response")
                if b"201 Created" in response2:
                    print("   ✅ SUCCESS: Document created!")
                    return True
                else:
                    status_line = response2.split(b"\r\n")[0].decode()
                    print(f"   ❌ FAILED: {status_line}")
                    if len(response2) < 200:
                        print(f"   Full response: {response2.decode()}")
            else:
                print("   ❌ No response received")
                
            ssl_sock2.close()
            
        else:
            print("   ❌ Login failed")
            
    except Exception as e:
        print(f"   ❌ Connection error: {e}")
        
    finally:
        try:
            ssl_sock.close()
        except:
            pass
            
    return False

if __name__ == "__main__":
    print("Testing 4KB+ Payload Issue")
    print("=" * 50)
    
    # Test specific sizes around the failure point
    sizes = [3584, 3600, 3700, 3800, 3900, 4000, 4096]
    
    for size in sizes:
        print(f"\nTesting {size} bytes...")
        if test_exact_size(size):
            print(f"✅ {size} bytes: SUCCESS")
        else:
            print(f"❌ {size} bytes: FAILED")
            # Check if server is still alive
            time.sleep(1)
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(1)
                sock.connect(('localhost', 5000))
                sock.close()
                print("   Server is still responding")
            except:
                print("   ⚠️  Server is not responding - may have crashed")