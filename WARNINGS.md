# Known Issues and Warnings for led Editor

## Critical Warnings
- **Crashes on Opening Certain File Types**: The editor may crash when opening .fs files (or other extensions) with complex, dense, or malformed content (e.g., files with null bytes, extremely long lines, or large sizes >10MB). This can result in no text being displayed or program termination.
- **Display Failures**: Some file types may fail to display content properly, showing nothing or truncated output.
- **Exit Crashes**: In certain scenarios, the editor may crash upon exit, especially after loading problematic files. (Note: Crashes related to unbalanced symbol pairs in paired_keywords, such as "(,)", have been fixed by adding substring matching for opens/closes.)

## Limitations
- **Language Support**: Primarily designed for C/C++ files. It may not handle other languages (e.g., Forth) reliably due to syntax assumptions in highlighting and parsing.
- **Highlighting Performance**: Syntax highlighting can be slow or unstable on files with dense code, keywords, or pairs. It is recommended to disable it for stability.

## Workarounds
- Check file size and type before opening (e.g., avoid large or binary-like files).
- Disable syntax highlighting by editing `~/.config/led/colorization.conf` and setting `syntax_extensions` to exclude problematic types.
- Test with smaller, simpler files first.
- If issues persist, report them for fixes.

## Reporting Issues
Please help improve led by reporting bugs:
- Include file details: size, type (e.g., `file filename`), and a preview (e.g., `head -c 100 filename`).
- Provide crash logs: Run with `gdb ./led`, use `run filename`, and `bt` on crash.
- Submit at: https://github.com/refgift/led/issues

## Disclaimer
This is a development version of led. Use at your own risk. Known issues are being addressed in updates. Thank you for your patience and feedback!</content>
