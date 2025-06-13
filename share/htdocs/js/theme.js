// Theme management
(function() {
    // Define theme variables
    const themes = {
        light: {
            '--bg-primary': '#f8f9fa',
            '--bg-secondary': '#ffffff',
            '--text-primary': '#333333',
            '--text-secondary': '#6c757d',
            '--border-color': '#dee2e6',
            '--shadow': '0 1px 3px rgba(0,0,0,0.1)',
            '--shadow-hover': '0 4px 6px rgba(0,0,0,0.1)',
            '--navbar-bg': '#ffffff',
            '--card-bg': '#ffffff',
            '--accent': '#667eea',
            '--accent-hover': '#5a67d8',
            '--success': '#28a745',
            '--danger': '#dc3545',
            '--warning': '#ffc107',
            '--info': '#17a2b8',
            '--code-bg': '#f8f9fa',
            '--code-border': '#e9ecef'
        },
        dark: {
            '--bg-primary': '#1a1a1a',
            '--bg-secondary': '#2d2d2d',
            '--text-primary': '#e9ecef',
            '--text-secondary': '#adb5bd',
            '--border-color': '#495057',
            '--shadow': '0 1px 3px rgba(0,0,0,0.3)',
            '--shadow-hover': '0 4px 6px rgba(0,0,0,0.4)',
            '--navbar-bg': '#2d2d2d',
            '--card-bg': '#2d2d2d',
            '--accent': '#667eea',
            '--accent-hover': '#5a67d8',
            '--success': '#28a745',
            '--danger': '#dc3545',
            '--warning': '#ffc107',
            '--info': '#17a2b8',
            '--code-bg': '#1e1e1e',
            '--code-border': '#3a3a3a'
        }
    };

    // Get stored theme or default to light
    function getStoredTheme() {
        return localStorage.getItem('jdbx_theme') || 'light';
    }

    // Set theme
    function setTheme(theme) {
        const root = document.documentElement;
        const themeVars = themes[theme] || themes.light;
        
        // Apply CSS variables
        Object.entries(themeVars).forEach(([key, value]) => {
            root.style.setProperty(key, value);
        });
        
        // Set data attribute
        root.setAttribute('data-theme', theme);
        
        // Store preference
        localStorage.setItem('jdbx_theme', theme);
        
        // Update toggle button if it exists
        updateToggleButton(theme);
        
        // Update Chart.js theme if charts exist
        updateChartTheme(theme);
        
        // Update logo based on theme
        updateLogo(theme);
        
        // Update Prism theme if present
        updatePrismTheme(theme);
    }

    // Update logo based on theme
    function updateLogo(theme) {
        // Update all logos with data-theme-logo attribute
        const logos = document.querySelectorAll('img[data-theme-logo]');
        logos.forEach(logo => {
            if (theme === 'dark') {
                logo.src = '/resources/jdbx_logo_white.svg';
            } else {
                logo.src = '/resources/jdbx_logo_dark.svg';
            }
        });
    }

    // Update toggle button icon
    function updateToggleButton(theme) {
        const btn = document.getElementById('themeToggle');
        if (btn) {
            const icon = btn.querySelector('i');
            if (icon) {
                if (theme === 'dark') {
                    icon.className = 'bi bi-sun-fill';
                    btn.title = 'Switch to light mode';
                } else {
                    icon.className = 'bi bi-moon-fill';
                    btn.title = 'Switch to dark mode';
                }
            }
        }
    }

    // Update Chart.js colors for dark mode
    function updateChartTheme(theme) {
        if (typeof Chart !== 'undefined') {
            const textColor = theme === 'dark' ? '#e9ecef' : '#333333';
            const gridColor = theme === 'dark' ? '#495057' : '#dee2e6';
            
            Chart.defaults.color = textColor;
            Chart.defaults.borderColor = gridColor;
            Chart.defaults.plugins.legend.labels.color = textColor;
            Chart.defaults.scale.grid.color = gridColor;
            Chart.defaults.scale.ticks.color = textColor;
            
            // Update existing charts
            if (window.Chart) {
                Object.keys(window.Chart.instances).forEach(key => {
                    const chart = window.Chart.instances[key];
                    if (chart) {
                        chart.options.plugins.legend.labels.color = textColor;
                        chart.options.scales.x.ticks.color = textColor;
                        chart.options.scales.x.grid.color = gridColor;
                        chart.options.scales.y.ticks.color = textColor;
                        chart.options.scales.y.grid.color = gridColor;
                        chart.update();
                    }
                });
            }
        }
    }

    // Toggle theme
    function toggleTheme() {
        const currentTheme = getStoredTheme();
        const newTheme = currentTheme === 'light' ? 'dark' : 'light';
        setTheme(newTheme);
    }

    // Create theme toggle button
    function createThemeToggle() {
        // Only create button if there's a navbar and no existing theme toggle
        const navbar = document.querySelector('.navbar-nav');
        if (navbar && !document.getElementById('themeToggle')) {
            const themeBtn = document.createElement('button');
            themeBtn.id = 'themeToggle';
            themeBtn.className = 'btn btn-sm btn-outline-secondary ms-2';
            themeBtn.innerHTML = '<i class="bi bi-moon-fill"></i>';
            themeBtn.title = 'Switch to dark mode';
            themeBtn.onclick = toggleTheme;
            
            // Insert before logout button if it exists
            const logoutBtn = navbar.querySelector('button[onclick*="logout"]');
            if (logoutBtn) {
                navbar.insertBefore(themeBtn, logoutBtn);
            } else {
                navbar.appendChild(themeBtn);
            }
        }
    }

    // Update Prism theme
    function updatePrismTheme(theme) {
        const lightTheme = document.getElementById('prism-light');
        const darkTheme = document.getElementById('prism-dark');
        
        if (lightTheme && darkTheme) {
            if (theme === 'dark') {
                lightTheme.disabled = true;
                darkTheme.disabled = false;
            } else {
                lightTheme.disabled = false;
                darkTheme.disabled = true;
            }
        }
    }

    // Initialize theme on page load
    function initTheme() {
        const theme = getStoredTheme();
        setTheme(theme);
    }

    // Initialize when DOM is ready
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', function() {
            initTheme();
            createThemeToggle();
        });
    } else {
        initTheme();
        createThemeToggle();
    }

    // Expose functions globally
    window.themeManager = {
        setTheme: setTheme,
        toggleTheme: toggleTheme,
        getStoredTheme: getStoredTheme
    };
    
    // Also expose toggleTheme directly on window for onclick handlers
    window.toggleTheme = toggleTheme;
})();