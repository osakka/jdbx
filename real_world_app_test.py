#!/usr/bin/env python3
"""
Real-world application test - Building a complete blog application
Tests JDBX usability for actual application development
"""

import requests
import json
import urllib3
import time
import random
from datetime import datetime

# Disable SSL warnings
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

class BlogApp:
    def __init__(self, base_url="https://localhost:5000"):
        self.base_url = base_url
        self.session = requests.Session()
        self.session.verify = False
        self.token = None
        self.user_id = None
        
    def login(self, username, password):
        """Login and store authentication token"""
        response = self.session.post(
            f"{self.base_url}/api/auth/login",
            json={"username": username, "password": password}
        )
        if response.status_code == 200:
            data = response.json()
            self.token = data.get('token')
            self.user_id = data.get('user_id', 'admin')
            self.session.headers.update({'Authorization': f'Bearer {self.token}'})
            return True
        return False
    
    def create_post(self, title, content, tags=None):
        """Create a blog post"""
        post_data = {
            "title": title,
            "content": content,
            "tags": tags or [],
            "status": "published",
            "created_at": datetime.utcnow().isoformat() + "Z",
            "type": "blog_post"  # Custom type for our blog posts
        }
        
        response = self.session.post(
            f"{self.base_url}/api/libraries/default/collections/blog_posts/documents",
            json=post_data
        )
        
        if response.status_code == 201:
            return response.json()
        else:
            print(f"Failed to create post: {response.status_code} - {response.text}")
            return None
    
    def get_all_posts(self):
        """Get all blog posts"""
        # Get all documents from the blog_posts collection
        response = self.session.get(
            f"{self.base_url}/api/libraries/default/collections/blog_posts/documents"
        )
        
        if response.status_code == 200:
            return response.json().get('documents', [])
        return []
    
    def get_post(self, post_id):
        """Get a specific blog post"""
        response = self.session.get(
            f"{self.base_url}/api/libraries/default/collections/blog_posts/documents/{post_id}"
        )
        
        if response.status_code == 200:
            return response.json()
        return None
    
    def update_post(self, post_id, updates):
        """Update a blog post"""
        response = self.session.put(
            f"{self.base_url}/api/libraries/default/collections/blog_posts/documents/{post_id}",
            json=updates
        )
        
        if response.status_code == 200:
            return response.json()
        return None
    
    def delete_post(self, post_id):
        """Delete a blog post"""
        response = self.session.delete(
            f"{self.base_url}/api/libraries/default/collections/blog_posts/documents/{post_id}"
        )
        
        return response.status_code == 204
    
    def add_comment(self, post_id, author, content):
        """Add a comment to a blog post"""
        comment_data = {
            "type": "comment",
            "post_id": post_id,
            "author": author,
            "content": content,
            "created_at": datetime.utcnow().isoformat() + "Z"
        }
        
        response = self.session.post(
            f"{self.base_url}/api/libraries/default/collections/comments/documents",
            json=comment_data
        )
        
        if response.status_code == 201:
            return response.json()
        return None
    
    def get_post_comments(self, post_id):
        """Get all comments for a post"""
        # Get all comments and filter client-side for now
        response = self.session.get(
            f"{self.base_url}/api/libraries/default/collections/comments/documents"
        )
        
        if response.status_code == 200:
            all_comments = response.json().get('documents', [])
            # Filter comments for this post
            return [c for c in all_comments if c.get('post_id') == post_id]
        return []

def main():
    print("🌐 Real-World Blog Application Test")
    print("="*60)
    print("Testing JDBX as a backend for a blog application")
    print("="*60)
    
    # Initialize blog app
    app = BlogApp()
    
    # Test results tracking
    results = {
        'login': False,
        'create_posts': 0,
        'read_posts': False,
        'update_posts': 0,
        'delete_posts': 0,
        'comments': 0,
        'performance': {},
        'errors': []
    }
    
    # 1. Authentication
    print("\n1️⃣ Testing Authentication...")
    start = time.time()
    if app.login("admin", "secure123456789"):
        results['login'] = True
        print("✅ Login successful")
    else:
        print("❌ Login failed")
        return
    results['performance']['login'] = time.time() - start
    
    # 2. Create blog posts
    print("\n2️⃣ Creating Blog Posts...")
    post_ids = []
    
    blog_posts = [
        {
            "title": "Getting Started with JDBX",
            "content": "JDBX is a JSON document database that makes it easy to build applications...",
            "tags": ["tutorial", "getting-started", "jdbx"]
        },
        {
            "title": "Building Real-Time Applications",
            "content": "Learn how to build real-time applications using JDBX's powerful features...",
            "tags": ["real-time", "websockets", "tutorial"]
        },
        {
            "title": "Performance Optimization Tips",
            "content": "Here are some tips to optimize your JDBX application performance...",
            "tags": ["performance", "optimization", "best-practices"]
        }
    ]
    
    start = time.time()
    for post_data in blog_posts:
        post = app.create_post(**post_data)
        if post:
            post_ids.append(post['uuid'])
            results['create_posts'] += 1
            print(f"✅ Created post: {post_data['title']}")
        else:
            results['errors'].append(f"Failed to create post: {post_data['title']}")
    results['performance']['create_posts'] = time.time() - start
    
    # 3. Read posts
    print("\n3️⃣ Reading Blog Posts...")
    start = time.time()
    all_posts = app.get_all_posts()
    if all_posts:
        results['read_posts'] = True
        print(f"✅ Retrieved {len(all_posts)} posts")
        for post in all_posts[:3]:  # Show first 3
            print(f"   - {post.get('title', 'Untitled')}")
    else:
        results['errors'].append("Failed to retrieve posts")
    results['performance']['read_posts'] = time.time() - start
    
    # 4. Update a post
    print("\n4️⃣ Updating Posts...")
    if post_ids:
        start = time.time()
        update_data = {
            "title": "Getting Started with JDBX (Updated)",
            "content": "JDBX is an amazing JSON document database that makes it easy to build applications...",
            "updated_at": datetime.utcnow().isoformat() + "Z",
            "tags": ["tutorial", "getting-started", "jdbx", "updated"]
        }
        
        updated = app.update_post(post_ids[0], update_data)
        if updated:
            results['update_posts'] += 1
            print("✅ Successfully updated post")
        else:
            results['errors'].append("Failed to update post")
        results['performance']['update_post'] = time.time() - start
    
    # 5. Add comments
    print("\n5️⃣ Adding Comments...")
    if post_ids:
        start = time.time()
        comments = [
            ("Alice", "Great tutorial! Very helpful."),
            ("Bob", "Thanks for sharing this!"),
            ("Charlie", "Can you add more examples?")
        ]
        
        for author, content in comments:
            comment = app.add_comment(post_ids[0], author, content)
            if comment:
                results['comments'] += 1
                print(f"✅ Added comment from {author}")
            else:
                results['errors'].append(f"Failed to add comment from {author}")
        results['performance']['add_comments'] = time.time() - start
    
    # 6. Read comments
    print("\n6️⃣ Reading Comments...")
    if post_ids:
        start = time.time()
        post_comments = app.get_post_comments(post_ids[0])
        print(f"✅ Retrieved {len(post_comments)} comments")
        for comment in post_comments:
            print(f"   - {comment.get('author', 'Anonymous')}: {comment.get('content', '')[:50]}...")
        results['performance']['read_comments'] = time.time() - start
    
    # 7. Complex query test
    print("\n7️⃣ Testing Complex Queries...")
    start = time.time()
    
    # Since JDBX might not support complex queries yet, let's do client-side filtering
    all_posts_for_filter = app.get_all_posts()
    filtered_posts = [
        post for post in all_posts_for_filter 
        if 'tutorial' in post.get('tags', [])
    ]
    
    print(f"✅ Found {len(filtered_posts)} posts with 'tutorial' tag (client-side filter)")
    results['performance']['complex_query'] = time.time() - start
    
    # 8. Delete a post
    print("\n8️⃣ Deleting Posts...")
    if post_ids and len(post_ids) > 1:
        start = time.time()
        if app.delete_post(post_ids[-1]):
            results['delete_posts'] += 1
            print("✅ Successfully deleted post")
        else:
            results['errors'].append("Failed to delete post")
        results['performance']['delete_post'] = time.time() - start
    
    # 9. Performance Summary
    print("\n📊 Performance Summary")
    print("="*60)
    total_time = sum(results['performance'].values())
    for operation, duration in results['performance'].items():
        print(f"{operation:<20} {duration*1000:>8.2f} ms")
    print("-"*60)
    print(f"{'Total':<20} {total_time*1000:>8.2f} ms")
    
    # 10. Final Report
    print("\n📋 Real-World Usability Report")
    print("="*60)
    print(f"✅ Authentication:     {'Working' if results['login'] else 'Failed'}")
    print(f"✅ Create Operations:  {results['create_posts']} posts created")
    print(f"✅ Read Operations:    {'Working' if results['read_posts'] else 'Failed'}")
    print(f"✅ Update Operations:  {results['update_posts']} posts updated")
    print(f"✅ Delete Operations:  {results['delete_posts']} posts deleted")
    print(f"✅ Related Data:       {results['comments']} comments added")
    print(f"❌ Errors:            {len(results['errors'])}")
    
    if results['errors']:
        print("\n⚠️  Issues Found:")
        for error in results['errors']:
            print(f"   - {error}")
    
    # Overall assessment
    print("\n🎯 Overall Assessment:")
    success_rate = (results['create_posts'] + results['update_posts'] + 
                   results['delete_posts'] + results['comments']) / 10 * 100
    
    if success_rate >= 80:
        print("✅ JDBX is ready for real-world blog application development!")
    elif success_rate >= 60:
        print("⚠️  JDBX works but has some limitations for complex applications")
    else:
        print("❌ JDBX needs improvements for production applications")
    
    print(f"\nSuccess Rate: {success_rate:.1f}%")
    print("="*60)

if __name__ == "__main__":
    main()