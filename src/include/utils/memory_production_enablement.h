#ifndef JDBX_MEMORY_PRODUCTION_ENABLEMENT_H
#define JDBX_MEMORY_PRODUCTION_ENABLEMENT_H

#include <stdint.h>

/* Production enablement states */
typedef enum {
    PRODUCTION_STATE_DISABLED = 0,
    PRODUCTION_STATE_VALIDATING = 1,
    PRODUCTION_STATE_ENABLING = 2,
    PRODUCTION_STATE_ENABLED = 3,
    PRODUCTION_STATE_EMERGENCY = 4
} production_state_t;

/* Atomic enablement operations */
int memory_production_start_validation(void);
int memory_production_validate_and_enable(void);
int memory_production_emergency_disable(void);
int memory_production_reset(void);

/* Status and monitoring */
production_state_t memory_production_get_state(void);
void memory_production_get_status(void);
int memory_production_health_check(void);

/* Configuration */
void memory_production_configure(uint32_t validation_duration, uint32_t test_count, int auto_rollback);

#endif /* JDBX_MEMORY_PRODUCTION_ENABLEMENT_H */