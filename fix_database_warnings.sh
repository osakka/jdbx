#!/bin/bash

# Fix unused parameters in database.c

# Add (void) casts for unused parameters
sed -i '327a\    (void)db; // Currently unused' src/components/database/database.c
sed -i '562a\    (void)db; // Currently unused' src/components/database/database.c
sed -i '1083a\    (void)db; // Currently unused' src/components/database/database.c
sed -i '1103a\    (void)db; // Currently unused' src/components/database/database.c
sed -i '1152a\    (void)db; // Currently unused' src/components/database/database.c
sed -i '1167a\    (void)db; // Currently unused' src/components/database/database.c
sed -i '1199a\    (void)db; (void)capacity; (void)ttl; // Currently unused' src/components/database/database.c
sed -i '1204a\    (void)db; // Currently unused' src/components/database/database.c
sed -i '1209a\    (void)db; // Currently unused' src/components/database/database.c
sed -i '1223a\    (void)db; // Currently unused' src/components/database/database.c
sed -i '1255a\    (void)db; // Currently unused' src/components/database/database.c
sed -i '1458a\    (void)db; // Currently unused' src/components/database/database.c

echo "Database.c warnings should be fixed."