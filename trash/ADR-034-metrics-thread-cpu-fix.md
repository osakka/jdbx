# ADR-034: Metrics Thread CPU Usage Fix

**Status**: Accepted  
**Date**: June 19, 2025  
**Decision**: Change metrics persistence thread sleep interval from 100ms to 5 seconds

## Context

The JDBX server was experiencing 100% CPU usage during normal operation. Investigation revealed the metrics persistence thread was the culprit, consuming an entire CPU core while the server was idle.

## Problem Details

### Root Cause

The metrics persistence thread was using a busy-wait anti-pattern:

```c
while (1) {
    // Check if need to save (every 60 seconds)
    // Check if need to cleanup (every 3600 seconds)
    
    usleep(100000); /* 100ms */
}
```

This meant the thread was:
- Waking up 10 times per second
- Checking timestamps 600 times per minute
- Actually saving metrics only once per minute
- Causing 599 unnecessary wake-ups per minute

### CPU Impact

- **Before**: 100%+ CPU usage (entire core consumed)
- **After**: ~0% CPU usage during idle
- **Reduction**: 99.9% CPU usage eliminated

## Decision

Change the sleep interval from 100 milliseconds to 5 seconds:

```c
/* Sleep for a reasonable time - check every 5 seconds instead of 10 times per second */
sleep(5); /* 5 seconds - still responsive but not CPU intensive */
```

### Why 5 Seconds?

- **Metrics save interval**: 60 seconds (12 checks needed)
- **Cleanup interval**: 3600 seconds (720 checks needed)
- **5 second sleep**: Ensures timely execution with minimal overhead
- **Worst case delay**: 4.99 seconds (acceptable for background task)

## Consequences

### Positive
- ✅ CPU usage reduced to near zero during idle
- ✅ Server resources available for actual work
- ✅ No impact on metrics functionality
- ✅ Simple one-line fix
- ✅ Better battery life on laptops/mobile

### Negative
- ❌ Slightly less precise timing (±5 seconds)
- ❌ Metrics might be saved up to 5 seconds late

### Neutral
- Same metrics persistence behavior
- No API changes required
- No configuration changes needed

## Alternatives Considered

1. **Use condition variables**: More complex, same end result
2. **Use timerfd**: Platform-specific, overkill for this use case
3. **Increase to 1 second**: Still 60x more wake-ups than needed
4. **Use 60 second sleep**: Would miss cleanup interval alignment

## Lessons Learned

This is a classic busy-wait anti-pattern that's easy to introduce but costly in production:

1. **Always calculate wake-up frequency**: 100ms = 10 Hz = 36,000 wake-ups/hour
2. **Match sleep interval to work interval**: If work happens every 60s, don't check every 0.1s
3. **Background threads should be invisible**: They shouldn't consume resources when idle
4. **Simple fixes are often best**: One line change eliminated the problem

## Testing Results

### CPU Usage Monitoring
```bash
# Before fix
PID USER      PR  NI    VIRT    RES    SHR S  %CPU  %MEM     TIME+ COMMAND
2012955 user   20   0  516996  20284  13312 S 100.0   0.1   6:09.43 jdbxd

# After fix  
PID USER      PR  NI    VIRT    RES    SHR S  %CPU  %MEM     TIME+ COMMAND
2174604 user  20   0  254852   7424   6016 S   0.0   0.1   0:00.01 jdbxd
```

### Functional Testing
- Metrics still saved every 60 seconds ✅
- Cleanup still runs every hour ✅
- No missed persistence cycles ✅
- API endpoints working normally ✅

## References

- Classic busy-wait anti-pattern
- Similar to spinlock vs sleep in kernel programming
- Related to polling vs interrupt-driven design