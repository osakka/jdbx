# Discovery Tests

Automated tests designed to discover the next improvement opportunities and edge cases in the JDBX system.

## Test Scripts

### next_discovery_test.sh
Comprehensive test script that probes the system to identify:
- Performance bottlenecks under various loads
- Edge cases that might cause failures
- Next optimization opportunities
- Stress test scenarios for reliability assessment

**Usage:**
```bash
cd tests/discovery
./next_discovery_test.sh
```

**Purpose:**
This script runs after major improvements to systematically discover what should be optimized next, helping maintain continuous improvement momentum.

## Test Categories

1. **Performance Profiling**: Identifies current bottlenecks
2. **Edge Case Discovery**: Finds unusual scenarios that might fail
3. **Load Testing**: Determines current capacity limits
4. **Reliability Assessment**: Tests system stability under stress

These tests help maintain the continuous improvement cycle by automatically identifying the next most impactful areas for enhancement.