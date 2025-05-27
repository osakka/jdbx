#!/usr/bin/env python3
import json
import urllib.request

# Get all metrics documents
url = "http://localhost:5000/api/collections/system_metrics/documents"
with urllib.request.urlopen(url) as response:
    data = json.loads(response.read())

documents = data.get("documents", [])
print(f"Total documents in system_metrics: {len(documents)}")

# Filter documents that don't have our fixed IDs
fixed_ids = ['metrics_operations', 'metrics_performance', 'metrics_cache', 'metrics_memory', 'metrics_connections']
old_docs = [d for d in documents if d.get('_id') not in fixed_ids]

print(f"Old documents to delete: {len(old_docs)}")

# Delete old documents
deleted = 0
for doc in old_docs:
    doc_id = doc.get('_id')
    if doc_id:
        delete_url = f"http://localhost:5000/api/collections/system_metrics/documents/{doc_id}"
        req = urllib.request.Request(delete_url, method='DELETE')
        try:
            with urllib.request.urlopen(req) as response:
                deleted += 1
                if deleted % 10 == 0:
                    print(f"Deleted {deleted} documents...")
        except Exception as e:
            print(f"Failed to delete {doc_id}: {e}")

print(f"\nDeleted {deleted} old metrics documents")
print("Cleanup complete!")