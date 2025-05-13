# Configuration Examples

This directory contains example configuration files for JSONdb. These files serve as templates that you can copy and modify for your own use.

## Available Configuration Examples

- `auth.json.example`: Authentication configuration example
- `config.json.example`: Main server configuration example
- `db.json.example`: Database configuration example
- `rbac.json.example`: Role-Based Access Control configuration example

## Usage

To use these configuration files:

1. Copy the example file to the project root or your preferred configuration directory
2. Remove the `.example` extension
3. Modify the file according to your needs

Example:

```bash
cp auth.json.example ../auth.json
vi ../auth.json  # Edit the configuration file
```

## Configuration File Descriptions

### auth.json

Contains authentication settings including:
- API keys
- Authentication methods
- Credentials

### config.json

Main server configuration including:
- Server port and host
- Log settings
- Database path
- Cache settings
- SSL configuration

### db.json

Database configuration including:
- Collections
- Indexes
- Default settings

### rbac.json

Role-Based Access Control configuration including:
- User roles
- Permissions
- Access control policies