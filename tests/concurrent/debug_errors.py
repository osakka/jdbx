#!/usr/bin/env python3
import requests
import urllib3

# Disable SSL warnings for self-signed certificates
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

BASE_URL = "https://localhost:5000"

# First, login to get JWT token
def get_auth_token():
    login_data = {
        "username": "admin",
        "password": "admin"
    }
    
    response = requests.post(
        f"{BASE_URL}/api/auth/login",
        json=login_data,
        verify=False,
        timeout=10
    )
    
    if response.status_code == 200:
        return response.json().get("token")
    else:
        print(f"Login failed: HTTP {response.status_code}")
        print(f"Response: {response.text}")
        return None

print("🔑 Getting authentication token...")
token = get_auth_token()
if not token:
    print("❌ Failed to authenticate!")
    exit(1)

print(f"✅ Got JWT token: {token[:20]}...")

# Try a single document creation
doc_data = {
    "title": "Single Test Document",
    "content": "Testing single operation",
    "timestamp": 123456789
}

headers = {
    "Authorization": f"Bearer {token}"
}

print("\n📝 Creating single document...")
try:
    response = requests.post(
        f"{BASE_URL}/api/collections/test-single/documents",
        json=doc_data,
        headers=headers,
        verify=False,
        timeout=10
    )
    
    print(f"Response status: {response.status_code}")
    print(f"Response body: {response.text}")
    
    if response.status_code != 200:
        print(f"Headers: {response.headers}")
        
except Exception as e:
    print(f"ERROR: {str(e)}")