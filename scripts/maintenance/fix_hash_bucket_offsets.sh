#!/bin/bash
# Fix hash bucket offset calculation to properly handle entry_offsets array

echo "Fixing hash bucket offset calculation bug..."

# Backup the original file
cp /opt/jsondb/src/components/index/hash_index.c /opt/jsondb/src/components/index/hash_index.c.backup

# Apply the fix
cat > /tmp/hash_fix.patch << 'EOF'
--- a/src/components/index/hash_index.c
+++ b/src/components/index/hash_index.c
@@ -30,9 +30,18 @@ static uint64_t allocate_bucket(hash_index_t* index) {
     }
     hash_bucket_t* bucket = (hash_bucket_t*)((char*)index->storage->base_addr + offset);
     memset(bucket, 0, HASH_BUCKET_SIZE);
+    
+    /* Initialize bucket header */
     bucket->local_depth = index->global_depth;
     bucket->num_entries = 0;
     bucket->next_bucket = 0;
+    
+    /* IMPORTANT: Clear the entry_offsets array to ensure no garbage values */
+    /* We reserve space for at least 32 entries */
+    uint32_t min_entries = 32;
+    for (uint32_t i = 0; i < min_entries; i++) {
+        bucket->entry_offsets[i] = 0;
+    }
     
     atomic_fetch_add(&index->num_buckets, 1);
     return offset;
@@ -278,10 +287,15 @@ retry:
     /* Reserve space for at least 32 entry offsets to avoid overlapping */
     uint32_t min_offset_slots = 32;
     uint32_t offset_slots = bucket->num_entries + 1;
     if (offset_slots < min_offset_slots) {
         offset_slots = min_offset_slots;
     }
-    uint32_t header_size = sizeof(hash_bucket_t) + offset_slots * sizeof(uint32_t);
+    
+    /* Calculate the actual header size including the flexible array member */
+    /* sizeof(hash_bucket_t) only includes the fixed fields, not entry_offsets[] */
+    uint32_t fixed_header_size = offsetof(hash_bucket_t, entry_offsets);
+    uint32_t header_size = fixed_header_size + offset_slots * sizeof(uint32_t);
     
     uint32_t data_size = 0;
     for (uint32_t i = 0; i < bucket->num_entries; i++) {
@@ -327,8 +341,12 @@ retry:
     uint32_t entry_offset = header_size + data_size;
     
     /* Ensure we don't write into the entry_offsets array */
-    if (entry_offset < sizeof(hash_bucket_t) + (bucket->num_entries + 1) * sizeof(uint32_t)) {
-        LOG_ERROR("Entry offset %u would overlap with header (min required: %zu)", 
+    uint32_t min_required_offset = offsetof(hash_bucket_t, entry_offsets) + 
+                                  (bucket->num_entries + 1) * sizeof(uint32_t);
+    
+    if (entry_offset < min_required_offset) {
+        LOG_ERROR("Entry offset %u would overlap with header (min required: %u)", 
                   entry_offset, sizeof(hash_bucket_t) + (bucket->num_entries + 1) * sizeof(uint32_t));
         pthread_rwlock_unlock(&index->dir_lock);
         return -1;
EOF

cd /opt/jsondb
patch -p1 < /tmp/hash_fix.patch

echo "Fix applied. Rebuilding..."
cd /opt/jsondb/src
make clean
make -j4

echo "Done! Hash bucket offset calculation should now be correct."