#!/bin/bash
#
# fix_buffer_overflows.sh
# Fixes buffer overflow warnings in the JSONdb codebase
#

echo "Starting to fix buffer overflow warnings..."

# JS File Utils - the main target file with buffer overflow warnings
JS_FILE_UTILS="src/utils/js_file_utils.c"

if [ -f "$JS_FILE_UTILS" ]; then
    echo "Processing $JS_FILE_UTILS..."
    
    # 1. Fix snprintf in cache_save function - line 247
    # Change direct snprintf to safer strlcpy + prepend
    sed -i 's/                snprintf(tmp, sizeof(tmp), "\/%s", dir_path);/                strlcpy(tmp, "\/", sizeof(tmp));\n                strlcat(tmp, dir_path, sizeof(tmp));/g' "$JS_FILE_UTILS"
    
    # 2. Fix snprintf in js_file_find function - line 377
    # Replace with path construction using strlcpy + strlcat
    sed -i 's/    snprintf(test_path, sizeof(test_path), "%s\/%s", cwd, filename);/    strlcpy(test_path, cwd, sizeof(test_path));\n    strlcat(test_path, "\/", sizeof(test_path));\n    strlcat(test_path, filename, sizeof(test_path));/g' "$JS_FILE_UTILS"
    
    # 3. Fix snprintf in js_file_find function - line 433 
    # Replace with path construction using strlcpy + strlcat
    sed -i 's/    snprintf(test_path, sizeof(test_path), "%s\/%s", cwd, filename_with_ext);/    strlcpy(test_path, cwd, sizeof(test_path));\n    strlcat(test_path, "\/", sizeof(test_path));\n    strlcat(test_path, filename_with_ext, sizeof(test_path));/g' "$JS_FILE_UTILS"
    
    # 4. Fix snprintf with 3 arguments in js_file_find - lines 455-456 or 460-461
    # Replace with path construction using strlcpy + strlcat
    sed -i '/        snprintf(test_path, sizeof(test_path), "%s\/%s\/%s",\n                cwd, common_js_dirs\[i\], filename);/c\        strlcpy(test_path, cwd, sizeof(test_path));\n        strlcat(test_path, "\/", sizeof(test_path));\n        strlcat(test_path, common_js_dirs[i], sizeof(test_path));\n        strlcat(test_path, "\/", sizeof(test_path));\n        strlcat(test_path, filename, sizeof(test_path));' "$JS_FILE_UTILS"

    # Handle case with different indentation or line numbers
    sed -i 's/        snprintf(test_path, sizeof(test_path), "%s\/%s\/%s",\n                cwd, common_js_dirs\[i\], filename);/        strlcpy(test_path, cwd, sizeof(test_path));\n        strlcat(test_path, "\/", sizeof(test_path));\n        strlcat(test_path, common_js_dirs[i], sizeof(test_path));\n        strlcat(test_path, "\/", sizeof(test_path));\n        strlcat(test_path, filename, sizeof(test_path));/g' "$JS_FILE_UTILS"

    # 5. Fix snprintf with 3 arguments in js_file_find - lines 477-478 or 482-483
    # Replace with path construction using strlcpy + strlcat
    sed -i '/        snprintf(test_path, sizeof(test_path), "%s\/%s\/%s",\n                cwd, common_js_dirs\[i\], filename_with_ext);/c\        strlcpy(test_path, cwd, sizeof(test_path));\n        strlcat(test_path, "\/", sizeof(test_path));\n        strlcat(test_path, common_js_dirs[i], sizeof(test_path));\n        strlcat(test_path, "\/", sizeof(test_path));\n        strlcat(test_path, filename_with_ext, sizeof(test_path));' "$JS_FILE_UTILS"

    # Handle case with different indentation or line numbers
    sed -i 's/        snprintf(test_path, sizeof(test_path), "%s\/%s\/%s",\n                cwd, common_js_dirs\[i\], filename_with_ext);/        strlcpy(test_path, cwd, sizeof(test_path));\n        strlcat(test_path, "\/", sizeof(test_path));\n        strlcat(test_path, common_js_dirs[i], sizeof(test_path));\n        strlcat(test_path, "\/", sizeof(test_path));\n        strlcat(test_path, filename_with_ext, sizeof(test_path));/g' "$JS_FILE_UTILS"
    
    # 6. Replace all strncpy with strlcpy
    # This addresses the warnings about strncpy output being truncated
    sed -i 's/strncpy(/strlcpy(/g' "$JS_FILE_UTILS"
    
    # 7. Add strl* function implementations if not available
    # Check if strlcpy/strlcat implementation is already present in the file
    if ! grep -q "strlcpy" "$JS_FILE_UTILS"; then
        # Add implementations at the top of the file after the include statements
        echo "Adding strlcpy/strlcat implementations to the file..."
        STRL_IMPL='
/* strlcpy and strlcat implementations if not available in libc */
#if !defined(HAVE_STRLCPY) && !defined(__FreeBSD__) && !defined(__OpenBSD__) && !defined(__NetBSD__) && !defined(__APPLE__)
/**
 * strlcpy - Copy a %NUL terminated string into a sized buffer
 * @dest: Where to copy the string to
 * @src: Where to copy the string from
 * @size: Size of destination buffer
 *
 * Compatible with *BSD: the result is always a valid
 * NUL-terminated string that fits in the buffer (unless,
 * of course, the buffer size is zero). It does not pad
 * out the result like strncpy() does.
 *
 * Returns the length of the source string (regardless of
 * whether it fits in the destination buffer or not).
 */
static size_t strlcpy(char *dest, const char *src, size_t size)
{
    size_t ret = strlen(src);

    if (size) {
        size_t len = (ret >= size) ? size - 1 : ret;
        memcpy(dest, src, len);
        dest[len] = '\0';
    }

    return ret;
}

/**
 * strlcat - Append a %NUL terminated string into a sized buffer
 * @dest: Where to append the string to
 * @src: Where to copy the string from
 * @size: Size of destination buffer
 *
 * Compatible with *BSD: the result is always a valid
 * NUL-terminated string that fits in the buffer (unless,
 * of course, the buffer size is zero). It does not pad
 * out the result like strncpy() does.
 *
 * Returns the length of the combined string (regardless of
 * whether it fits in the destination buffer or not).
 */
static size_t strlcat(char *dest, const char *src, size_t size)
{
    size_t dest_len = strnlen(dest, size);
    size_t src_len = strlen(src);

    if (dest_len < size) {
        size_t copy_len = size - dest_len - 1;
        if (src_len < copy_len)
            copy_len = src_len;
        memcpy(dest + dest_len, src, copy_len);
        dest[dest_len + copy_len] = '\0';
    }

    return dest_len + src_len;
}
#endif
'
        # Insert the strlcpy and strlcat implementations after the include statements
        sed -i "/^#include/a\\$STRL_IMPL" "$JS_FILE_UTILS"
    fi
    
    echo "Fixed buffer overflow warnings in $JS_FILE_UTILS"
else
    echo "Error: $JS_FILE_UTILS not found!"
fi

echo "Buffer overflow fix script completed."