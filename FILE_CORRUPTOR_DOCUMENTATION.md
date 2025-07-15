# File Corruptor Utility

## Overview

The File Corruptor is a host-only test utility designed to introduce controlled corruption into files for testing archive integrity validation. It randomly corrupts exactly 5 bytes in a target file, making it ideal for testing CRC validation and error detection in archive processing tools.

## Purpose

This tool is specifically designed to test the robustness of archive processing systems by:
- Creating predictable corruption patterns for testing
- Validating that CRC checks properly detect corrupted archives
- Supporting automated testing of error handling paths
- Providing detailed corruption information for debugging

## Features

- **Controlled Corruption**: Always corrupts exactly 5 bytes
- **Random Positioning**: Selects unique random positions throughout the file
- **Detailed Reporting**: Shows exact positions and byte changes
- **Safety Limits**: Prevents corruption of files that are too small or too large
- **Cross-Platform**: Works on Windows, Linux, and macOS host systems

## Usage

### Basic Usage
```bash
./file_corruptor.exe <filename>
```

### Examples
```bash
# Corrupt a test archive
./file_corruptor.exe test_archive.lha

# Corrupt any file for testing
./file_corruptor.exe data_file.dat
```

## Building

### Build the File Corruptor
```bash
make build-file-corruptor TARGET=host
```

### Build the Test Suite
```bash
make build-file-corruptor-test TARGET=host
```

### Build All Host Tools
```bash
make TARGET=host
```

## Output Example

```
File Corruptor - Host Test Utility
==================================
Target file: test_archive.lha
File size: 1835853 bytes
Corrupting 5 random bytes...

Corruption Details:
===================
File: test_archive.lha
Size: 1835853 bytes
Bytes corrupted: 5

Corruption map:
  Position 27292: 0x98 -> 0xFB (decimal: 152 -> 251)
  Position 408: 0xFC -> 0x04 (decimal: 252 -> 4)
  Position 16808: 0x6E -> 0xF2 (decimal: 110 -> 242)
  Position 19874: 0x7B -> 0x57 (decimal: 123 -> 87)
  Position 13469: 0xDE -> 0x90 (decimal: 222 -> 144)

File corruption completed successfully!
Note: This file should now fail CRC checks.
```

## Safety Features

### File Size Limits
- **Minimum Size**: 5 bytes (must have enough bytes to corrupt)
- **Maximum Size**: 10MB (prevents processing of extremely large files)

### Corruption Rules
- **Unique Positions**: No two corruptions occur at the same position
- **Different Values**: Each corrupted byte is guaranteed to be different from the original
- **In-Place Modification**: Files are modified directly (make backups first!)

## Integration with Test Suite

The File Corruptor Test (`file_corruptor_test.c`) demonstrates complete integration:

1. **Baseline Test**: Verifies original archive passes integrity checks
2. **Corruption Test**: Uses the corruptor to introduce errors
3. **Validation Test**: Confirms corrupted archive fails integrity checks
4. **Results Summary**: Reports expected vs actual behavior

### Test Execution
```bash
cd build/host
./file_corruptor_test.exe
```

## Platform Compatibility

### Supported Platforms
- ✅ Windows (primary development platform)
- ✅ Linux (tested with GCC)
- ✅ macOS (tested with Clang)

### Not Supported
- ❌ Amiga (intentionally host-only for testing)

## Technical Details

### Architecture
- **Language**: C99 standard
- **Memory Management**: Static allocation only
- **Error Handling**: Comprehensive error checking and reporting
- **Random Generation**: Uses system time as seed for repeatability

### Implementation Notes
- Uses `fseek()` and `fread()`/`fwrite()` for precise byte manipulation
- Implements position uniqueness checking to avoid duplicate corruptions
- Provides detailed hex and decimal output for debugging
- Handles file I/O errors gracefully with descriptive messages

## Usage in Testing Workflows

### Automated Testing
```bash
# Create test archive copy
cp original.lha test_copy.lha

# Verify original is valid
lha t test_copy.lha

# Corrupt the copy
./file_corruptor.exe test_copy.lha

# Verify corruption is detected
lha t test_copy.lha  # Should fail with CRC errors
```

### Manual Testing
1. Copy your test archive to a safe location
2. Run the file corruptor on the copy
3. Test your archive processing tools with the corrupted file
4. Verify that CRC errors are properly detected and reported

## Future Enhancements

### Potential Improvements
- Configurable corruption count (currently fixed at 5 bytes)
- Specific corruption patterns (sequential, clustered, etc.)
- Corruption targeting specific file regions (headers, data, etc.)
- Restore functionality to undo corruptions

### Integration Opportunities
- Automated test harness integration
- CI/CD pipeline integration for regression testing
- Performance benchmarking with various corruption patterns

## Troubleshooting

### Common Issues
1. **"File too small"**: Target file must be at least 5 bytes
2. **"File too large"**: Target file must be under 10MB
3. **"Cannot open file"**: Check file permissions and path
4. **"Cannot write"**: Ensure file is not read-only

### Debug Tips
- Always make backups before corruption
- Check file permissions on target files
- Verify file exists and is accessible
- Review detailed corruption map output for analysis

## License and Attribution

This tool is part of the Amiga CLI Wrapper project and follows the same licensing terms. It's designed specifically for testing and development purposes and should not be used on production data without proper backups.
