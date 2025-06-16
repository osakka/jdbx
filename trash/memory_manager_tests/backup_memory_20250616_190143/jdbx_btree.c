/*
 * JDBX B-tree Implementation - Key-value storage engine
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "storage/jdbx.h"
#include "utils/logger.h"
#include "utils/buffer_pool.h"

/* B-tree constants */
#define BTREE_MIN_KEYS 50
#define BTREE_MAX_KEYS 100
#define BTREE_MIN_CHILDREN (BTREE_MIN_KEYS + 1)
#define BTREE_MAX_CHILDREN (BTREE_MAX_KEYS + 1)

/* Default key comparison function */
static int default_compare(const void* a, size_t a_len, 
                          const void* b, size_t b_len) {
    size_t min_len = a_len < b_len ? a_len : b_len;
    int cmp = memcmp(a, b, min_len);
    if (cmp != 0) return cmp;
    return a_len < b_len ? -1 : (a_len > b_len ? 1 : 0);
}

/* Create a new B-tree */
jdbx_btree_t* jdbx_btree_create(jdbx_page_manager_t* pm, 
                                int (*compare)(const void*, size_t, const void*, size_t)) {
    jdbx_btree_t* tree = calloc(1, sizeof(jdbx_btree_t));
    if (!tree) {
        jdbx_error("Failed to allocate B-tree");
        return NULL;
    }
    
    tree->pm = pm;
    tree->compare = compare ? compare : default_compare;
    
    /* Allocate root page */
    tree->root_page = jdbx_alloc_page(pm, PAGE_TYPE_BTREE_LEAF);
    if (tree->root_page == 0) {
        jdbx_error("Failed to allocate root page");
        BUFFER_FREE(tree);
        return NULL;
    }
    
    /* Initialize root node */
    btree_node_t* root = (btree_node_t*)jdbx_get_page_for_write(pm, tree->root_page);
    if (!root) {
        jdbx_free_page(pm, tree->root_page);
        BUFFER_FREE(tree);
        return NULL;
    }
    
    root->header.type = PAGE_TYPE_BTREE_LEAF;
    root->header.page_id = tree->root_page;
    root->num_keys = 0;
    root->level = 0; /* Leaf node */
    root->total_size = sizeof(btree_node_t);
    root->right_sibling = 0;
    root->parent_page = 0;
    
    /* Update page checksum */
    root->header.checksum = jdbx_crc32(root, JDBX_PAGE_SIZE);
    
    /* Initialize statistics */
    tree->stats.height = 1;
    tree->stats.num_keys = 0;
    tree->stats.num_pages = 1;
    
    LOG_DEBUG("Created B-tree with root page %llu", 
              (unsigned long long)tree->root_page);
    
    return tree;
}

/* Open existing B-tree */
jdbx_btree_t* jdbx_btree_open(jdbx_page_manager_t* pm, uint64_t root_page,
                              int (*compare)(const void*, size_t, const void*, size_t)) {
    jdbx_btree_t* tree = calloc(1, sizeof(jdbx_btree_t));
    if (!tree) {
        jdbx_error("Failed to allocate B-tree");
        return NULL;
    }
    
    tree->pm = pm;
    tree->root_page = root_page;
    tree->compare = compare ? compare : default_compare;
    
    /* Verify root page */
    btree_node_t* root = (btree_node_t*)jdbx_get_page(pm, root_page);
    if (!root) {
        jdbx_error("Failed to read root page %llu", 
                   (unsigned long long)root_page);
        BUFFER_FREE(tree);
        return NULL;
    }
    
    if (root->header.type != PAGE_TYPE_BTREE_LEAF && 
        root->header.type != PAGE_TYPE_BTREE_INTERNAL) {
        jdbx_error("Invalid root page type: %d", root->header.type);
        BUFFER_FREE(tree);
        return NULL;
    }
    
    /* TODO: Calculate statistics by traversing tree */
    tree->stats.height = 1; /* Will be updated */
    tree->stats.num_keys = 0;
    tree->stats.num_pages = 1;
    
    LOG_DEBUG("Opened B-tree with root page %llu",
              (unsigned long long)tree->root_page);
    
    return tree;
}

/* Get pointer to key data in node */
static uint8_t* get_key_ptr(btree_node_t* node, int index) {
    uint8_t* data = (uint8_t*)(node + 1); /* After node header */
    
    /* Skip to the requested key */
    for (int i = 0; i < index; i++) {
        btree_entry_t* entry = (btree_entry_t*)data;
        data += sizeof(btree_entry_t) + entry->key_size;
        
        /* For leaf nodes, skip value data */
        if (node->level == 0 && entry->overflow_page == 0) {
            data += entry->value_size;
        }
        
        /* For internal nodes, skip child pointer */
        if (node->level > 0) {
            data += sizeof(uint64_t);
        }
    }
    
    return data;
}

/* Binary search for key in node */
static int find_key_position(jdbx_btree_t* tree, btree_node_t* node,
                            const void* key, size_t key_len,
                            int* found) {
    *found = 0;
    
    if (node->num_keys == 0) {
        return 0;
    }
    
    int left = 0;
    int right = node->num_keys - 1;
    
    while (left <= right) {
        int mid = (left + right) / 2;
        
        /* Get key at mid position */
        uint8_t* entry_ptr = get_key_ptr(node, mid);
        btree_entry_t* entry = (btree_entry_t*)entry_ptr;
        void* mid_key = entry_ptr + sizeof(btree_entry_t);
        
        int cmp = tree->compare(key, key_len, mid_key, entry->key_size);
        
        if (cmp == 0) {
            *found = 1;
            return mid;
        } else if (cmp < 0) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }
    
    return left;
}

/* Get child page ID from internal node */
static uint64_t get_child_page(btree_node_t* node, int index) {
    assert(node->level > 0); /* Must be internal node */
    
    uint8_t* data = (uint8_t*)(node + 1);
    
    /* Left-most child is stored separately */
    if (index == 0) {
        return *(uint64_t*)data;
    }
    
    /* Skip to the child after key[index-1] */
    data += sizeof(uint64_t); /* Skip left-most child */
    
    for (int i = 0; i < index - 1; i++) {
        btree_entry_t* entry = (btree_entry_t*)data;
        data += sizeof(btree_entry_t) + entry->key_size + sizeof(uint64_t);
    }
    
    btree_entry_t* entry = (btree_entry_t*)data;
    data += sizeof(btree_entry_t) + entry->key_size;
    
    return *(uint64_t*)data;
}

/* Check if node needs splitting */
static int node_is_full(btree_node_t* node) {
    return node->num_keys >= BTREE_MAX_KEYS;
}

/* Forward declarations */
static int split_node(jdbx_btree_t* tree, uint64_t parent_page,
                     uint64_t full_page, int parent_index);

/* Overflow page threshold - values larger than this go to overflow pages */
#define OVERFLOW_THRESHOLD 2048

/* Write value to overflow pages */
static uint64_t write_overflow_value(jdbx_btree_t* tree, const void* value, size_t value_len) {
    /* Calculate number of pages needed */
    size_t pages_needed = (value_len + JDBX_PAGE_SIZE - 1) / JDBX_PAGE_SIZE;
    
    uint64_t first_page = 0;
    uint64_t prev_page = 0;
    
    const uint8_t* data = (const uint8_t*)value;
    size_t remaining = value_len;
    
    for (size_t i = 0; i < pages_needed; i++) {
        /* Allocate overflow page */
        uint64_t page_num = jdbx_alloc_page(tree->pm, PAGE_TYPE_OVERFLOW);
        if (page_num == 0) {
            /* TODO: Free already allocated pages */
            return 0;
        }
        
        if (first_page == 0) {
            first_page = page_num;
        }
        
        /* Get page for writing */
        page_header_t* page = jdbx_get_page_for_write(tree->pm, page_num);
        if (!page) {
            /* TODO: Free already allocated pages */
            return 0;
        }
        
        /* Link to previous page */
        if (prev_page != 0) {
            page_header_t* prev = jdbx_get_page_for_write(tree->pm, prev_page);
            if (prev) {
                prev->next_page = page_num;
            }
        }
        
        /* Write data to page */
        size_t chunk_size = remaining > (JDBX_PAGE_SIZE - sizeof(page_header_t)) ?
                           (JDBX_PAGE_SIZE - sizeof(page_header_t)) : remaining;
        
        memcpy((uint8_t*)page + sizeof(page_header_t), data, chunk_size);
        
        /* Store size in first word of first page */
        if (i == 0) {
            uint32_t* size_ptr = (uint32_t*)((uint8_t*)page + sizeof(page_header_t));
            *size_ptr = (uint32_t)value_len;
        }
        
        data += chunk_size;
        remaining -= chunk_size;
        prev_page = page_num;
        
        /* Update page checksum */
        page->checksum = jdbx_crc32(page, JDBX_PAGE_SIZE);
    }
    
    return first_page;
}

/* Read value from overflow pages */
static void* read_overflow_value(jdbx_btree_t* tree, uint64_t overflow_page, size_t* value_len) {
    if (overflow_page == 0) return NULL;
    
    /* Read first page to get size */
    page_header_t* first_page = jdbx_get_page(tree->pm, overflow_page);
    if (!first_page || first_page->type != PAGE_TYPE_OVERFLOW) {
        return NULL;
    }
    
    /* Get total size from first word */
    uint32_t total_size = *(uint32_t*)((uint8_t*)first_page + sizeof(page_header_t));
    *value_len = total_size;
    
    /* Allocate buffer for full value */
    void* value = malloc(total_size);
    if (!value) return NULL;
    
    uint8_t* data = (uint8_t*)value;
    size_t copied = 0;
    uint64_t current_page = overflow_page;
    
    while (current_page != 0 && copied < total_size) {
        page_header_t* page = jdbx_get_page(tree->pm, current_page);
        if (!page || page->type != PAGE_TYPE_OVERFLOW) {
            BUFFER_FREE(value);
            return NULL;
        }
        
        /* Calculate copy size */
        size_t offset = (current_page == overflow_page) ? sizeof(uint32_t) : 0;
        size_t chunk_size = JDBX_PAGE_SIZE - sizeof(page_header_t) - offset;
        if (copied + chunk_size > total_size) {
            chunk_size = total_size - copied;
        }
        
        /* Copy data */
        memcpy(data + copied, (uint8_t*)page + sizeof(page_header_t) + offset, chunk_size);
        copied += chunk_size;
        
        /* Move to next page */
        current_page = page->next_page;
    }
    
    if (copied != total_size) {
        BUFFER_FREE(value);
        return NULL;
    }
    
    return value;
}

/* Insert key into parent node */
static int insert_into_parent(jdbx_btree_t* tree, uint64_t parent_page,
                              const void* key, size_t key_len,
                              uint64_t left_child, uint64_t right_child) {
    btree_node_t* parent = (btree_node_t*)jdbx_get_page_for_write(tree->pm, parent_page);
    if (!parent) return -1;
    
    /* Check if parent is full */
    if (node_is_full(parent)) {
        /* Split parent first */
        if (split_node(tree, parent->parent_page, parent_page, 0) != 0) {
            return -1;
        }
        /* After parent split, find correct parent for our key */
        return insert_into_parent(tree, parent_page, key, key_len, left_child, right_child);
    }
    
    /* Find position to insert key in parent */
    int found;
    int pos = find_key_position(tree, parent, key, key_len, &found);
    
    /* Calculate space needed for new entry */
    size_t entry_size = sizeof(btree_entry_t) + key_len + sizeof(uint64_t);
    
    /* Check if there's enough space */
    if (parent->total_size + entry_size > JDBX_PAGE_SIZE) {
        /* Split parent */
        if (split_node(tree, parent->parent_page, parent_page, 0) != 0) {
            return -1;
        }
        return insert_into_parent(tree, parent_page, key, key_len, left_child, right_child);
    }
    
    /* Insert key and update child pointers */
    uint8_t* insert_ptr = get_key_ptr(parent, pos);
    size_t shift_size = 0;
    
    /* Calculate size of data to shift */
    for (int i = pos; i < parent->num_keys; i++) {
        btree_entry_t* entry = (btree_entry_t*)get_key_ptr(parent, i);
        shift_size += sizeof(btree_entry_t) + entry->key_size + sizeof(uint64_t);
    }
    
    /* Shift existing entries */
    if (shift_size > 0) {
        memmove(insert_ptr + entry_size, insert_ptr, shift_size);
    }
    
    /* Insert new entry */
    btree_entry_t* new_entry = (btree_entry_t*)insert_ptr;
    new_entry->key_size = key_len;
    new_entry->value_size = 0; /* Internal nodes don't store values */
    new_entry->overflow_page = 0;
    
    memcpy(insert_ptr + sizeof(btree_entry_t), key, key_len);
    
    /* Insert right child pointer after the key */
    uint64_t* child_ptr = (uint64_t*)(insert_ptr + sizeof(btree_entry_t) + key_len);
    *child_ptr = right_child;
    
    /* Update child pointers - the left child is already correctly positioned */
    /* We need to update the child pointer that was at position 'pos' to point to left_child */
    if (pos == 0) {
        /* Update left-most child pointer */
        uint64_t* leftmost_ptr = (uint64_t*)(parent + 1);
        *leftmost_ptr = left_child;
    } else {
        /* Update the child pointer after the previous key */
        uint8_t* prev_entry_ptr = get_key_ptr(parent, pos - 1);
        btree_entry_t* prev_entry = (btree_entry_t*)prev_entry_ptr;
        uint64_t* prev_child_ptr = (uint64_t*)(prev_entry_ptr + sizeof(btree_entry_t) + prev_entry->key_size);
        *prev_child_ptr = left_child;
    }
    
    /* Update node metadata */
    parent->num_keys++;
    parent->total_size += entry_size;
    parent->header.checksum = jdbx_crc32(parent, JDBX_PAGE_SIZE);
    
    return 0;
}

/* Split a full node */
static int split_node(jdbx_btree_t* tree, uint64_t parent_page,
                     uint64_t full_page, int parent_index) {
    (void)parent_index; /* Will be used for parent update */
    btree_node_t* full_node = (btree_node_t*)jdbx_get_page_for_write(tree->pm, full_page);
    if (!full_node) return -1;
    
    /* Allocate new sibling page */
    uint64_t sibling_page = jdbx_alloc_page(tree->pm, 
                                           full_node->level == 0 ? 
                                           PAGE_TYPE_BTREE_LEAF : 
                                           PAGE_TYPE_BTREE_INTERNAL);
    if (sibling_page == 0) return -1;
    
    btree_node_t* sibling = (btree_node_t*)jdbx_get_page_for_write(tree->pm, sibling_page);
    if (!sibling) {
        jdbx_free_page(tree->pm, sibling_page);
        return -1;
    }
    
    /* Initialize sibling node */
    sibling->header.type = full_node->header.type;
    sibling->header.page_id = sibling_page;
    sibling->level = full_node->level;
    sibling->parent_page = parent_page;
    sibling->right_sibling = full_node->right_sibling;
    
    /* Split keys: keep first half in full_node, move second half to sibling */
    int mid_index = full_node->num_keys / 2;
    
    /* Get middle key that will be promoted to parent */
    uint8_t* mid_ptr = get_key_ptr(full_node, mid_index);
    btree_entry_t* mid_entry = (btree_entry_t*)mid_ptr;
    
    /* Copy middle key for promotion */
    size_t mid_key_size = mid_entry->key_size;
    uint8_t* mid_key = malloc(mid_key_size);
    if (!mid_key) {
        jdbx_free_page(tree->pm, sibling_page);
        return -1;
    }
    memcpy(mid_key, mid_ptr + sizeof(btree_entry_t), mid_key_size);
    
    /* Copy second half of keys to sibling */
    uint8_t* src_ptr = get_key_ptr(full_node, mid_index + 1);
    uint8_t* dst_ptr = (uint8_t*)(sibling + 1);
    
    if (full_node->level > 0) {
        /* Internal node: also need to copy child pointers */
        dst_ptr += sizeof(uint64_t); /* Space for left-most child */
    }
    
    size_t copy_size = 0;
    for (int i = mid_index + 1; i < full_node->num_keys; i++) {
        btree_entry_t* entry = (btree_entry_t*)src_ptr;
        size_t entry_size = sizeof(btree_entry_t) + entry->key_size;
        
        if (full_node->level == 0 && entry->overflow_page == 0) {
            entry_size += entry->value_size;
        } else if (full_node->level > 0) {
            entry_size += sizeof(uint64_t);
        }
        
        copy_size += entry_size;
        src_ptr += entry_size;
    }
    
    memcpy(dst_ptr, get_key_ptr(full_node, mid_index + 1), copy_size);
    
    /* Update key counts */
    sibling->num_keys = full_node->num_keys - mid_index - 1;
    full_node->num_keys = mid_index;
    
    /* Update total sizes */
    sibling->total_size = sizeof(btree_node_t) + copy_size;
    full_node->total_size -= copy_size + sizeof(btree_entry_t) + mid_key_size;
    
    /* Link siblings */
    full_node->right_sibling = sibling_page;
    
    /* Now insert middle key into parent */
    if (parent_page == 0) {
        /* Splitting root - create new root */
        uint64_t new_root_page = jdbx_alloc_page(tree->pm, PAGE_TYPE_BTREE_INTERNAL);
        if (new_root_page == 0) {
            BUFFER_FREE(mid_key);
            jdbx_free_page(tree->pm, sibling_page);
            return -1;
        }
        
        btree_node_t* new_root = (btree_node_t*)jdbx_get_page_for_write(tree->pm, 
                                                                        new_root_page);
        if (!new_root) {
            BUFFER_FREE(mid_key);
            jdbx_free_page(tree->pm, sibling_page);
            jdbx_free_page(tree->pm, new_root_page);
            return -1;
        }
        
        /* Initialize new root */
        new_root->header.type = PAGE_TYPE_BTREE_INTERNAL;
        new_root->header.page_id = new_root_page;
        new_root->level = full_node->level + 1;
        new_root->num_keys = 1;
        new_root->parent_page = 0;
        new_root->right_sibling = 0;
        
        /* Insert middle key and child pointers */
        uint8_t* root_data = (uint8_t*)(new_root + 1);
        
        /* Left-most child */
        *(uint64_t*)root_data = full_page;
        root_data += sizeof(uint64_t);
        
        /* Middle key */
        btree_entry_t* root_entry = (btree_entry_t*)root_data;
        root_entry->key_size = mid_key_size;
        root_entry->value_size = 0;
        root_entry->overflow_page = 0;
        memcpy(root_data + sizeof(btree_entry_t), mid_key, mid_key_size);
        root_data += sizeof(btree_entry_t) + mid_key_size;
        
        /* Right child */
        *(uint64_t*)root_data = sibling_page;
        
        /* Update total size */
        new_root->total_size = sizeof(btree_node_t) + sizeof(uint64_t) + 
                              sizeof(btree_entry_t) + mid_key_size + sizeof(uint64_t);
        
        /* Update parent pointers */
        full_node->parent_page = new_root_page;
        sibling->parent_page = new_root_page;
        
        /* Update tree root */
        tree->root_page = new_root_page;
        tree->stats.height++;
        tree->stats.num_pages++;
        
        /* Update checksums */
        new_root->header.checksum = jdbx_crc32(new_root, JDBX_PAGE_SIZE);
    } else {
        /* Insert middle key into existing parent */
        if (insert_into_parent(tree, parent_page, mid_key, mid_key_size,
                              full_page, sibling_page) != 0) {
            BUFFER_FREE(mid_key);
            jdbx_free_page(tree->pm, sibling_page);
            return -1;
        }
    }
    
    /* Update checksums */
    full_node->header.checksum = jdbx_crc32(full_node, JDBX_PAGE_SIZE);
    sibling->header.checksum = jdbx_crc32(sibling, JDBX_PAGE_SIZE);
    
    BUFFER_FREE(mid_key);
    tree->stats.num_pages++;
    
    return 0;
}

/* Insert key-value pair into B-tree */
int jdbx_btree_insert(jdbx_btree_t* tree, 
                      const void* key, size_t key_len,
                      const void* value, size_t value_len) {
    if (!tree || !key || key_len == 0 || !value || value_len == 0) {
        return -1;
    }
    
    if (key_len > JDBX_MAX_KEY_SIZE) {
        jdbx_error("Key too large: %zu bytes", key_len);
        return -1;
    }
    
    /* Start from root */
    uint64_t current_page = tree->root_page;
    
    while (1) {
        btree_node_t* node = (btree_node_t*)jdbx_get_page_for_write(tree->pm, current_page);
        if (!node) return -1;
        
        /* Check if node is full and split if needed */
        if (node_is_full(node)) {
            if (split_node(tree, node->parent_page, current_page, 0) != 0) {
                return -1;
            }
            
            /* After split, restart from root */
            current_page = tree->root_page;
            continue;
        }
        
        /* Find position for key */
        int found;
        int pos = find_key_position(tree, node, key, key_len, &found);
        
        if (node->level == 0) {
            /* Leaf node - insert here */
            if (found) {
                /* Key already exists - update value */
                uint8_t* entry_ptr = get_key_ptr(node, pos);
                btree_entry_t* entry = (btree_entry_t*)entry_ptr;
                
                /* Check if new value fits in existing space */
                if (value_len <= entry->value_size && entry->overflow_page == 0) {
                    /* In-place update */
                    entry->value_size = value_len;
                    memcpy(entry_ptr + sizeof(btree_entry_t) + entry->key_size, value, value_len);
                    return 0;
                } else {
                    /* Delete old entry and insert new one */
                    if (jdbx_btree_delete(tree, key, key_len) != 0) {
                        return -1;
                    }
                    /* Restart insertion after deletion */
                    current_page = tree->root_page;
                    continue;
                }
            }
            
            /* Check if value should go to overflow pages */
            uint64_t overflow_page = 0;
            size_t stored_value_len = value_len;
            
            if (value_len > OVERFLOW_THRESHOLD) {
                /* Write value to overflow pages */
                overflow_page = write_overflow_value(tree, value, value_len);
                if (overflow_page == 0) {
                    return -1; /* Failed to allocate overflow pages */
                }
                stored_value_len = 0; /* Don't store value inline */
                LOG_DEBUG("Stored large value (%zu bytes) in overflow pages starting at %llu",
                         value_len, (unsigned long long)overflow_page);
            }
            
            /* Calculate space needed */
            size_t entry_size = sizeof(btree_entry_t) + key_len + stored_value_len;
            
            if (node->total_size + entry_size > JDBX_PAGE_SIZE) {
                /* Would overflow page - need to split */
                if (split_node(tree, node->parent_page, current_page, 0) != 0) {
                    return -1;
                }
                
                /* Restart from root */
                current_page = tree->root_page;
                continue;
            }
            
            /* Insert new entry */
            uint8_t* insert_ptr = get_key_ptr(node, pos);
            size_t shift_size = 0;
            
            /* Calculate size of data to shift */
            for (int i = pos; i < node->num_keys; i++) {
                btree_entry_t* entry = (btree_entry_t*)get_key_ptr(node, i);
                shift_size += sizeof(btree_entry_t) + entry->key_size;
                if (entry->overflow_page == 0) {
                    shift_size += entry->value_size;
                }
            }
            
            /* Shift existing entries */
            if (shift_size > 0) {
                memmove(insert_ptr + entry_size, insert_ptr, shift_size);
            }
            
            /* Insert new entry */
            btree_entry_t* new_entry = (btree_entry_t*)insert_ptr;
            new_entry->key_size = key_len;
            new_entry->value_size = value_len;
            new_entry->overflow_page = overflow_page;
            
            memcpy(insert_ptr + sizeof(btree_entry_t), key, key_len);
            if (overflow_page == 0) {
                memcpy(insert_ptr + sizeof(btree_entry_t) + key_len, value, value_len);
            }
            
            /* Update node metadata */
            node->num_keys++;
            node->total_size += entry_size;
            node->header.checksum = jdbx_crc32(node, JDBX_PAGE_SIZE);
            
            /* Update statistics */
            tree->stats.num_keys++;
            
            LOG_DEBUG("Inserted key of size %zu into page %llu",
                     key_len, (unsigned long long)current_page);
            
            return 0;
        } else {
            /* Internal node - descend to appropriate child */
            uint64_t child_page = get_child_page(node, pos);
            current_page = child_page;
        }
    }
}

/* Get value for key from B-tree */
int jdbx_btree_get(jdbx_btree_t* tree,
                   const void* key, size_t key_len,
                   void** value, size_t* value_len) {
    if (!tree || !key || key_len == 0 || !value || !value_len) {
        return -1;
    }
    
    *value = NULL;
    *value_len = 0;
    
    /* Start from root */
    uint64_t current_page = tree->root_page;
    
    while (1) {
        btree_node_t* node = (btree_node_t*)jdbx_get_page(tree->pm, current_page);
        if (!node) return -1;
        
        /* Find position for key */
        int found;
        int pos = find_key_position(tree, node, key, key_len, &found);
        
        if (node->level == 0) {
            /* Leaf node */
            if (!found) {
                /* Key not found */
                return -1;
            }
            
            /* Get value */
            uint8_t* entry_ptr = get_key_ptr(node, pos);
            btree_entry_t* entry = (btree_entry_t*)entry_ptr;
            
            if (entry->overflow_page != 0) {
                /* Read value from overflow pages */
                *value = read_overflow_value(tree, entry->overflow_page, value_len);
                if (!*value) {
                    jdbx_error("Failed to read overflow value from page %llu",
                              (unsigned long long)entry->overflow_page);
                    return -1;
                }
                LOG_DEBUG("Read large value (%zu bytes) from overflow pages starting at %llu",
                         *value_len, (unsigned long long)entry->overflow_page);
            } else {
                /* Allocate and copy value */
                *value_len = entry->value_size;
                *value = malloc(*value_len);
                if (!*value) {
                    return -1;
                }
                
                memcpy(*value, entry_ptr + sizeof(btree_entry_t) + entry->key_size, 
                       *value_len);
            }
            
            return 0;
        } else {
            /* Internal node - descend to appropriate child */
            uint64_t child_page = get_child_page(node, 
                                               found ? pos + 1 : pos);
            current_page = child_page;
        }
    }
}

/* Delete key from B-tree */
int jdbx_btree_delete(jdbx_btree_t* tree,
                      const void* key, size_t key_len) {
    if (!tree || !key || key_len == 0) {
        jdbx_error("Invalid delete parameters");
        return -1;
    }
    
    /* Find the key */
    uint64_t current_page = tree->root_page;
    
    while (current_page != 0) {
        btree_node_t* node = (btree_node_t*)jdbx_get_page_for_write(tree->pm, current_page);
        if (!node) return -1;
        
        int found = 0;
        int pos = find_key_position(tree, node, key, key_len, &found);
        
        if (node->level == 0) {
            /* Leaf node */
            if (found) {
                uint8_t* entry_ptr = get_key_ptr(node, pos);
                btree_entry_t* entry = (btree_entry_t*)entry_ptr;
                
                /* Calculate entry size */
                size_t entry_size = sizeof(btree_entry_t) + entry->key_size;
                if (entry->overflow_page == 0) {
                    entry_size += entry->value_size;
                }
                
                /* Calculate size to shift */
                size_t shift_size = 0;
                for (int i = pos + 1; i < node->num_keys; i++) {
                    btree_entry_t* shift_entry = (btree_entry_t*)get_key_ptr(node, i);
                    shift_size += sizeof(btree_entry_t) + shift_entry->key_size;
                    if (shift_entry->overflow_page == 0) {
                        shift_size += shift_entry->value_size;
                    }
                }
                
                /* Shift entries left */
                if (shift_size > 0) {
                    memmove(entry_ptr, entry_ptr + entry_size, shift_size);
                }
                
                /* Update node metadata */
                node->num_keys--;
                node->total_size -= entry_size;
                tree->stats.num_keys--;
                
                return 0;
            }
            /* Key not found */
            return -1;
        } else {
            /* Internal node - traverse to child */
            if (pos < node->num_keys) {
                uint8_t* child_ptr = get_key_ptr(node, pos) + sizeof(btree_entry_t) + 
                                    ((btree_entry_t*)get_key_ptr(node, pos))->key_size;
                current_page = *(uint64_t*)child_ptr;
            } else {
                /* Use rightmost child */
                current_page = node->right_sibling;
            }
        }
    }
    
    return -1; /* Key not found */
}

/* Close B-tree */
void jdbx_btree_close(jdbx_btree_t* tree) {
    if (!tree) return;
    
    LOG_DEBUG("Closed B-tree with %llu keys in %llu pages",
             (unsigned long long)tree->stats.num_keys,
             (unsigned long long)tree->stats.num_pages);
    
    free(tree);
}

/* Find leftmost leaf page */
static uint64_t find_leftmost_leaf(jdbx_btree_t* tree) {
    uint64_t current_page = tree->root_page;
    
    while (current_page != 0) {
        btree_node_t* node = (btree_node_t*)jdbx_get_page(tree->pm, current_page);
        if (!node) return 0;
        
        if (node->level == 0) {
            /* Leaf node - this is our leftmost */
            return current_page;
        } else {
            /* Internal node - go to leftmost child */
            current_page = get_child_page(node, 0);
        }
    }
    
    return 0;
}

/* Create B-tree iterator */
jdbx_btree_iterator_t* jdbx_btree_iterator_create(jdbx_btree_t* tree) {
    if (!tree) return NULL;
    
    jdbx_btree_iterator_t* iter = calloc(1, sizeof(jdbx_btree_iterator_t));
    if (!iter) {
        jdbx_error("Failed to allocate iterator");
        return NULL;
    }
    
    iter->tree = tree;
    iter->current_page = find_leftmost_leaf(tree);
    iter->current_index = 0;
    iter->finished = (iter->current_page == 0);
    
    return iter;
}

/* Get next key-value pair from iterator */
int jdbx_btree_iterator_next(jdbx_btree_iterator_t* iter, 
                             void** key, size_t* key_len,
                             void** value, size_t* value_len) {
    if (!iter || iter->finished) return -1;
    
    while (iter->current_page != 0) {
        btree_node_t* node = (btree_node_t*)jdbx_get_page(iter->tree->pm, iter->current_page);
        if (!node) {
            iter->finished = 1;
            return -1;
        }
        
        if (iter->current_index < node->num_keys) {
            /* Get current entry */
            uint8_t* entry_ptr = get_key_ptr(node, iter->current_index);
            btree_entry_t* entry = (btree_entry_t*)entry_ptr;
            
            /* Extract key */
            *key_len = entry->key_size;
            *key = malloc(*key_len);
            if (!*key) return -1;
            memcpy(*key, entry_ptr + sizeof(btree_entry_t), *key_len);
            
            /* Extract value */
            if (entry->overflow_page == 0) {
                *value_len = entry->value_size;
                *value = malloc(*value_len);
                if (!*value) {
                    free(*key);
                    return -1;
                }
                memcpy(*value, entry_ptr + sizeof(btree_entry_t) + entry->key_size, *value_len);
            } else {
                /* TODO: Handle overflow pages */
                jdbx_error("Overflow pages not supported in iterator");
                free(*key);
                return -1;
            }
            
            /* Advance iterator */
            iter->current_index++;
            return 0;
        } else {
            /* Move to next page */
            iter->current_page = node->right_sibling;
            iter->current_index = 0;
            
            if (iter->current_page == 0) {
                iter->finished = 1;
                return -1;
            }
        }
    }
    
    iter->finished = 1;
    return -1;
}

/* Destroy iterator */
void jdbx_btree_iterator_destroy(jdbx_btree_iterator_t* iter) {
    if (iter) {
        free(iter);
    }
}