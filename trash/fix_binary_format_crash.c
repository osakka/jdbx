// Fix for binary format crash
// The issue appears to be with buffer management during serialization

// In binary_format.c, around line 575-580:
// Problem: The buffer might not be allocated on first use if estimated_size <= temp_size (which is 0 initially)

// Current code:
//   if (estimated_size > temp_size) {
//     void* new_buffer = realloc(temp_buffer, estimated_size);

// Fixed code should be:
//   if (!temp_buffer || estimated_size > temp_size) {
//     void* new_buffer = realloc(temp_buffer, estimated_size);

// Additionally, we should add bounds checking in serialize_json_value to prevent buffer overflows

// Here's the complete fix:

// 1. Fix the initial allocation check (line ~578):
if (!temp_buffer || estimated_size > temp_size) {
    void* new_buffer = realloc(temp_buffer, estimated_size);
    if (!new_buffer) {
        LOG_ERROR("allocate document serialization buffer");
        if (temp_buffer) free(temp_buffer);
        close(fd);
        return 0;
    }
    temp_buffer = new_buffer;
    temp_size = estimated_size;
}

// 2. Add safety margin to estimation (line ~575):
size_t estimated_size = get_json_binary_size(document) + 1024; /* Increased safety margin */

// 3. Validate serialized size doesn't exceed buffer (after line ~591):
if (doc_size > temp_size) {
    LOG_ERROR("Serialized document size (%zu) exceeds buffer size (%zu)", doc_size, temp_size);
    free(temp_buffer);
    close(fd);
    return 0;
}

// 4. Clear sensitive pointers after free (line ~631):
if (temp_buffer) {
    free(temp_buffer);
    temp_buffer = NULL;
    temp_size = 0;
}