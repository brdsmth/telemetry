# Agent Guidelines

## Commits

All commits follow the [Conventional Commits](https://www.conventionalcommits.org/) format:

```
type(scope): description
```

- **type** is one of `feat`, `fix`, `refactor`, `chore`, `docs`, `test`, `perf`, `ci`, `build`, `style`.
- **scope** is the area of the repo the change touches. Use the existing scopes: `api`, `app`, `gateway`, `sensor`, `lib`, `infra`, `db`, `ingester`, `dev`. Add a new scope only when a change clearly belongs to a new area.
- **description** is a short, lowercase, imperative summary with no trailing period.

Examples from this repo:

```
feat(gateway): gnss connection and reporting
feat(app): support for resetting bluetooth connections
fix(api): handle missing device id on ingest
```

### Keep commits manageable and modular

- **One logical change per commit.** A commit should do one thing and be describable in a single line. If the description needs "and", split it.
- **Split by scope.** A change that touches `gateway` and `api` should usually be two commits, one per scope, unless the pieces cannot function independently.
- **Separate refactors from behavior changes.** Move or rename code in its own `refactor` commit, then make the functional change in a `feat` or `fix` commit.
- **Keep unrelated cleanup out.** Formatting fixes, dependency bumps, and drive-by tweaks go in their own `chore` or `style` commits.
- **Every commit should build.** Do not leave the tree in a broken state between commits in a series.
- **Commit as you go.** Stage and commit each finished unit of work rather than accumulating a large diff and committing it all at once.

Use a commit body only when the one-line description is not enough to explain why the change was made.
