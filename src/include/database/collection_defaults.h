#ifndef COLLECTION_DEFAULTS_H
#define COLLECTION_DEFAULTS_H

struct database;

/* Initialize default metadata for system collections */
void collection_defaults_init(struct database* db);

#endif /* COLLECTION_DEFAULTS_H */