// RBAC Debug Trap - Find where "admin" text is being set
(function() {
    console.log('RBAC Debug Trap loaded');
    
    // Intercept all property setters
    const props = ['innerHTML', 'innerText', 'textContent'];
    const intercepted = new WeakSet();
    
    props.forEach(prop => {
        const original = Object.getOwnPropertyDescriptor(Element.prototype, prop) || 
                        Object.getOwnPropertyDescriptor(HTMLElement.prototype, prop);
        
        if (original && original.set) {
            Object.defineProperty(Element.prototype, prop, {
                set: function(value) {
                    // Check if setting to "admin"
                    if (typeof value === 'string' && value.trim() === 'admin') {
                        console.error(`TRAP: ${prop} being set to "admin"!`);
                        console.error('Element:', this);
                        console.error('Element ID:', this.id);
                        console.error('Element classes:', this.className);
                        console.error('Parent element:', this.parentElement);
                        console.trace('Stack trace:');
                        
                        // Special check for RBAC view
                        if (this.id === 'rbac-view' || this.closest('#rbac-view')) {
                            console.error('THIS IS THE RBAC VIEW BEING SET TO ADMIN!');
                            debugger; // This will pause in debugger if dev tools are open
                        }
                    }
                    
                    // Log all changes to RBAC view
                    if (this.id === 'rbac-view' || (this.id && this.id.includes('rbac'))) {
                        console.log(`RBAC element ${this.id} ${prop} being set to:`, value?.substring(0, 100));
                    }
                    
                    return original.set.call(this, value);
                },
                get: original.get,
                configurable: true
            });
        }
    });
    
    // Also monitor document.write
    const originalWrite = document.write;
    document.write = function(content) {
        if (content && content.includes('admin')) {
            console.error('TRAP: document.write with "admin":', content);
            console.trace();
        }
        return originalWrite.call(this, content);
    };
})();