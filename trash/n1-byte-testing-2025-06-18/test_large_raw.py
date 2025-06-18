#!/usr/bin/env python3
"""
Test large documents with raw sockets
"""

import socket
import json

def test_size(size):
    """Test a specific document size"""
    # Get token
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('localhost', 5000))
    
    login = '{"username":"admin","password":"secure123456789"}'
    req = f"POST /api/auth/login HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\nContent-Length: {len(login)}\r\nConnection: close\r\n\r\n{login}"
    sock.sendall(req.encode())
    
    resp = b""
    while True:
        data = sock.recv(4096)
        if not data:
            break
        resp += data
    sock.close()
    
    token = json.loads(resp.split(b'\r\n\r\n')[1])['token']
    
    # Send large document
    doc = {"title": f"Size {size}", "type": "test", "data": "x" * size}
    body = json.dumps(doc)
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('localhost', 5000))
    
    req = f"POST /api/documents HTTP/1.1\r\n"
    req += f"Host: localhost\r\n"
    req += f"Authorization: Bearer {token}\r\n"
    req += f"Content-Type: application/json\r\n"
    req += f"Content-Length: {len(body)}\r\n"
    req += f"Connection: close\r\n"
    req += f"\r\n"
    req += body
    
    req_bytes = req.encode()
    print(f"Testing {size:,} byte document (total request: {len(req_bytes):,} bytes)...")
    
    # Send all at once
    sent = sock.sendall(req_bytes)
    
    # Get response
    sock.settimeout(5)
    try:
        resp = b""
        while True:
            data = sock.recv(4096)
            if not data:
                break
            resp += data
        
        if resp:
            status = resp.split(b'\r\n')[0]
            if b'201' in status:
                print(f"  ✅ Success")
                return True
            else:
                print(f"  ❌ Failed: {status}")
                if b'413' in status:
                    print("     Server rejected: Request too large")
                return False
    except Exception as e:
        print(f"  ❌ Error: {e}")
        return False
    finally:
        sock.close()

def main():
    print("🧪 Testing Large Documents with Raw Sockets\n")
    
    sizes = [1000, 5000, 10000, 50000, 100000, 500000, 1000000]
    
    for size in sizes:
        if not test_size(size):
            print(f"\nFound limit around {size:,} bytes")
            
            # Binary search for exact limit
            if size > 10000:
                low = sizes[sizes.index(size)-1]
                high = size
                
                print(f"\nBinary search between {low:,} and {high:,}...")
                
                while high - low > 1000:
                    mid = (low + high) // 2
                    if test_size(mid):
                        low = mid
                    else:
                        high = mid
                
                print(f"\n📊 Maximum working document size: ~{low:,} bytes")
            break
    
    # Check server logs
    print("\nChecking server logs for errors...")
    import subprocess
    result = subprocess.run(['tail', '-n', '20', '/opt/jdbx/build/var/jdbxd.log'], 
                          capture_output=True, text=True)
    
    for line in result.stdout.split('\n'):
        if 'ERROR' in line or 'limit' in line or 'large' in line:
            print(f"LOG: {line}")

if __name__ == "__main__":
    main()