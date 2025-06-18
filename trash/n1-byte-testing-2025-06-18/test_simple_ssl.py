#!/usr/bin/env python3
import ssl
import socket
import json

# Create SSL context
context = ssl.create_default_context()
context.check_hostname = False
context.verify_mode = ssl.CERT_NONE

# Create document
body = json.dumps({
    "title": "Simple SSL test",
    "type": "ssl-test",
    "data": "x" * 100
})

# Build request
request = f"""POST /api/documents HTTP/1.1\r
Host: localhost:5000\r
Content-Type: application/json\r
Content-Length: {len(body)}\r
Authorization: Bearer dummy\r
\r
{body}"""

print(f"Request length: {len(request)} bytes")
print(f"Body length: {len(body)} bytes")

# Send request
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
ssl_sock = context.wrap_socket(s, server_hostname="localhost")
ssl_sock.connect(("localhost", 5000))
ssl_sock.sendall(request.encode())

# Read response
response = ssl_sock.recv(4096)
ssl_sock.close()

print("\nResponse:")
print(response.decode('utf-8', errors='replace'))