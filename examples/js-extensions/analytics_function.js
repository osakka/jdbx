/**
 * Analytics user function
 * Provides analytics capabilities for the database
 */
function userFunction(args) {
  const { collection, analysis, groupBy, metric, filter } = args;
  
  if (!collection) {
    return { error: "Collection name is required" };
  }
  
  // Get all documents from the collection
  let documents = db.getCollection(collection);
  
  // Apply filter if provided
  if (filter && typeof filter === 'object') {
    documents = db.queryDocuments(collection, filter);
  }
  
  // Handle different analysis types
  switch (analysis) {
    case 'count':
      return { count: documents.length };
      
    case 'group':
      return groupDocuments(documents, groupBy, metric);
      
    case 'timeseries':
      return timeSeriesAnalysis(documents, args.timeField, args.interval, metric);
      
    case 'distribution':
      return distributionAnalysis(documents, args.field, args.buckets);
      
    default:
      return { error: "Unknown analysis type" };
  }
}

/**
 * Group documents by a field and calculate metrics
 */
function groupDocuments(documents, groupBy, metric) {
  if (!groupBy) {
    return { error: "groupBy field is required" };
  }
  
  // Group documents
  const groups = {};
  
  documents.forEach(doc => {
    // Extract the group key (handling nested paths with dot notation)
    let value = doc;
    const parts = groupBy.split('.');
    
    for (const part of parts) {
      if (value && typeof value === 'object') {
        value = value[part];
      } else {
        value = undefined;
        break;
      }
    }
    
    const key = (value === undefined || value === null) ? 'null' : String(value);
    
    if (!groups[key]) {
      groups[key] = [];
    }
    groups[key].push(doc);
  });
  
  // Calculate metrics for each group
  const result = {};
  
  for (const key in groups) {
    const groupDocs = groups[key];
    
    switch (metric) {
      case 'count':
        result[key] = groupDocs.length;
        break;
        
      case 'sum':
        if (!args.field) {
          result[key] = { error: "Field is required for sum metric" };
        } else {
          result[key] = groupDocs.reduce((sum, doc) => {
            const value = getNestedValue(doc, args.field);
            return sum + (typeof value === 'number' ? value : 0);
          }, 0);
        }
        break;
        
      case 'avg':
        if (!args.field) {
          result[key] = { error: "Field is required for avg metric" };
        } else {
          const sum = groupDocs.reduce((total, doc) => {
            const value = getNestedValue(doc, args.field);
            return total + (typeof value === 'number' ? value : 0);
          }, 0);
          result[key] = groupDocs.length > 0 ? sum / groupDocs.length : 0;
        }
        break;
        
      case 'min':
        if (!args.field) {
          result[key] = { error: "Field is required for min metric" };
        } else {
          const values = groupDocs.map(doc => getNestedValue(doc, args.field))
                               .filter(v => typeof v === 'number');
          result[key] = values.length > 0 ? Math.min(...values) : null;
        }
        break;
        
      case 'max':
        if (!args.field) {
          result[key] = { error: "Field is required for max metric" };
        } else {
          const values = groupDocs.map(doc => getNestedValue(doc, args.field))
                               .filter(v => typeof v === 'number');
          result[key] = values.length > 0 ? Math.max(...values) : null;
        }
        break;
        
      default:
        // Default to returning the count
        result[key] = groupDocs.length;
    }
  }
  
  return { results: result };
}

/**
 * Analyze data over time periods
 */
function timeSeriesAnalysis(documents, timeField, interval, metric) {
  if (!timeField) {
    return { error: "Time field is required" };
  }
  
  if (!interval) {
    interval = 'day'; // Default interval
  }
  
  // Parse dates and group by time interval
  const timeGroups = {};
  
  documents.forEach(doc => {
    const dateStr = getNestedValue(doc, timeField);
    if (!dateStr) return;
    
    try {
      const date = new Date(dateStr);
      if (isNaN(date.getTime())) return; // Invalid date
      
      // Format date based on interval
      let timeKey;
      
      switch (interval) {
        case 'hour':
          timeKey = `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())} ${pad(date.getHours())}:00`;
          break;
        case 'day':
          timeKey = `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}`;
          break;
        case 'week':
          // Get the Monday of the week
          const day = date.getDay();
          const diff = date.getDate() - day + (day === 0 ? -6 : 1); // Adjust for Sunday
          const monday = new Date(date);
          monday.setDate(diff);
          timeKey = `${monday.getFullYear()}-${pad(monday.getMonth() + 1)}-${pad(monday.getDate())}`;
          break;
        case 'month':
          timeKey = `${date.getFullYear()}-${pad(date.getMonth() + 1)}`;
          break;
        case 'year':
          timeKey = `${date.getFullYear()}`;
          break;
        default:
          timeKey = `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}`;
      }
      
      if (!timeGroups[timeKey]) {
        timeGroups[timeKey] = [];
      }
      timeGroups[timeKey].push(doc);
    } catch (e) {
      // Skip documents with invalid dates
    }
  });
  
  // Sort time keys
  const sortedKeys = Object.keys(timeGroups).sort();
  
  // Calculate metrics for each time period
  const result = {};
  
  sortedKeys.forEach(key => {
    const groupDocs = timeGroups[key];
    
    switch (metric) {
      case 'count':
        result[key] = groupDocs.length;
        break;
      case 'sum':
        if (!args.field) {
          result[key] = 0;
        } else {
          result[key] = groupDocs.reduce((sum, doc) => {
            const value = getNestedValue(doc, args.field);
            return sum + (typeof value === 'number' ? value : 0);
          }, 0);
        }
        break;
      default:
        result[key] = groupDocs.length;
    }
  });
  
  return { timeline: result };
}

/**
 * Create a distribution analysis for a numeric field
 */
function distributionAnalysis(documents, field, numBuckets) {
  if (!field) {
    return { error: "Field is required for distribution analysis" };
  }
  
  if (!numBuckets || numBuckets < 1) {
    numBuckets = 10; // Default number of buckets
  }
  
  // Extract numeric values for the field
  const values = documents.map(doc => {
    const value = getNestedValue(doc, field);
    return typeof value === 'number' ? value : null;
  }).filter(v => v !== null);
  
  if (values.length === 0) {
    return { error: "No numeric values found for the specified field" };
  }
  
  // Find min and max values
  const min = Math.min(...values);
  const max = Math.max(...values);
  
  // Create buckets
  const bucketSize = (max - min) / numBuckets;
  const buckets = Array(numBuckets).fill(0);
  
  // Count values in each bucket
  values.forEach(value => {
    const bucketIndex = Math.min(numBuckets - 1, Math.floor((value - min) / bucketSize));
    buckets[bucketIndex]++;
  });
  
  // Format result
  const result = {
    field,
    min,
    max,
    count: values.length,
    mean: values.reduce((sum, v) => sum + v, 0) / values.length,
    distribution: {}
  };
  
  for (let i = 0; i < numBuckets; i++) {
    const lowerBound = min + (i * bucketSize);
    const upperBound = i < numBuckets - 1 ? min + ((i + 1) * bucketSize) : max;
    const label = `${lowerBound.toFixed(2)}-${upperBound.toFixed(2)}`;
    result.distribution[label] = buckets[i];
  }
  
  return result;
}

// Helper function to get a value from a nested path
function getNestedValue(obj, path) {
  if (!obj || !path) return undefined;
  
  const parts = path.split('.');
  let value = obj;
  
  for (const part of parts) {
    if (value === null || value === undefined || typeof value !== 'object') {
      return undefined;
    }
    value = value[part];
  }
  
  return value;
}

// Helper function to pad numbers with leading zero
function pad(num) {
  return num.toString().padStart(2, '0');
}