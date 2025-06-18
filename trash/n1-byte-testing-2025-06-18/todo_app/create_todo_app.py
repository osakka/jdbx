#!/usr/bin/env python3
"""
JDBX TODO App - User Testing Application
Tests real-world usage patterns and uncovers issues
"""

import requests
import json
import sys
import time
from datetime import datetime

# Configuration
BASE_URL = "https://localhost:5000"
USERNAME = "admin"
PASSWORD = "secure123456789"

# Disable SSL warnings for self-signed cert
import urllib3
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

class TodoApp:
    def __init__(self):
        self.session = requests.Session()
        self.session.verify = False  # For self-signed SSL cert
        self.token = None
        
    def login(self):
        """Test: User authentication"""
        print("🔐 Testing login...")
        try:
            response = self.session.post(f"{BASE_URL}/api/auth/login", json={
                "username": USERNAME,
                "password": PASSWORD
            })
            if response.status_code == 200:
                self.token = response.json().get("token")
                self.session.headers.update({"Authorization": f"Bearer {self.token}"})
                print("✅ Login successful")
                return True
            else:
                print(f"❌ Login failed: {response.status_code} - {response.text}")
                return False
        except Exception as e:
            print(f"❌ Login error: {e}")
            return False
    
    def create_task(self, title, description="", priority="medium"):
        """Test: Create a new task"""
        print(f"📝 Creating task: {title}")
        try:
            task = {
                "title": title,
                "description": description,
                "priority": priority,
                "status": "pending",
                "created_at": datetime.now().isoformat(),
                "type": "task"
            }
            response = self.session.post(f"{BASE_URL}/api/documents", json=task)
            if response.status_code == 201:
                task_id = response.json().get("uuid")
                print(f"✅ Task created: {task_id}")
                return task_id
            else:
                print(f"❌ Failed to create task: {response.status_code} - {response.text}")
                return None
        except Exception as e:
            print(f"❌ Error creating task: {e}")
            return None
    
    def list_tasks(self, limit=10):
        """Test: List all tasks"""
        print("📋 Listing tasks...")
        try:
            query = {
                "type": "task",
                "limit": limit
            }
            response = self.session.post(f"{BASE_URL}/api/documents/query", json=query)
            if response.status_code == 200:
                tasks = response.json().get("documents", [])
                print(f"✅ Found {len(tasks)} tasks")
                for task in tasks:
                    print(f"  - [{task.get('status', 'unknown')}] {task.get('title', 'Untitled')} (ID: {task.get('uuid', 'unknown')})")
                return tasks
            else:
                print(f"❌ Failed to list tasks: {response.status_code} - {response.text}")
                return []
        except Exception as e:
            print(f"❌ Error listing tasks: {e}")
            return []
    
    def update_task_status(self, task_id, new_status):
        """Test: Update task status"""
        print(f"🔄 Updating task {task_id} to {new_status}")
        try:
            update = {
                "status": new_status,
                "updated_at": datetime.now().isoformat()
            }
            response = self.session.put(f"{BASE_URL}/api/documents/{task_id}", json=update)
            if response.status_code == 200:
                print(f"✅ Task updated successfully")
                return True
            else:
                print(f"❌ Failed to update task: {response.status_code} - {response.text}")
                return False
        except Exception as e:
            print(f"❌ Error updating task: {e}")
            return False
    
    def delete_task(self, task_id):
        """Test: Delete a task"""
        print(f"🗑️  Deleting task {task_id}")
        try:
            response = self.session.delete(f"{BASE_URL}/api/documents/{task_id}")
            if response.status_code in [200, 204]:
                print(f"✅ Task deleted successfully")
                return True
            else:
                print(f"❌ Failed to delete task: {response.status_code} - {response.text}")
                return False
        except Exception as e:
            print(f"❌ Error deleting task: {e}")
            return False
    
    def stress_test_create(self, count=100):
        """Test: Create many tasks rapidly"""
        print(f"🚀 Stress test: Creating {count} tasks...")
        start_time = time.time()
        success_count = 0
        
        for i in range(count):
            task_id = self.create_task(
                title=f"Stress Test Task #{i+1}",
                description=f"Created at {datetime.now().isoformat()}",
                priority="low" if i % 3 == 0 else "medium"
            )
            if task_id:
                success_count += 1
            
            # Print progress every 10 tasks
            if (i + 1) % 10 == 0:
                print(f"  Progress: {i+1}/{count} tasks created...")
        
        elapsed = time.time() - start_time
        rate = success_count / elapsed if elapsed > 0 else 0
        
        print(f"✅ Created {success_count}/{count} tasks in {elapsed:.2f}s ({rate:.1f} tasks/sec)")
        return success_count == count
    
    def test_large_document(self):
        """Test: Create a task with large description"""
        print("📦 Testing large document creation...")
        large_text = "x" * 10000  # 10KB of text
        task_id = self.create_task(
            title="Large Document Test",
            description=large_text,
            priority="high"
        )
        return task_id is not None
    
    def test_concurrent_updates(self):
        """Test: Concurrent updates to same task"""
        print("🔀 Testing concurrent updates...")
        # Create a task first
        task_id = self.create_task("Concurrent Test Task")
        if not task_id:
            return False
        
        # Try to update it multiple times rapidly
        import threading
        results = []
        
        def update_task(status):
            result = self.update_task_status(task_id, status)
            results.append(result)
        
        threads = []
        statuses = ["in_progress", "completed", "pending", "cancelled", "on_hold"]
        
        for status in statuses:
            t = threading.Thread(target=update_task, args=(status,))
            threads.append(t)
            t.start()
        
        for t in threads:
            t.join()
        
        success_rate = sum(results) / len(results) if results else 0
        print(f"✅ Concurrent update success rate: {success_rate*100:.1f}%")
        return success_rate > 0.8  # At least 80% should succeed

def main():
    """Run the TODO app tests"""
    print("🧪 JDBX TODO App - User Testing")
    print("=" * 50)
    
    app = TodoApp()
    
    # Phase 1: Basic Operations
    print("\n📍 Phase 1: Basic Operations")
    if not app.login():
        print("❌ Cannot proceed without authentication")
        return 1
    
    # Create some tasks
    task1 = app.create_task("Buy groceries", "Milk, eggs, bread")
    task2 = app.create_task("Write report", "Q4 financial report", "high")
    task3 = app.create_task("Call dentist", priority="low")
    
    # List tasks
    app.list_tasks()
    
    # Update a task
    if task1:
        app.update_task_status(task1, "completed")
    
    # Delete a task
    if task3:
        app.delete_task(task3)
    
    # List again to see changes
    app.list_tasks()
    
    # Phase 2: Edge Cases
    print("\n📍 Phase 2: Edge Cases")
    app.test_large_document()
    
    # Phase 3: Stress Testing
    print("\n📍 Phase 3: Stress Testing")
    # app.stress_test_create(50)  # Start with 50 for quick test
    
    # Phase 4: Concurrency
    print("\n📍 Phase 4: Concurrency Testing")
    app.test_concurrent_updates()
    
    print("\n✅ User testing completed!")
    return 0

if __name__ == "__main__":
    sys.exit(main())