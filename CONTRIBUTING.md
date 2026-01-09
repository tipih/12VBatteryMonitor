# Contributing to 12V Battery Monitor

Thanks for your interest in contributing! Contributions small and large are welcome.

## Reporting Issues

- Open a new issue describing the problem or feature request.
- Include steps to reproduce, expected vs actual behavior, and device/firmware versions if applicable.

## Proposing Changes (Pull Requests)

1. Fork the repository and clone your fork.
2. Create a feature branch from `main` named like `feat/describe-thing` or `fix/issue-123`.
3. Make small, focused changes and commit with clear messages (see Commit Message format).
4. Build and test locally (see Quick checks below).
5. Push your branch and open a Pull Request against `main` with a clear description and rationale.

## Commit Message Format

- Use short, descriptive messages. Examples:
  - `feat: add BLE command for setting capacity`
  - `fix: prevent divide-by-zero in ocv_estimator`
- Optionally reference an issue: `fix: ... (#12)`

## Quick Checks (before opening a PR)

- Build the firmware with PlatformIO to ensure no compilation errors:

```bash
pio run
```

- Run native unit tests locally:

```bash
pio test -e native
```

- Run upload/test steps you typically use (USB or OTA):

```bash
pio run --target upload
# or
pio run -e firebeatleV2_ota -t upload --upload-port <device-ip>
```

- All pull requests automatically run through our CI pipeline which includes:
  - Firmware build verification
  - Native unit tests
  - Code quality checks
  - Secrets file verification

## Coding Style

- Keep changes focused and readable. Follow existing naming and formatting conventions in `src/`.
- Avoid large refactors in the same PR as functional changes; split them into separate PRs where possible.

## PR Checklist

- [ ] Code builds without errors (`pio run`).
- [ ] Native tests pass (`pio test -e native`).
- [ ] CI pipeline passes (automatically checked on PR).
- [ ] Changes include a short description and reasoning in the PR body.
- [ ] Sensitive information (passwords, keys) is not committed — use `secrets.h` excluded from source control.
- [ ] Update documentation (`README.md` or `docs/`) if the change affects usage.

## Review Process

- Maintainers may request changes; please address them in follow-up commits to the same branch.
- Once approved, a maintainer will merge the PR.

## Thank you

We appreciate your help making this project better! If you're unsure where to start, open an issue and we'll suggest a good first task.
