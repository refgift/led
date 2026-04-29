# PROCESS.md: Development Process for led Editor Project

## Introduction
This document outlines the standard development workflow for the led text editor project. It ensures consistency, stability, and best practices like cleaning stale files, testing, and documentation updates. Follow these guidelines to contribute effectively.

## Setup and Environment
- **Clone the Repository**: `git clone <repo-url>` (assuming remote exists).
- **Dependencies**: Install ncurses (e.g., `sudo apt install libncurses5-dev` on Ubuntu).
- **Build Environment**: Use GCC (as per Makefile). The project is configured for Linux.

## Build Workflow
- Always start with `make clean` to remove stale build artifacts (.o files, led binary).
- Build the project: `make`.
- For sanitized builds (debug memory issues): `make sanitize`.
- Lint the code: `make lint` (uses splint for static analysis).

Integrate cleaning into scripts or hooks:
- Add to Git pre-commit hook: Edit `.git/hooks/pre-commit` to include `make clean && make lint && make test` (create if not exists).

## Development Best Practices
- **Code Changes**: Follow MVC structure (model.c for data, view.c for rendering, controller.c for input).
- **Bug Fixes**: Reproduce issues, fix in relevant files (e.g., controller.c for input crashes), add tests in test_*.c.
- **Commit Standards**: Use descriptive messages (e.g., "Fix Ctrl-X crash on last line"). Run `make clean && make` before committing.
- **Handling Features**: For disabled features like word wrap, document in WARNINGS.md and spec in WORDWRAP_SPEC.md.
- **Large Files**: Test with largefile.txt to avoid crashes in editing/word wrap.

## Testing
- Run full test suite: Compile with tests (included in Makefile), then `./led` or specific test binaries.
- Add unit tests: For new fixes (e.g., crash in controller.c, add to test_controller.c).
- Stress Testing: Use test_performance_stress() for large inputs.

## Documentation and Release
- Update README.md for overviews, WARNINGS.md for issues.
- Man Page: Edit led.1 and run `make doc` to install.
- Release: Tag versions in Git, update VERSION in code.

## Tools and Conventions
- Coding Style: Adhere to standards.
- Security: Avoid secrets in repo.
- Feedback: Report issues at https://github.com/anomalyco/opencode/issues.

This process evolves—update as needed.
