/*
 * JDBX B-tree Implementation - Key-value storage engine
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "storage/jdbx.h"
#include "utils/logger.h"

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
        free(tree);
        return NULL;
    }
    
    /* Initialize root node */
    btree_node_t* root = (btree_node_t*)jdbx_get_page_for_write(pm, tree->root_page);
    if (!root) {
        jdbx_free_page(pm, tree->root_page);
        free(tree);
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
        free(tree);
        return NULL;
    }
    
    if (root->header.type != PAGE_TYPE_BTREE_LEAF && 
        root->header.type != PAGE_TYPE_BTREE_INTERNAL) {
        jdbx_error("Invalid root page type: %d", root->header.type);
        free(tree);
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
            free(mid_key);
            jdbx_free_page(tree->pm, sibling_page);
            return -1;
        }
        
        btree_node_t* new_root = (btree_node_t*)jdbx_get_page_for_write(tree->pm, 
                                                                        new_root_page);
        if (!new_root) {
            free(mid_key);
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
        /* TODO: Implement insertion into parent node */
        /* This requires shifting keys and updating child pointers */
        /* For now, return error */
        free(mid_key);
        return -1;
    }
    
    /* Update checksums */
    full_node->header.checksum = jdbx_crc32(full_node, JDBX_PAGE_SIZE);
    sibling->header.checksum = jdbx_crc32(sibling, JDBX_PAGE_SIZE);
    
    free(mid_key);
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
                (void)entry_ptr; /* Unused for now */
                /* btree_entry_t* entry = (btree_entry_t*)entry_ptr; - for future use */
                
                /* For simplicity, we don't support in-place update yet */
                /* Would need to handle size changes */
                jdbx_error("Key already exists (updates not implemented)");
                return -1;
            }
            
            /* Calculate space needed */
            size_t entry_size = sizeof(btree_entry_t) + key_len + value_len;
            
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
            new_entry->overflow_page = 0; /* TODO: Handle large values */
            
            memcpy(insert_ptr + sizeof(btree_entry_t), key, key_len);
            memcpy(insert_ptr + sizeof(btree_entry_t) + key_len, value, value_len);
            
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
                /* TODO: Handle overflow pages */
                jdbx_error("Overflow pages not implemented");
                return -1;
            }
            
            /* Allocate and copy value */
            *value_len = entry->value_size;
            *value = malloc(*value_len);
            if (!*value) {
                return -1;
            }
            
            memcpy(*value, entry_ptr + sizeof(btree_entry_t) + entry->key_size, 
                   *value_len);
            
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
    /* TODO: Implement B-tree deletion */
    /* This is complex and requires handling:
     * - Leaf deletion
     * - Borrowing from siblings
     * - Merging nodes
     * - Updating parent keys
     */
    (void)tree;
    (void)key;
    (void)key_len;
    
    jdbx_error("B-tree deletion not implemented");
    return -1;
}

/* Close B-tree */
void jdbx_btree_close(jdbx_btree_t* tree) {
    if (!tree) return;
    
    LOG_DEBUG("Closed B-tree with %llu keys in %llu pages",
             (unsigned long long)tree->stats.num_keys,
             (unsigned long long)tree->stats.num_pages);
    
    free(tree);
}