# Welcome Panel Feature

The JSONdb dashboard now includes a customizable welcome/introduction panel that can display markdown content to users.

## Features

- **Markdown Support**: Full markdown rendering including headers, lists, code blocks, links, and tables
- **Configurable**: Controlled via a document in the `_config` collection
- **Dismissible**: Users can close the panel with an X button
- **Version Control**: Change the version to re-show the panel to users who previously dismissed it
- **Beautiful Design**: Styled with gradient background and matching the dashboard theme

## Setup

1. **Run the setup script** (requires authentication token):
   ```bash
   cd /opt/jsondb/share/examples
   ./setup_welcome_panel.sh YOUR_AUTH_TOKEN
   ```

2. **Or manually create** the configuration:
   - Create a `_config` collection
   - Insert a document with the structure shown in `welcome_panel_config.json`

## Configuration Structure

```json
{
    "_id": "welcome-panel-config",
    "type": "welcome_panel",
    "enabled": true,
    "title": "Welcome to JSONdb",
    "version": "v1",
    "content": "# Your Markdown Content Here\n\nSupports **bold**, *italic*, [links](url), etc."
}
```

### Fields:
- **type**: Must be `"welcome_panel"` (required)
- **enabled**: Boolean to show/hide the panel (required)
- **title**: Panel header title (optional, defaults to "Welcome")
- **version**: Version string for dismissal tracking (optional, defaults to "v1")
- **content**: Markdown content to display (required)

## Customization

### To modify the welcome content:
1. Navigate to Browser → _config collection
2. Find the document with `type: "welcome_panel"`
3. Edit the `content` field with your markdown
4. Save the document

### To temporarily hide the panel:
- Set `enabled: false` in the configuration

### To show the panel again to all users:
- Change the `version` field to a new value (e.g., "v2")

## Markdown Examples

The panel supports full markdown syntax:

```markdown
# Headers
## Subheaders

**Bold text** and *italic text*

- Bullet lists
- With multiple items

1. Numbered lists
2. Also supported

[Links to documentation](https://example.com)

`inline code` and code blocks:

​```javascript
const example = "code";
​```

> Blockquotes for important notes

| Tables | Are | Supported |
|--------|-----|-----------|
| Row 1  | Data| Here      |
```

## Styling

The welcome panel includes:
- Gradient background matching the theme
- Accent color borders
- Responsive design
- Dark mode support
- Smooth animations

## Best Practices

1. **Keep it concise**: Users should be able to quickly scan the content
2. **Include actionable items**: Guide users on what to do next
3. **Update regularly**: Use the version field to show new content
4. **Make it dismissible**: Users appreciate being able to hide panels
5. **Use markdown effectively**: Headers, lists, and emphasis help readability

## Troubleshooting

- **Panel not showing?** Check that `enabled: true` and the `_config` collection exists
- **Changes not reflected?** Refresh the dashboard page
- **Panel keeps reappearing?** The version was changed - dismiss it again or revert the version