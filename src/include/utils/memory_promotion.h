#ifndef JDBX_MEMORY_PROMOTION_H
#define JDBX_MEMORY_PROMOTION_H

#include <stddef.h>
#include <stdint.h>

/* Forward declarations */
typedef struct memory_checkpoint memory_checkpoint_t;

/* Promotion statistics structure */
typedef struct {
    uint64_t arena_to_tlsf_promotions;
    uint64_t arena_to_system_promotions;
    uint64_t tlsf_to_system_promotions;
    uint64_t promotion_failures;
    uint64_t bytes_promoted;
} memory_promotion_stats_t;

/* Lifetime hints for automatic promotion */
#define MEMORY_LIFETIME_CHECKPOINT  0  /* Dies with checkpoint */
#define MEMORY_LIFETIME_SHORT_TERM  1  /* Lives beyond checkpoint but not long */
#define MEMORY_LIFETIME_LONG_TERM   2  /* Very long-lived, consider system malloc */

/* Explicit promotion functions */
void* memory_promote_arena_to_tlsf(void* arena_ptr, size_t size);
void* memory_promote_tlsf_to_system(void* tlsf_ptr, size_t size);

/* Automatic promotion based on lifetime hints */
void* memory_auto_promote(void* ptr, size_t size, int lifetime_hint);

/* Batch promotion for checkpoint operations */
int memory_promote_checkpoint_survivors(memory_checkpoint_t* checkpoint);

/* Statistics and monitoring */
void memory_get_promotion_stats(memory_promotion_stats_t* stats);
void memory_reset_promotion_stats(void);
void memory_log_promotion_stats(void);

#endif /* JDBX_MEMORY_PROMOTION_H */