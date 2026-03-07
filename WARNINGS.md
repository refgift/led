# Known Issues and Warnings for led Editor

## Critical Warnings
- **File Validation**: The editor now rejects files larger than 10MB or containing null bytes (binary files) to prevent crashes and display issues. If you encounter loading failures, check file size and type.
- **Display Robustness**: Long lines (>10,000 chars) are rendered simply without highlighting for stability. Extremely large files may be slow to load.
- **Legacy Issues**: Some edge cases with malformed content or unsupported file types may still cause issues, though crash prevention has been significantly improved.

## Limitations
- **Language Support**: Primarily designed for C/C++ files. Syntax highlighting and parsing assumptions may not work reliably for other languages.
- **Performance**: Large files or dense content can impact performance; use the --safe flag for problematic files.

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
<parameter name="filePath">/home/larry/Documents/grok/led/WARNINGS.md