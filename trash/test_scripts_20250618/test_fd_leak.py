#!/usr/bin/env python3
"""Test for file descriptor leaks"""

import subprocess
import time
import os

def count_open_fds(pid):
    """Count open file descriptors for a process"""
    try:
        fd_dir = f"/proc/{pid}/fd"
        if os.path.exists(fd_dir):
            return len(os.listdir(fd_dir))
    except:
        pass
    return -1

def test_fd_leak():
    """Monitor file descriptors during requests"""
    print("Testing for file descriptor leaks...")
    
    # Get server PID
    try:
        with open("/opt/jdbx/build/var/jdbxd.pid", "r") as f:
            pid = int(f.read().strip())
    except:
        print("❌ Could not read server PID")
        return
    
    print(f"Server PID: {pid}")
    
    # Initial FD count
    initial_fds = count_open_fds(pid)
    if initial_fds < 0:
        print("❌ Could not count file descriptors")
        return
    
    print(f"Initial FDs: {initial_fds}")
    
    # Make some requests
    for i in range(5):
        # Use curl to make a simple request
        subprocess.run([
            "curl", "-k", "-s", 
            "https://localhost:5000/api/health"
        ], capture_output=True)
        
        fds = count_open_fds(pid)
        if fds < 0:
            print(f"  Request {i+1}: Server crashed!")
            break
        else:
            print(f"  Request {i+1}: {fds} FDs (delta: {fds - initial_fds})")
        
        time.sleep(0.5)
    
    # Final count
    final_fds = count_open_fds(pid)
    if final_fds >= 0:
        print(f"\nFinal FDs: {final_fds}")
        print(f"FD leak: {final_fds - initial_fds}")
    else:
        print("\n❌ Server is not running")

if __name__ == "__main__":
    test_fd_leak()