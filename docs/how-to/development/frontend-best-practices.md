# Frontend Best Practices - Preventing ID Conflicts and DOM Issues

## 1. ID Naming Conventions

**NEVER use generic IDs like:**
- `id="users"`
- `id="roles"`
- `id="permissions"`

**ALWAYS use specific, prefixed IDs:**
```html
<!-- Tab panes -->
<div id="rbac-users-tab" class="tab-pane">
<div id="rbac-roles-tab" class="tab-pane">

<!-- Form elements -->
<select id="form-user-roles-select">
<input id="form-user-email-input">

<!-- Display containers -->
<div id="display-roles-list">
<div id="display-users-table">
```

## 2. DOM Safety Functions

Create safe DOM manipulation functions:

```javascript
// Safe element getter that warns on conflicts
function getSafeElement(id, expectedType = null) {
    const elements = document.querySelectorAll(`#${id}`);
    
    if (elements.length > 1) {
        console.error(`CRITICAL: Multiple elements with id="${id}" found!`);
        console.error('Elements:', elements);
        throw new Error(`ID conflict detected: ${id}`);
    }
    
    const element = elements[0];
    if (expectedType && element && element.tagName !== expectedType.toUpperCase()) {
        console.error(`CRITICAL: Element #${id} is ${element.tagName}, expected ${expectedType}`);
        throw new Error(`Type mismatch for #${id}`);
    }
    
    return element;
}

// Safe innerHTML setter
function safeSetHTML(id, html, expectedType = null) {
    const element = getSafeElement(id, expectedType);
    if (element) {
        element.innerHTML = html;
    }
}

// Usage:
safeSetHTML('form-user-roles-select', optionsHTML, 'SELECT');
```

## 3. Development-Time Validation

Add this to your initialization:

```javascript
// ID Conflict Detector - Run in development
function detectIDConflicts() {
    const allElements = document.querySelectorAll('[id]');
    const idMap = {};
    
    allElements.forEach(el => {
        const id = el.id;
        if (!idMap[id]) {
            idMap[id] = [];
        }
        idMap[id].push(el);
    });
    
    // Report conflicts
    Object.entries(idMap).forEach(([id, elements]) => {
        if (elements.length > 1) {
            console.error(`ID CONFLICT: "${id}" used ${elements.length} times:`);
            elements.forEach(el => {
                console.error(`  - <${el.tagName}> in`, el.parentElement);
            });
        }
    });
}

// Run on page load in development
if (window.location.hostname === 'localhost') {
    window.addEventListener('DOMContentLoaded', detectIDConflicts);
}
```

## 4. TypeScript or JSDoc Type Safety

Use type annotations to catch errors:

```javascript
/**
 * @param {HTMLSelectElement} selectElement
 * @param {Array<{id: string, name: string}>} options
 */
function populateSelect(selectElement, options) {
    if (!(selectElement instanceof HTMLSelectElement)) {
        throw new TypeError('First argument must be a SELECT element');
    }
    selectElement.innerHTML = options.map(opt => 
        `<option value="${opt.id}">${opt.name}</option>`
    ).join('');
}
```

## 5. CSS Scoping

Use data attributes and classes instead of IDs where possible:

```html
<!-- Instead of: -->
<div id="roles">

<!-- Use: -->
<div data-rbac-panel="roles" class="rbac-panel">

<!-- Then query with: -->
document.querySelector('[data-rbac-panel="roles"]')
```

## 6. Automated Testing

Create tests that verify DOM structure:

```javascript
// test-dom-integrity.js
describe('DOM Integrity Tests', () => {
    it('should have no duplicate IDs', () => {
        const ids = [...document.querySelectorAll('[id]')].map(el => el.id);
        const uniqueIds = [...new Set(ids)];
        expect(ids.length).toBe(uniqueIds.length);
    });
    
    it('should have expected elements in RBAC view', () => {
        const rbacView = document.getElementById('rbac-view');
        expect(rbacView).toBeTruthy();
        expect(rbacView.querySelector('#rolesList')).toBeTruthy();
        expect(rbacView.querySelector('#roles')).toBeFalsy(); // Should not exist
    });
});
```

## 7. Linting Rules

Add ESLint rules:

```javascript
// .eslintrc.js
module.exports = {
    rules: {
        'no-duplicate-ids': ['error', {
            enforceUniqueIds: true,
            checkDataAttributes: true
        }],
        'prefer-data-attributes': ['warn', {
            instead: ['id', 'name']
        }]
    }
};
```

## 8. Component-Based Architecture

Use a component system to encapsulate IDs:

```javascript
class RBACPanel {
    constructor(container) {
        this.container = container;
        this.id = `rbac-panel-${Date.now()}`;
        this.render();
    }
    
    render() {
        this.container.innerHTML = `
            <div data-component="rbac-panel" data-instance="${this.id}">
                <div data-element="roles-list"></div>
                <select data-element="roles-select"></select>
            </div>
        `;
    }
    
    getRolesList() {
        return this.container.querySelector('[data-element="roles-list"]');
    }
}
```

## 9. Debug Mode

Add a debug mode that highlights potential issues:

```javascript
// Enable with ?debug=true in URL
if (new URLSearchParams(window.location.search).get('debug')) {
    // Highlight elements with same IDs
    const ids = {};
    document.querySelectorAll('[id]').forEach(el => {
        if (ids[el.id]) {
            el.style.border = '3px solid red';
            console.error(`Duplicate ID: ${el.id}`);
        }
        ids[el.id] = true;
    });
}
```

## 10. Code Review Checklist

Before merging any frontend changes:

- [ ] Run `detectIDConflicts()` function
- [ ] Search for `getElementById` calls and verify uniqueness
- [ ] Check that all IDs follow naming convention
- [ ] Verify SELECT elements use specific IDs
- [ ] Test all tab switching functionality
- [ ] Check for console errors
- [ ] Run automated DOM tests

## Implementation Priority

1. **Immediate**: Add ID conflict detector to app.js
2. **Next Sprint**: Refactor all IDs to use prefixes
3. **Long-term**: Move to component-based architecture