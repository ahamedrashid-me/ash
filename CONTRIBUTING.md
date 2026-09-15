Here's how you can contribute:

## Repository Overview
**Ash** is a small programming language written in C, featuring a lexer, parser, tree-walk interpreter, and a bytecode VM—all hand-written with no external dependencies. The repo is public and has been actively updated recently.

## Ways to Contribute

### 1. **Report Issues & Suggest Features**
- The repository has issues enabled. You can report bugs, edge cases, or suggest enhancements.
- Currently there are 0 open issues, so there's room for fresh contributions.

### 2. **Fork & Submit Pull Requests**
- The repository allows forking (`allow_forking: true`), so you can:
  - Fork the repo
  - Create a feature branch
  - Make your changes
  - Submit a pull request to the main branch

### 3. **Areas to Contribute**

Based on the README, potential contribution areas include:

- **Language Features**: Enhance the existing features or add new language constructs
- **Performance Optimizations**: Work on making the VM faster or optimize the tree-walk interpreter
- **Built-in Functions**: The project has 20+ built-ins—you could extend them
- **Editor Support**: The README mentions VS Code extension support coming soon—you could help with that
- **Tests**: Add more test programs to the `tests/` directory
- **Documentation**: Improve docs, add tutorials, or create language guides
- **Benchmarks**: Add new benchmark programs to `benchmarks/`

### 4. **Get Started**
```bash
git clone https://github.com/lennoxrose/ash.git
cd ash
make  # builds both ./ashc and ./ashvm
```

### 5. **Check for Guidelines**
While there's no `CONTRIBUTING.md` file yet, you can:
- Review the existing code structure in `src/` to understand the project's conventions
- Open a discussion or issue to ask the maintainer (lennoxrose) about contribution guidelines before starting major work
