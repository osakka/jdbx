#include "index/btree_disk.h"
#include "utils/logger.h"
#include "utils/generic_cache.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

/* Default comparison function */
static int default_compare(const void* a, size_t a_len, const void* b, size_t b_len) {
    size_t min_len = a_len < b_len ? a_len : b_len;
    int cmp = memcmp(a, b, min_len);
    if (cmp != 0) return cmp;
    return (a_len < b_len) ? -1 : (a_len > b_len) ? 1 : 0;
}

/* Allocate a new page */
static uint64_t allocate_page(btree_disk_t* tree) {
    uint64_t page_id = tree->storage->header->free_offset;
    tree->storage->header->free_offset += BTREE_PAGE_SIZE;
    
    /* Ensure we have space */
    if (tree->storage->header->free_offset > tree->storage->mapped_size) {
        size_t new_size = tree->storage->mapped_size * 2;
        if (mmap_storage_resize(tree->storage, new_size) != 0) {
            return 0;
        }
    }
    
    return page_id;
}

/* Load a node from disk */
static btree_node_t* load_node(btree_disk_t* tree, uint64_t page_id) {
    if (page_id == 0) return NULL;
    
    /* Check cache first */
    btree_node_t* cached = (btree_node_t*)generic_cache_get(tree->node_cache, &page_id, sizeof(page_id));
    if (cached) {
        atomic_fetch_add(&tree->cache_hits, 1);
        return cached;
    }
    
    atomic_fetch_add(&tree->cache_misses, 1);
    
    /* Load from disk */
    if (page_id >= tree->storage->mapped_size) {
        LOG_ERROR("Invalid page_id %lu", page_id);
        return NULL;
    }
    
    btree_node_t* node = (btree_node_t*)((char*)tree->storage->base_addr + page_id);
    
    /* Add to cache */
    generic_cache_put(tree->node_cache, &page_id, sizeof(page_id), node, btree_node_size(node));
    
    return node;
}

/* Create a new node */
static btree_node_t* create_node(btree_disk_t* tree, btree_node_type_t type, uint32_t level) {
    uint64_t page_id = allocate_page(tree);
    if (page_id == 0) return NULL;
    
    btree_node_t* node = (btree_node_t*)((char*)tree->storage->base_addr + page_id);
    memset(node, 0, BTREE_PAGE_SIZE);
    
    node->type = type;
    node->num_keys = 0;
    node->level = level;
    node->page_id = page_id;
    node->parent_page_id = 0;
    node->prev_page_id = 0;
    node->next_page_id = 0;
    
    return node;
}

/* Split a full node */
static int split_node(btree_disk_t* tree, btree_node_t* parent, uint16_t index, btree_node_t* child) {
    uint16_t mid = tree->order / 2;
    
    /* Create new node */
    btree_node_t* new_node = create_node(tree, child->type, child->level);
    if (!new_node) return -1;
    
    /* Copy upper half of keys to new node */
    uint32_t total_key_size = 0;
    for (uint16_t i = mid; i < child->num_keys; i++) {
        total_key_size += child->key_lengths[i];
    }
    
    /* Copy keys */
    memcpy(new_node->key_data, 
           child->key_data + child->key_offsets[mid],
           total_key_size);
    
    /* Update key offsets and lengths */
    uint32_t offset = 0;
    for (uint16_t i = 0; i < child->num_keys - mid; i++) {
        new_node->key_offsets[i] = offset;
        new_node->key_lengths[i] = child->key_lengths[mid + i];
        offset += new_node->key_lengths[i];
    }
    new_node->num_keys = child->num_keys - mid;
    
    /* Copy pointers */
    if (child->type == BTREE_INTERNAL) {
        memcpy(new_node->pointers,
               &child->pointers[mid],
               (new_node->num_keys + 1) * sizeof(uint64_t));
    } else {
        memcpy(new_node->pointers,
               &child->pointers[mid],
               new_node->num_keys * sizeof(uint64_t));
        
        /* Update sibling pointers for leaves */
        new_node->next_page_id = child->next_page_id;
        new_node->prev_page_id = child->page_id;
        child->next_page_id = new_node->page_id;
        
        if (new_node->next_page_id) {
            btree_node_t* next = load_node(tree, new_node->next_page_id);
            if (next) {
                next->prev_page_id = new_node->page_id;
            }
        }
    }
    
    /* Update child */
    child->num_keys = mid;
    
    /* Get middle key to promote */
    void* mid_key = btree_node_get_key(child, mid - 1);
    size_t mid_key_len = child->key_lengths[mid - 1];
    
    /* If we're splitting an internal node, remove the promoted key */
    if (child->type == BTREE_INTERNAL) {
        child->num_keys--;
    }
    
    /* Insert middle key into parent */
    if (parent == NULL) {
        /* Create new root */
        parent = create_node(tree, BTREE_INTERNAL, child->level + 1);
        if (!parent) return -1;
        
        /* Set first key */
        memcpy(parent->key_data, mid_key, mid_key_len);
        parent->key_offsets[0] = 0;
        parent->key_lengths[0] = mid_key_len;
        parent->num_keys = 1;
        
        /* Set pointers */
        parent->pointers[0] = child->page_id;
        parent->pointers[1] = new_node->page_id;
        
        /* Update parent pointers */
        child->parent_page_id = parent->page_id;
        new_node->parent_page_id = parent->page_id;
        
        /* Update root */
        tree->root_page_id = parent->page_id;
        tree->height++;
    } else {
        /* Insert into existing parent */
        /* Shift keys and pointers to make room */
        uint32_t insert_offset = parent->key_offsets[index];
        uint32_t shift_size = 0;
        
        for (uint16_t i = index; i < parent->num_keys; i++) {
            shift_size += parent->key_lengths[i];
        }
        
        if (shift_size > 0) {
            memmove(parent->key_data + insert_offset + mid_key_len,
                    parent->key_data + insert_offset,
                    shift_size);
        }
        
        /* Insert new key */
        memcpy(parent->key_data + insert_offset, mid_key, mid_key_len);
        
        /* Update offsets */
        for (uint16_t i = parent->num_keys; i > index; i--) {
            parent->key_offsets[i] = parent->key_offsets[i-1] + mid_key_len;
            parent->key_lengths[i] = parent->key_lengths[i-1];
            parent->pointers[i+1] = parent->pointers[i];
        }
        
        parent->key_lengths[index] = mid_key_len;
        parent->pointers[index+1] = new_node->page_id;
        parent->num_keys++;
        
        /* Update parent pointer */
        new_node->parent_page_id = parent->page_id;
    }
    
    atomic_fetch_add(&tree->splits, 1);
    return 0;
}

/* Find the child index for a key */
static uint16_t find_child_index(btree_node_t* node, const void* key, size_t key_len,
                                 int (*compare)(const void*, size_t, const void*, size_t)) {
    uint16_t left = 0;
    uint16_t right = node->num_keys;
    
    while (left < right) {
        uint16_t mid = (left + right) / 2;
        void* node_key = btree_node_get_key(node, mid);
        size_t node_key_len = node->key_lengths[mid];
        
        int cmp = compare(key, key_len, node_key, node_key_len);
        if (cmp > 0) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    
    return left;
}

/* Insert into a non-full node */
static int insert_non_full(btree_disk_t* tree, btree_node_t* node, 
                          const void* key, size_t key_len, uint64_t value) {
    if (node->type == BTREE_LEAF) {
        /* Find position to insert */
        uint16_t index = find_child_index(node, key, key_len, tree->compare);
        
        /* Check if key already exists */
        if (index < node->num_keys) {
            void* existing_key = btree_node_get_key(node, index);
            if (tree->compare(key, key_len, existing_key, node->key_lengths[index]) == 0) {
                /* Update existing value */
                node->pointers[index] = value;
                return 0;
            }
        }
        
        /* Calculate space needed */
        uint32_t insert_offset = (index < node->num_keys) ? 
                                node->key_offsets[index] : 
                                (node->num_keys > 0 ? 
                                 node->key_offsets[node->num_keys-1] + node->key_lengths[node->num_keys-1] : 
                                 0);
        
        /* Shift existing keys */
        uint32_t shift_size = 0;
        for (uint16_t i = index; i < node->num_keys; i++) {
            shift_size += node->key_lengths[i];
        }
        
        if (shift_size > 0) {
            memmove(node->key_data + insert_offset + key_len,
                    node->key_data + insert_offset,
                    shift_size);
        }
        
        /* Insert new key */
        memcpy(node->key_data + insert_offset, key, key_len);
        
        /* Update offsets and pointers */
        for (uint16_t i = node->num_keys; i > index; i--) {
            node->key_offsets[i] = node->key_offsets[i-1] + key_len;
            node->key_lengths[i] = node->key_lengths[i-1];
            node->pointers[i] = node->pointers[i-1];
        }
        
        if (index < node->num_keys) {
            node->key_offsets[index+1] = node->key_offsets[index] + key_len;
        }
        
        node->key_offsets[index] = insert_offset;
        node->key_lengths[index] = key_len;
        node->pointers[index] = value;
        node->num_keys++;
        
        return 0;
    } else {
        /* Internal node - find child */
        uint16_t index = find_child_index(node, key, key_len, tree->compare);
        btree_node_t* child = load_node(tree, node->pointers[index]);
        
        if (!child) return -1;
        
        /* Split child if full */
        if (child->num_keys >= tree->order - 1) {
            split_node(tree, node, index, child);
            
            /* Determine which child to use after split */
            void* split_key = btree_node_get_key(node, index);
            if (tree->compare(key, key_len, split_key, node->key_lengths[index]) > 0) {
                child = load_node(tree, node->pointers[index + 1]);
            }
        }
        
        return insert_non_full(tree, child, key, key_len, value);
    }
}

/* Create B+tree */
btree_disk_t* btree_disk_create(const char* path, uint32_t order,
                               int (*compare)(const void*, size_t, const void*, size_t)) {
    if (!path || order < 3) return NULL;
    
    btree_disk_t* tree = calloc(1, sizeof(btree_disk_t));
    if (!tree) return NULL;
    
    /* Create storage */
    tree->storage = mmap_storage_create(path, 1024 * 1024 * 1024); /* 1GB initial */
    if (!tree->storage) {
        free(tree);
        return NULL;
    }
    
    tree->order = order;
    tree->compare = compare ? compare : default_compare;
    tree->height = 0;
    tree->num_keys = 0;
    
    /* Create node cache */
    tree->node_cache = generic_cache_create(BTREE_CACHE_SIZE);
    if (!tree->node_cache) {
        mmap_storage_destroy(tree->storage);
        free(tree);
        return NULL;
    }
    
    /* Create write buffer */
    tree->write_buffer = skiplist_create(compare);
    if (!tree->write_buffer) {
        generic_cache_destroy(tree->node_cache);
        mmap_storage_destroy(tree->storage);
        free(tree);
        return NULL;
    }
    
    tree->buffer_size_limit = 1024 * 1024; /* 1MB write buffer */
    atomic_init(&tree->current_buffer_size, 0);
    pthread_mutex_init(&tree->flush_mutex, NULL);
    
    /* Initialize statistics */
    atomic_init(&tree->cache_hits, 0);
    atomic_init(&tree->cache_misses, 0);
    atomic_init(&tree->splits, 0);
    atomic_init(&tree->merges, 0);
    
    /* Create root node if new tree */
    if (tree->storage->header->doc_count == 0) {
        btree_node_t* root = create_node(tree, BTREE_LEAF, 0);
        if (!root) {
            btree_disk_destroy(tree);
            return NULL;
        }
        tree->root_page_id = root->page_id;
        tree->height = 1;
    }
    
    LOG_INFO("Created B+tree with order %u", order);
    return tree;
}

/* Destroy B+tree */
void btree_disk_destroy(btree_disk_t* tree) {
    if (!tree) return;
    
    /* Flush any pending writes */
    btree_disk_flush_buffer(tree);
    
    skiplist_destroy(tree->write_buffer);
    generic_cache_destroy(tree->node_cache);
    mmap_storage_destroy(tree->storage);
    pthread_mutex_destroy(&tree->flush_mutex);
    
    free(tree);
}

/* Insert a key-value pair */
int btree_disk_insert(btree_disk_t* tree, const void* key, size_t key_len,
                     uint64_t value_offset) {
    if (!tree || !key) return -1;
    
    /* Add to write buffer first */
    size_t entry_size = sizeof(write_buffer_entry_t) + key_len;
    
    if (atomic_load(&tree->current_buffer_size) + entry_size > tree->buffer_size_limit) {
        /* Buffer full, flush first */
        if (btree_disk_flush_buffer(tree) != 0) {
            return -1;
        }
    }
    
    /* Create buffer entry */
    write_buffer_entry_t* entry = malloc(sizeof(write_buffer_entry_t));
    if (!entry) return -1;
    
    entry->key = malloc(key_len);
    if (!entry->key) {
        free(entry);
        return -1;
    }
    
    memcpy(entry->key, key, key_len);
    entry->key_len = key_len;
    entry->value_offset = value_offset;
    entry->type = WB_INSERT;
    
    /* Add to skip list */
    if (skiplist_insert(tree->write_buffer, key, key_len, entry, sizeof(void*))) {
        atomic_fetch_add(&tree->current_buffer_size, entry_size);
        tree->num_keys++;
        return 0;
    }
    
    free(entry->key);
    free(entry);
    return -1;
}

/* Search for a key */
int btree_disk_search(btree_disk_t* tree, const void* key, size_t key_len,
                     uint64_t* value_offset) {
    if (!tree || !key || !value_offset) return -1;
    
    /* Check write buffer first */
    size_t entry_size;
    write_buffer_entry_t** entry_ptr = (write_buffer_entry_t**)
        skiplist_search(tree->write_buffer, key, key_len, &entry_size);
    
    if (entry_ptr && *entry_ptr) {
        write_buffer_entry_t* entry = *entry_ptr;
        if (entry->type == WB_INSERT) {
            *value_offset = entry->value_offset;
            free(entry_ptr);
            return 0;
        } else if (entry->type == WB_DELETE) {
            free(entry_ptr);
            return -1; /* Deleted */
        }
        free(entry_ptr);
    }
    
    /* Search in tree */
    btree_node_t* node = load_node(tree, tree->root_page_id);
    if (!node) return -1;
    
    while (node->type != BTREE_LEAF) {
        uint16_t index = find_child_index(node, key, key_len, tree->compare);
        node = load_node(tree, node->pointers[index]);
        if (!node) return -1;
    }
    
    /* Search in leaf */
    uint16_t index = find_child_index(node, key, key_len, tree->compare);
    if (index < node->num_keys) {
        void* node_key = btree_node_get_key(node, index);
        if (tree->compare(key, key_len, node_key, node->key_lengths[index]) == 0) {
            *value_offset = node->pointers[index];
            return 0;
        }
    }
    
    return -1; /* Not found */
}

/* Flush write buffer to disk */
int btree_disk_flush_buffer(btree_disk_t* tree) {
    if (!tree) return -1;
    
    pthread_mutex_lock(&tree->flush_mutex);
    
    /* Get all entries from buffer */
    skiplist_iterator_t* iter = skiplist_iterator_create(tree->write_buffer);
    if (!iter) {
        pthread_mutex_unlock(&tree->flush_mutex);
        return -1;
    }
    
    void* key;
    size_t key_len;
    void* value;
    size_t value_len;
    
    while (skiplist_iterator_next(iter, &key, &key_len, &value, &value_len)) {
        write_buffer_entry_t* entry = *(write_buffer_entry_t**)value;
        
        if (entry->type == WB_INSERT) {
            /* Insert into tree */
            btree_node_t* root = load_node(tree, tree->root_page_id);
            if (!root) continue;
            
            /* Split root if full */
            if (root->num_keys >= tree->order - 1) {
                split_node(tree, NULL, 0, root);
                root = load_node(tree, tree->root_page_id);
            }
            
            insert_non_full(tree, root, entry->key, entry->key_len, entry->value_offset);
        }
        
        /* Free entry */
        free(entry->key);
        free(entry);
        free(key);
        free(value);
    }
    
    skiplist_iterator_destroy(iter);
    
    /* Clear buffer */
    skiplist_destroy(tree->write_buffer);
    tree->write_buffer = skiplist_create(tree->compare);
    atomic_store(&tree->current_buffer_size, 0);
    
    /* Sync storage */
    mmap_storage_sync(tree->storage);
    
    pthread_mutex_unlock(&tree->flush_mutex);
    
    return 0;
}

/* Get statistics */
void btree_disk_stats(btree_disk_t* tree, uint64_t* height, uint64_t* num_keys,
                     uint64_t* cache_hits, uint64_t* cache_misses) {
    if (!tree) return;
    
    if (height) *height = tree->height;
    if (num_keys) *num_keys = tree->num_keys;
    if (cache_hits) *cache_hits = atomic_load(&tree->cache_hits);
    if (cache_misses) *cache_misses = atomic_load(&tree->cache_misses);
}