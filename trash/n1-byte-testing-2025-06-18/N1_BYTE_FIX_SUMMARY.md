# N-1 Byte Fix Summary

## Problem
The JDBX server was reading 1 byte less than the Content-Length header specified, causing incomplete request errors (especially noticeable with OpenSSL 3.x clients like curl 8.x and Python requests).

## Root Cause
The server was treating HTTP content as a C string that needs null termination, reserving 1 byte for the null terminator during reading:
- `buffer_size - 1` was used in multiple places when reading data
- This caused the server to read N-1 bytes instead of the full N bytes specified in Content-Length

## Locations Fixed
1. **`client_read_data()` function (lines 53, 109)**:
   - SSL path: `ssl_read(ssl_conn, buffer, buffer_size - 1, &bytes_read)` → `ssl_read(ssl_conn, buffer, buffer_size, &bytes_read)`
   - Non-SSL path: `read(client->client_fd, buffer, buffer_size - 1)` → `read(client->client_fd, buffer, buffer_size)`

2. **Initial header reading loop (line 427)**:
   - `size_t read_size = buffer_size - total_bytes_read - 1` → `size_t read_size = buffer_size - total_bytes_read`

3. **Body reading calculations (line 652)**:
   - `size_t bytes_to_read = buffer_size - total_bytes_read - 1` → `size_t bytes_to_read = buffer_size - total_bytes_read`

4. **Loop conditions (lines 409, 1262, 1330)**:
   - `while (total_bytes_read < (int)(buffer_size - 1))` → `while (total_bytes_read < (int)buffer_size)`

5. **Keep-alive request reading (line 1263, 1332)**:
   - `client_read_data(..., buffer_size - total_bytes_read - 1)` → `client_read_data(..., buffer_size - total_bytes_read)`

## Solution
- HTTP content is binary data and should be read in full according to Content-Length
- Buffers are allocated with size Content-Length + 1 to accommodate null terminator AFTER the data
- The full Content-Length is read without reserving space during the read operation
- Null terminator is added AFTER reading all data for string processing

## Testing Results
✅ All document sizes from 100 bytes to 10,000 bytes now work correctly
✅ Compatible with curl 8.x and Python requests (OpenSSL 3.x clients)
✅ No regression - server still handles all other requests properly

## Code Changes
All changes were made in `/opt/jdbx/src/components/core/handle_client.c` to:
1. Remove the "- 1" from all buffer size calculations during reading
2. Ensure buffers are allocated with +1 for null terminator
3. Add null terminator after reading, not during reading