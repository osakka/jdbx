#!/bin/bash
# Basic HTTP Test Script for JSONdb
# This script tests basic HTTP functionality

SERVER_URL="http://localhost:5000"

echo "Testing basic HTTP connectivity to $SERVER_URL"
echo "--------------------------------------------"
echo ""

# Test raw HTTP request
echo "Sending simple GET request to root endpoint..."
curl -i $SERVER_URL/

echo ""
echo ""
echo "Testing API endpoints..."
echo "1. /api endpoint:"
curl -i $SERVER_URL/api

echo ""
echo ""
echo "2. /api/health endpoint:"
curl -i $SERVER_URL/api/health

echo ""
echo ""
echo "3. /api/collections endpoint:"
curl -i $SERVER_URL/api/collections

echo ""
echo ""
echo "4. Login attempt:"
curl -i -X POST -H "Content-Type: application/json" -d '{"username":"admin","password":"admin"}' $SERVER_URL/api/auth/login

echo ""
echo ""
echo "Basic testing completed."