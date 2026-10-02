#!/usr/bin/env python3
"""Pair `quality` temperature with Jev semantics. `quality` rates any text.

Why both: `quality` is deterministic/instant/offline - the tight 1-change
loop gate. Jev sees what it can't. Headers get the contract battery.
Any other text gets a purpose/claim/term/bulk battery. Don't replace;
correlate, then let Jev propose the single highest-leverage next change.

Docs: concepts/how-to-build-with-system-one, primitives(+noul/score),
patterns/composite-scoring, confidence, api. Model: jev-latest.

Usage:
  python3 tools/quality_jev.py score                 # no args = tracked *.h
  python3 tools/quality_jev.py score tools/daily.sh  # any text file
  python3 tools/quality_jev.py score --dry-run tools/daily.sh
  python3 tools/quality_jev.py score --save /tmp/opencode/hdr_base.json
  python3 tools/quality_jev.py score --compare /tmp/opencode/hdr_base.json
    # delta includes whether the quality verse held, warmed, or cooled
"""

import json
import os
import subprocess
import sys
import urllib.request

API_URL = "https://api.typesafe.ai/v1/systemone"
MODEL = "jev-latest"
QUALITY_BIN = os.environ.get("QUALITY_BIN", "quality")

CONVENTIONS = (
    "A led .h is a public contract: every public function/type needs a doc "
    "comment (purpose, params, ownership/lifetime); names are clear and "
    "consistent; only client-needed declarations and includes; no dead "
    "declarations; comments must match the declarations they describe."
)

TEXT_CONVENTIONS = (
    "Any text should state its purpose, keep claims consistent with the "
    "rest of the text, use clear consistent terms, and omit repetition "
    "and dead bulk that hides the point."
)

HEADER_PROBLEMS = ("has_undocumented_api", "has_misleading_comment",
                   "has_naming_issue", "has_interface_bloat")
TEXT_PROBLEMS = ("has_missing_purpose", "has_misleading_claim",
                 "has_naming_issue", "has_bloat")


def kind_of(path):
    return "c-header" if path.endswith(".h") else "text"


def build_state(path, body, kind):
    state = {
        "file": {"path": path, "kind": kind},
        "conventions": CONVENTIONS if kind == "c-header" else TEXT_CONVENTIONS,
    }
    if kind == "c-header":
        state["header"] = body
    else:
        state["text"] = body
    return state


def build_questions(kind):
    if kind != "c-header":
        return build_text_questions()
    return build_header_questions()


def build_header_questions():
    return {
        "has_undocumented_api": {
            "type": "noul",
            "instructions": "Does `header` declare any public function, type, or macro with no doc comment describing its purpose (and params/ownership where applicable)?",
            "criteria": {
                "true": "At least one public declaration in `header` lacks a purpose-describing comment",
                "false": "Every public declaration in `header` has a purpose-describing comment",
            },
        },
        "has_misleading_comment": {
            "type": "noul",
            "instructions": "Does any comment in `header` contradict, overpromise, or misdescribe the declaration it documents (wrong params, wrong ownership, stale behavior)?",
            "criteria": {
                "true": "At least one comment in `header` misdescribes its declaration",
                "false": "All comments in `header` accurately match their declarations",
            },
        },
        "has_naming_issue": {
            "type": "noul",
            "instructions": "Does `header` use unclear, inconsistent, or collision-prone names (cryptic abbreviations, mixed conventions, misleading verbs)?",
            "criteria": {
                "true": "At least one public name in `header` is unclear or inconsistent",
                "false": "Public names in `header` are clear and consistent",
            },
        },
        "has_interface_bloat": {
            "type": "noul",
            "instructions": "Does `header` expose things clients should not see (dead declarations, implementation internals, unneeded includes)?",
            "criteria": {
                "true": "`header` contains dead, internal, or unneeded exposed surface",
                "false": "`header` exposes only what clients need",
            },
        },
        "excellence": {
            "type": "score",
            "instructions": "How excellent is `header` as a public contract under `conventions`?",
            "criteria": [
                "Poor or misleading contract: missing or wrong docs, confusing names, bloated surface",
                "Adequate contract: usable, but needs polish on docs, names, or surface",
                "Excellent contract: complete accurate docs, clear names, minimal surface",
            ],
        },
        "primary_concern": {
            "type": "choice",
            "instructions": "Which single concern is the most important fix for `header`?",
            "criteria": {
                "undocumented": "Missing doc comments are the top issue",
                "misleading": "Inaccurate or stale comments are the top issue",
                "naming": "Unclear or inconsistent names are the top issue",
                "bloat": "Excess surface or dead declarations are the top issue",
                "none": "No fix needed; contract is excellent",
            },
        },
    }


def build_text_questions():
    return {
        "has_missing_purpose": {
            "type": "noul",
            "instructions": "Does `text` fail to state what it is for, so a reader cannot tell its purpose from the text itself?",
            "criteria": {
                "true": "`text` never states its purpose",
                "false": "`text` states what it is for",
            },
        },
        "has_misleading_claim": {
            "type": "noul",
            "instructions": "Does `text` contradict itself, or state something the rest of `text` shows is false?",
            "criteria": {
                "true": "At least one claim in `text` conflicts with the rest of `text`",
                "false": "Claims in `text` agree with the rest of `text`",
            },
        },
        "has_naming_issue": {
            "type": "noul",
            "instructions": "Does `text` use unclear, inconsistent, or colliding names or terms?",
            "criteria": {
                "true": "At least one name or term in `text` is unclear or inconsistent",
                "false": "Names and terms in `text` are clear and consistent",
            },
        },
        "has_bloat": {
            "type": "noul",
            "instructions": "Does `text` contain repetition, dead bulk, or material that obscures the point?",
            "criteria": {
                "true": "`text` has repetition or dead bulk that hides the point",
                "false": "`text` stays on its point without dead bulk",
            },
        },
        "excellence": {
            "type": "score",
            "instructions": "How excellent is `text` as a readable artifact under `conventions`?",
            "criteria": [
                "Poor: purpose missing or claims conflict, terms confuse, bulk hides the point",
                "Adequate: usable, but purpose, claims, terms, or bulk need polish",
                "Excellent: purpose clear, claims consistent, terms clear, no dead bulk",
            ],
        },
        "primary_concern": {
            "type": "choice",
            "instructions": "Which single concern is the most important fix for `text`?",
            "criteria": {
                "undocumented": "Missing statement of purpose is the top issue",
                "misleading": "A self-contradiction or false claim is the top issue",
                "naming": "Unclear or inconsistent terms are the top issue",
                "bloat": "Repetition or dead bulk is the top issue",
                "none": "No fix needed; the text is excellent",
            },
        },
    }


def call_systemone(state, questions):
    key = os.environ.get("TYPESAFE_API_KEY", "")
    if not key:
        raise RuntimeError("TYPESAFE_API_KEY not set")
    body = json.dumps(
        {"state": state, "model": MODEL, "questions": questions}).encode()
    req = urllib.request.Request(
        API_URL, data=body,
        headers={"Authorization": f"Bearer {key}",
                 "Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.loads(r.read().decode())


def compose_verdict(answers, problem_ids):
    def noul(k):
        return float(answers.get(k, {}).get("noul", 0.0))
    problems = sum(noul(k) for k in problem_ids) / float(len(problem_ids) or 1)
    ex = answers.get("excellence", {})
    excellence01 = float(ex.get("score", 0.0)) / 2.0
    ex_conf = float(ex.get("confidence", 0.0))
    cat = answers.get("primary_concern", {})
    concern, conf = cat.get("choice", "none"), float(cat.get("confidence", 0.0))
    health = 0.6 * (1.0 - problems) + 0.4 * excellence01
    uncertain = (any(0.4 < noul(k) < 0.6 for k in problem_ids)
                 or conf < 0.75 or ex_conf < 0.6)
    if uncertain:
        action = "review"
    elif health >= 0.7 and concern == "none":
        action = "excellent"
    elif health >= 0.45:
        action = "needs_work"
    else:
        action = "poor"
    return {"problems": round(problems, 3),
            "excellence01": round(excellence01, 3),
            "health": round(health, 3), "concern": concern,
            "concern_confidence": conf, "excellence_confidence": ex_conf,
            "action": action}


def quality_reading(path):
    """Return (temp °F, verse, error). A summary line is ``path temp verse``.

    The verse labels the integer degree. The number is the token after the
    path, and the rest of that line is the verse. A line that does not
    start with the path still uses its last token, so a bare number works.
    """
    try:
        out = subprocess.run([QUALITY_BIN, path], capture_output=True,
                             text=True, timeout=60)
    except Exception as e:
        return None, "", f"quality exec failure: {e}"
    txt = (out.stdout or "").strip()
    if not txt:
        return None, "", (out.stderr or "quality: no output").strip()
    lines = [ln.strip() for ln in txt.splitlines() if ln.strip()]
    token = ""
    verse = ""
    for line in lines:
        if line.startswith(path) and (len(line) == len(path)
                                       or line[len(path)].isspace()):
            rest = line[len(path):].lstrip()
            parts = rest.split(None, 1)
            token = parts[0] if parts else ""
            verse = parts[1].strip() if len(parts) > 1 else ""
            break
    if not token and lines:
        token = lines[-1].split()[-1]
    try:
        return float(token), verse, None
    except ValueError:
        return None, "", f"quality unparseable: {txt[:120]}"


def header_path(path):
    """A verse is a header reading. C puts the hot contract in the .h."""
    return path.endswith(".h") or path.endswith(".H")


def verse_move(before, after, temp_before, temp_after):
    """Say whether a header's degree-label held, warmed, or cooled.

    The same words mean the integer degree did not change. A higher
    temperature with different words warmed; a lower one cooled. The
    table is the hot band, about 0.1°F to 100°F, which is where a .h
    lands. A .c is cold and is not given a verse to compare.
    """
    b = (before or "").strip()
    a = (after or "").strip()
    if not b and not a:
        return "absent"
    if b == a:
        if (a == "Out of range" and temp_before is not None
                and temp_after is not None and temp_before != temp_after):
            return f"held out of range: {a}"
        return f"held: {a or b}"
    if temp_before is not None and temp_after is not None:
        if temp_after > temp_before:
            direction = "warmed"
        elif temp_after < temp_before:
            direction = "cooled"
        else:
            direction = "relabeled"
    else:
        direction = "moved"
    return f"{direction}: {b or '(none)'} -> {a or '(none)'}"


def tracked_headers():
    try:
        out = subprocess.run(["git", "ls-files", "*.h"], capture_output=True,
                             text=True, timeout=30)
        files = [l for l in (out.stdout or "").splitlines() if l.endswith(".h")]
        if files:
            return files
    except Exception:
        pass
    import glob
    return sorted(glob.glob("**/*.h", recursive=True))


def spearman(xs, ys):
    """Rank correlation in stdlib. Returns None if undefined."""
    def ranks(v):
        order = sorted(range(len(v)), key=lambda i: v[i])
        r = [0.0] * len(v)
        i = 0
        while i < len(order):
            j = i
            while j + 1 < len(order) and v[order[j + 1]] == v[order[i]]:
                j += 1
            avg = (i + j) / 2.0 + 1
            for k in range(i, j + 1):
                r[order[k]] = avg
            i = j + 1
        return r
    if len(xs) < 3 or len(set(xs)) < 2 or len(set(ys)) < 2:
        return None
    rx, ry = ranks(xs), ranks(ys)
    mx, my = sum(rx) / len(rx), sum(ry) / len(ry)
    cov = sum((a - mx) * (b - my) for a, b in zip(rx, ry))
    vx = sum((a - mx) ** 2 for a in rx)
    vy = sum((b - my) ** 2 for b in ry)
    if vx == 0 or vy == 0:
        return None
    return round(cov / (vx ** 0.5 * vy ** 0.5), 3)


def score_file(path, dry=False):
    kind = kind_of(path)
    questions = build_questions(kind)
    with open(path, "rb") as f:
        raw = f.read()
    if b"\x00" in raw:
        return {"file": path, "quality_error": "null bytes"}
    body = raw.decode("utf-8", errors="replace")
    temp, verse, qerr = quality_reading(path)
    if not header_path(path):
        verse = ""
    state = build_state(path, body, kind)
    if dry or not os.environ.get("TYPESAFE_API_KEY"):
        return {"file": path, "quality_F": temp, "interpretation": verse,
                "quality_error": qerr,
                "dry_request": {"state": state, "model": MODEL,
                                "questions": questions}}
    try:
        resp = call_systemone(state, questions)
    except Exception as e:
        return {"file": path, "quality_F": temp, "interpretation": verse,
                "quality_error": qerr, "service_error": str(e)[:200]}
    v = compose_verdict(resp.get("answers", {}),
                        HEADER_PROBLEMS if kind == "c-header" else TEXT_PROBLEMS)
    return {"file": path, "kind": kind, "quality_F": temp,
            "interpretation": verse, "quality_error": qerr,
            **v, "usage": resp.get("usage")}


def main(argv):
    if len(argv) < 2 or argv[1] not in ("score",):
        print(__doc__)
        return 2
    args = argv[2:]
    dry = "--dry-run" in args
    save = compare = None
    files = []
    skip_next = False
    for i, a in enumerate(args):
        if skip_next:
            skip_next = False
            continue
        if a in ("--save", "--compare") and i + 1 < len(args):
            if a == "--save":
                save = args[i + 1]
            else:
                compare = args[i + 1]
            skip_next = True
            continue
        if a.startswith("-"):
            continue
        files.append(a)
    if compare and not files:
        with open(compare) as f:
            base = json.load(f)
        files = [r["file"] for r in base.get("results", [])]
    if not files:
        files = tracked_headers()
    missing = [f for f in files if not os.path.isfile(f)]
    if missing:
        print(f"missing: {', '.join(missing)}", file=sys.stderr)
        return 2
    if dry or not os.environ.get("TYPESAFE_API_KEY"):
        r = score_file(files[0], dry=True)
        print(json.dumps(r["dry_request"], indent=2))
        print("\n# dry-run: first file only; paste state+questions into "
              "https://console.typesafe.ai/decode",
              file=sys.stderr)
        return 0
    results = [score_file(f) for f in files]
    temps = [r["quality_F"] for r in results if r.get("quality_F") is not None]
    healths = [r["health"] for r in results if "health" in r]
    print(f"{'file':32} {'qualF':>8} {'health':>7} {'exc':>5} "
          f"{'prob':>5} concern(action)")
    for r in results:
        if "health" in r:
            print(f"{r['file']:32} {r['quality_F']:8.1f} {r['health']:7.3f} "
                  f"{r['excellence01']:5.2f} {r['problems']:5.2f} "
                  f"{r['concern']}({r['action']})")
        else:
            print(f"{r['file']:32} ERROR "
                  f"{r.get('service_error') or r.get('quality_error')}")
    if len(temps) == len(healths) and len(healths) >= 3:
        print(f"\nSpearman(quality_F, health) = {spearman(temps, healths)} "
              f"(n={len(healths)}; expect positive if aligned)")
    else:
        print("\ncorrelation needs >=3 paired results", file=sys.stderr)
    if save:
        with open(save, "w") as f:
            json.dump({"results": results}, f, indent=2)
        print(f"saved baseline -> {save}", file=sys.stderr)
    if compare:
        with open(compare) as f:
            base = {r["file"]: r for r in json.load(f).get("results", [])}
        print("\ndelta vs baseline (now - base):")
        for r in results:
            b = base.get(r["file"])
            if not b or "health" not in r or "health" not in b:
                continue
            dq = (r["quality_F"] - b["quality_F"]
                  if r.get("quality_F") is not None
                  and b.get("quality_F") is not None else float("nan"))
            print(f"{r['file']:32} dQual={dq:+.1f}F "
                  f"dHealth={r['health'] - b['health']:+.3f} "
                  f"{b.get('action', '?')}->{r['action']}")
            if header_path(r["file"]):
                print("  verse "
                      + verse_move(b.get("interpretation"),
                                   r.get("interpretation"),
                                   b.get("quality_F"), r.get("quality_F")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
