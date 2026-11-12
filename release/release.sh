#!/bin/bash
# Copyright Andy Wick
# SPDX-License-Identifier: Apache-2.0
#
# Drive an Arkime release from start to finish.
#   release/release.sh 6.9.2 [iteration]
# Re-runnable: it works out which steps are already done and resumes.

set -euo pipefail

REPO=arkime/arkime
WORKFLOW=release.yml
RELEASE_FILES="CHANGELOG configure.ac capture/arkime.h"
HOMEBREW_DIR=${HOMEBREW_ARKIME:-$HOME/homebrew-arkime}
POLL_SECS=30

die() { echo "ERROR - $*" >&2; exit 1; }
info() { echo "==> $*"; }

ask() {
  local reply
  read -r -p "$* [y/N] " reply < /dev/tty || die "Couldn't read from the terminal"
  [[ $reply =~ ^[Yy] ]]
}

confirm() {
  ask "$@" || die "Stopped at: $*  (re-run to resume)"
}

ac_version() {
  perl -ne 'if (/^AC_INIT\(\[arkime\],\s*\[([^\]]+)\]\)/) { print $1; exit }'
}

tracked_dirty() {
  git status --porcelain --untracked-files=no
}

# Fully staged files are allowed, they go in the release commit
other_dirty() {
  tracked_dirty | grep -vE '^(.. (CHANGELOG|configure\.ac|capture/arkime\.h)|[MADRC]  .*)$' || true
}

changelog_section() {
  V="$VERSION" perl -ne '
    if (/^\Q$ENV{V}\E \d{4}\//) { $in = 1; next }
    last if $in && (/^\s*$/ || /^\d+\.\d+\.\d+\S* \d{4}\//);
    print if $in;
  ' CHANGELOG
}

# Escaped for markdown, so <code> and friends in the CHANGELOG aren't treated as HTML
notes_section() {
  changelog_section | perl -pe 's/([<*~])/\\$1/g'
}

######################################################################
preflight() {
  local t url
  for t in git gh perl curl shasum; do
    command -v "$t" > /dev/null || die "Required tool '$t' not found"
  done
  [ -r /dev/tty ] || die "Needs an interactive terminal"

  cd "$(dirname "$0")/.."
  cd "$(git rev-parse --show-toplevel)" || die "Not inside a git checkout"
  url=$(git remote get-url origin 2> /dev/null) || die "No origin remote"
  [[ $url =~ [:/]arkime/arkime(\.git)?/?$ ]] || die "origin is '$url', expected $REPO"
  [ -n "$(ac_version < configure.ac)" ] || die "configure.ac doesn't have an AC_INIT([arkime],[...]) line"
  grep -qE '^[[:space:]]+release_tag:' ".github/workflows/$WORKFLOW" || die "$WORKFLOW no longer has a release_tag input"

  gh auth status > /dev/null 2>&1 || die "gh is not logged in, run: gh auth login"

  local cur
  cur=$(git branch --show-current)
  if [ -n "${RELEASE_BASE:-}" ]; then
    BASE=$RELEASE_BASE
  elif [[ $cur =~ ^(main|dev[0-9]+)$ ]]; then
    BASE=$cur
  else
    BASE=$(gh pr list -R "$REPO" --head "$BRANCH" --state all --limit 1 --json baseRefName --jq '.[0].baseRefName // ""') || die "gh pr list failed"
    [ -n "$BASE" ] || die "On branch '$cur', run from main or devN, or set RELEASE_BASE"
  fi
  info "Releasing $TAG from $BASE"

  info "Fetching origin"
  git fetch origin || die "git fetch failed"
  git rev-parse --verify --quiet "origin/$BASE" > /dev/null || die "origin/$BASE not found"
}

######################################################################
detect_stage() {
  local mainVersion prState
  mainVersion=$(git show "origin/$BASE:configure.ac" | ac_version)

  if [ -n "$(git ls-remote --tags origin "refs/tags/$TAG")" ]; then
    STAGE=4
    info "Tag $TAG is already on origin, resuming at the release workflow"
    return
  fi

  if [ "$mainVersion" = "$VERSION" ]; then
    STAGE=3
    info "origin/$BASE is already at $VERSION, resuming at tagging"
    return
  fi

  prState=$(gh pr list -R "$REPO" --head "$BRANCH" --state all --limit 1 --json state --jq '.[0].state // ""') || die "gh pr list failed"
  case "$prState" in
  OPEN)
    STAGE=2
    info "PR for $BRANCH is already open, resuming at waiting for the merge"
    ;;
  MERGED)
    die "PR for $BRANCH is merged but origin/$BASE configure.ac has '$mainVersion' not '$VERSION'"
    ;;
  CLOSED)
    die "PR for $BRANCH was closed without merging, reopen it or delete the branch and re-run"
    ;;
  *)
    STAGE=1
    ;;
  esac
}

######################################################################
update_changelog() {
  local today line first count
  today=$(date +%Y/%m/%d)

  count=$(grep -cE "^${VERSION_RE} [0-9]{4}/[0-9x]{2}/[0-9x]{2}\$" CHANGELOG || true)
  [ "$count" = 1 ] || die "CHANGELOG needs exactly one '$VERSION YYYY/MM/DD' line, found $count"
  line=$(grep -E "^${VERSION_RE} [0-9]{4}/" CHANGELOG)
  first=$(grep -m1 -E '^[0-9]+\.[0-9]+\.[0-9]+[^ ]* [0-9]{4}/' CHANGELOG)
  [ "$first" = "$line" ] || die "CHANGELOG's newest entry is '$first', not $VERSION"
  [ "$(changelog_section | grep -c '^  - ' || true)" -gt 0 ] || die "CHANGELOG has no entries under $VERSION"

  if [ "$line" = "$VERSION $today" ]; then
    info "CHANGELOG already says '$line'"
    return
  fi
  if [[ $line != *x* ]]; then
    confirm "CHANGELOG says '$line', change the date to $today?"
  fi
  V="$VERSION" D="$today" perl -pi -e 's{^\Q$ENV{V}\E \d{4}/[\dx]{2}/[\dx]{2}$}{$ENV{V} $ENV{D}}' CHANGELOG
  grep -qxF "$VERSION $today" CHANGELOG || die "Failed to set the CHANGELOG date for $VERSION"
  info "CHANGELOG: '$line' -> '$VERSION $today'"
}

update_configure() {
  local cur
  cur=$(ac_version < configure.ac)
  if [ "$cur" = "$VERSION" ]; then
    info "configure.ac already at $VERSION"
    return
  fi
  if [ "$cur" != "$VERSION-GIT" ] && [ "$cur" != "${VERSION%%-*}-GIT" ]; then
    confirm "configure.ac has '$cur', expected '${VERSION%%-*}-GIT'. Set it to $VERSION anyway?"
  fi
  V="$VERSION" perl -pi -e 's/^AC_INIT\(\[arkime\],\s*\[[^\]]+\]\)/AC_INIT([arkime],[$ENV{V}])/' configure.ac
  [ "$(ac_version < configure.ac)" = "$VERSION" ] || die "Failed to set AC_INIT in configure.ac"
  info "configure.ac: $cur -> $VERSION"
}

check_api_version() {
  local prev cur old
  cur=$(perl -ne 'print $1 if /^#define ARKIME_API_VERSION\s+(\d+)/' capture/arkime.h)
  [ -n "$cur" ] || die "Couldn't find ARKIME_API_VERSION in capture/arkime.h"
  prev=$(git describe --tags --abbrev=0 --match 'v[0-9]*' "origin/$BASE" 2> /dev/null || true)

  echo
  echo "ARKIME_API_VERSION is $cur"
  if [ -n "$prev" ]; then
    old=$(git show "$prev:capture/arkime.h" | perl -ne 'print $1 if /^#define ARKIME_API_VERSION\s+(\d+)/')
    echo "  $prev had $old"
    if git diff --quiet "$prev" -- capture/arkime.h; then
      echo "  capture/arkime.h is unchanged since $prev"
    else
      echo "  capture/arkime.h changed since $prev:"
      git --no-pager diff --stat "$prev" -- capture/arkime.h | sed 's/^/  /'
      echo "  review with: git diff $prev -- capture/arkime.h"
    fi
  fi
  echo "Bump it now in capture/arkime.h if there are incompatible C API changes."
  confirm "Is ARKIME_API_VERSION correct for $VERSION?"
}

prepare_branch() {
  local cur dirty msg remoteSha
  msg="$TAG [skip ci]"
  cur=$(git branch --show-current)
  dirty=$(other_dirty)
  [ -z "$dirty" ] || die "Working tree has other modified files, stash them or 'git add' them to include them in the release commit:
$dirty"

  if [ "$cur" = "$BASE" ]; then
    git rev-parse --verify --quiet "refs/heads/$BRANCH" > /dev/null && die "Local branch $BRANCH already exists, 'git checkout $BRANCH' and re-run to resume or delete it"
    [ -z "$(git ls-remote --heads origin "refs/heads/$BRANCH")" ] || die "Branch $BRANCH already exists on origin without a PR, check it out and re-run or delete it"
    [ "$(git rev-parse HEAD)" = "$(git rev-parse "origin/$BASE")" ] || die "Local $BASE is not at origin/$BASE, git pull (or push) first"
    info "Creating branch $BRANCH"
    git checkout -b "$BRANCH"
  elif [ "$cur" = "$BRANCH" ]; then
    info "Already on $BRANCH, resuming"
    git merge-base --is-ancestor "origin/$BASE" HEAD || die "$BRANCH is not based on the current origin/$BASE"
  else
    die "On branch '$cur', expected $BASE (or $BRANCH to resume)"
  fi

  update_changelog
  update_configure

  # shellcheck disable=SC2086
  if git diff --quiet HEAD -- $RELEASE_FILES && git diff --cached --quiet; then
    [ "$(git log -1 --format=%s)" = "$msg" ] || die "Nothing to commit and HEAD isn't the '$msg' commit"
    info "Release commit already made"
  else
    check_api_version
    # shellcheck disable=SC2086
    git add -- $RELEASE_FILES
    echo
    # shellcheck disable=SC2086
    git --no-pager diff --cached -- $RELEASE_FILES
    git --no-pager diff --cached --stat
    confirm "Commit all of the above files as '$msg'?"
    git commit -m "$msg" || die "git commit failed"
  fi
  [ -z "$(tracked_dirty)" ] || die "Working tree still dirty after the commit"

  remoteSha=$(git ls-remote --heads origin "refs/heads/$BRANCH" | cut -f1)
  if [ "$remoteSha" = "$(git rev-parse HEAD)" ]; then
    info "$BRANCH already pushed"
  else
    confirm "Push $BRANCH to origin?"
    git push -u origin "$BRANCH" || die "git push failed"
  fi

  confirm "Open a PR for $BRANCH against $BASE?"
  gh pr create -R "$REPO" --base "$BASE" --head "$BRANCH" --title "$msg" --body "" || die "gh pr create failed"
}

######################################################################
wait_for_merge() {
  local url state
  url=$(gh pr view "$BRANCH" -R "$REPO" --json url --jq .url) || die "Couldn't find the PR for $BRANCH"
  info "Merge $url (this script never merges it), checking every ${POLL_SECS}s, ctrl-c and re-run is fine"
  while true; do
    state=$(gh pr view "$BRANCH" -R "$REPO" --json state --jq .state 2> /dev/null || echo UNKNOWN)
    case "$state" in
    MERGED)
      info "PR merged"
      return
      ;;
    CLOSED)
      die "PR $url was closed without merging"
      ;;
    esac
    sleep "$POLL_SECS"
  done
}

######################################################################
tag_main() {
  local dirty v head
  dirty=$(tracked_dirty)
  [ -z "$dirty" ] || die "Working tree has modified files, can't switch to $BASE:
$dirty"

  info "git checkout $BASE && git pull"
  git checkout "$BASE" || die "git checkout $BASE failed"
  git pull --ff-only origin "$BASE" || die "git pull failed"

  v=$(ac_version < configure.ac)
  [ "$v" = "$VERSION" ] || die "configure.ac on $BASE has '$v', expected '$VERSION'. Was the PR merged?"
  grep -qE "^${VERSION_RE} [0-9]{4}/[0-9]{2}/[0-9]{2}\$" CHANGELOG || die "CHANGELOG on $BASE has no dated $VERSION entry"

  head=$(git rev-parse HEAD)
  if git rev-parse --verify --quiet "refs/tags/$TAG" > /dev/null; then
    [ "$(git rev-parse "refs/tags/$TAG^{commit}")" = "$head" ] || die "Local tag $TAG exists but isn't $BASE's HEAD, fix with: git tag -d $TAG"
    info "Local tag $TAG already exists at HEAD"
  else
    echo
    git --no-pager log -1 --format='%h %s (%an, %ar)'
    confirm "Tag this commit as $TAG?"
    git tag "$TAG" || die "git tag failed"
  fi

  confirm "Push tag $TAG to origin?"
  git push origin "refs/tags/$TAG" || die "Pushing the tag failed"
}

######################################################################
watch_run() {
  local id=$1
  info "Watching https://github.com/$REPO/actions/runs/$id"
  gh run watch "$id" -R "$REPO" --exit-status --interval "$POLL_SECS" || die "Run $id failed or the watch was interrupted.
  See: gh run view $id -R $REPO --log-failed
  Rerun failed jobs with: gh run rerun $id -R $REPO --failed
  Then re-run this script"
  info "Release workflow finished"
}

run_workflow() {
  local v active assets start out id i
  git fetch origin "refs/tags/$TAG:refs/tags/$TAG" || die "Couldn't fetch tag $TAG, does a different local $TAG exist?"
  v=$(git show "refs/tags/$TAG:configure.ac" | ac_version)
  [ "$v" = "$VERSION" ] || die "Tag $TAG has configure.ac version '$v', expected '$VERSION'"

  active=$(gh run list -R "$REPO" --workflow "$WORKFLOW" --limit 10 --json databaseId,status --jq '[.[] | select(.status != "completed")][0].databaseId // ""') || die "gh run list failed"
  if [ -n "$active" ] && ask "Release workflow run $active is in progress, watch it instead of starting a new one?"; then
    watch_run "$active"
    return
  fi

  assets=$(gh release view "$TAG" -R "$REPO" --json assets --jq '.assets | length' 2> /dev/null || echo 0)
  if [ "$assets" -gt 0 ] && ask "Release $TAG already exists with $assets assets, skip the release workflow?"; then
    return
  fi

  confirm "Run $WORKFLOW with release_tag=$VERSION iteration=$ITERATION?"
  start=$(date -u +%Y-%m-%dT%H:%M:%SZ)
  out=$(gh workflow run "$WORKFLOW" -R "$REPO" --ref "$BASE" -f "release_tag=$VERSION" -f "iteration=$ITERATION" 2>&1) || die "gh workflow run failed: $out"
  echo "$out"

  id=""
  if [[ $out =~ actions/runs/([0-9]+) ]]; then
    id=${BASH_REMATCH[1]}
  fi
  i=0
  while [ -z "$id" ] && [ $i -lt 20 ]; do
    sleep 3
    id=$(gh run list -R "$REPO" --workflow "$WORKFLOW" --event workflow_dispatch --limit 5 --json databaseId,createdAt \
      --jq "[.[] | select(.createdAt >= \"$start\")][0].databaseId // \"\"" 2> /dev/null || true)
    i=$((i + 1))
  done
  [ -n "$id" ] || die "Started the workflow but couldn't find its run, check https://github.com/$REPO/actions/workflows/$WORKFLOW and re-run this script"
  watch_run "$id"
}

######################################################################
update_release_notes() {
  local tmp section pre
  tmp=$(mktemp -d)
  # shellcheck disable=SC2064
  trap "rm -rf '$tmp'" EXIT

  gh release view "$TAG" -R "$REPO" --json body --jq .body > "$tmp/body.md" || die "Release $TAG not found on GitHub, did the workflow finish?"
  notes_section > "$tmp/section.md"
  [ -s "$tmp/section.md" ] || die "No CHANGELOG entries found for $VERSION"

  if grep -qF -- "$(grep -m1 '^  - ' "$tmp/section.md")" "$tmp/body.md"; then
    info "Release notes already contain the CHANGELOG entries"
    cp "$tmp/body.md" "$tmp/new.md"
  elif grep -q "^# :sparkles: What's new" "$tmp/body.md"; then
    S="$tmp/section.md" perl -pe '
      if (!$done && /^# :sparkles: What.s new/) {
        open(my $fh, "<", $ENV{S}) || die "section: $!";
        local $/; $_ .= <$fh>; $done = 1;
      }' "$tmp/body.md" > "$tmp/new.md"
  else
    echo "WARNING - no \"What's new\" heading in the release body, putting the CHANGELOG entries first"
    cat "$tmp/section.md" <(echo) "$tmp/body.md" > "$tmp/new.md"
  fi

  if ask "Open the notes in ${EDITOR:-vi} first (e.g. to add an intro paragraph)?"; then
    ${EDITOR:-vi} "$tmp/new.md" < /dev/tty > /dev/tty || die "Editor failed"
  fi
  [ -s "$tmp/new.md" ] || die "Release notes are empty"

  echo "----------------------------------------------------------------------"
  cat "$tmp/new.md"
  echo "----------------------------------------------------------------------"
  if cmp -s "$tmp/body.md" "$tmp/new.md"; then
    info "Release notes unchanged"
  else
    confirm "Update the $TAG release notes with the above?"
    gh release edit "$TAG" -R "$REPO" --notes-file "$tmp/new.md" || die "gh release edit failed"
  fi

  pre=$(gh release view "$TAG" -R "$REPO" --json isPrerelease --jq .isPrerelease) || die "gh release view failed"
  if [ -n "$PRERELEASE" ]; then
    [ "$pre" = true ] || echo "WARNING - $TAG is a prerelease version but isn't marked prerelease on GitHub"
  elif [ "$pre" = true ] && ask "$TAG is marked prerelease, clear that flag?"; then
    if ask "Also mark $TAG as the Latest release?"; then
      gh release edit "$TAG" -R "$REPO" --prerelease=false --latest || die "gh release edit failed"
    else
      gh release edit "$TAG" -R "$REPO" --prerelease=false --latest=false || die "gh release edit failed"
    fi
  fi
  info "https://github.com/$REPO/releases/tag/$TAG"
}

######################################################################
hb() { git -C "$HOMEBREW_DIR" "$@"; }

update_homebrew() {
  local f msg dirty url
  msg="Bump to $TAG"
  [ -d "$HOMEBREW_DIR/.git" ] || die "$HOMEBREW_DIR is not a git checkout"
  [ -x "$HOMEBREW_DIR/bump.sh" ] || die "$HOMEBREW_DIR/bump.sh not found"
  url=$(hb remote get-url origin)
  [[ $url =~ [:/]arkime/homebrew-arkime(\.git)?/?$ ]] || die "$HOMEBREW_DIR origin is '$url'"
  [ "$(hb branch --show-current)" = main ] || die "$HOMEBREW_DIR is not on main"

  dirty=$(hb status --porcelain --untracked-files=no | grep -vE '^.. Formula/[^/]+\.rb$' || true)
  [ -z "$dirty" ] || die "$HOMEBREW_DIR has modified files:
$dirty"

  if [ -z "$(hb status --porcelain --untracked-files=no)" ]; then
    hb pull --quiet --ff-only origin main || die "git pull in $HOMEBREW_DIR failed"
  fi

  if grep -qF "tags/$TAG.tar.gz" "$HOMEBREW_DIR/Formula/arkime.rb" && [ -z "$(hb status --porcelain --untracked-files=no)" ]; then
    info "homebrew-arkime formulas already at $TAG"
  else
    confirm "Run bump.sh $VERSION in $HOMEBREW_DIR?"
    "$HOMEBREW_DIR/bump.sh" "$VERSION" || die "bump.sh failed"
    for f in "$HOMEBREW_DIR"/Formula/*.rb; do
      grep -qF "tags/$TAG.tar.gz" "$f" || die "$f wasn't updated to $TAG"
      grep -qE 'sha256 "[0-9a-f]{64}"' "$f" || die "$f has a bad sha256"
    done
    [ -n "$(hb status --porcelain --untracked-files=no)" ] || die "bump.sh changed nothing"
    hb --no-pager diff --stat
    confirm "Commit '$msg' in homebrew-arkime?"
    hb commit -am "$msg" || die "homebrew-arkime commit failed"
  fi

  hb fetch --quiet origin
  if [ "$(hb rev-parse HEAD)" = "$(hb rev-parse origin/main)" ]; then
    info "homebrew-arkime already pushed"
  else
    hb --no-pager log --oneline origin/main..HEAD
    confirm "Push homebrew-arkime main to origin?"
    hb push origin main || die "homebrew-arkime push failed"
  fi
}

######################################################################
main() {
  if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    echo "Usage: $0 <version> [iteration]"
    echo "Example: $0 6.9.2"
    exit 1
  fi

  VERSION=$1
  ITERATION=${2:-1}
  [[ $VERSION =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.]+)?$ ]] || die "Invalid version '$VERSION', expected e.g. 6.9.2 or 7.0.0-rc1 without a leading v"
  PRERELEASE=""
  if [[ $VERSION == *-* ]]; then PRERELEASE=1; fi
  [[ $ITERATION =~ ^[0-9]+$ ]] || die "Invalid iteration '$ITERATION', expected a number"
  VERSION_RE=${VERSION//./\\.}
  TAG="v$VERSION"
  BRANCH="release-$TAG"
  STAGE=1

  preflight
  detect_stage

  if [ "$STAGE" -le 1 ]; then prepare_branch; fi
  if [ "$STAGE" -le 2 ]; then wait_for_merge; fi
  if [ "$STAGE" -le 3 ]; then tag_main; fi
  run_workflow
  update_release_notes
  if [ -n "$PRERELEASE" ]; then
    info "Prerelease, leaving homebrew-arkime alone"
  else
    update_homebrew
  fi

  echo
  info "Release $TAG done"
  echo "Next: start the next version (new CHANGELOG entry and -GIT in configure.ac)"
}

main "$@"
