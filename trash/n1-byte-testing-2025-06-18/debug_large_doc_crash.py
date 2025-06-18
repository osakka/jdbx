#!/usr/bin/env python3
"""
Debug the large document crash issue
"""

import socket
import ssl
import json
import time

def send_large_doc(size):
    """Send a document of specific size using HTTP/1.0"""
    
    # Create test data
    data = "x" * size
    doc = {
        "title": f"Test {size} bytes",
        "type": "crash-test",
        "data": data
    }
    body = json.dumps(doc)
    
    # Get token first
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    context = ssl.create_default_context()
    context.check_hostname = False
    context.verify_mode = ssl.CERT_NONE
    ssl_sock = context.wrap_socket(sock, server_hostname='localhost')
    
    try:
        ssl_sock.connect(('localhost', 5000))
        
        # Login request
        login_body = '{"username":"admin","password":"secure123456789"}'
        login_req = f"POST /api/auth/login HTTP/1.0\r\n"
        login_req += f"Host: localhost:5000\r\n"
        login_req += f"Content-Type: application/json\r\n"
        login_req += f"Content-Length: {len(login_body)}\r\n"
        login_req += f"\r\n"
        login_req += login_body
        
        ssl_sock.sendall(login_req.encode())
        
        response = b""
        while True:
            data = ssl_sock.recv(4096)
            if not data:
                break
            response += data
        
        # Extract token
        resp_body = response.split(b'\r\n\r\n')[1]
        token = json.loads(resp_body)['token']
        
    finally:
        ssl_sock.close()
    
    # Now send the large document
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    ssl_sock = context.wrap_socket(sock, server_hostname='localhost')
    
    try:
        ssl_sock.connect(('localhost', 5000))
        
        # Create document request
        req = f"POST /api/documents HTTP/1.0\r\n"
        req += f"Host: localhost:5000\r\n"
        req += f"Authorization: Bearer {token}\r\n"
        req += f"Content-Type: application/json\r\n"
        req += f"Content-Length: {len(body)}\r\n"
        req += f"\r\n"
        req += body
        
        print(f"Sending {size} byte document (total request: {len(req)} bytes)...")
        ssl_sock.sendall(req.encode())
        
        # Try to read response
        response = b""
        ssl_sock.settimeout(5)  # 5 second timeout
        
        try:
            while True:
                data = ssl_sock.recv(4096)
                if not data:
                    break
                response += data
        except socket.timeout:
            print("   Timeout waiting for response")
            return False
        
        if response:
            status_line = response.split(b'\r\n')[0]
            print(f"   Response: {status_line}")
            return True
        else:
            print("   No response - server may have crashed")
            return False
            
    except Exception as e:
        print(f"   Error: {e}")
        return False
    finally:
        ssl_sock.close()

def check_server():
    """Check if server is still running"""
    import subprocess
    result = subprocess.run(['ps', 'aux'], capture_output=True, text=True)
    return 'jdbxd' in result.stdout

def main():
    print("🔍 Debugging Large Document Crash\n")
    
    # Test increasing sizes
    sizes = [1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000]
    
    for size in sizes:
        if not check_server():
            print("\n❌ Server has crashed!")
            break
            
        success = send_large_doc(size)
        
        if not success:
            print(f"\n⚠️  Failed at {size} bytes")
            time.sleep(1)
            
            if not check_server():
                print("❌ Server crashed after {size} byte document")
                
                # Check logs
                print("\nLast 20 lines of log:")
                import subprocess
                subprocess.run(['tail', '-n', '20', '/opt/jdbx/build/var/jdbxd.log'])
                break
        else:
            print(f"✅ {size} bytes succeeded")
    
    print("\n🔍 Checking request size limits...")
    
    # The issue might be total request size, not just document size
    # HTTP headers + JSON overhead adds to the size
    
    # Calculate actual request sizes
    token = "x" * 500  # Approximate token size
    
    for size in [1000, 2000, 3000, 4000, 5000]:
        doc = {"title": f"Test {size}", "type": "test", "data": "x" * size}
        body = json.dumps(doc)
        
        # Approximate request size
        headers_size = len(f"POST /api/documents HTTP/1.0\r\nHost: localhost:5000\r\nAuthorization: Bearer {token}\r\nContent-Type: application/json\r\nContent-Length: {len(body)}\r\n\r\n")
        total_size = headers_size + len(body)
        
        print(f"Document {size} bytes → Total request ~{total_size} bytes")

if __name__ == "__main__":
    main()