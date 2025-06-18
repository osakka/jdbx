#!/usr/bin/env python3
"""
Test to find exactly where N-1 happens
"""

import socket
import time

def send_exact_request(body_size):
    """Send a request with exact body size and see what happens"""
    
    # Create body
    body = '{"data":"' + 'x' * (body_size - 12) + '"}'
    
    # Build minimal headers
    headers = f"""POST /api/documents HTTP/1.1\r
Host: localhost:5000\r
Content-Type: application/json\r
Content-Length: {len(body)}\r
Authorization: Bearer dummy\r
\r
"""
    
    request = headers + body
    
    print(f"\n📊 Testing body size: {len(body)}")
    print(f"Headers size: {len(headers)}")
    print(f"Total request size: {len(request)}")
    print(f"Content-Length header: {len(body)}")
    
    try:
        # Connect and send
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('localhost', 5000))
        
        # Send request in chunks to see what happens
        sent = 0
        chunk_size = 1000
        
        while sent < len(request):
            end = min(sent + chunk_size, len(request))
            chunk = request[sent:end]
            n = s.send(chunk.encode())
            sent += n
            print(f"  Sent {n} bytes (total: {sent}/{len(request)})")
        
        print(f"✅ All {sent} bytes sent")
        
        # Important: shutdown write side to signal we're done sending
        # but keep read side open
        s.shutdown(socket.SHUT_WR)
        print("  Shutdown write side")
        
        # Try to read response
        s.settimeout(2.0)
        try:
            response = s.recv(4096)
            print(f"Response: {response.decode()[:100]}...")
        except socket.timeout:
            print("❌ No response (timeout)")
        except Exception as e:
            print(f"❌ Response error: {e}")
        
        s.close()
        
    except Exception as e:
        print(f"❌ Error: {e}")

# Test exact boundaries
# Initial buffer is 4096
# We reserve 1 byte for null terminator
# So we should be able to handle requests up to 4095 bytes total

# Test around the boundary
for body_size in [3800, 3850, 3900, 3950, 3960, 3962, 3963, 3964, 3965]:
    send_exact_request(body_size)
    time.sleep(0.5)