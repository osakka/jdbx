/**
 * User collection document transformer
 * This transforms user documents during various operations
 */
function transformDocument(doc, operation) {
  // Add timestamps
  const now = new Date().toISOString();
  
  if (operation === 'insert') {
    doc.created_at = now;
    doc.updated_at = now;
    
    // Set default values if not provided
    if (!doc.status) {
      doc.status = 'pending';
    }
    
    if (!doc.role) {
      doc.role = 'user';
    }
    
    // Generate display name if not provided
    if (!doc.display_name && doc.username) {
      doc.display_name = doc.username;
    }
  } 
  
  if (operation === 'update') {
    // Update timestamp on any change
    doc.updated_at = now;
    
    // If status is being set to active, record activation time
    if (doc.status === 'active' && doc.previous_status !== 'active') {
      doc.activated_at = now;
    }
  }
  
  // Normalize email (for both insert and update)
  if (doc.email) {
    doc.email = doc.email.toLowerCase().trim();
  }
  
  // Normalize username (for both insert and update)
  if (doc.username) {
    doc.username = doc.username.trim();
  }
  
  // Remove sensitive fields for query results
  if (operation === 'query') {
    delete doc.password;
    delete doc.password_reset_token;
    delete doc.security_questions;
  }
  
  return doc;
}