# Upstream sync attempt

Attempted to integrate latest commits from the original upstream repository into this fork.

## What was attempted

1. Added upstream remote:
   - `https://github.com/moonlight-stream/moonlight-qt.git`
2. Tried to fetch upstream refs.
3. Tried SSH fallback to read upstream refs.

## Result

Sync could not be completed in this environment because outbound network access to GitHub is blocked:

- HTTPS fetch failed with `CONNECT tunnel failed, response 403`
- SSH fetch failed with `connect to host github.com port 22: Network is unreachable`

## Next step (outside this environment)

Run these commands from a machine with GitHub access:

```bash
git remote add upstream https://github.com/moonlight-stream/moonlight-qt.git
git fetch upstream --prune
git checkout work
git merge --ff-only upstream/master  # or upstream/main depending on default branch
```
