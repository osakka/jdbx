#!/usr/bin/env python3
"""Debug login issue"""

import requests
import urllib3
import json

urllib3.disable_warnings()

def test_login():
    # Create session
    session = requests.Session()
    session.verify = False
    session.headers.update({
        'User-Agent': 'Python-Test/1.0',
        'Accept': 'application/json',
        'Content-Type': 'application/json'
    })
    
    # Test data
    login_data = {
        "username": "admin", 
        "password": "secure123456789"
    }
    
    print(f"Login data: {json.dumps(login_data)}")
    
    try:
        # Method 1: Using json parameter
        print("\n=== Method 1: Using json parameter ===")
        response = session.post('https://localhost:5000/api/auth/login', 
                               json=login_data,
                               timeout=10)
        print(f"Status: {response.status_code}")
        print(f"Response: {response.text}")
        print(f"Headers sent: {response.request.headers}")
        
        # Method 2: Using data parameter with manual JSON
        print("\n=== Method 2: Using data parameter ===")
        response2 = session.post('https://localhost:5000/api/auth/login',
                                data=json.dumps(login_data),
                                headers={'Content-Type': 'application/json'},
                                timeout=10)
        print(f"Status: {response2.status_code}")
        print(f"Response: {response2.text}")
        
        # Method 3: Simple GET to health to test connection
        print("\n=== Method 3: Testing health endpoint ===")
        response3 = session.get('https://localhost:5000/api/health', timeout=10)
        print(f"Status: {response3.status_code}")
        print(f"Response: {response3.text}")
        
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    test_login()