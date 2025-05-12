/**
 * User collection document validator
 * This validates user documents according to business rules
 */
function validateDocument(doc) {
  // Required fields
  if (!doc.username) {
    addError('username', 'Username is required');
  } else if (doc.username.length < 3) {
    addError('username', 'Username must be at least 3 characters long');
  }
  
  if (!doc.email) {
    addError('email', 'Email is required');
  } else {
    // Email format validation
    const emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
    if (!emailRegex.test(doc.email)) {
      addError('email', 'Invalid email format');
    }
  }
  
  // Password requirements if present
  if (doc.password !== undefined) {
    if (doc.password.length < 8) {
      addError('password', 'Password must be at least 8 characters long');
    }
    
    // Check for a mix of character types
    const hasLowerCase = /[a-z]/.test(doc.password);
    const hasUpperCase = /[A-Z]/.test(doc.password);
    const hasNumber = /[0-9]/.test(doc.password);
    const hasSpecial = /[!@#$%^&*(),.?":{}|<>]/.test(doc.password);
    
    if (!(hasLowerCase && (hasUpperCase || hasNumber || hasSpecial))) {
      addError('password', 'Password must include a mix of character types');
    }
  }
  
  // Age restrictions if present
  if (doc.age !== undefined) {
    if (typeof doc.age !== 'number') {
      addError('age', 'Age must be a number');
    } else if (doc.age < 13) {
      addError('age', 'User must be at least 13 years old');
    } else if (doc.age > 120) {
      addError('age', 'Age cannot be greater than 120');
    }
  }
  
  // Role validation
  if (doc.role) {
    const validRoles = ['user', 'admin', 'moderator', 'guest'];
    if (!validRoles.includes(doc.role)) {
      addError('role', `Role must be one of: ${validRoles.join(', ')}`);
    }
  }
  
  // Status validation
  if (doc.status) {
    const validStatuses = ['active', 'inactive', 'pending', 'suspended'];
    if (!validStatuses.includes(doc.status)) {
      addError('status', `Status must be one of: ${validStatuses.join(', ')}`);
    }
  }
  
  return isValid;
}