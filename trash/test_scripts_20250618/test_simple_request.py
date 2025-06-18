#!/usr/bin/env python3
"""Test simple SSL request to verify basic functionality"""

import requests
import json
import urllib3

# Disable SSL warnings for self-signed certificates
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

BASE_URL = "https://localhost:5000"

def test_health():
    """Test health endpoint"""
    print("Testing health endpoint...")
    try:
        response = requests.get(f"{BASE_URL}/api/health", verify=False, timeout=5)
        print(f"✅ Health check: {response.status_code}")
        print(f"   Response: {response.text}")
        return response.status_code == 200
    except Exception as e:
        print(f"❌ Health check failed: {e}")
        return False

def test_login():
    """Test login with admin credentials"""
    print("\nTesting login...")
    try:
        login_data = {
            "username": "admin",
            "password": "secure123456789"
        }
        response = requests.post(
            f"{BASE_URL}/api/auth/login",
            json=login_data,
            verify=False,
            timeout=5
        )
        print(f"✅ Login: {response.status_code}")
        if response.status_code == 200:
            data = response.json()
            print(f"   JWT Token: {data.get('token', 'No token')[:50]}...")
            return data.get('token')
        else:
            print(f"   Response: {response.text}")
            return None
    except Exception as e:
        print(f"❌ Login failed: {e}")
        return None

def test_create_document(token, size=100):
    """Test creating document with specific size"""
    print(f"\nTesting document creation with {size} byte content...")
    try:
        # Create content of specific size
        content = "x" * size
        doc = {
            "name": f"Test Document {size}",
            "content": content
        }
        
        headers = {"Authorization": f"Bearer {token}"}
        response = requests.post(
            f"{BASE_URL}/api/documents",
            json=doc,
            headers=headers,
            verify=False,
            timeout=5
        )
        
        if response.status_code == 200:
            print(f"✅ Document created: {response.status_code}")
            data = response.json()
            print(f"   UUID: {data.get('uuid', 'No UUID')}")
        else:
            print(f"❌ Document creation failed: {response.status_code}")
            print(f"   Response: {response.text}")
            
        return response.status_code == 200
    except Exception as e:
        print(f"❌ Document creation failed: {e}")
        return False

if __name__ == "__main__":
    print("Simple SSL Request Test")
    print("=" * 50)
    
    # Test health endpoint
    if not test_health():
        print("\n⚠️  Basic health check failed!")
        exit(1)
    
    # Test login
    token = test_login()
    if not token:
        print("\n⚠️  Login failed!")
        exit(1)
    
    # Test document creation with different sizes
    sizes = [10, 100, 1000, 5000, 10000]
    for size in sizes:
        if not test_create_document(token, size):
            print(f"\n⚠️  Failed at {size} bytes")
            break
    
    print("\n✅ All tests completed!")