# Installation Guide

**Version**: 3.3.0  
**Last Updated**: June 12, 2025

This guide covers installing JSONdb v3.3.0 with its lock-free JDBX architecture.

## System Requirements

### Operating System
- **Linux**: Ubuntu 18.04+, CentOS 7+, Debian 9+, Fedora 30+
- **macOS**: macOS 10.14+ with Xcode Command Line Tools
- **Architecture**: x86_64 (amd64)

### Hardware Requirements
| Deployment | RAM | CPU | Storage |
|------------|-----|-----|---------|
| Development | 1GB | 1 core | 1GB |
| Small Production | 4GB | 2 cores | 10GB |
| Medium Production | 8GB | 4 cores | 50GB |
| Large Production | 16GB+ | 8+ cores | 100GB+ |

### Software Dependencies

#### Required
- **GCC**: Version 4.9+ with C99 support
- **UUID Library**: For unique identifier generation
- **OpenSSL**: Version 1.1.0+ for SSL/TLS and cryptography
- **Make**: Standard build system

#### Optional  
- **QuickJS**: Included with JSONdb for JavaScript integration
- **Git**: For source code management and updates

## Installation Methods

### Method 1: Build from Source (Recommended)

#### 1. Install System Dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install gcc libuuid-dev libssl-dev make git
```

**CentOS/RHEL/Fedora:**
```bash
# CentOS 7/RHEL 7
sudo yum install gcc uuid-devel openssl-devel make git

# CentOS 8+/RHEL 8+/Fedora
sudo dnf install gcc uuid-devel openssl-devel make git
```

**macOS:**
```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install dependencies via Homebrew (optional)
brew install ossp-uuid openssl
```

#### 2. Clone and Build

```bash
# Clone repository
git clone <repository-url> jsondb
cd jsondb

# Verify directory structure
ls -la src/

# Build JSONdb
cd src && make

# Verify successful build
ls -la ../build/bin/jsondb_server
ls -la ../build/lib/libjsondb.a
```

#### 3. Build Output Verification

After successful compilation, you should see:
```
../build/bin/jsondb_server     # Main server executable
../build/bin/jsondb_tools      # Administration tools
../build/lib/libjsondb.a       # Static library
../build/obj/                  # Compiled object files
```

## Configuration

### Basic Configuration

JSONdb uses a three-tier configuration system:

1. **Environment Variables** (lowest priority)
2. **Command Line Arguments** (medium priority)  
3. **Database Configuration** (highest priority)

#### Environment Configuration File
Create `/opt/jsondb/share/config/jsondb.env`:
```bash
# Database configuration
JSONDB_DB_PATH=/opt/jsondb/build/var/jsondb.jdbx
JSONDB_HOST=0.0.0.0
JSONDB_PORT=5000

# SSL configuration (optional)
JSONDB_USE_SSL=false
JSONDB_SSL_CERT=/etc/ssl/certs/jsondb.pem
JSONDB_SSL_KEY=/etc/ssl/private/jsondb.key

# Performance tuning
JSONDB_THREAD_POOL_MIN=4
JSONDB_THREAD_POOL_MAX=16
JSONDB_CACHE_SIZE=50MB

# Logging
JSONDB_LOG_LEVEL=INFO
JSONDB_LOG_FILE=/opt/jsondb/build/var/jsondb.log
```

### Production Security Setup

#### 1. Create JSONdb User (Recommended)
```bash
# Create dedicated user for JSONdb
sudo useradd -r -s /bin/false jsondb
sudo mkdir -p /opt/jsondb/build/var
sudo chown -R jsondb:jsondb /opt/jsondb
```

#### 2. SSL Certificate Setup
```bash
# Generate self-signed certificate for testing
sudo openssl req -x509 -nodes -days 365 -newkey rsa:2048 \\
  -keyout /etc/ssl/private/jsondb.key \\
  -out /etc/ssl/certs/jsondb.pem

# Set proper permissions
sudo chmod 600 /etc/ssl/private/jsondb.key
sudo chmod 644 /etc/ssl/certs/jsondb.pem
```

## Starting JSONdb

### Development Mode

```bash
# Start JSONdb server (development)
cd /opt/jsondb
./build/jsondb_runtime.sh start

# Check status
./build/jsondb_runtime.sh status

# View logs
tail -f /opt/jsondb/build/var/jsondb.log

# Stop server
./build/jsondb_runtime.sh stop
```

### Production Mode with SSL

```bash
# Start with SSL enabled
cd /opt/jsondb
JSONDB_USE_SSL=true ./build/jsondb_runtime.sh start

# Or use direct configuration
./build/bin/jsondb_server \\
  --host 0.0.0.0 \\
  --port 5443 \\
  --ssl \\
  --ssl-cert /etc/ssl/certs/jsondb.pem \\
  --ssl-key /etc/ssl/private/jsondb.key \\
  --daemon
```

### Systemd Service (Production)

Create `/etc/systemd/system/jsondb.service`:
```ini
[Unit]
Description=JSONdb Document Database
After=network.target

[Service]
Type=forking
User=jsondb
Group=jsondb
WorkingDirectory=/opt/jsondb
ExecStart=/opt/jsondb/build/jsondb_runtime.sh start
ExecStop=/opt/jsondb/build/jsondb_runtime.sh stop
ExecReload=/bin/kill -HUP $MAINPID
PIDFile=/opt/jsondb/build/var/jsondb.pid
Restart=always
RestartSec=10

# Security settings
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ReadWritePaths=/opt/jsondb/build/var

[Install]
WantedBy=multi-user.target
```

Enable and start:
```bash
sudo systemctl daemon-reload
sudo systemctl enable jsondb
sudo systemctl start jsondb
sudo systemctl status jsondb
```

## Verification

### Health Check

```bash
# Basic health check
curl http://localhost:5000/health

# Expected response:
# {"status":"healthy","version":"3.3.0","uptime":45}
```

### API Access Test

```bash
# Test API endpoint
curl -X GET http://localhost:5000/api/openapi.json

# Test admin interface (browser)
open http://localhost:5000/admin
```

### Database File Verification

```bash
# Check JDBX database file exists
ls -la /opt/jsondb/build/var/jsondb.jdbx

# Check log file for errors
grep ERROR /opt/jsondb/build/var/jsondb.log
```

## Troubleshooting

### Common Issues

#### 1. Compilation Errors
```bash
# Missing development packages
sudo apt-get install build-essential uuid-dev libssl-dev

# Wrong GCC version
gcc --version  # Should be 4.9+
```

#### 2. Permission Errors
```bash
# Fix file permissions
sudo chown -R $USER:$USER /opt/jsondb
chmod 755 /opt/jsondb/build/bin/jsondb_server
```

#### 3. Port Already in Use
```bash
# Check what's using port 5000
netstat -tulpn | grep :5000
lsof -i :5000

# Use different port
JSONDB_PORT=5001 ./build/jsondb_runtime.sh start
```

#### 4. SSL Certificate Issues
```bash
# Verify certificate
openssl x509 -in /etc/ssl/certs/jsondb.pem -text -noout

# Check private key
openssl rsa -in /etc/ssl/private/jsondb.key -check
```

### Debug Mode

```bash
# Start with debug logging
JSONDB_LOG_LEVEL=DEBUG ./build/jsondb_runtime.sh start

# Monitor detailed logs
tail -f /opt/jsondb/build/var/jsondb.log | grep DEBUG
```

## Next Steps

1. **[Quick Start Guide](quick-start.md)** - Create your first database and documents
2. **[Configuration Reference](../reference/configuration.md)** - Complete configuration options
3. **[Authentication Setup](../guides/authentication.md)** - Set up users and roles
4. **[Production Deployment](../guides/production-deployment.md)** - Production best practices

## Version-Specific Notes

### v3.3.0 Changes
- **JDBX Storage**: New single-file database backend (default)
- **Lock-Free Architecture**: Significantly improved concurrent performance  
- **Field-Level Operations**: Granular document field access
- **Multi-Library Support**: Tenant isolation with library namespaces

### Migration from v3.2.x
JSONdb v3.3.0 is backward compatible with v3.2.x data files. The JDBX backend will automatically migrate existing data on first startup.

---

**Installation complete?** Continue with the [Quick Start Guide](quick-start.md) to create your first application.