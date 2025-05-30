#!/bin/bash

# Setup Welcome Panel for JSONdb Dashboard
# This script creates a _config collection and adds the welcome panel configuration

API_URL="http://localhost:5000/api"
AUTH_TOKEN="${1:-}"

if [ -z "$AUTH_TOKEN" ]; then
    echo "Usage: $0 <auth_token>"
    echo "Please provide your JSONdb authentication token"
    exit 1
fi

echo "Setting up welcome panel configuration..."

# Create _config collection if it doesn't exist
echo "Creating _config collection..."
curl -X POST "${API_URL}/collections" \
    -H "Authorization: Bearer ${AUTH_TOKEN}" \
    -H "Content-Type: application/json" \
    -d '{
        "name": "_config",
        "description": "System configuration collection"
    }' 2>/dev/null

echo ""
echo "Adding welcome panel configuration..."

# Insert welcome panel configuration
curl -X POST "${API_URL}/collections/_config/documents" \
    -H "Authorization: Bearer ${AUTH_TOKEN}" \
    -H "Content-Type: application/json" \
    -d @welcome_panel_config.json

echo ""
echo "Welcome panel configuration complete!"
echo "Refresh your dashboard to see the welcome panel."
echo ""
echo "To customize the welcome panel:"
echo "1. Navigate to Browser > _config collection"
echo "2. Find the document with type: 'welcome_panel'"
echo "3. Edit the 'content' field with your markdown content"
echo "4. Set 'enabled' to false to disable the panel"
echo "5. Change 'version' to show the panel again to users who dismissed it"