# Quick Start Guide

> Get JDBX up and running in 5 minutes

## Overview

This tutorial will get you from zero to a working JDBX server with your first document stored and retrieved.

## Prerequisites

- Linux or macOS system
- GCC compiler
- 10 minutes of your time

## Step 1: Build JDBX

```bash
cd /opt/jdbx/src
make
```

## Step 2: Start the Server

```bash
cd /opt/jdbx
build/jdbx_runtime.sh start
```

## Step 3: Configure Bootstrap Admin

```bash
export JDBX_BOOTSTRAP_ADMIN_USER=admin
export JDBX_BOOTSTRAP_ADMIN_PASS=secure123456789
export JDBX_DEFAULT_ADMIN_EMAIL=admin@example.com
```

## Step 4: Login and Get Token

```bash
curl -X POST https://localhost:5000/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{
    "username": "admin",
    "password": "secure123456789"
  }' -k
```

Save the token from the response for the next steps.

## Step 5: Create Your First Document

```bash
TOKEN="your-jwt-token-here"

curl -X POST https://localhost:5000/api/collections/users/documents \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "name": "John Doe",
    "email": "john@example.com",
    "age": 30
  }' -k
```

## Step 6: Query Your Document

```bash
curl -X GET "https://localhost:5000/api/collections/users/documents?query={}" \
  -H "Authorization: Bearer $TOKEN" -k
```

## Congratulations! 🎉

You now have a working JDBX server with:
- ✅ Secure authentication
- ✅ Document storage and retrieval
- ✅ RESTful API access

## Next Steps

- **[Your First Database](first-database.md)** - Learn more about collections and documents
- **[Authentication Basics](authentication-basics.md)** - Understand JWT tokens and security
- **[API Reference](../../reference/api/rest-api.md)** - Explore all available endpoints

## Troubleshooting

**Server won't start?**
- Check logs: `cat /opt/jdbx/var/jdbxd.log`
- Verify port 5000 is available: `netstat -an | grep 5000`

**Authentication fails?**
- Verify environment variables are set
- Check password meets minimum 12 character requirement

**SSL certificate errors?**
- Use `-k` flag with curl for self-signed certificates
- See [SSL Configuration](../../guides/ssl-setup.md) for production setup