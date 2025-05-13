# Runtime Configuration Examples

This directory contains example runtime configuration files that can be used when running JSONdb server. These are actual example configuration files that have been used in testing and development.

## Available Configuration Files

- `auth.json` - Basic authentication configuration example
- `auth2.json` - Alternative authentication configuration
- `config.json` - Server configuration example
- `db.json` - Database configuration example
- `rbac.json` - Role-based access control configuration example

## Usage

These files can be used as starting points for your own configuration. Copy them to your server's configuration directory and modify as needed.

Example:

```bash
# Copy to the server's configuration directory
cp auth.json config.json db.json rbac.json /path/to/server/config/

# Edit the configuration as needed
vi /path/to/server/config/config.json
```

## Notes

These files are complementary to the template examples in `/examples/config/*.example` which provide detailed documentation about the available options in each configuration file.