# Contributing to QJSDB

Thank you for your interest in contributing to QJSDB! This document outlines our Git workflow and collaboration practices to ensure smooth development with multiple contributors.

## Repository Information

- **Repository URL**: https://git.home.arpa/itdlabs/qjsdb.git
- **Main Branch**: `master` (the primary branch where all changes eventually merge)
- **Username**: Use your assigned account (example: `claude-3`)

## Git Workflow

We follow a feature branch workflow with pull requests. This helps maintain code quality and enables team members to work concurrently on different features.

### Basic Workflow Overview

1. **Always start with the latest code**
2. **Create feature branches for your work**
3. **Make small, focused commits with clear messages**
4. **Push your branch and create a pull request**
5. **Request code reviews from team members**
6. **Merge to main only after approval**

## Detailed Workflow Steps

### 1. Setting Up Your Repository

Clone the repository and set up your remotes:

```bash
# Clone the repository
git clone https://git.home.arpa/itdlabs/qjsdb.git
cd qjsdb

# Verify remotes are set correctly
git remote -v
```

### 2. Before Starting New Work

Always start with the latest code from the master branch:

```bash
# Switch to master
git checkout master

# Pull the latest changes
git pull origin master

# Create a new feature branch
git checkout -b feature/your-feature-name
```

Use descriptive branch names that reflect what you're working on:
- Feature branches: `feature/add-transaction-logging`
- Bug fixes: `fix/query-parser-error`
- Documentation: `docs/update-api-documentation`

### 3. Making Changes

Follow these guidelines for your changes:

- Make commits that are focused on a single purpose
- Write clear commit messages that explain WHY the change was made
- Commit frequently to make potential merges easier
- Follow the project's code style and conventions

```bash
# Example commit workflow
git add <files>
git commit -m "Add transaction logging with file-based persistence"
```

### 4. Keeping Your Branch Updated

Regularly update your feature branch with changes from master to reduce merge conflicts:

```bash
# Get latest changes from master
git checkout master
git pull origin master

# Return to your feature branch
git checkout feature/your-feature-name

# Rebase or merge from master
git rebase master  # Preferred method
# OR
git merge master
```

### 5. Creating a Pull Request

Once your feature is complete:

1. Push your branch to the remote repository:
   ```bash
   git push -u origin feature/your-feature-name
   ```

2. Go to the repository on the Git server and create a new pull request:
   - Base branch should be `master`
   - Compare branch should be your feature branch
   - Add a descriptive title and detailed description
   - Reference any related issues with `#issue-number`
   - Assign reviewers who are familiar with the code you modified

### 6. Code Review Process

- All code changes must be reviewed before merging to master
- Address all feedback in the pull request
- Reviewers should focus on:
  - Code correctness and functionality
  - Adherence to project standards
  - Potential bugs or edge cases
  - Performance implications
  - Security concerns

### 7. Merging to Master

Only merge to master after your PR has been approved:

- Ensure CI checks pass (if configured)
- All review comments have been addressed
- At least one approval from a team member is required

The preferred merge method is "Squash and Merge" for a cleaner history, but this depends on your team's preference.

## Handling Merge Conflicts

Merge conflicts occur when multiple people change the same lines of code. Here's how to handle them:

### Preventing Merge Conflicts

1. **Communicate with your team** about which files you're working on
2. **Keep pull requests small and focused** to minimize overlap
3. **Update your branch regularly** with changes from master
4. **Use separate files when possible** to reduce direct conflicts

### Resolving Merge Conflicts

If you encounter merge conflicts:

1. First, update your feature branch:
   ```bash
   git checkout master
   git pull origin master
   git checkout feature/your-feature-name
   git merge master
   ```

2. Git will tell you which files have conflicts. Open these files and look for the conflict markers (`<<<<<<<`, `=======`, `>>>>>>>`).

3. Edit the files to resolve the conflicts:
   - The code between `<<<<<<< HEAD` and `=======` is from your branch
   - The code between `=======` and `>>>>>>> master` is from the master branch
   - Decide which code to keep, or combine them appropriately
   - Remove the conflict markers when you're done

4. After resolving all conflicts:
   ```bash
   git add <resolved-files>
   git commit -m "Resolve merge conflicts"
   git push origin feature/your-feature-name
   ```

5. Continue with your pull request process

### When Conflicts Are Complex

For very complex conflicts:

1. Discuss the conflicts with teammates who worked on the conflicting code
2. Consider pair programming to resolve particularly difficult conflicts
3. If needed, use `git mergetool` if you have a visual diff/merge tool configured

## Best Practices

1. **Never force push to master** - This can overwrite others' work
2. **Avoid long-lived feature branches** - They lead to more conflicts
3. **Write good commit messages** - Start with a verb and explain the change
4. **Keep commits focused** - Each commit should do one thing
5. **Document your code** - Makes it easier for others to understand
6. **Run tests before committing** - Ensure your changes don't break existing functionality
7. **Review your own code first** - Look for obvious issues before requesting a review

## Troubleshooting

### Common Git Issues and Solutions

- **Accidentally committed to master**: 
  ```bash
  git reset HEAD~1  # Undo last commit but keep changes
  git checkout -b feature/your-feature  # Create a new branch
  git add .  # Re-add your changes
  git commit -m "Your commit message"  # Commit to the feature branch
  ```

- **Need to undo a commit**:
  ```bash
  git reset --soft HEAD~1  # Undo commit but keep changes staged
  # OR
  git reset --hard HEAD~1  # Undo commit and discard changes (be careful!)
  ```

- **Need to update PR after feedback**:
  ```bash
  # Make your changes
  git add .
  git commit -m "Address PR feedback"
  git push origin feature/your-feature-name
  ```

## Git Command Cheatsheet

- **Create a new branch**: `git checkout -b branch-name`
- **Switch branches**: `git checkout branch-name`
- **View status**: `git status`
- **View commit history**: `git log`
- **Discard changes to a file**: `git checkout -- filename`
- **Stash changes temporarily**: `git stash`
- **Retrieve stashed changes**: `git stash pop`
- **View differences**: `git diff`
- **View configured remotes**: `git remote -v`
- **Update remote branches**: `git fetch origin`

---

If you have any questions about this workflow, please reach out to the team lead or repository maintainers.

Remember that the goal of these practices is to make collaboration smoother for everyone. Thank you for following these guidelines!