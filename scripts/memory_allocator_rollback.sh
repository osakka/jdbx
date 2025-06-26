#!/bin/bash

# Memory Allocator Emergency Rollback Script
# This script provides instant rollback capability for exotic memory allocators
# in production environments. Can be used for emergency situations.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Default paths
ENV_FILE="${PROJECT_ROOT}/var/jdbx.env"
BACKUP_ENV_FILE="${PROJECT_ROOT}/var/jdbx.env.backup.$(date +%Y%m%d_%H%M%S)"
PID_FILE="${PROJECT_ROOT}/build/var/jdbxd.pid"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Print usage information
print_usage() {
    cat << EOF
Memory Allocator Emergency Rollback Script

Usage: $0 [COMMAND] [OPTIONS]

Commands:
    disable         Emergency disable exotic allocators (force system malloc)
    enable          Enable exotic allocators with configuration
    status          Show current allocator configuration status
    rollback        Complete rollback to system malloc with service restart
    validate        Validate current configuration
    help            Show this help message

Options:
    --env-file FILE     Environment file path (default: $ENV_FILE)
    --no-restart        Don't restart service (config changes require restart)
    --arena             Enable Arena allocator (with enable command)
    --tlsf              Enable TLSF allocator (with enable command)
    --debug             Enable debug output
    --force             Skip confirmation prompts

Examples:
    $0 disable                    # Emergency disable all exotic allocators
    $0 enable --arena --tlsf      # Enable both allocators
    $0 rollback --force           # Complete rollback without prompts
    $0 status                     # Show current status

Environment Variables:
    JDBX_ENABLE_EXOTIC_ALLOCATORS   Master enable/disable (true/false)
    JDBX_ENABLE_ARENA_ALLOCATOR     Arena allocator control (true/false)
    JDBX_ENABLE_TLSF_ALLOCATOR      TLSF allocator control (true/false)
    JDBX_FORCE_SYSTEM_MALLOC        Emergency fallback (true/false)
    JDBX_MEM_DEBUG                  Debug output (true/false)

EOF
}

# Check if service is running
is_service_running() {
    if [[ -f "$PID_FILE" ]]; then
        local pid=$(cat "$PID_FILE" 2>/dev/null || echo "")
        if [[ -n "$pid" ]] && kill -0 "$pid" 2>/dev/null; then
            return 0
        fi
    fi
    return 1
}

# Get service PID
get_service_pid() {
    if [[ -f "$PID_FILE" ]]; then
        cat "$PID_FILE" 2>/dev/null || echo ""
    fi
}

# Update environment variable in file
update_env_var() {
    local var_name="$1"
    local var_value="$2"
    local env_file="$3"
    
    if grep -q "^${var_name}=" "$env_file" 2>/dev/null; then
        # Variable exists, update it
        sed -i "s/^${var_name}=.*/${var_name}=${var_value}/" "$env_file"
    elif grep -q "^# ${var_name}=" "$env_file" 2>/dev/null; then
        # Variable exists but commented, uncomment and update
        sed -i "s/^# ${var_name}=.*/${var_name}=${var_value}/" "$env_file"
    else
        # Variable doesn't exist, add it
        echo "${var_name}=${var_value}" >> "$env_file"
    fi
}

# Backup environment file
backup_env_file() {
    if [[ -f "$ENV_FILE" ]]; then
        cp "$ENV_FILE" "$BACKUP_ENV_FILE"
        log_info "Environment file backed up to: $BACKUP_ENV_FILE"
    fi
}

# Show current configuration status
show_status() {
    log_info "Current Memory Allocator Configuration:"
    echo
    
    # Check environment variables
    local exotic_enabled=$(grep "^JDBX_ENABLE_EXOTIC_ALLOCATORS=" "$ENV_FILE" 2>/dev/null | cut -d'=' -f2 || echo "false")
    local arena_enabled=$(grep "^JDBX_ENABLE_ARENA_ALLOCATOR=" "$ENV_FILE" 2>/dev/null | cut -d'=' -f2 || echo "true")
    local tlsf_enabled=$(grep "^JDBX_ENABLE_TLSF_ALLOCATOR=" "$ENV_FILE" 2>/dev/null | cut -d'=' -f2 || echo "true")
    local force_system=$(grep "^JDBX_FORCE_SYSTEM_MALLOC=" "$ENV_FILE" 2>/dev/null | cut -d'=' -f2 || echo "false")
    local debug_enabled=$(grep "^JDBX_MEM_DEBUG=" "$ENV_FILE" 2>/dev/null | cut -d'=' -f2 || echo "false")
    
    echo "  Master exotic allocators: $exotic_enabled"
    echo "  Arena allocator: $arena_enabled"
    echo "  TLSF allocator: $tlsf_enabled"
    echo "  Force system malloc: $force_system"
    echo "  Debug mode: $debug_enabled"
    echo
    
    # Determine effective configuration
    if [[ "$force_system" == "true" ]]; then
        log_warn "EMERGENCY MODE: System malloc forced"
    elif [[ "$exotic_enabled" == "true" ]]; then
        log_info "Exotic allocators enabled"
        if [[ "$arena_enabled" == "true" ]]; then
            echo "  ✓ Arena allocator active"
        else
            echo "  ✗ Arena allocator disabled"
        fi
        if [[ "$tlsf_enabled" == "true" ]]; then
            echo "  ✓ TLSF allocator active"
        else
            echo "  ✗ TLSF allocator disabled"
        fi
    else
        log_info "Using system malloc (exotic allocators disabled)"
    fi
    echo
    
    # Service status
    if is_service_running; then
        local pid=$(get_service_pid)
        log_info "JDBX service is running (PID: $pid)"
    else
        log_warn "JDBX service is not running"
    fi
}

# Emergency disable all exotic allocators
emergency_disable() {
    local force_flag="$1"
    
    log_warn "EMERGENCY DISABLE: Forcing system malloc for all allocations"
    
    if [[ "$force_flag" != "--force" ]]; then
        echo
        echo "This will immediately disable all exotic memory allocators and force"
        echo "the system to use standard malloc. This action is reversible."
        echo
        read -p "Continue? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            log_info "Operation cancelled"
            exit 0
        fi
    fi
    
    # Backup current configuration
    backup_env_file
    
    # Set emergency configuration
    update_env_var "JDBX_FORCE_SYSTEM_MALLOC" "true" "$ENV_FILE"
    update_env_var "JDBX_ENABLE_EXOTIC_ALLOCATORS" "false" "$ENV_FILE"
    
    log_info "Emergency configuration applied"
    log_warn "Service restart required for changes to take effect"
    
    # Restart service if running
    if is_service_running && [[ "${NO_RESTART:-}" != "true" ]]; then
        log_info "Restarting JDBX service..."
        restart_service
    fi
    
    log_info "Emergency disable completed"
}

# Enable exotic allocators
enable_allocators() {
    local enable_arena="$1"
    local enable_tlsf="$2"
    local force_flag="$3"
    
    log_info "Enabling exotic memory allocators"
    log_info "  Arena allocator: $enable_arena"
    log_info "  TLSF allocator: $enable_tlsf"
    
    if [[ "$force_flag" != "--force" ]]; then
        echo
        echo "This will enable exotic memory allocators for improved performance."
        echo "Make sure you have validated the allocators in your environment."
        echo
        read -p "Continue? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            log_info "Operation cancelled"
            exit 0
        fi
    fi
    
    # Backup current configuration
    backup_env_file
    
    # Set configuration
    update_env_var "JDBX_FORCE_SYSTEM_MALLOC" "false" "$ENV_FILE"
    update_env_var "JDBX_ENABLE_EXOTIC_ALLOCATORS" "true" "$ENV_FILE"
    update_env_var "JDBX_ENABLE_ARENA_ALLOCATOR" "$enable_arena" "$ENV_FILE"
    update_env_var "JDBX_ENABLE_TLSF_ALLOCATOR" "$enable_tlsf" "$ENV_FILE"
    
    log_info "Allocator configuration applied"
    log_warn "Service restart required for changes to take effect"
    
    # Restart service if running
    if is_service_running && [[ "${NO_RESTART:-}" != "true" ]]; then
        log_info "Restarting JDBX service..."
        restart_service
    fi
    
    log_info "Allocator enablement completed"
}

# Complete rollback to system malloc
complete_rollback() {
    local force_flag="$1"
    
    log_warn "COMPLETE ROLLBACK: Reverting to system malloc and restarting service"
    
    if [[ "$force_flag" != "--force" ]]; then
        echo
        echo "This will:"
        echo "  1. Disable all exotic memory allocators"
        echo "  2. Force system malloc usage"
        echo "  3. Restart the JDBX service"
        echo "  4. Validate the rollback was successful"
        echo
        read -p "Continue with complete rollback? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            log_info "Operation cancelled"
            exit 0
        fi
    fi
    
    # Backup current configuration
    backup_env_file
    
    # Emergency disable
    update_env_var "JDBX_FORCE_SYSTEM_MALLOC" "true" "$ENV_FILE"
    update_env_var "JDBX_ENABLE_EXOTIC_ALLOCATORS" "false" "$ENV_FILE"
    update_env_var "JDBX_ENABLE_ARENA_ALLOCATOR" "false" "$ENV_FILE"
    update_env_var "JDBX_ENABLE_TLSF_ALLOCATOR" "false" "$ENV_FILE"
    
    log_info "Rollback configuration applied"
    
    # Restart service
    if is_service_running; then
        log_info "Restarting JDBX service for rollback..."
        restart_service
        
        # Wait for service to stabilize
        sleep 3
        
        if is_service_running; then
            log_info "Service restarted successfully"
        else
            log_error "Service failed to restart after rollback"
            exit 1
        fi
    else
        log_warn "Service not running - start manually to apply rollback"
    fi
    
    log_info "Complete rollback successful"
}

# Restart service
restart_service() {
    if is_service_running; then
        local pid=$(get_service_pid)
        log_info "Stopping JDBX service (PID: $pid)..."
        kill "$pid"
        
        # Wait for shutdown
        local count=0
        while is_service_running && [[ $count -lt 30 ]]; do
            sleep 1
            ((count++))
        done
        
        if is_service_running; then
            log_error "Service did not stop gracefully, forcing termination"
            kill -9 "$pid" || true
            sleep 2
        fi
    fi
    
    # Start service
    log_info "Starting JDBX service..."
    cd "$PROJECT_ROOT"
    ./build/jdbx_runtime.sh start
    
    # Wait for startup
    sleep 2
    
    if is_service_running; then
        local new_pid=$(get_service_pid)
        log_info "Service started successfully (PID: $new_pid)"
    else
        log_error "Failed to start service"
        exit 1
    fi
}

# Validate configuration
validate_config() {
    log_info "Validating memory allocator configuration..."
    
    # Check environment file exists
    if [[ ! -f "$ENV_FILE" ]]; then
        log_error "Environment file not found: $ENV_FILE"
        exit 1
    fi
    
    # Check for conflicting settings
    local force_system=$(grep "^JDBX_FORCE_SYSTEM_MALLOC=" "$ENV_FILE" 2>/dev/null | cut -d'=' -f2 || echo "false")
    local exotic_enabled=$(grep "^JDBX_ENABLE_EXOTIC_ALLOCATORS=" "$ENV_FILE" 2>/dev/null | cut -d'=' -f2 || echo "false")
    
    if [[ "$force_system" == "true" && "$exotic_enabled" == "true" ]]; then
        log_warn "Conflicting configuration detected:"
        log_warn "  JDBX_FORCE_SYSTEM_MALLOC=true"
        log_warn "  JDBX_ENABLE_EXOTIC_ALLOCATORS=true"
        log_warn "System malloc will be used (emergency mode takes precedence)"
    fi
    
    # Check if service is running with current config
    if is_service_running; then
        log_info "Service is running - configuration will be active"
    else
        log_warn "Service is not running - start service to apply configuration"
    fi
    
    log_info "Configuration validation completed"
}

# Main script logic
main() {
    local command="${1:-help}"
    shift || true
    
    # Parse global options
    local force_flag=""
    local enable_arena="false"
    local enable_tlsf="false"
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            --env-file)
                ENV_FILE="$2"
                shift 2
                ;;
            --no-restart)
                NO_RESTART="true"
                shift
                ;;
            --arena)
                enable_arena="true"
                shift
                ;;
            --tlsf)
                enable_tlsf="true"
                shift
                ;;
            --debug)
                set -x
                shift
                ;;
            --force)
                force_flag="--force"
                shift
                ;;
            *)
                log_error "Unknown option: $1"
                print_usage
                exit 1
                ;;
        esac
    done
    
    # Ensure environment file directory exists
    mkdir -p "$(dirname "$ENV_FILE")"
    
    case "$command" in
        disable)
            emergency_disable "$force_flag"
            ;;
        enable)
            enable_allocators "$enable_arena" "$enable_tlsf" "$force_flag"
            ;;
        status)
            show_status
            ;;
        rollback)
            complete_rollback "$force_flag"
            ;;
        validate)
            validate_config
            ;;
        help|--help|-h)
            print_usage
            ;;
        *)
            log_error "Unknown command: $command"
            print_usage
            exit 1
            ;;
    esac
}

# Run main function with all arguments
main "$@"