# Installation Guide

**Version**: 6.5.0  
**Last Updated**: June 17, 2025

This guide covers installing JDBX v6.5.0 with revolutionary memory management, authentication security excellence, and unified documents architecture.

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
- **QuickJS**: Included with JDBX for JavaScript integration
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
git clone <repository-url> jdbx
cd jdbx

# Verify directory structure
ls -la src/

# Build JDBX
cd src && make

# Verify successful build
ls -la ../build/bin/jdbxd
ls -la ../build/lib/libjdbx.a
```

#### 3. Build Output Verification

After successful compilation, you should see:
```
../build/bin/jdbxd     # Main server executable
../build/bin/jdbx_tools      # Administration tools
../build/lib/libjdbx.a       # Static library
../build/obj/                  # Compiled object files
```

## Configuration

### Basic Configuration

JDBX uses a three-tier configuration system:

1. **Environment Variables** (lowest priority)
2. **Command Line Arguments** (medium priority)  
3. **Database Configuration** (highest priority)

#### Environment Configuration File
Create `/opt/jdbx/share/config/jdbx.env`:
```bash
# Database configuration
JDBX_DB_PATH=/opt/jdbx/build/var/jdbx.jdbx
JDBX_HOST=0.0.0.0
JDBX_PORT=5000

# SSL configuration (optional)
JDBX_USE_SSL=false
JDBX_SSL_CERT=/etc/ssl/certs/jdbx.pem
JDBX_SSL_KEY=/etc/ssl/private/jdbx.key

# Performance tuning
JDBX_THREAD_POOL_MIN=4
JDBX_THREAD_POOL_MAX=16
JDBX_CACHE_SIZE=50MB

# Logging
JDBX_LOG_LEVEL=INFO
JDBX_LOG_FILE=/opt/jdbx/build/var/jdbx.log
```

### Production Security Setup

#### 1. Create JDBX User (Recommended)
```bash
# Create dedicated user for JDBX
sudo useradd -r -s /bin/false jdbx
sudo mkdir -p /opt/jdbx/build/var
sudo chown -R jdbx:jdbx /opt/jdbx
```

#### 2. SSL Certificate Setup
```bash
# Generate self-signed certificate for testing
sudo openssl req -x509 -nodes -days 365 -newkey rsa:2048 \\
  -keyout /etc/ssl/private/jdbx.key \\
  -out /etc/ssl/certs/jdbx.pem

# Set proper permissions
sudo chmod 600 /etc/ssl/private/jdbx.key
sudo chmod 644 /etc/ssl/certs/jdbx.pem
```

## Starting JDBX

### Development Mode

```bash
# Start JDBX server (development)
cd /opt/jdbx
./build/jdbx_runtime.sh start

# Check status
./build/jdbx_runtime.sh status

# View logs
tail -f /opt/jdbx/build/var/jdbx.log

# Stop server
./build/jdbx_runtime.sh stop
```

### Production Mode with SSL

```bash
# Start with SSL enabled
cd /opt/jdbx
JDBX_USE_SSL=true ./build/jdbx_runtime.sh start

# Or use direct configuration
./build/bin/jdbxd \\
  --host 0.0.0.0 \\
  --port 5443 \\
  --ssl \\
  --ssl-cert /etc/ssl/certs/jdbx.pem \\
  --ssl-key /etc/ssl/private/jdbx.key \\
  --daemon
```

### Systemd Service (Production)

Create `/etc/systemd/system/jdbx.service`:
```ini
[Unit]
Description=JDBX Document Database
After=network.target

[Service]
Type=forking
User=jdbx
Group=jdbx
WorkingDirectory=/opt/jdbx
ExecStart=/opt/jdbx/build/jdbx_runtime.sh start
ExecStop=/opt/jdbx/build/jdbx_runtime.sh stop
ExecReload=/bin/kill -HUP $MAINPID
PIDFile=/opt/jdbx/build/var/jdbx.pid
Restart=always
RestartSec=10

# Security settings
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ReadWritePaths=/opt/jdbx/build/var

[Install]
WantedBy=multi-user.target
```

Enable and start:
```bash
sudo systemctl daemon-reload
sudo systemctl enable jdbx
sudo systemctl start jdbx
sudo systemctl status jdbx
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
ls -la /opt/jdbx/build/var/jdbx.jdbx

# Check log file for errors
grep ERROR /opt/jdbx/build/var/jdbx.log
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
sudo chown -R $USER:$USER /opt/jdbx
chmod 755 /opt/jdbx/build/bin/jdbxd
```

#### 3. Port Already in Use
```bash
# Check what's using port 5000
netstat -tulpn | grep :5000
lsof -i :5000

# Use different port
JDBX_PORT=5001 ./build/jdbx_runtime.sh start
```

#### 4. SSL Certificate Issues
```bash
# Verify certificate
openssl x509 -in /etc/ssl/certs/jdbx.pem -text -noout

# Check private key
openssl rsa -in /etc/ssl/private/jdbx.key -check
```

### Debug Mode

```bash
# Start with debug logging
JDBX_LOG_LEVEL=DEBUG ./build/jdbx_runtime.sh start

# Monitor detailed logs
tail -f /opt/jdbx/build/var/jdbx.log | grep DEBUG
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
JDBX v3.3.0 is backward compatible with v3.2.x data files. The JDBX backend will automatically migrate existing data on first startup.

---

**Installation complete?** Continue with the [Quick Start Guide](quick-start.md) to create your first application.