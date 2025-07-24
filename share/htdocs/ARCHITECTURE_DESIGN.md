# JDBX UI - Definitive Architecture Design

## Core Principle: ABSOLUTE Single Source of Truth

```
USER ACTION → CONTROLLER → STATE → RENDERER → DOM
     ↑                                           ↓
     ←───────── SINGLE DATA FLOW CYCLE ──────────
```

## 1. Data Flow (One Way, No Exceptions)

```javascript
// SINGLE PATH - NO ALTERNATIVES
UserAction → Controller.handleAction()
          → DataService.fetchData() 
          → State.update()
          → Renderer.render()
          → DOM Updates
```

## 2. Core Components (Four Only)

### **Controller** (Single Entry Point)
- Handles ALL user interactions
- Routes ALL actions to appropriate data service
- NO direct DOM manipulation
- NO data storage

### **DataService** (Single Data Source)  
- Handles ALL API interactions
- NO UI knowledge
- Returns pure data objects
- Handles authentication, caching, error handling

### **State** (Single State Store)
- Stores ALL application state
- Triggers renders on updates
- NO business logic
- Pure reactive store

### **Renderer** (Single UI Update Point)
- Handles ALL DOM updates
- NO data fetching
- NO state management
- Pure data → DOM transformation

## 3. Strict Interfaces

### State Schema (Complete Application State)
```javascript
{
  // Authentication
  auth: {
    user: { username, uuid, library },
    token: "jwt_token",
    isAuthenticated: boolean
  },
  
  // Navigation  
  navigation: {
    currentView: "dashboard|browser|rbac|metrics|operations|api",
    previousView: string
  },
  
  // Data (All Views)
  data: {
    collections: Array<{name, documentCount}>,
    documents: Array<{id, type, content}>,
    stats: {totalCollections, totalDocuments, databaseSize},
    metrics: Array<{name, value, type}>,
    users: Array<{username, roles}>,
    roles: Array<{name, permissions}>,
    libraries: Array<{name, displayName}>
  },
  
  // UI State
  ui: {
    loading: boolean,
    currentLibrary: string,
    notifications: Array<{type, message}>,
    polling: {enabled: boolean, intervals: Object}
  }
}
```

### Controller Interface
```javascript
class Controller {
  // Navigation
  switchView(viewName)
  
  // Authentication  
  login(username, password)
  logout()
  
  // Data Actions
  loadDashboard()
  loadBrowser() 
  loadRBAC()
  loadMetrics()
  switchLibrary(name)
  
  // User Actions
  createDocument(data)
  updateDocument(id, data)
  deleteDocument(id)
}
```

### DataService Interface  
```javascript
class DataService {
  // Pure data fetch methods
  async fetchDashboardData()
  async fetchCollections()
  async fetchDocuments(query)
  async fetchUsers()
  async fetchRoles()
  async fetchMetrics()
  async fetchLibraries()
  
  // Pure data mutation methods
  async createDocument(data)
  async updateDocument(id, data)
  async deleteDocument(id)
}
```

### Renderer Interface
```javascript
class Renderer {
  // Single render method
  render(state)
  
  // Private render methods (called by render())
  private renderNavigation(state)
  private renderDashboard(state) 
  private renderBrowser(state)
  private renderRBAC(state)
  private renderMetrics(state)
  private renderNotifications(state)
}
```

## 4. Polling Strategy (Unified)

```javascript
// Single polling service
class PollingService {
  start() {
    // Single timer for all views
    setInterval(() => {
      const currentView = State.get('navigation.currentView');
      Controller.refreshCurrentView();
    }, 30000); // Single interval
  }
}
```

## 5. Error Handling (Unified)

```javascript
// All errors flow through single handler
class ErrorHandler {
  handle(error, context) {
    State.update('ui.notifications', [...notifications, {
      type: 'error',
      message: error.message,
      context
    }]);
  }
}
```

## 6. Implementation Rules (Non-Negotiable)

1. **NO component can access DOM directly except Renderer**
2. **NO component can call APIs directly except DataService**  
3. **NO component can store state except State**
4. **NO component can handle user input except Controller**
5. **NO bypass paths - ALL data flows through the four components**
6. **NO parallel implementations - ONE way to do each thing**
7. **NO hacks, workarounds, or temporary solutions**

## 7. File Structure (Clean)

```
js/
├── app.js                 // Application bootstrap only
├── controller.js          // Single controller
├── dataService.js         // Single data service  
├── state.js              // Single state store
├── renderer.js           // Single renderer
└── utils/
    ├── api.js            // Low-level API client
    ├── auth.js           // Authentication utilities
    └── polling.js        // Polling service
```

## 8. Validation Checklist

Before implementation complete, verify:
- [ ] Only 4 core files (controller, dataService, state, renderer)
- [ ] Zero direct DOM access outside renderer
- [ ] Zero API calls outside dataService
- [ ] Zero state storage outside state  
- [ ] Zero user input handling outside controller
- [ ] All data flows through single path
- [ ] No parallel implementations exist
- [ ] No hacks or workarounds exist
- [ ] All features working end-to-end
- [ ] Zero compiler warnings