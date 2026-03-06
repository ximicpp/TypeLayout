Push the current branch to the remote repository.

## Important Notes

- On Windows, push from Windows git (not WSL) — HTTPS credentials are in Windows credential manager
- Always show `git log --oneline origin/main..HEAD` first so the user sees what will be pushed
- Default remote is `origin`, default branch is `main`

## Steps

1. Run `git log --oneline origin/main..HEAD` to show unpushed commits
2. If there are no unpushed commits, inform the user and stop
3. Push to remote: `git push origin main`
4. Report success
