#!/usr/bin/env python3
"""
Test thread pool exhaustion
"""

import socket
import ssl
import time

def create_hanging_connections(count):
    """Create connections that don't send any data"""
    print(f"Creating {count} hanging connections...")
    
    connections = []
    
    for i in range(count):
        try:
            # Create SSL context
            context = ssl.create_default_context()
            context.check_hostname = False
            context.verify_mode = ssl.CERT_NONE
            
            # Create socket
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            ssl_sock = context.wrap_socket(sock)
            ssl_sock.connect(('localhost', 5000))
            
            # Don't send any data - just hold the connection
            connections.append(ssl_sock)
            print(f"Connection {i+1} established")
            
        except Exception as e:
            print(f"Failed to create connection {i+1}: {e}")
            break
    
    return connections

def main():
    print("Thread Exhaustion Test")
    print("="*50)
    
    # First, verify server is responsive
    import requests
    import urllib3
    urllib3.disable_warnings()
    
    try:
        response = requests.get("https://localhost:5000/api/health", verify=False, timeout=2)
        print(f"✅ Initial health check: {response.status_code}")
    except Exception as e:
        print(f"❌ Server not responding: {e}")
        return
    
    # Create hanging connections
    # Default thread pool is usually 4-16 threads
    # Test with more connections than threads to ensure timeouts work
    connections = create_hanging_connections(50)
    
    print(f"\n{len(connections)} connections created and hanging...")
    
    # Now try to make a normal request
    print("\nTrying normal request with hanging connections...")
    try:
        response = requests.get("https://localhost:5000/api/health", verify=False, timeout=5)
        print(f"✅ Server still responsive: {response.status_code}")
    except Exception as e:
        print(f"❌ Server not responding - thread pool exhausted: {e}")
    
    # Clean up
    print("\nClosing connections...")
    for conn in connections:
        try:
            conn.close()
        except:
            pass
    
    # Test recovery
    time.sleep(2)
    print("\nTesting recovery after closing connections...")
    try:
        response = requests.get("https://localhost:5000/api/health", verify=False, timeout=5)
        print(f"✅ Server recovered: {response.status_code}")
    except Exception as e:
        print(f"❌ Server still not responding: {e}")

if __name__ == "__main__":
    main()