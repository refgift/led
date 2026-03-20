# Known Issues and Warnings for led Editor
## Status Update (2026-03-19)
All major functions are operational with enhanced stability:
- **Search & Replace**: Fully functional with improved regex handling and safety checks ✓
- **Syntax Highlighting**: Multi-line nested blocks now correctly colorized ✓
- **Status Bar**: Optimized and overflow-protected ✓
- **Test Suite**: Expanded to 95 tests covering edge cases, large files, and word wrap scenarios ✓
## Critical Warnings
- **Large Files**: Files over 30000 lines hog the system and will need to process terminated.
- **File Validation**: Rejects files >10MB or with null bytes to avoid crashes. Check file properties if loading fails.
- **Display Robustness**: Very long lines (>10,000 chars) use basic rendering without highlighting for performance.
## Limitations
- **Language Support**: Optimized for C/C++; other languages may have incomplete highlighting or parsing.
- **Performance**: Dense or very large files may load slowly; consider splitting files for better experience.
- **Word Wrap**: Enabled with improvements for editing and tabs; report any remaining issues with large files.
## Workarounds
- For large files, use external tools to split before editing.
- Toggle off syntax highlighting in config for problematic files.
- Run tests with `./led -t` to verify stability on your system.
## Reporting Issues
Report bugs to help us improve:
- Provide file details (size, type via `file`, sample via `head`).
- Crash logs: Use `gdb ./led` and share backtrace.
- Submit at: https://github.com/refgift/led/issues
## Disclaimer
This is led version 1.0-beta3, a development build. Use cautiously—report issues for faster fixes. Thanks for your feedback!</content>