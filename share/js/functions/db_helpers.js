/**
 * Database Helper Functions
 * Provides a standardized interface for JavaScript to interact with the JSON database.
 * 
 * Usage:
 *   1. Import this file in your JavaScript: load("db_helpers.js");
 *   2. Use the provided helpers to interact with collections
 */

// Make sure the database object is available
if (typeof db === 'undefined') {
  throw new Error('Database object not available. This helper must be used within the JSON database context.');
}

/**
 * Database class providing helper methods for common operations
 */
class JsonDB {
  /**
   * Create a collection if it doesn't exist
   * @param {string} collectionName - Name of the collection
   * @returns {boolean} - True if the collection exists or was created
   */
  static ensureCollection(collectionName) {
    // Check if collection already exists by trying to query it
    try {
      const result = db.queryDocuments(collectionName, {});
      return true;
    } catch (err) {
      // Collection doesn't exist - it will be created automatically
      // when the first document is inserted
      return true;
    }
  }
  
  /**
   * Insert a document into a collection
   * @param {string} collectionName - Name of the collection
   * @param {object} document - Document to insert
   * @returns {object} - The inserted document
   */
  static insert(collectionName, document) {
    if (!document.id) {
      document.id = this.generateId();
    }
    
    return db.insertDocument(collectionName, document);
  }
  
  /**
   * Get a document by ID
   * @param {string} collectionName - Name of the collection
   * @param {string} id - Document ID
   * @returns {object|null} - The document or null if not found
   */
  static get(collectionName, id) {
    return db.getDocument(collectionName, id);
  }
  
  /**
   * Update a document
   * @param {string} collectionName - Name of the collection
   * @param {string} id - Document ID
   * @param {object} document - Updated document
   * @returns {object|null} - The updated document or null if not found
   */
  static update(collectionName, id, document) {
    // Ensure ID is preserved
    document.id = id;
    
    return db.updateDocument(collectionName, id, document);
  }
  
  /**
   * Delete a document
   * @param {string} collectionName - Name of the collection
   * @param {string} id - Document ID
   * @returns {boolean} - True if the document was deleted
   */
  static delete(collectionName, id) {
    return db.deleteDocument(collectionName, id);
  }
  
  /**
   * Query documents in a collection
   * @param {string} collectionName - Name of the collection
   * @param {object} query - Query criteria
   * @returns {array} - Array of matching documents
   */
  static query(collectionName, query) {
    return db.queryDocuments(collectionName, query);
  }
  
  /**
   * Find all documents in a collection
   * @param {string} collectionName - Name of the collection
   * @returns {array} - All documents in the collection
   */
  static findAll(collectionName) {
    return db.queryDocuments(collectionName, {});
  }
  
  /**
   * Generate a unique ID
   * @returns {string} - A unique ID
   */
  static generateId() {
    return Date.now().toString(36) + Math.random().toString(36).substring(2, 9);
  }
}

/**
 * Collection class for working with specific collections
 */
class Collection {
  /**
   * Create a collection helper
   * @param {string} name - Collection name
   */
  constructor(name) {
    this.name = name;
    JsonDB.ensureCollection(name);
  }
  
  /**
   * Insert a document
   * @param {object} document - Document to insert
   * @returns {object} - The inserted document
   */
  insert(document) {
    return JsonDB.insert(this.name, document);
  }
  
  /**
   * Get a document by ID
   * @param {string} id - Document ID
   * @returns {object|null} - The document or null if not found
   */
  get(id) {
    return JsonDB.get(this.name, id);
  }
  
  /**
   * Update a document
   * @param {string} id - Document ID
   * @param {object} document - Updated document
   * @returns {object|null} - The updated document or null if not found
   */
  update(id, document) {
    return JsonDB.update(this.name, id, document);
  }
  
  /**
   * Delete a document
   * @param {string} id - Document ID
   * @returns {boolean} - True if the document was deleted
   */
  delete(id) {
    return JsonDB.delete(this.name, id);
  }
  
  /**
   * Query documents
   * @param {object} query - Query criteria
   * @returns {array} - Array of matching documents
   */
  query(query) {
    return JsonDB.query(this.name, query);
  }
  
  /**
   * Find all documents
   * @returns {array} - All documents in the collection
   */
  findAll() {
    return JsonDB.findAll(this.name);
  }
}

// Export the helper classes
const DB = JsonDB;

// Example usage:
// const users = new Collection('users');
// const user = users.insert({ name: 'John', age: 30 });
// const foundUser = users.get(user.id);