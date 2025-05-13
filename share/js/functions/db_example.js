/**
 * Database Example
 * Shows how to use the db_helpers.js library to interact with the database.
 * 
 * To run this example:
 * 1. Start the JSON database server
 * 2. Use the -js_eval_file option to run this file:
 *    bin/jsondb_server -js_eval_file functions/db_example.js
 */

// Load the database helper library
load("functions/db_helpers.js");

// Create a users collection
const users = new Collection('users');

// Create a few test users
console.log("Creating test users...");
const user1 = users.insert({ name: 'Alice', email: 'alice@example.com', age: 28 });
const user2 = users.insert({ name: 'Bob', email: 'bob@example.com', age: 32 });
const user3 = users.insert({ name: 'Charlie', email: 'charlie@example.com', age: 45 });

// Find all users
console.log("Finding all users...");
const allUsers = users.findAll();
console.log(`Found ${allUsers.length} users:`);
allUsers.forEach(user => {
  console.log(`- ${user.name} (${user.email}), ${user.age} years old`);
});

// Get a specific user
console.log("\nGetting user by ID...");
const foundUser = users.get(user1.id);
console.log(`Found user: ${foundUser.name} (${foundUser.email})`);

// Update a user
console.log("\nUpdating user...");
const updatedUser = users.update(user2.id, { 
  name: 'Bob Smith', 
  email: 'bob.smith@example.com', 
  age: 33 
});
console.log(`Updated user: ${updatedUser.name} (${updatedUser.email}), ${updatedUser.age} years old`);

// Query users by criteria
console.log("\nQuerying users by age...");
const olderUsers = users.query({ age: { $gt: 30 } });
console.log(`Found ${olderUsers.length} users over 30:`);
olderUsers.forEach(user => {
  console.log(`- ${user.name} (${user.email}), ${user.age} years old`);
});

// Delete a user
console.log("\nDeleting a user...");
const deleteResult = users.delete(user3.id);
console.log(`User deleted: ${deleteResult}`);

// Verify deletion
console.log("\nVerifying deletion...");
const remainingUsers = users.findAll();
console.log(`Remaining users: ${remainingUsers.length}`);
remainingUsers.forEach(user => {
  console.log(`- ${user.name} (${user.email}), ${user.age} years old`);
});

// Return summary results
({
  success: true,
  usersCreated: 3,
  usersRemaining: remainingUsers.length,
  operations: ['insert', 'get', 'update', 'query', 'delete', 'findAll']
});