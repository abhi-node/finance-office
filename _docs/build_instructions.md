# LibreOffice Build Instructions

This document contains the specific build instructions for LibreOffice on this macOS system.

## Prerequisites

The following prerequisites are already installed:
- **GNU Make 4.4.1** (via Homebrew)
- **OpenJDK 17** (via Homebrew at `/opt/homebrew/opt/openjdk@17`)
- **Xcode 16.2** with command line tools
- **Python 3.13.0**
- **autoconf, aclocal, pkg-config** (via Homebrew)

## Build Process

### 1. Navigate to Source Directory
```bash
cd /Users/abhijithbalagurusamy/Documents/GauntletAI/finance-office
```

### 2. Configure the Build
```bash
./autogen.sh
```

### 3. Build LibreOffice
```bash
/opt/homebrew/opt/make/libexec/gnubin/make
```

**Note**: Use `gmake` command if you have sourced the updated `.zshrc` file.

## Build Configuration

The `autogen.input` file contains:
```
--with-macosx-version-min-required=12.0
--enable-debug
--with-parallelism=8
--disable-cve-tests
--without-dotnet
--enable-bogus-pkg-config
--without-junit
--with-jdk-home=/opt/homebrew/opt/openjdk@17
```

## After Build Completion

Once the build finishes successfully, run LibreOffice with:
```bash
open instdir/LibreOfficeDev.app
```

## Additional Commands

- **Run unit tests**: `make check`
- **Show build help**: `make help`
- **Clean build**: `make clean`

## Environment Variables

The following are configured in `~/.zshrc`:
```bash
export PATH="/opt/homebrew/opt/make/libexec/gnubin:/opt/homebrew/opt/openjdk@17/bin:$PATH"
export JAVA_HOME="/opt/homebrew/opt/openjdk@17"
export MAKE="/opt/homebrew/opt/make/libexec/gnubin/make"
```

## Build Process Summary

✅ **Working Commands:**
1. `./autogen.sh` - Configure the build
2. `/opt/homebrew/opt/make/libexec/gnubin/make` - Build LibreOffice
   - Alternative: `gmake` (if `.zshrc` is sourced)

**Note**: The system `make` (3.81) causes Makefile syntax errors. Must use GNU Make 4.4.1 from Homebrew.