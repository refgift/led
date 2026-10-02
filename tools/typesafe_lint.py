#!/usr/bin/env python3
"""Semantic lint sidecar for led using TypeSafe System One (Jev).

Keeps deterministic work in code; Jev supplies narrow semantic judgments.
No third-party deps: stdlib only. Needs TYPESAFE_API_KEY env for live calls,
otherwise runs --dry-run to print the exact request for Playground review.

Docs source of truth:
- https://docs.typesafe.ai/concepts/how-to-build-with-system-one.md
- https://docs.typesafe.ai/primitives.md, /primitives/noul.md, /primitives/score.md
- https://docs.typesafe.ai/patterns/composite-scoring.md
- https://docs.typesafe.ai/confidence.md
- https://docs.typesafe.ai/api.md

Usage:
  python3 tools/typesafe_lint.py <file.c> [--chunk-lines 120] [--dry-run]
  python3 tools/typesafe_lint.py model/model.c --dry-run | head -c 4000
"""

import json
import os
import sys
import urllib.request

API_URL = "https://api.typesafe.ai/v1/systemone"
MODEL = "jev-latest"

# Deterministic pre-filters mirror led limits (README: 10MB, 10k line len).
MAX_BYTES = 10 * 1024 * 1024
MAX_LINE_LEN = 10_000
LINT_EXTS = (".c", ".h", ".C", ".H", ".cpp", ".cc", ".cxx",
             ".hpp", ".hh", ".hxx")

CONVENTIONS = (
    "led C11: check malloc/calloc/realloc return before use; "
    "prefer bounded string ops (snprintf/strncpy) over strcpy/strcat/sprintf; "
    "every gap_buffer_create/malloc path must have a matching free on all exits; "
    "validate indices against buffer_size/text_len/num_lines before indexing."
)


def read_chunks(path, chunk_lines=120):
    with open(path, "rb") as f:
        raw = f.read()
    if len(raw) > MAX_BYTES:
        raise SystemExit(f"skip: {path} exceeds 10MB led limit")
    if b"\x00" in raw:
        raise SystemExit(f"skip: {path} contains null bytes (led rejects)")
    text = raw.decode("utf-8", errors="replace")
    lines = text.splitlines()
    # Truncate overlong lines like led rendering does; mark them.
    safe = [(ln[:MAX_LINE_LEN] + " /* [truncated 10k+] */"
             if len(ln) > MAX_LINE_LEN else ln) for ln in lines]
    for i in range(0, len(safe), chunk_lines):
        yield i + 1, "\n".join(safe[i:i + chunk_lines])


def build_state(path, start_line, snippet):
    return {
        "file": {"path": path, "language": "c", "start_line": start_line},
        "snippet": snippet,
        "conventions": CONVENTIONS,
    }


def build_questions():
    """One narrow judgment per question; all share the same state."""
    return {
        # Noul: one per independently-useful label (several may apply).
        "missing_null_check": {
            "type": "noul",
            "instructions": "Does `snippet` dereference or index a pointer/buffer that `snippet` itself does not null/bounds-check on that path?",
            "criteria": {
                "true": "A malloc/calloc/realloc result, function arg, or buffer pointer is used without a visible NULL or bounds guard in `snippet`",
                "false": "All uses in `snippet` are guarded, or no risky dereference exists",
            },
        },
        "unchecked_malloc": {
            "type": "noul",
            "instructions": "Does `snippet` call malloc/calloc/realloc/strdup without checking the return value before use?",
            "criteria": {
                "true": "An allocation call has no NULL check before the result is used",
                "false": "Every allocation is checked, or no allocation exists",
            },
        },
        "risky_buffer_op": {
            "type": "noul",
            "instructions": "Does `snippet` use strcpy/strcat/sprintf/gets or an unbounded copy/loop without an explicit size guard from `conventions`?",
            "criteria": {
                "true": "Unbounded string/buffer op with no size check visible in `snippet`",
                "false": "Only bounded ops (snprintf/strncpy/explicit length checks), or no buffer op",
            },
        },
        "resource_leak": {
            "type": "noul",
            "instructions": "Does `snippet` allocate (malloc/gap_buffer_create/initscr/fopen) on a path with no matching free/destroy/close/endwin in `snippet`?",
            "criteria": {
                "true": "An acquired resource has an exit path in `snippet` with no release",
                "false": "All paths release, or no acquisition in `snippet`",
            },
        },
        # Score: degree along one ordered dimension, concrete situations only.
        "severity": {
            "type": "score",
            "instructions": "How severe is the riskiest issue in `snippet` under `conventions`?",
            "criteria": [
                "No issue or cosmetic/style only; no crash, leak, or overflow path",
                "Should fix: possible NULL deref, overflow, or leak on an unusual/error path",
                "Blocking: likely crash, memory corruption, or leak on a normal path",
            ],
        },
        # Choice: exactly one primary bucket; 'none' is the no-match outcome.
        "category": {
            "type": "choice",
            "instructions": "Which single bucket best describes the primary risk in `snippet`?",
            "criteria": {
                "memory": "NULL deref, unchecked allocation, use-after-free",
                "bounds": "Overflow, overlong line, unchecked index/copy length",
                "resource": "Leak or missing cleanup (free/destroy/close/endwin)",
                "style": "Readability/naming only, no safety impact",
                "none": "No issue to flag",
            },
        },
    }


def call_systemone(state, questions):
    key = os.environ.get("TYPESAFE_API_KEY", "")
    if not key:
        raise RuntimeError("TYPESAFE_API_KEY not set")
    body = json.dumps(
        {"state": state, "model": MODEL, "questions": questions}
    ).encode()
    req = urllib.request.Request(
        API_URL,
        data=body,
        headers={"Authorization": f"Bearer {key}",
                 "Content-Type": "application/json"},
    )
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.loads(r.read().decode())


def compose_verdict(answers):
    """Policy lives in code; raw judgments stay reusable. Thresholds are
    starting points to tune on led data (see confidence.md)."""
    def noul(k):
        return float(answers[k].get("noul", 0.0))

    sev = answers.get("severity", {})
    score = float(sev.get("score", 0.0)) / 2.0  # 0..2 -> 0..1
    sev_conf = float(sev.get("confidence", 0.0))
    cat = answers.get("category", {})
    choice, conf = cat.get("choice", "none"), float(cat.get("confidence", 0.0))

    risk = (0.35 * noul("missing_null_check")
            + 0.25 * noul("unchecked_malloc")
            + 0.25 * noul("risky_buffer_op")
            + 0.15 * noul("resource_leak"))
    composite = 0.6 * risk + 0.4 * score

    uncertain = (any(0.4 < noul(k) < 0.6 for k in
                     ("missing_null_check", "unchecked_malloc",
                      "risky_buffer_op", "resource_leak"))
                 or conf < 0.75 or sev_conf < 0.6)
    if uncertain:
        action = "review"
    elif composite >= 0.6 and choice != "none":
        action = "flag"
    elif composite < 0.3:
        action = "pass"
    else:
        action = "review"
    return {"risk": round(risk, 3), "severity01": round(score, 3),
            "composite": round(composite, 3), "category": choice,
            "category_confidence": conf, "severity_confidence": sev_conf,
            "action": action}


def main(argv):
    if len(argv) < 2 or "-h" in argv or "--help" in argv:
        print(__doc__)
        return 2
    path = argv[1]
    if not path.endswith(LINT_EXTS):
        print(f"skip: {path} not a C/C++ source ext")
        return 0
    chunk_lines = 120
    dry = "--dry-run" in argv
    for i, a in enumerate(argv):
        if a.startswith("--chunk-lines="):
            chunk_lines = int(a.split("=", 1)[1])
    live_key = bool(os.environ.get("TYPESAFE_API_KEY"))
    if dry or not live_key:
        # Print one exact request for Playground verification; no network.
        for start, snippet in read_chunks(path, chunk_lines):
            req = {"state": build_state(path, start, snippet),
                   "model": MODEL, "questions": build_questions()}
            print(json.dumps(req, indent=2))
            print("\n# ^ paste state+questions into "
                  "https://console.typesafe.ai/decode ; "
                  "composition thresholds are examples to tune, not rules.",
                  file=sys.stderr)
            break  # first chunk only in dry-run
        if not live_key and not dry:
            print("note: TYPESAFE_API_KEY unset, printed dry-run request",
                  file=sys.stderr)
        return 0
    questions = build_questions()
    for start, snippet in read_chunks(path, chunk_lines):
        try:
            resp = call_systemone(build_state(path, start, snippet),
                                  questions)
        except Exception as e:  # separate service failure from verdict
            print(f"{path}:{start}: ERROR service failure: {e}")
            continue
        v = compose_verdict(resp.get("answers", {}))
        print(f"{path}:{start}: {v['action']} "
              f"composite={v['composite']} cat={v['category']} "
              f"(risk={v['risk']} sev={v['severity01']})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
