# Continuous Integration (CI) Documentation

This project uses GitHub Actions for continuous integration and automated testing. The CI pipeline ensures code quality, runs tests, and builds firmware automatically.

## CI Workflows

### 1. CI Workflow (`.github/workflows/ci.yml`)

Runs on every push to `main` and `develop` branches, and on all pull requests.

**Jobs:**

#### Build Firmware
- Checks out code
- Sets up Python and PlatformIO
- Creates dummy secrets file from `secrets.example.h`
- Builds firmware for `firebeatleV2` environment
- Uploads firmware binary as artifact (retained for 30 days)
- Uses caching to speed up subsequent builds

#### Run Unit Tests
- Checks out code
- Sets up Python and PlatformIO
- Installs code coverage tools (lcov)
- Runs native unit tests using the `native` environment with coverage
- Publishes test summary to GitHub Actions summary page
- Generates coverage report using lcov
- Uploads coverage to Codecov
- Uploads coverage report and test results as artifacts (retained for 30 days)
- Tests always run (even if other jobs fail)

#### Code Quality Checks
- Verifies `src/secret.h` is not committed (security check)
- Checks for large files that shouldn't be in the repository
- Verifies all required files are present
- Fast execution (no dependencies to install)

#### C++ Linting
- Checks out code
- Installs clang-format and cppcheck
- Checks C++ code formatting with clang-format
- Runs cppcheck static analysis for warnings, performance, and portability issues
- Uploads lint results as artifact (retained for 30 days)
- Helps maintain code quality and consistency

**Triggers:**
- Push to `main` or `develop` branches
- Pull requests targeting `main` or `develop`
- Manual workflow dispatch

**Caching:**
- PlatformIO dependencies and build artifacts are cached
- Separate caches for build and test jobs for optimal performance

**Code Coverage:**
- Code coverage is measured using lcov and reported to Codecov
- Coverage data is available as a downloadable artifact
- Codecov integration requires setting up a `CODECOV_TOKEN` repository secret
- To set up Codecov:
  1. Sign up at https://codecov.io with your GitHub account
  2. Add your repository to Codecov
  3. Copy the Codecov token
  4. Go to GitHub repository Settings → Secrets and variables → Actions
  5. Add a new secret named `CODECOV_TOKEN` with the token value

### 2. Release Workflow (`.github/workflows/release.yml`)

Runs when a new release is created or a version tag is pushed.

**Jobs:**

#### Build Release Firmware
- Builds firmware for both environments:
  - `firebeatleV2` - Standard serial upload
  - `firebeatleV2_ota` - OTA (Over-The-Air) upload
- Renames binaries for clarity
- Automatically attaches firmware binaries to GitHub releases

**Triggers:**
- Release creation
- Tags matching pattern `v*.*.*` (e.g., `v1.0.0`, `v2.1.3`)

## Viewing CI Status

### Status Badge
The README displays status badges showing the current build status, code coverage, and license:

[![CI](https://github.com/tipih/12VBatteryMonitor/actions/workflows/ci.yml/badge.svg)](https://github.com/tipih/12VBatteryMonitor/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/tipih/12VBatteryMonitor/branch/main/graph/badge.svg)](https://codecov.io/gh/tipih/12VBatteryMonitor)
[![License](https://img.shields.io/github/license/tipih/12VBatteryMonitor)](LICENSE)

### GitHub Actions Tab
1. Navigate to the "Actions" tab in the GitHub repository
2. Select a workflow from the left sidebar
3. Click on a specific workflow run to view logs and results

## Running CI Locally

You can replicate CI checks on your local machine before pushing:

### Build Firmware
```bash
pio run -e firebeatleV2
```

### Run Unit Tests
```bash
pio test -e native -v
```

### Run Code Coverage Locally
```bash
# Install lcov (Ubuntu/Debian)
sudo apt-get install lcov

# Run tests with coverage
pio test -e native -v

# Generate coverage report
lcov --capture --directory .pio/build/native --output-file coverage.info --no-external
lcov --list coverage.info
```

### Run C++ Linting Locally
```bash
# Install tools (Ubuntu/Debian)
sudo apt-get install clang-format cppcheck

# Check formatting
find src test -name "*.cpp" -o -name "*.h" -o -name "*.hpp" | while read file; do
  clang-format --dry-run --Werror "$file"
done

# Run static analysis
cppcheck --enable=warning,performance,portability \
         --inline-suppr \
         --suppress=missingIncludeSystem \
         --std=c++11 \
         src/
```

### Code Quality Checks
```bash
# Check if secrets file exists (should not be in repo)
if [ -f "src/secret.h" ]; then
  echo "ERROR: src/secret.h should not be committed!"
  exit 1
fi
echo "✓ No secrets file found"

# Check for large files (warning only - some large files may be legitimate)
large_files=$(find . -type f -size +1M -not -path "./.git/*" -not -path "./.pio/*")
if [ -n "$large_files" ]; then
  echo "Warning: Large files found:"
  echo "$large_files"
  # Note: This is a warning only. Review if files should be excluded or use Git LFS
fi
```

## Artifacts

CI generates artifacts that can be downloaded:

- **firmware** - Built firmware binary (`.bin` file)
- **coverage-report** - Code coverage data (`coverage.info`) and test output
- **lint-results** - Static analysis report from cppcheck

Artifacts are retained for 30 days and can be downloaded from the workflow run page.

## Troubleshooting CI Failures

### Build Failures
1. Check the build log in the "Build Firmware" job
2. Verify `platformio.ini` configuration is valid
3. Ensure all library dependencies are specified
4. Try building locally with `pio run -e firebeatleV2`

### Test Failures
1. Check the test log in the "Run Unit Tests" job
2. Review the test summary in the GitHub Actions summary page
3. Review which specific test failed
4. Run tests locally with `pio test -e native -v`
5. Fix the failing test or code

### Linting Failures
1. Check the lint log in the "C++ Linting" job
2. Download the lint-results artifact for detailed cppcheck report
3. For formatting issues, run `clang-format -i <file>` to auto-fix
4. For static analysis issues, review and fix the reported code issues

### Coverage Issues
1. View coverage report on Codecov (linked in README badge)
2. Download the coverage-report artifact for detailed data
3. Add tests for uncovered code paths

### Code Quality Check Failures
1. **Secrets file detected**: Remove `src/secret.h` from the repository (it should be in `.gitignore`)
2. **Large files detected**: Review if large files are necessary, consider using Git LFS or removing them
3. **Missing required files**: Ensure all required files are present in the repository

## Creating a Release

To trigger the release workflow:

### Using GitHub Releases UI
1. Go to "Releases" → "Draft a new release"
2. Choose or create a tag (e.g., `v1.0.0`)
3. Fill in release title and description
4. Publish release
5. Firmware binaries will be automatically attached

### Using Git Tags
```bash
git tag v1.0.0
git push origin v1.0.0
```

The release workflow will build and attach firmware binaries to the release.

## Workflow Customization

### Adding More Build Environments
Edit `.github/workflows/ci.yml` and add additional `pio run` commands:
```yaml
- name: Build firmware (custom_env)
  run: pio run -e custom_env
```

### Modifying Test Configuration
Tests are configured in `platformio.ini` under the `[env:native]` section.

### Cache Management
Caches are automatically managed by GitHub Actions. To force a cache refresh:
1. Update `platformio.ini` (changes cache key automatically)
2. Or manually clear caches in repository settings

## Best Practices

1. **Always run tests locally** before pushing
2. **Keep CI fast** - use caching effectively
3. **Monitor CI status** - don't ignore failures
4. **Review artifacts** - download and test firmware binaries
5. **Keep secrets out** - never commit credentials or sensitive data

## Support

If you encounter CI issues:
1. Check the Actions tab for detailed logs
2. Review this documentation
3. Open an issue with relevant workflow run URL
4. Check PlatformIO and GitHub Actions documentation
