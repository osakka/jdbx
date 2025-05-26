// RBAC View Protection Script
// This script prevents the RBAC view from being overwritten with plain text

(function() {
    // Store the original RBAC view HTML
    let originalRBACHTML = null;
    
    // Override innerHTML setter to catch who's setting "admin"
    const originalInnerHTML = Object.getOwnPropertyDescriptor(Element.prototype, 'innerHTML');
    Object.defineProperty(Element.prototype, 'innerHTML', {
        set: function(value) {
            // Log ALL changes to RBAC-related elements
            if (this.id && (this.id.includes('rbac') || this.id === 'roles' || this.id === 'rolesList')) {
                console.log(`[RBAC Protection] Setting innerHTML on ${this.id}, value length: ${value?.length}, first 100 chars:`, value?.substring(0, 100));
                console.trace('Stack trace for RBAC innerHTML change:');
            }
            
            // Check if someone is trying to set just "admin"
            if (typeof value === 'string' && value.trim() === 'admin') {
                console.error('CAUGHT: Attempt to set innerHTML to "admin"!');
                console.error('Element:', this);
                console.error('Element ID:', this.id);
                console.error('Element classes:', this.className);
                console.trace('Stack trace:');
                
                // Block it for RBAC-related elements
                if (this.id === 'rbac-view' || this.classList.contains('view-container') || this.closest('#rbac-view')) {
                    console.error('BLOCKING: Prevented setting RBAC element to "admin"');
                    return;
                }
            }
            originalInnerHTML.set.call(this, value);
        },
        get: originalInnerHTML.get
    });
    
    // Monitor RBAC view for changes
    function protectRBACView() {
        const rbacView = document.getElementById('rbac-view');
        if (!rbacView) return;
        
        // Store original HTML if not already stored
        if (!originalRBACHTML && rbacView.innerHTML.length > 100) {
            originalRBACHTML = rbacView.innerHTML;
            console.log('RBAC Protection: Original HTML stored (length:', originalRBACHTML.length, ')');
        }
        
        // Create observer to detect changes
        const observer = new MutationObserver(function(mutations) {
            mutations.forEach(function(mutation) {
                // Check if RBAC view was replaced with plain text
                if (rbacView.textContent.trim() === 'admin' || 
                    (rbacView.children.length === 0 && rbacView.textContent.trim().length < 50)) {
                    
                    console.error('RBAC Protection: View corrupted! Text content:', rbacView.textContent);
                    
                    // Restore original HTML if available
                    if (originalRBACHTML) {
                        console.log('RBAC Protection: Restoring original HTML');
                        rbacView.innerHTML = originalRBACHTML;
                        
                        // Re-initialize RBAC if the function exists
                        if (typeof initializeRBAC === 'function') {
                            setTimeout(initializeRBAC, 100);
                        }
                    } else {
                        // Force page reload if we can't restore
                        console.error('RBAC Protection: Cannot restore, reloading page');
                        window.location.reload();
                    }
                }
            });
        });
        
        // Start observing
        observer.observe(rbacView, {
            childList: true,
            characterData: true,
            subtree: true
        });
        
        console.log('RBAC Protection: Monitoring enabled');
    }
    
    // Initialize protection when DOM is ready
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', protectRBACView);
    } else {
        protectRBACView();
    }
})();