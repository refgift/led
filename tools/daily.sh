#!/usr/bin/env bash
set -euo pipefail

REPO="/home/lbd/Projects/led"
STATE="${XDG_STATE_HOME:-$HOME/.local/state}/led-daily"
ENV_FILE="${HOME}/.config/led/daily.env"
OPENCODE="/home/lbd/.local/share/mise/installs/opencode/latest/opencode"
WHAT_FILE="${REPO}/.led-daily-what"
MAX_FILES=3
MAX_LINES=150

mkdir -p "$STATE"
chmod 700 "$STATE"
exec 9>"$STATE/lock"
if ! flock -n 9; then
  printf '%s skip: already running\n' "$(date -Is)" >&2
  exit 0
fi

export HOME="${HOME:-/home/lbd}"
export PATH="/home/lbd/.local/bin:/home/lbd/.local/share/mise/shims:/usr/local/bin:/usr/bin:/bin"
export XDG_CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"
export XDG_DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
export XDG_STATE_HOME="${XDG_STATE_HOME:-$HOME/.local/state}"
export GIT_TERMINAL_PROMPT=0
if [[ -z "${DBUS_SESSION_BUS_ADDRESS:-}" && -n "${XDG_RUNTIME_DIR:-}" ]]; then
  export DBUS_SESSION_BUS_ADDRESS="unix:path=${XDG_RUNTIME_DIR}/bus"
fi
if [[ -f "$ENV_FILE" ]]; then
  set -a
  # shellcheck disable=SC1090
  source "$ENV_FILE"
  set +a
fi

mode="run"
if [[ "${1:-}" == "dry" ]]; then
  mode="dry"
fi

log() {
  printf '%s %s\n' "$(date -Is)" "$*" | tee -a "$STATE/run.log" >&2
}

commit_path_log() {
  local msg="$1"
  if [[ "$(git branch --show-current)" != "master" ]]; then
    log "path.log not committed: not on master"
    return 0
  fi
  if git diff --quiet -- path.log && git diff --cached --quiet -- path.log; then
    return 0
  fi
  git commit -m "$msg" -- path.log
  git rev-parse HEAD >"$STATE/pending-push"
  if git push origin HEAD; then
    rm -f "$STATE/pending-push"
  else
    log "fail: push path.log"
  fi
}

record_day() {
  local why="$1" files qb hb vb
  [[ "$mode" == "run" ]] || return 0
  [[ "$day_logged" == 1 ]] && return 0
  why="$(printf '%s' "$why" | tr '|' '/' | tr -d '\r' | sed 's/[[:space:]]\+/ /g; s/^ //; s/ $//')"
  [[ -n "$why" ]] || why="fail: no outcome"
  why="${why:0:120}"
  if ! grep -q "^${today} |" path.log; then
    files="${target:--}"
    qb="${qual_before:-n/a}"
    hb="${health_before:-n/a}"
    vb="${verse_before:-}"
    if [[ -n "$vb" ]]; then
      vb="$(printf '%s' "$vb" | tr '|' '/' | tr -d '\r')"
      printf '%s | - | %s | %s | qual %s->n/a | health %s->n/a | tests n/a | verse %s -> n/a\n' \
        "$today" "$files" "$why" "$qb" "$hb" "$vb" >>path.log
    else
      printf '%s | - | %s | %s | qual %s->n/a | health %s->n/a | tests n/a\n' \
        "$today" "$files" "$why" "$qb" "$hb" >>path.log
    fi
  fi
  day_logged=1
  commit_path_log "path.log: ${today} ${why}" || log "fail: commit path.log"
}

cd "$REPO"

today="$(date +%F)"
HEAD=""
agent_started=0
committed=0
day_claimed=0
day_logged=0
outcome=""

restore_tools() {
  if [[ -f "$STATE/script.bak" ]]; then
    cp -a "$STATE/script.bak" "$REPO/tools/daily.sh"
  fi
  if [[ -d "$STATE/tools-bak" ]]; then
    cp -a "$STATE/tools-bak/." "$REPO/tools/"
  fi
}

drop_new_untracked() {
  [[ -f "$STATE/untracked.before" ]] || return 0
  git ls-files --others --exclude-standard | LC_ALL=C sort >"$STATE/untracked.after"
  comm -13 "$STATE/untracked.before" "$STATE/untracked.after" | while IFS= read -r f; do
    [[ -n "$f" ]] || continue
    rm -rf -- "$REPO/$f"
  done
}

reject_target() {
  printf '%s\t%s\t%s\n' "$today" "$1" "$2" >>"$STATE/rejected"
}

undo_agent_files() {
  local f
  if [[ -n "$HEAD" && "$(git rev-parse HEAD)" != "$HEAD" ]]; then
    if [[ "$(git branch --show-current)" != "master" ]]; then
      log "fail: branch moved off master; leaving the tree"
      return 1
    fi
    git reset --mixed "$HEAD"
  fi
  if [[ -s "$STATE/agent-files" ]]; then
    while IFS= read -r f; do
      [[ -n "$f" ]] || continue
      git checkout -f "$HEAD" -- "$f" 2>/dev/null || rm -rf -- "$REPO/$f"
    done <"$STATE/agent-files"
  fi
  drop_new_untracked
  restore_tools
  rm -f "$WHAT_FILE"
}

trap 'rc=$?; if [[ "$agent_started" == 1 && "$committed" != 1 && "$rc" != 0 ]]; then undo_agent_files || true; fi; if [[ "$day_claimed" == 1 && "$day_logged" != 1 ]]; then record_day "${outcome:-fail: exit ${rc}}" || true; fi' EXIT

if [[ "$mode" == "run" ]]; then
  day_claimed=1
fi

if [[ "$mode" == "run" && -f "${HOME}/.config/led/daily.pause" ]]; then
  log "skip: pause file present"
  outcome="fail: paused"
  exit 0
fi

if [[ "$(git branch --show-current)" != "master" ]]; then
  log "skip: not on master"
  outcome="fail: not on master"
  exit 0
fi

if [[ -n "$(git status --porcelain --untracked-files=no)" ]]; then
  if grep -q "^${today} |" path.log; then
    commit_path_log "path.log: ${today}" || true
    day_logged=1
    day_claimed=0
    log "skip: path.log already has ${today}"
    exit 0
  fi
  log "skip: tracked tree dirty"
  outcome="fail: tracked tree dirty"
  exit 0
fi

if [[ -f "$STATE/pending-push" ]]; then
  want="$(cat "$STATE/pending-push")"
  git fetch origin master || { log "fail: fetch while retrying push"; outcome="fail: fetch while retrying push"; exit 1; }
  if [[ "$(git rev-parse HEAD)" == "$want" ]] && ! git merge-base --is-ancestor "$want" origin/master; then
    git push origin HEAD
    rm -f "$STATE/pending-push"
    log "pushed pending ${want}"
  fi
fi

if [[ "$mode" == "run" ]] && grep -q "^${today} |" path.log; then
  log "skip: path.log already has ${today}"
  day_claimed=0
  exit 0
fi

hour="$(date +%H)"
min="$(date +%M)"
in_window=0
if [[ "$hour" == "09" && "$min" -ge 20 ]] || [[ "$hour" == "10" && "$min" -le 15 ]]; then
  in_window=1
fi
if [[ "$mode" == "run" && "$in_window" == 0 ]]; then
  sid="$(loginctl list-sessions --no-legend | awk '$3=="lbd" && $0 ~ /user/ {print $1; exit}')"
  idle="no"
  if [[ -n "$sid" ]]; then
    idle="$(loginctl show-session "$sid" -p IdleHint --value 2>/dev/null || echo no)"
  fi
  if [[ "$idle" != "yes" ]]; then
    log "skip: catch-up while session is active"
    day_claimed=0
    exit 0
  fi
fi

git fetch origin master || { log "fail: fetch"; outcome="fail: fetch"; exit 1; }
ahead="$(git rev-list --count origin/master..HEAD)"
behind="$(git rev-list --count HEAD..origin/master)"
if [[ "$ahead" != "0" ]]; then
  log "skip: ${ahead} unpushed commit(s) on master"
  outcome="fail: ${ahead} unpushed commit(s) on master"
  exit 0
fi
if [[ "$behind" != "0" ]]; then
  git merge --ff-only origin/master
fi

if [[ "$mode" == "run" ]] && grep -q "^${today} |" path.log; then
  log "skip: path.log already has ${today} after pull"
  day_claimed=0
  exit 0
fi

if [[ -z "${TYPESAFE_API_KEY:-}" ]]; then
  log "skip: TYPESAFE_API_KEY unset (put it in ${ENV_FILE})"
  outcome="fail: TYPESAFE_API_KEY unset"
  exit 0
fi

if ! timeout 300 python3 tools/quality_jev.py score --save "$STATE/baseline.json" >"$STATE/score.txt"; then
  log "fail: header score"
  outcome="fail: header score"
  exit 1
fi
set +e
python3 - "$STATE/baseline.json" "$STATE/rejected" <<'PY' >"$STATE/target"
import json, sys
from datetime import date, datetime
base = json.load(open(sys.argv[1]))
rejected = set()
try:
    lines = open(sys.argv[2])
except FileNotFoundError:
    lines = []
today = date.today()
for line in lines:
    parts = line.rstrip("\n").split("\t")
    if len(parts) != 3:
        continue
    try:
        when = datetime.strptime(parts[0], "%Y-%m-%d").date()
    except ValueError:
        continue
    if (today - when).days <= 7:
        rejected.add((parts[1], parts[2]))
cands = []
for r in base.get("results", []):
    if "health" not in r:
        continue
    action = r.get("action") or ""
    if action not in ("needs_work", "poor", "review"):
        continue
    concern = r.get("concern") or "none"
    conf = float(r.get("concern_confidence") or 0)
    if action in ("needs_work", "poor") and (conf < 0.75 or concern == "none"):
        continue
    if (r["file"], concern) in rejected:
        continue
    cands.append(r)
if not cands:
    sys.exit(2)
cands.sort(key=lambda r: (r["health"], -float(r.get("concern_confidence") or 0)))
r = cands[0]
print(r["file"])
print(r.get("concern") or "none")
print(f"{float(r.get('concern_confidence') or 0):.3f}")
print(f"{float(r['health']):.3f}".rstrip("0").rstrip("."))
q = r.get("quality_F")
print("" if q is None else f"{float(q):.1f}")
print(r.get("action") or "")
print((r.get("interpretation") or "").replace("\n", " ").strip())
PY
pick_rc=$?
set -e
if [[ "$pick_rc" == "2" ]]; then
  log "skip: no header to work or review"
  outcome="fail: no header to work or review"
  exit 0
fi
if [[ "$pick_rc" != "0" ]]; then
  log "fail: could not pick a target"
  outcome="fail: could not pick a target"
  exit 1
fi

target="$(sed -n '1p' "$STATE/target")"
concern="$(sed -n '2p' "$STATE/target")"
conf="$(sed -n '3p' "$STATE/target")"
health_before="$(sed -n '4p' "$STATE/target")"
qual_before="$(sed -n '5p' "$STATE/target")"
action="$(sed -n '6p' "$STATE/target")"
verse_before="$(sed -n '7p' "$STATE/target")"
[[ -n "$qual_before" ]] || qual_before="unknown"
verse_move=""
review_line=""

log "target ${target} action ${action} concern ${concern} health ${health_before} qual ${qual_before} verse ${verse_before:-absent}"
if [[ "$mode" == "dry" ]]; then
  exit 0
fi

if [[ ! -x "$OPENCODE" ]]; then
  OPENCODE="$(command -v opencode || true)"
fi
if [[ -z "$OPENCODE" || ! -x "$OPENCODE" ]]; then
  log "fail: opencode not found"
  outcome="fail: opencode not found"
  exit 1
fi

if [[ "$action" == "review" ]]; then
  log "review ${target} before the edit"
  review_head="$(git rev-parse HEAD)"
  review_prompt=$(cat <<EOF
Read ${REPO}/${target} and the implementation it describes. Do not edit any file. Do not commit.

Jev marked this file action=review, concern=${concern}, confidence=${conf}. The verdict is too uncertain to edit from the score alone.

A change to the header is major. A change to the .c that leaves the header as it is is minor.
Name the major change when the contract is wrong. Name the minor change when the contract is true and the behavior is wrong.
If you cannot verify one safe change against the implementation, print exactly:
SKIP
Otherwise print exactly one line and nothing else:
CHANGE: <the one verified edit, no pipe characters, under 120 characters>
EOF
)
  set +e
  GROK="$(command -v grok || true)"
  if [[ -n "$GROK" ]]; then
    timeout 8m "$GROK" -p --verbatim --no-alt-screen --output-format plain \
      --permission-mode plan --cwd "$REPO" "$review_prompt" \
      >"$STATE/review.txt" 2>"$STATE/review.err"
    review_rc=$?
  else
    review_rc=127
  fi
  set -e
  if [[ "$review_rc" != 0 || ! -s "$STATE/review.txt" ]]; then
    log "grok review failed (${review_rc}); opencode review"
    timeout 12m "$OPENCODE" run --dir "$REPO" --auto --title "led daily review ${today}" \
      "$review_prompt Print the line only. Write nothing in the repo." \
      >"$STATE/review.txt" 2>"$STATE/review.err" || true
  fi
  if [[ "$(git rev-parse HEAD)" != "$review_head" || -n "$(git status --porcelain --untracked-files=no)" ]]; then
    log "review dirtied the tree; restoring"
    git reset --mixed "$review_head"
    git checkout -f "$review_head" -- .
  fi
  review_line="$(grep -E '^(SKIP|CHANGE:)' "$STATE/review.txt" | tail -1 || true)"
  if [[ -z "$review_line" || "$review_line" == "SKIP" ]]; then
    log "skip: review declined ${target} ${concern}"
    outcome="fail: review declined ${target} ${concern}"
    reject_target "$target" "$concern"
    exit 0
  fi
  review_line="${review_line#CHANGE: }"
  log "review assigned: ${review_line}"
fi

HEAD="$(git rev-parse HEAD)"
git ls-files --others --exclude-standard | LC_ALL=C sort >"$STATE/untracked.before"
cp -a "$0" "$STATE/script.bak"
mkdir -p "$STATE/tools-bak"
cp -a tools/*.py "$STATE/tools-bak/" 2>/dev/null || true
: >"$STATE/agent-files"
rm -f "$WHAT_FILE"

prompt=$(cat <<EOF
You are the daily maintainer of ${REPO}. Make exactly one change, then stop.

Target header: ${target}
Jev action: ${action}
Primary concern: ${concern} (confidence ${conf})
Health now: ${health_before}
quality temperature now: ${qual_before}
${review_line:+Review assigned this one change, and only this change: ${review_line}}

Follow doc/PROCESS.md:
- One change only, aimed at that concern. A header edit is major. A .c edit that leaves the header untouched is minor.
- Make the major change when the header itself is wrong. Make the minor change when the contract is true and the behavior is wrong.
- Verify every new or edited comment against the implementation, not the declaration. A wrong comment is worse than silence. If you cannot verify it, do not edit.
- A matching .c and a focused test may accompany a major change.
- Do not refactor, rename broadly, reformat, or touch unrelated code.
- Do not commit, push, amend, tag, or edit path.log, tools/, .git, or any secret or env file.
- Stay inside the target's module. Nothing else.
- If the honest fix cannot be one verified edit under ${MAX_LINES} lines, do not edit. Write exactly SKIP to ${WHAT_FILE} and stop.
- Otherwise write one line to ${WHAT_FILE}: what changed, no pipe characters, under 100 characters. Then stop.
EOF
)

agent_started=1
set +e
timeout 25m "$OPENCODE" run --dir "$REPO" --auto --title "led daily ${today}" "$prompt" >"$STATE/agent.log" 2>&1
agent_rc=$?
set -e
log "opencode exit ${agent_rc}"

if [[ "$(git branch --show-current)" != "master" ]]; then
  log "fail: agent left master; not forcing a checkout"
  outcome="fail: agent left master"
  exit 1
fi
if [[ "$(git rev-parse HEAD)" != "$HEAD" ]]; then
  git reset --mixed "$HEAD"
fi
restore_tools
rm -f "$WHAT_FILE.tmp"

if [[ -f "$WHAT_FILE" ]] && grep -qx 'SKIP' "$WHAT_FILE"; then
  log "skip: agent declined ${target} ${concern}"
  outcome="fail: agent declined ${target} ${concern}"
  reject_target "$target" "$concern"
  undo_agent_files
  agent_started=0
  exit 0
fi

git diff --name-only HEAD | grep -vxF '.led-daily-what' >"$STATE/agent-files" || true
git ls-files --others --exclude-standard | LC_ALL=C sort >"$STATE/untracked.after"
comm -13 "$STATE/untracked.before" "$STATE/untracked.after" | grep -vxF '.led-daily-what' >>"$STATE/agent-files" || true
if [[ ! -s "$STATE/agent-files" ]]; then
  log "skip: agent made no change"
  outcome="fail: agent made no change"
  agent_started=0
  exit 0
fi

module_ok() {
  local f="$1" dir stem
  [[ "$f" == "$target" ]] && return 0
  dir="$(dirname "$target")"
  stem="$(basename "$target" .h)"
  case "$f" in
    test/test_*.c|test/test_*.h) return 0 ;;
  esac
  if [[ "$dir" == "." ]]; then
    [[ "$f" == "${stem}.c" || "$f" == "${stem}.h" ]]
    return
  fi
  [[ "$f" == "${dir}/${stem}.c" || "$f" == "${dir}/"*".c" || "$f" == "${dir}/"*".h" ]]
}

bad=0
file_count=0
while IFS= read -r f; do
  [[ -n "$f" ]] || continue
  file_count=$((file_count + 1))
  case "$f" in
    *../*|/*|.git/*|.opencode/*|tools/*|path.log|*.env|*.bak|*.bak.*) bad=1 ;;
  esac
  if ! module_ok "$f"; then
    bad=1
  fi
done <"$STATE/agent-files"

if [[ "$bad" != 0 || "$file_count" -gt "$MAX_FILES" ]]; then
  log "reject: change left the module or touched too many files"
  outcome="fail: change left the module or touched too many files"
  reject_target "$target" "$concern"
  undo_agent_files
  agent_started=0
  exit 0
fi

change_class="minor"
while IFS= read -r f; do
  [[ "$f" == "$target" ]] && change_class="major"
done <"$STATE/agent-files"
log "change is ${change_class}"

lines="$(git diff --numstat -- $(git diff --name-only) | awk '{a+=$1+$2} END {print a+0}')"
new_lines="$(git ls-files --others --exclude-standard | comm -13 "$STATE/untracked.before" - | while IFS= read -r f; do
  [[ -f "$f" ]] && wc -l <"$f"
done | awk '{a+=$1} END {print a+0}')"
lines=$((lines + new_lines))
if [[ "$lines" -gt "$MAX_LINES" ]]; then
  log "reject: diff is ${lines} lines"
  outcome="fail: diff is ${lines} lines"
  reject_target "$target" "$concern"
  undo_agent_files
  agent_started=0
  exit 0
fi

if git diff | grep -E 'TYPESAFE_API_KEY[[:space:]]*=[[:space:]]*[^[:space:]]|BEGIN (OPENSSH|RSA|PRIVATE) KEY|AKIA[0-9A-Z]{16}' >/dev/null; then
  log "reject: diff looks like a secret"
  outcome="fail: diff looks like a secret"
  reject_target "$target" "$concern"
  undo_agent_files
  agent_started=0
  exit 1
fi

if ! make -j1 clean || ! make -j1; then
  log "fail: build failed"
  outcome="fail: build failed"
  undo_agent_files
  agent_started=0
  exit 1
fi

set +e
./led -t >"$STATE/test.log" 2>&1
set -e
summary="$(grep -E 'Test summary: [0-9]+ passed, [0-9]+ failed' "$STATE/test.log" | tail -1 || true)"
passed="$(sed -n 's/Test summary: \([0-9][0-9]*\) passed, \([0-9][0-9]*\) failed/\1/p' <<<"$summary")"
failed="$(sed -n 's/Test summary: \([0-9][0-9]*\) passed, \([0-9][0-9]*\) failed/\2/p' <<<"$summary")"
if [[ -z "$passed" || -z "$failed" || "$failed" != "0" ]] || ! grep -q 'ALL TESTS PASSED' "$STATE/test.log"; then
  log "fail: tests ${summary:-missing}"
  outcome="fail: tests ${summary:-missing}"
  undo_agent_files
  agent_started=0
  exit 1
fi
ran=$((passed + failed))

extra=""
while IFS= read -r f; do
  [[ -n "$f" ]] || continue
  if ! module_ok "$f"; then
    extra="$f"
    break
  fi
done < <(git diff --name-only HEAD)
if [[ -n "$extra" ]]; then
  log "fail: extra tracked edit ${extra}; not committing"
  outcome="fail: extra tracked edit ${extra}"
  undo_agent_files
  agent_started=0
  exit 1
fi

timeout 180 python3 tools/quality_jev.py score "$target" --save "$STATE/after.json" >"$STATE/score-after.txt" || true
{
  read -r qual_after
  read -r health_after
  read -r verse_move
} < <(python3 - "$STATE/after.json" "$target" "$qual_before" "$health_before" "$verse_before" <<'PY'
import importlib.util
import json
import sys
path, target, qb, hb, vb = sys.argv[1:]
spec = importlib.util.spec_from_file_location("qj", "tools/quality_jev.py")
qj = importlib.util.module_from_spec(spec)
spec.loader.exec_module(qj)
try:
    results = json.load(open(path)).get("results", [])
except Exception:
    results = []
hit = next((r for r in results if r.get("file") == target), None)

def num(text):
    try:
        return float(text)
    except (TypeError, ValueError):
        return None

if not hit:
    print(qb)
    print(hb)
    print(qj.verse_move(vb, "", num(qb), None).replace("\n", " "))
    raise SystemExit
q = hit.get("quality_F")
h = hit.get("health")
print(qb if q is None else f"{float(q):.1f}")
print(hb if h is None else f"{float(h):.3f}".rstrip("0").rstrip("."))
print(qj.verse_move(vb, hit.get("interpretation"), num(qb), q).replace("\n", " "))
PY
)
log "verse ${verse_move:-absent}"

what="one measured change"
if [[ -f "$WHAT_FILE" ]]; then
  what="$(head -1 "$WHAT_FILE" | tr '|' ' ' | tr -d '\r' | sed 's/[[:space:]]\+/ /g; s/^ //; s/ $//')"
fi
if [[ -z "$what" || "$what" == "SKIP" ]]; then
  what="one measured change in ${target}"
fi
what="${what:0:90}"
case "$what" in
  major:*|minor:*) ;;
  *) what="${change_class}: ${what}" ;;
esac
stat="$(git diff --numstat | awk '{a+=$1;b+=$2} END {printf "(+%d/-%d)", a+0, b+0}')"
case "$what" in
  *"(+"*) ;;
  *) what="${what} ${stat}" ;;
esac

grep -v '^$' "$STATE/agent-files" | LC_ALL=C sort -u >"$STATE/agent-files.sorted"
mv "$STATE/agent-files.sorted" "$STATE/agent-files"
files="$(paste -sd, "$STATE/agent-files")"
git add --pathspec-from-file="$STATE/agent-files"
git commit -m "$what"
code_hash="$(git rev-parse --short HEAD)"

verse_field="$(printf '%s' "${verse_move:-}" | tr '|' '/' | tr -d '\r')"
if [[ -n "$verse_field" ]]; then
  printf '%s | %s | %s | %s | qual %s->%s | health %s->%s | tests %s/%s | verse %s\n' \
    "$today" "$code_hash" "$files" "$what" \
    "$qual_before" "$qual_after" "$health_before" "$health_after" \
    "$passed" "$ran" "$verse_field" >>path.log
else
  printf '%s | %s | %s | %s | qual %s->%s | health %s->%s | tests %s/%s\n' \
    "$today" "$code_hash" "$files" "$what" \
    "$qual_before" "$qual_after" "$health_before" "$health_after" \
    "$passed" "$ran" >>path.log
fi
day_logged=1
git add path.log
git commit -m "path.log: ${today} ${what}"
committed=1
rm -f "$WHAT_FILE"

git rev-parse HEAD >"$STATE/pending-push"
git push origin HEAD
rm -f "$STATE/pending-push"
log "pushed ${code_hash} ${what}"
