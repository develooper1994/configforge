#!/usr/bin/env bash
# Puts run.sh's Markdown on a pull request as one comment, updated on
# every push rather than added again: the comment that begins with
# run.sh's marker and was written by the action's own account is the
# action's own. The marker alone is not enough: anyone may comment, and a
# comment of theirs that began with it would be taken for Doctor's, which
# the token cannot edit - so Doctor's findings would never be posted and
# a forged "all clear" would stand in their place.
#
#   GH_TOKEN      a token that may write the pull request's comments
#   CF_REPO       owner/name
#   CF_PR         the pull request's number
#   CF_MARKDOWN   the file run.sh wrote
#   CF_COMMENT_AUTHOR   the login the token comments as; when empty the
#                 token is asked, and a token that cannot say is the
#                 workflow's, github-actions[bot]
#
# A pull request from a fork gets a token that cannot write; the comment
# is then skipped with a note, and the job summary still has it all.
set -u

: "${CF_REPO:?owner/name}"
: "${CF_PR:?the pull request number}"
: "${CF_MARKDOWN:?the Markdown to post}"
case "$CF_PR" in
  '' | *[!0-9]*)
    echo "doctor: '$CF_PR' is not a pull request number" >&2
    exit 2
    ;;
esac
# Whose comments are the action's: the login given, else the token's own
# (a personal token can say who it is), else the workflow token's, which
# cannot.
author="${CF_COMMENT_AUTHOR:-}"
if [ -z "$author" ]; then
  author="$(gh api user --jq .login 2>/dev/null)" || author=""
fi
author="${author:-github-actions[bot]}"
case "$author" in
  *[!A-Za-z0-9\[\]-]*)
    echo "doctor: '$author' is not a login" >&2
    exit 2
    ;;
esac
marker="<!-- configforge-doctor -->"

id="$(gh api --paginate "repos/$CF_REPO/issues/$CF_PR/comments" \
  --jq ".[] | select((.user.login == \"$author\") and (.body | startswith(\"$marker\"))) | .id" |
  head -n 1)"
if [ -n "$id" ]; then
  gh api -X PATCH "repos/$CF_REPO/issues/comments/$id" -F "body=@$CF_MARKDOWN" >/dev/null
else
  gh api -X POST "repos/$CF_REPO/issues/$CF_PR/comments" -F "body=@$CF_MARKDOWN" >/dev/null
fi || {
  echo "doctor: could not write the comment (a fork's token cannot); the job summary has it" >&2
  exit 0
}
if [ -n "$id" ]; then verb=updated; else verb=posted; fi
echo "doctor: $verb the comment on #$CF_PR"
