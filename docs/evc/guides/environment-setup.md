# EVC Environment Setup Guide

> **Configure your development environment for optimal human-AI collaboration**

## Minimum Requirements

### Hardware
- **RAM**: 8GB minimum (16GB recommended)
- **Storage**: 50GB free space
- **Network**: Stable internet connection
- **Display**: Large monitor recommended (code + AI chat)

### Software Essentials

#### 1. Version Control
```bash
# Git 2.0+ required
git --version

# Configure git for clear history
git config --global user.name "Your Name"
git config --global user.email "your.email@example.com"
git config --global commit.verbose true
git config --global diff.algorithm histogram
```

#### 2. AI Access Options

**Option A: Claude (Recommended)**
- Claude Pro subscription ($20/month)
- 100k+ context window
- Code-aware interface
- Project knowledge persistence

**Option B: API Access**
```bash
# Install API client
pip install anthropic  # or openai

# Set API key
export ANTHROPIC_API_KEY="your-key-here"
```

**Option C: Local Models** (Advanced)
- LM Studio or Ollama
- Minimum 24GB VRAM
- Reduced capability vs cloud

#### 3. Development Tools

**Terminal Setup**
```bash
# Essential: tmux or screen for session management
sudo apt install tmux  # or brew install tmux

# Recommended: Modern shell
# zsh with oh-my-zsh or fish
```

**Editor Configuration**
```bash
# VS Code with extensions
code --install-extension ms-python.python
code --install-extension ms-vscode.cpptools
code --install-extension yzhang.markdown-all-in-one
code --install-extension streetsidesoftware.code-spell-checker

# Or Vim with plugins
# .vimrc additions for EVC
set number
set autowrite
set autoread
set updatetime=100
```

## Workspace Organization

### Directory Structure
```
~/evc-workspace/
├── projects/           # Active EVC projects
│   ├── project-1/
│   └── project-2/
├── templates/          # Reusable templates
│   ├── python/
│   ├── javascript/
│   └── c/
├── logs/              # Session logs
└── archives/          # Completed projects
```

### Project Template
```bash
#!/bin/bash
# save as ~/evc-workspace/templates/init-project.sh

PROJECT_NAME=$1
LANGUAGE=$2

mkdir -p "$PROJECT_NAME"/{src,tests,docs}
cd "$PROJECT_NAME"

# Core files
cat > README.md << 'EOF'
# Project Name

Built with Extreme Vibe Coding (EVC).

## Vision
[Clear project vision here]

## Principles
1. Single Source of Truth
2. Test-Driven Development
3. Clean Architecture
4. Comprehensive Logging
EOF

cat > CLAUDE.md << 'EOF'
# Development Guidelines

## Core Principles
1. Single Source of Truth - No duplicate implementations
2. Test First - Write tests before implementation
3. Clean Commits - Atomic, descriptive commits
4. Quality Standards - Zero warnings, full coverage

## Architecture Decisions
- [Document key decisions here]

## Patterns Established
- [Document patterns as they emerge]
EOF

# Initialize git
git init
git add .
git commit -m "Initial project structure"

echo "Project $PROJECT_NAME initialized!"
```

## Session Management

### Tmux Configuration
```bash
# ~/.tmux.conf for EVC sessions
# Split panes for code + AI
bind | split-window -h
bind - split-window -v

# Easy pane navigation
bind h select-pane -L
bind j select-pane -D
bind k select-pane -U
bind l select-pane -R

# Large history for context
set -g history-limit 50000

# Mouse support
set -g mouse on
```

### Session Launcher
```bash
#!/bin/bash
# save as ~/bin/evc-session

PROJECT=$1
SESSION_NAME="evc-$PROJECT"

# Create or attach to session
tmux new-session -d -s "$SESSION_NAME" -c "$PROJECT"

# Split for AI interaction
tmux split-window -h -p 40
tmux send-keys -t "$SESSION_NAME:0.1" "claude" C-m

# Open editor in main pane
tmux send-keys -t "$SESSION_NAME:0.0" "code ." C-m

# Attach to session
tmux attach -t "$SESSION_NAME"
```

## AI Interface Optimization

### Browser Setup (Claude.ai)
```javascript
// Bookmarklet for better code viewing
javascript:(function(){
    document.body.style.fontFamily = 'monospace';
    document.body.style.fontSize = '14px';
})();
```

### Terminal AI Setup
```python
# ~/.config/evc/ai_client.py
import anthropic
import os
from datetime import datetime

class EVCClient:
    def __init__(self):
        self.client = anthropic.Client(
            api_key=os.environ.get("ANTHROPIC_API_KEY")
        )
        self.context = []
        self.session_file = f"session_{datetime.now():%Y%m%d_%H%M%S}.log"
    
    def send(self, message):
        # Add to context
        self.context.append({"role": "user", "content": message})
        
        # Get response
        response = self.client.messages.create(
            model="claude-3-sonnet-20240229",
            messages=self.context,
            max_tokens=4000
        )
        
        # Log everything
        with open(self.session_file, "a") as f:
            f.write(f"\n\nUser: {message}\n")
            f.write(f"\nClaude: {response.content}\n")
        
        return response.content
```

## Quality Assurance Tools

### Language-Specific Linters
```bash
# Python
pip install pylint black mypy pytest pytest-cov

# JavaScript  
npm install -g eslint prettier jest

# C
sudo apt install cppcheck clang-format valgrind

# Go
go install golang.org/x/lint/golint@latest
```

### Pre-commit Hooks
```yaml
# .pre-commit-config.yaml
repos:
  - repo: https://github.com/pre-commit/pre-commit-hooks
    rev: v4.4.0
    hooks:
      - id: trailing-whitespace
      - id: end-of-file-fixer
      - id: check-yaml
      - id: check-added-large-files
      
  - repo: https://github.com/psf/black
    rev: 23.3.0
    hooks:
      - id: black
        
  - repo: https://github.com/pycqa/pylint
    rev: v2.17.4
    hooks:
      - id: pylint
```

## Performance Monitoring

### System Resources
```bash
# Monitor during EVC sessions
# install htop
sudo apt install htop

# Custom monitoring script
cat > ~/bin/evc-monitor << 'EOF'
#!/bin/bash
while true; do
    clear
    echo "=== EVC Session Monitor ==="
    echo "CPU: $(top -bn1 | grep "Cpu(s)" | awk '{print $2}')"
    echo "Memory: $(free -h | grep Mem | awk '{print $3 "/" $2}')"
    echo "Disk I/O: $(iostat -d 1 2 | tail -n 2)"
    echo "Git Status: $(cd $1 && git status -s | wc -l) uncommitted files"
    sleep 5
done
EOF
chmod +x ~/bin/evc-monitor
```

## Troubleshooting

### Common Issues

**1. Context Window Limits**
```bash
# Split large files before feeding to AI
split -l 500 large_file.py part_

# Or use context management
find . -name "*.py" -exec wc -l {} + | sort -n
```

**2. Git Conflicts**
```bash
# EVC-friendly merge strategy
git config merge.ours.driver true
git config merge.tool vimdiff
```

**3. Performance Degradation**
```bash
# Clear caches and restart
tmux kill-server
git gc --aggressive
# Restart AI session
```

## Quick Setup Script

```bash
#!/bin/bash
# Complete EVC environment setup

echo "Setting up EVC environment..."

# Install essentials
sudo apt update
sudo apt install -y git tmux vim curl wget htop

# Configure git
git config --global init.defaultBranch main
git config --global core.editor vim

# Create workspace
mkdir -p ~/evc-workspace/{projects,templates,logs,archives}

# Install language tools (customize as needed)
curl -fsSL https://deb.nodesource.com/setup_lts.x | sudo -E bash -
sudo apt install -y nodejs python3-pip

# Python tools
pip3 install --user pylint black mypy pytest pytest-cov

# Create helper scripts
mkdir -p ~/bin
curl -o ~/bin/evc-session https://raw.githubusercontent.com/.../evc-session
chmod +x ~/bin/*

echo "EVC environment ready!"
echo "Start your first session with: evc-session my-project"
```

## Conclusion

A well-configured environment is crucial for EVC success. Focus on:
- Reliable AI access
- Efficient workspace organization  
- Quality assurance automation
- Session management tools

The investment in setup pays dividends in productivity.

---

> "Your environment should amplify the human-AI partnership, not hinder it."