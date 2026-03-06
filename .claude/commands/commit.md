Commit current changes to the TypeLayout repository.

## Commit Message Convention

This project uses the format: `type: description`

Valid types: `fix:`, `test:`, `docs:`, `chore:`, `perf:`, `feat:`, `refactor:`

## Steps

1. Run `git status` and `git diff --staged` and `git diff` to understand what changed
2. Run `git log --oneline -5` to see recent commit style
3. Analyze the changes and draft a commit message following the convention above
4. Stage the relevant files (prefer specific files over `git add .`)
5. Create the commit
6. Show the result with `git log --oneline -1`

## Important Notes

- NEVER push automatically. Git push must be done from Windows (WSL lacks credentials): `git push origin main`
- Ask the user to confirm the commit message before committing if the changes are complex
- Do not stage `.env`, credentials, or large binary files
- Use HEREDOC format for commit messages to preserve formatting
