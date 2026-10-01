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
- **Handling Features**: For known limits, document in README.md (What Works / What Fails). Word wrap spec lives in WORDWRAP_SPEC.md.
- **Large Files**: Test with largefile.txt to avoid crashes in editing/word wrap.

## Testing
- Run full test suite: Compile with tests (included in Makefile), then `./led` or specific test binaries.
- Add unit tests: For new fixes (e.g., crash in controller.c, add to test_controller.c).
- Stress Testing: Use test_performance_stress() for large inputs.

## Quality Measurement (Software Thermometer + Jev contract battery)

Two mandatory steps in the development process. They measure different
things and disagree on purpose — read them together, never either alone.

### 1. Deterministic gate (fast loop)

After any meaningful change (especially before committing):
```bash
make clean && make && ./led -t
for f in $(git ls-files '*.c' '*.h'); do quality "$f"; done
```

Scope to tracked sources: bare `quality .` also scans
`.opencode/node_modules` and pollutes the number. `quality` is instant,
offline, and free — it is the tight 1-change loop gate. Observed: it
rewards density and taxes added lines, so documenting a header can *lower*
its temperature (model.h: 73.1 → 70.0 → 59.0 across two doc-only
iterations). That is information about the metric, not a veto on the change.

### 2. Semantic review (header contracts)

`tools/quality_jev.py` pairs each header's temperature with Jev judgments
it cannot see: undocumented API, misleading comments, naming, interface
bloat, plus an excellence score and a primary concern. Needs
`TYPESAFE_API_KEY`; stdlib only.

Find the next thing to work on:
```bash
python3 tools/quality_jev.py score            # rank all tracked headers
```

Sort by `health` ascending. Lowest health with a high-confidence concern
is the next change. Then, one change at a time:
```bash
python3 tools/quality_jev.py score --save /tmp/opencode/hdr_base.json
# ... make exactly one change, checked against the implementation, not the declaration ...
make clean && make && ./led -t
python3 tools/quality_jev.py score --compare /tmp/opencode/hdr_base.json
```

Rules learned the hard way:
- Verify every new comment against the implementation before measuring.
  A wrong comment is worse than silence — it passes tests and lies to
  readers. (`buffer_delete_range` swaps reversed points; `get_line_length`
  has no truncation marker. Both were caught this way.)
- Low-confidence verdicts route to `review`: go read the file yourself.
- Unanimous verdicts describe the codebase's era, not a ranking. The
  numbers that discriminate are `health` and `excellence`, not the label.
- Expect run-to-run jitter (~±0.016 health). Use wide bands, thin
  thresholds lose.

**Core Philosophy for this project**:
- **.h header files are "hot"** — they are the public contracts, types, and interfaces. They have the highest leverage on overall quality and maintainability. Prioritize making every `.h` file excellent (high comment density, clean declarations, minimal duplication, no dead includes).
- **.c implementation files are "cold"** — they can tolerate more internal entropy (complexity, duplication) provided the behavior is correct, the tests pass, and the `.h` surface remains clean and well-documented.

Measured 2026-10-01 over 21 tracked `*.c/*.h`: average **-163.9°F**.
Headers run warm (25–85°F, `utils.h` highest); large implementations run
arctic (`view.c`, `controller.c`). The old 38.0°F figure predates this
scoping. Weak rank-alignment between temperature and Jev health
(Spearman ≈ 0.21, n=8) confirms they track different qualities.

The goal is steady, measurable improvement in the average temperature, with special attention paid to the temperatures of all `.h` files.

## Documentation and Release
- Update README.md (What Works / What Fails) for overviews and known issues.
- Man Page: Edit led.1 and run `make doc` to install.
- Release: Tag versions in Git, update VERSION in code.

## Tools and Conventions
- Coding Style: Adhere to standards.
- Security: Avoid secrets in repo.
- Feedback: Report issues at https://github.com/anomalyco/opencode/issues.

This process evolves—update as needed.
