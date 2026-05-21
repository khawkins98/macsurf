#!/usr/bin/env bash
# scripts/setup-github.sh
#
# Idempotently provision the mplsllc/macsurf repository's GitHub scaffolding:
#   - Enable Issues / Wiki / Discussions / Projects
#   - Create the full label set (Type / Status / Priority / Severity / Area)
#   - Create the roadmap milestones
#
# Issue templates live in .github/ISSUE_TEMPLATE/ and are managed by git;
# running this script does NOT touch them.
#
# Usage:  bash scripts/setup-github.sh
#         REPO=other-org/other-repo bash scripts/setup-github.sh
#
# Requires: gh >= 2.20, authenticated with repo+admin:repo scopes.

set -euo pipefail

REPO="${REPO:-mplsllc/macsurf}"

# ----- prerequisites -----
command -v gh >/dev/null 2>&1 || {
  echo "ERROR: gh CLI not installed." >&2
  exit 1
}
gh auth status >/dev/null 2>&1 || {
  echo "ERROR: gh not authenticated. Run 'gh auth login'." >&2
  exit 1
}
gh repo view "$REPO" >/dev/null 2>&1 || {
  echo "ERROR: cannot access $REPO. Check the repo name and your auth scopes." >&2
  exit 1
}

echo "==> Provisioning $REPO"

# ----- repo features -----
echo "--> Enabling repo features (issues/wiki/discussions/projects)"
gh api -X PATCH "repos/$REPO" \
  -F has_issues=true \
  -F has_wiki=true \
  -F has_discussions=true \
  -F has_projects=true \
  --jq '"   features now: issues=\(.has_issues) wiki=\(.has_wiki) discussions=\(.has_discussions) projects=\(.has_projects)"'

# ----- labels -----
echo "--> Creating / updating labels"
# Each line: NAME|COLOR|DESCRIPTION
LABELS=$(cat <<'LABELS_EOF'
bug|d73a4a|Something isn't working
enhancement|a2eeef|New feature or request
documentation|0075ca|Improvements or additions to documentation
question|d876e3|Further information is requested
discussion|cc317c|Open-ended discussion or design topic
confirmed|fbca04|Reproduced and validated
needs-repro|d4c5f9|Cannot yet reproduce; more info needed
blocked|000000|Blocked on external work or decision
wontfix|ffffff|This will not be worked on
duplicate|cfd3d7|This issue or pull request already exists
regression|e99695|Previously worked; now broken
priority: high|b60205|Should ship in current release
priority: medium|fbca04|Targeted for an upcoming release
priority: low|0e8a16|Nice-to-have / backlog
severity: crash|b60205|Crashes the browser or freezes the OS
severity: major|d93f0b|Substantial loss of function but recoverable
severity: minor|fef2c0|Small visual or behavioral issue
area: network|1d76db|Fetcher, sockets, TLS, OpenTransport
area: rendering|0e8a16|Layout, plotters, QuickDraw, fonts
area: ui|5319e7|Carbon windows, menus, address bar, scrolling
area: build|bfd4f2|CW8, Retro68, scripts, packaging
area: js-engine|c5def5|Duktape, DOM bindings, script execution
area: css|bfdadc|libcss, cssh, cascade, selectors, units
good first issue|7057ff|Good for newcomers
help wanted|008672|Extra attention is needed
LABELS_EOF
)

while IFS='|' read -r name color desc; do
  [ -z "$name" ] && continue
  gh label create "$name" --color "$color" --description "$desc" --repo "$REPO" --force >/dev/null
  printf "    %s\n" "$name"
done <<< "$LABELS"

# ----- milestones -----
echo "--> Creating milestones (only if missing)"
existing_titles=$(gh api "repos/$REPO/milestones?state=all" --jq '.[].title' | tr '\n' '|')

create_ms() {
  local title="$1"
  local desc="$2"
  if echo "|$existing_titles" | grep -q "|$title|"; then
    printf "    %s (exists)\n" "$title"
  else
    gh api "repos/$REPO/milestones" -f title="$title" -f description="$desc" --jq '"    \(.title) (created #\(.number))"'
  fi
}

create_ms "v0.1a2" "Modern-site survival release. Heavy modern pages (apple, github, wikipedia, yahoo, MDN) load partially and reliably without crashing. Images may be skipped or placeholdered; huge stylesheets may be capped."
create_ms "v0.1a3" "Stability + polish after 0.1a2. Deferred wheel-mouse crash, image-OOM tail-cases, logger format hygiene, partial-load UX."
create_ms "v0.2"   "HTTPS via macSSL, CSS Grid V2 alignment (justify/align), JavaScript expansion beyond ES5 baseline."
create_ms "v1.0"   "Public beta release. Feature-complete for daily-driver use on Mac OS 9 against the modern web."

echo "==> Done."
