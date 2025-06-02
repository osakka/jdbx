/**
 * JSONdb JavaScript Script Examples
 * ==================================
 * 
 * This file contains comprehensive examples of validators, transformers, and functions
 * that can be used with the JSONdb JavaScript integration system.
 * 
 * Each script is designed to be stored as a JSON document in the appropriate collection:
 * - Validators: _validators collection
 * - Transformers: _transformers collection  
 * - Functions: _functions collection
 */

// =============================================================================
// VALIDATOR EXAMPLES
// =============================================================================

/**
 * Example 1: Email Validator
 * Validates that email field exists and has valid format
 */
const emailValidator = {
    "name": "Email Validator",
    "description": "Validates email field format and presence",
    "type": "validator",
    "tags": ["users", "contacts", "global"],
    "enabled": true,
    "code": `
function validate(document, context) {
    const errors = [];
    const warnings = [];
    
    // Check if email field exists
    if (!document.email) {
        errors.push("Email field is required");
        return { valid: false, errors, warnings };
    }
    
    // Validate email format
    const emailRegex = /^[^\\s@]+@[^\\s@]+\\.[^\\s@]+$/;
    if (!emailRegex.test(document.email)) {
        errors.push("Invalid email format");
    }
    
    // Check for common email providers (warning)
    const commonProviders = ['gmail.com', 'yahoo.com', 'hotmail.com'];
    const domain = document.email.split('@')[1];
    if (commonProviders.includes(domain)) {
        warnings.push("Consider using business email for professional accounts");
    }
    
    return {
        valid: errors.length === 0,
        errors,
        warnings
    };
}
    `,
    "version": "1.0.0",
    "author": "JSONdb System",
    "created_at": new Date().toISOString(),
    "updated_at": new Date().toISOString()
};

/**
 * Example 2: Age Range Validator
 * Validates age is within acceptable range
 */
const ageValidator = {
    "name": "Age Range Validator",
    "description": "Validates age is within 13-120 range",
    "type": "validator", 
    "tags": ["users", "profiles"],
    "enabled": true,
    "code": `
function validate(document, context) {
    const errors = [];
    const warnings = [];
    
    if (document.age !== undefined) {
        const age = parseInt(document.age);
        
        if (isNaN(age)) {
            errors.push("Age must be a valid number");
        } else {
            if (age < 13) {
                errors.push("Age must be at least 13 years");
            } else if (age > 120) {
                errors.push("Age must be less than 120 years");
            } else if (age < 18) {
                warnings.push("Minor account - additional verification may be required");
            }
        }
    }
    
    return {
        valid: errors.length === 0,
        errors,
        warnings
    };
}
    `,
    "version": "1.0.0",
    "author": "JSONdb System"
};

/**
 * Example 3: Required Fields Validator
 * Validates that specified required fields are present
 */
const requiredFieldsValidator = {
    "name": "Required Fields Validator",
    "description": "Validates presence of required fields based on document type",
    "type": "validator",
    "tags": ["*"],
    "enabled": true,
    "code": `
function validate(document, context) {
    const errors = [];
    const warnings = [];
    
    // Define required fields by collection
    const requiredFields = {
        'users': ['username', 'email'],
        'products': ['name', 'price', 'category'],
        'orders': ['customer_id', 'items', 'total'],
        'default': ['name']
    };
    
    const collection = context.collection || 'default';
    const required = requiredFields[collection] || requiredFields.default;
    
    required.forEach(field => {
        if (!document[field] || document[field] === '') {
            errors.push(\`Required field '\${field}' is missing or empty\`);
        }
    });
    
    return {
        valid: errors.length === 0,
        errors,
        warnings
    };
}
    `,
    "version": "1.0.0",
    "author": "JSONdb System"
};

// =============================================================================
// TRANSFORMER EXAMPLES  
// =============================================================================

/**
 * Example 1: User Data Normalizer
 * Normalizes user data fields (email lowercase, name title case)
 */
const userNormalizer = {
    "name": "User Data Normalizer",
    "description": "Normalizes user data - lowercase email, title case names",
    "type": "transformer",
    "tags": ["users", "contacts"],
    "enabled": true,
    "code": `
function transform(document, context) {
    const result = { ...document };
    
    // Normalize email to lowercase
    if (result.email) {
        result.email = result.email.toLowerCase().trim();
    }
    
    // Title case for names
    ['firstName', 'lastName', 'name'].forEach(field => {
        if (result[field]) {
            result[field] = result[field]
                .toLowerCase()
                .split(' ')
                .map(word => word.charAt(0).toUpperCase() + word.slice(1))
                .join(' ')
                .trim();
        }
    });
    
    // Add normalized full name if first/last provided
    if (result.firstName && result.lastName) {
        result.fullName = \`\${result.firstName} \${result.lastName}\`;
    }
    
    return result;
}
    `,
    "version": "1.0.0",
    "author": "JSONdb System"
};

/**
 * Example 2: Price Calculator
 * Calculates total price including tax
 */
const priceCalculator = {
    "name": "Price Calculator",
    "description": "Calculates total price with tax and discounts",
    "type": "transformer",
    "tags": ["products", "orders"],
    "enabled": true,
    "code": `
function transform(document, context) {
    const result = { ...document };
    
    if (result.price !== undefined) {
        const basePrice = parseFloat(result.price) || 0;
        const taxRate = result.taxRate || 0.08; // 8% default
        const discount = result.discount || 0;
        
        // Calculate discount amount
        const discountAmount = basePrice * (discount / 100);
        const discountedPrice = basePrice - discountAmount;
        
        // Calculate tax on discounted price
        const taxAmount = discountedPrice * taxRate;
        const totalPrice = discountedPrice + taxAmount;
        
        // Add calculated fields
        result.discountAmount = Math.round(discountAmount * 100) / 100;
        result.taxAmount = Math.round(taxAmount * 100) / 100;
        result.totalPrice = Math.round(totalPrice * 100) / 100;
        result.calculatedAt = new Date().toISOString();
    }
    
    return result;
}
    `,
    "version": "1.0.0",
    "author": "JSONdb System"
};

/**
 * Example 3: Data Enrichment Transformer
 * Adds metadata and computed fields
 */
const dataEnricher = {
    "name": "Data Enrichment Transformer",
    "description": "Adds metadata, timestamps, and computed fields",
    "type": "transformer",
    "tags": ["*"],
    "enabled": true,
    "code": `
function transform(document, context) {
    const result = { ...document };
    const now = new Date().toISOString();
    
    // Add metadata if not in preview mode
    if (!context.preview) {
        result._metadata = {
            processed_at: now,
            processor: 'data_enricher_v1',
            collection: context.collection,
            operation: context.operation || 'unknown'
        };
    }
    
    // Add created_at if new document
    if (!result.created_at && context.operation === 'create') {
        result.created_at = now;
    }
    
    // Always update updated_at
    result.updated_at = now;
    
    // Generate slug from name if present
    if (result.name && !result.slug) {
        result.slug = result.name
            .toLowerCase()
            .replace(/[^a-z0-9]+/g, '-')
            .replace(/^-+|-+$/g, '');
    }
    
    return result;
}
    `,
    "version": "1.0.0",
    "author": "JSONdb System"
};

// =============================================================================
// FUNCTION EXAMPLES
// =============================================================================

/**
 * Example 1: Statistics Calculator
 * Calculates various statistics for numeric data
 */
const statsCalculator = {
    "name": "Statistics Calculator",
    "description": "Calculates mean, median, mode for numeric arrays",
    "type": "function",
    "tags": ["analytics", "math"],
    "enabled": true,
    "code": `
function execute(input, context) {
    const { data, field } = input;
    
    if (!Array.isArray(data)) {
        throw new Error("Data must be an array");
    }
    
    let numbers;
    if (field) {
        numbers = data.map(item => parseFloat(item[field])).filter(n => !isNaN(n));
    } else {
        numbers = data.map(n => parseFloat(n)).filter(n => !isNaN(n));
    }
    
    if (numbers.length === 0) {
        return { error: "No valid numeric data found" };
    }
    
    // Calculate mean
    const mean = numbers.reduce((sum, n) => sum + n, 0) / numbers.length;
    
    // Calculate median
    const sorted = [...numbers].sort((a, b) => a - b);
    const median = sorted.length % 2 === 0
        ? (sorted[sorted.length / 2 - 1] + sorted[sorted.length / 2]) / 2
        : sorted[Math.floor(sorted.length / 2)];
    
    // Calculate mode
    const frequency = {};
    numbers.forEach(n => frequency[n] = (frequency[n] || 0) + 1);
    const maxFreq = Math.max(...Object.values(frequency));
    const mode = Object.keys(frequency).filter(key => frequency[key] === maxFreq);
    
    return {
        count: numbers.length,
        mean: Math.round(mean * 100) / 100,
        median: Math.round(median * 100) / 100,
        mode: mode.map(m => parseFloat(m)),
        min: Math.min(...numbers),
        max: Math.max(...numbers)
    };
}
    `,
    "version": "1.0.0",
    "author": "JSONdb System"
};

/**
 * Example 2: Data Export Helper
 * Exports data in various formats
 */
const dataExporter = {
    "name": "Data Export Helper",
    "description": "Exports data to CSV, JSON, or XML format",
    "type": "function",
    "tags": ["export", "utility"],
    "enabled": true,
    "code": `
function execute(input, context) {
    const { data, format, fields } = input;
    
    if (!Array.isArray(data)) {
        throw new Error("Data must be an array");
    }
    
    switch (format.toLowerCase()) {
        case 'csv':
            return exportCSV(data, fields);
        case 'json':
            return exportJSON(data, fields);
        case 'xml':
            return exportXML(data, fields);
        default:
            throw new Error("Unsupported format. Use 'csv', 'json', or 'xml'");
    }
}

function exportCSV(data, fields) {
    if (data.length === 0) return { content: '', contentType: 'text/csv' };
    
    const headers = fields || Object.keys(data[0]);
    const csvContent = [
        headers.join(','),
        ...data.map(row => 
            headers.map(header => {
                const value = row[header] || '';
                return typeof value === 'string' && value.includes(',') 
                    ? \`"\${value}"\` : value;
            }).join(',')
        )
    ].join('\\n');
    
    return { content: csvContent, contentType: 'text/csv' };
}

function exportJSON(data, fields) {
    const exportData = fields ? 
        data.map(item => fields.reduce((obj, field) => {
            obj[field] = item[field];
            return obj;
        }, {})) : data;
    
    return { 
        content: JSON.stringify(exportData, null, 2), 
        contentType: 'application/json' 
    };
}

function exportXML(data, fields) {
    const xmlData = data.map(item => {
        const selectedData = fields ? 
            fields.reduce((obj, field) => {
                obj[field] = item[field];
                return obj;
            }, {}) : item;
        
        const xmlItem = Object.entries(selectedData)
            .map(([key, value]) => \`    <\${key}>\${value || ''}</\${key}>\`)
            .join('\\n');
        
        return \`  <item>\\n\${xmlItem}\\n  </item>\`;
    }).join('\\n');
    
    const xmlContent = \`<?xml version="1.0" encoding="UTF-8"?>\\n<data>\\n\${xmlData}\\n</data>\`;
    
    return { content: xmlContent, contentType: 'application/xml' };
}
    `,
    "version": "1.0.0",
    "author": "JSONdb System"
};

/**
 * Example 3: Notification Generator
 * Generates notifications based on data changes
 */
const notificationGenerator = {
    "name": "Notification Generator", 
    "description": "Generates notifications for data changes and events",
    "type": "function",
    "tags": ["notifications", "events"],
    "enabled": true,
    "code": `
function execute(input, context) {
    const { document, previousDocument, event } = input;
    const notifications = [];
    
    // New document created
    if (event === 'create') {
        notifications.push({
            type: 'info',
            title: 'New Record Created',
            message: \`New \${context.collection} record created\`,
            data: { id: document._id, collection: context.collection }
        });
    }
    
    // Document updated
    if (event === 'update' && previousDocument) {
        const changes = findChanges(previousDocument, document);
        
        if (changes.length > 0) {
            notifications.push({
                type: 'info',
                title: 'Record Updated',
                message: \`\${changes.length} field(s) changed in \${context.collection}\`,
                data: { 
                    id: document._id, 
                    collection: context.collection,
                    changes: changes 
                }
            });
            
            // Special notifications for critical fields
            const criticalFields = ['email', 'password', 'status', 'role'];
            const criticalChanges = changes.filter(c => criticalFields.includes(c.field));
            
            if (criticalChanges.length > 0) {
                notifications.push({
                    type: 'warning',
                    title: 'Critical Field Changed',
                    message: \`Critical field(s) modified: \${criticalChanges.map(c => c.field).join(', ')}\`,
                    data: { id: document._id, criticalChanges }
                });
            }
        }
    }
    
    return { notifications, count: notifications.length };
}

function findChanges(oldDoc, newDoc) {
    const changes = [];
    const allKeys = new Set([...Object.keys(oldDoc), ...Object.keys(newDoc)]);
    
    allKeys.forEach(key => {
        if (key.startsWith('_')) return; // Skip metadata fields
        
        const oldValue = oldDoc[key];
        const newValue = newDoc[key];
        
        if (JSON.stringify(oldValue) !== JSON.stringify(newValue)) {
            changes.push({
                field: key,
                oldValue: oldValue,
                newValue: newValue
            });
        }
    });
    
    return changes;
}
    `,
    "version": "1.0.0",
    "author": "JSONdb System"
};

// Export all examples for easy access
module.exports = {
    validators: {
        emailValidator,
        ageValidator,
        requiredFieldsValidator
    },
    transformers: {
        userNormalizer,
        priceCalculator,
        dataEnricher
    },
    functions: {
        statsCalculator,
        dataExporter,
        notificationGenerator
    }
};