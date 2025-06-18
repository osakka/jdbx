#!/usr/bin/env python3
"""
Test to see exact header size
"""

import socket

# Create minimal request
body = '{"test":"x"}'
request = f"""POST /api/documents HTTP/1.1\r
Host: localhost:5000\r
Content-Type: application/json\r
Content-Length: {len(body)}\r
Authorization: Bearer dummy\r
\r
{body}"""

header_end = request.find('\r\n\r\n') + 4
print(f"Request headers end at: {header_end}")
print(f"Total request size: {len(request)}")
print(f"Body size: {len(body)}")
print(f"Header size: {len(request) - len(body)}")
print(f"\nRaw request:\n{repr(request[:200])}...")

# Now test what Python requests actually sends
import requests
import io
import sys

class CapturingTransport(requests.adapters.HTTPAdapter):
    def send(self, request, **kwargs):
        # Capture the raw request
        print(f"\n\nPython requests sends:")
        print(f"Method: {request.method}")
        print(f"URL: {request.url}")
        print(f"Headers: {dict(request.headers)}")
        if request.body:
            print(f"Body length: {len(request.body)}")
            print(f"Body: {request.body[:100]}...")
        
        # Calculate approximate header size
        header_lines = [f"{request.method} {request.path_url} HTTP/1.1"]
        for k, v in request.headers.items():
            header_lines.append(f"{k}: {v}")
        headers_str = "\r\n".join(header_lines) + "\r\n\r\n"
        print(f"\nApproximate header size: {len(headers_str)}")
        
        return super().send(request, **kwargs)

# Test with requests
s = requests.Session()
s.mount('http://', CapturingTransport())

try:
    s.post("http://localhost:5000/api/documents", json={"test": "x" * 3900})
except:
    pass