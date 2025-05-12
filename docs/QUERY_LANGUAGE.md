# JSON Database Query Language

This document describes the query language syntax for the JSON Database Server. The query language is inspired by MongoDB's query language and supports a range of operators for filtering documents.

## Basic Query Structure

A query is a JSON object where each key-value pair represents a condition to match against documents. For example:

```json
{
  "name": "John",
  "age": 25
}
```

This query matches documents with the exact name "John" AND age 25.

## Comparison Operators

The following comparison operators are available:

| Operator | Description |
|----------|-------------|
| `$eq` | Equal to |
| `$ne` | Not equal to |
| `$gt` | Greater than |
| `$gte` | Greater than or equal to |
| `$lt` | Less than |
| `$lte` | Less than or equal to |
| `$in` | In an array |
| `$nin` | Not in an array |

Example:

```json
{
  "age": { "$gte": 21, "$lte": 65 },
  "status": { "$in": ["active", "pending"] }
}
```

This matches documents with an age between 21 and 65 (inclusive) AND status that is either "active" or "pending".

## Logical Operators

The following logical operators are available:

| Operator | Description |
|----------|-------------|
| `$and` | Logical AND (all conditions must match) |
| `$or` | Logical OR (at least one condition must match) |
| `$not` | Logical NOT (negates the condition) |
| `$nor` | Logical NOR (none of the conditions match) |

Example:

```json
{
  "$or": [
    { "status": "active" },
    { "$and": [ 
      { "age": { "$gte": 65 } },
      { "status": "retired" }
    ]}
  ]
}
```

This matches documents that are either active OR (age >= 65 AND status is "retired").

## Array Operators

The following array operators are available:

| Operator | Description |
|----------|-------------|
| `$all` | Array field contains all specified elements |
| `$elemMatch` | Array field contains at least one element that matches all conditions |
| `$size` | Array field has the specified number of elements |

Example:

```json
{
  "tags": { "$all": ["database", "json"] },
  "comments": { "$elemMatch": { "author": "John", "approved": true } },
  "roles": { "$size": 3 }
}
```

This matches documents that have both "database" and "json" in their tags array, at least one approved comment by John, and exactly 3 roles.

## Field Path Notation

You can query nested fields using dot notation:

```json
{
  "profile.address.city": "New York",
  "profile.phone": { "$ne": null }
}
```

This matches documents where the city field in the address object inside the profile object is "New York" AND the phone field in the profile object is not null.

## Querying Arrays by Index

You can query array elements by their index:

```json
{
  "scores.0": { "$gte": 90 }
}
```

This matches documents where the first element in the scores array is greater than or equal to 90.

## Query Projection

You can specify which fields to include or exclude in the results using a projection object. A projection is a JSON object where the keys are field names and the values are either 1 (include) or 0 (exclude).

Example:

```json
{
  "query": { "status": "active" },
  "projection": { "name": 1, "email": 1, "_id": 1 }
}
```

This returns only the name, email, and _id fields for active users.

You can also exclude fields:

```json
{
  "query": { "status": "active" },
  "projection": { "password": 0, "securityAnswer": 0 }
}
```

This returns all fields except password and securityAnswer for active users.

Note: You cannot mix inclusion and exclusion in the same projection object, except for the `_id` field.

## Pagination

You can paginate results using the `limit` and `skip` parameters:

```json
{
  "query": { "status": "active" },
  "skip": 10,
  "limit": 5
}
```

This skips the first 10 active user documents and returns the next 5.

## Sorting

You can sort results using the `sort` parameter. The sort parameter is a JSON object where the keys are field names and the values are either 1 (ascending) or -1 (descending).

```json
{
  "query": { "status": "active" },
  "sort": { "lastName": 1, "firstName": 1 }
}
```

This returns active users sorted alphabetically by last name and then first name.

## Examples

### Finding Users by Age Range

```json
{
  "age": { "$gte": 18, "$lte": 30 }
}
```

### Finding Products with Specific Tags

```json
{
  "tags": { "$all": ["electronics", "sale"] }
}
```

### Finding Orders with Specific Status and Total

```json
{
  "$and": [
    { "status": "shipped" },
    { "total": { "$gt": 100 } }
  ]
}
```

### Finding Users in Specific Locations

```json
{
  "profile.address.country": { "$in": ["USA", "Canada", "Mexico"] }
}
```

### Complex Query with Multiple Conditions

```json
{
  "$or": [
    {
      "status": "premium",
      "subscriptionEnd": { "$gte": "2023-01-01" }
    },
    {
      "$and": [
        { "status": "trial" },
        { "createdAt": { "$gte": "2022-12-01" } },
        { "emailVerified": true }
      ]
    }
  ]
}
```

This matches documents that are either:
- Premium users with a subscription ending on or after 2023-01-01, OR
- Trial users who signed up on or after 2022-12-01 and have verified their email

## Error Handling

If a query contains invalid syntax or unsupported operators, the server will return a 400 Bad Request response with an error message describing the issue.