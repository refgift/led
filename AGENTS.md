# AGENTS.md

Agentic coding guidelines for the led project.

## Lint and Typecheck Commands

To ensure code quality, run the following commands after changes:

- Lint: `make lint` (uses splint for static analysis: `splint -weak -bounds -null -compdef -warnposix +matchanyintegral +longintegral main.c model.c view.c controller.c editor.c config.c test_controller.c`)
- Build: `make` (compiles with -Wall -Wextra -std=c11)
- Test: `LED_TEST=1 ./led` (runs comprehensive test suite)

## Test Framework

The project includes a dual-controller testing system:
- Normal mode: ncurses-based editor
- Test mode: Console-based test suite (activated with LED_TEST=1 env var)
- Tests cover model, controller, and basic integration scenarios

## Development Workflow

1. Make changes to source files
2. Run `make` to build
3. Run `LED_TEST=1 ./led` to test
4. Run `make lint` for static analysis
5. Commit changes if tests pass

## Key Fixes Implemented

- Fixed insert new lines on new files (buffer_insert_char handles '\n')
- Changed Ctrl+A to select all
- Fixed paste to handle multi-line text splitting
- Fixed search crash when cursor beyond line end
- Implemented unlimited undo with dynamic stack and redo (Ctrl+Y)
- Added extended ASCII support (128-255) with locale configuration
- Enhanced status bar with version/time display, operation feedback messages, filename with modification indicator, and prevented line wrapping corruption
- Added file validation to reject oversized or binary files for crash prevention
- Added word wrap toggle (F3) for visual soft wrapping of long lines
- Added forward delete key (KEY_DC) support
- Disabled selection highlighting for non-syntax files
- Implemented configurable auto-save system (keystrokes and timeout, defaults 50/2s, with versioned backups and crash recovery)

## Test Suite Status

Comprehensive automated test suite implemented with numbered tests:

- Test 1: buffer_init_free - Basic buffer lifecycle
- Test 2: buffer_load_save - File I/O operations
- Test 3: buffer_manipulation - Line and character operations
- Test 4: search_functionality - Search operations

Framework supports 11 additional test categories ready for implementation. Run with `LED_TEST=1 ./led` for automated testing.

## Development Journal

### Phase 1: Core Editor Fixes
- Identified bugs via manual testing
- Implemented fixes for insert, select-all, paste operations
- Added basic test framework

### Phase 2: Crash Prevention
- Fixed search crash with cursor clamping
- Added view coordinate bounds checking
- Enhanced error handling in buffer operations

### Phase 3: Test Suite Expansion
- Implemented dual-controller testing (normal + test modes)
- Added comprehensive model and controller tests
- Introduced numbered test reporting for better tracking

### Phase 4: Colorization Testing
- Extracted compute_line_colors for testability
- Added syntax highlighting tests for keywords, brackets, nesting
- Verified color assignment logic

### Current State
- Editor core functionality stable
- Automated test coverage for critical paths
- Documentation updated with guidelines and status
- Ready for advanced features and integration testing