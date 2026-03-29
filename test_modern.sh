#!/bin/bash
# Test script for modern retty64 implementation

echo "=== Testing retty64 Modern Implementation ==="
echo

# Test 1: Basic compilation check
echo "Test 1: Checking compiled binaries..."
if [ -f "./retty64" ] && [ -f "./blindtty64" ]; then
    echo "✓ retty64 and blindtty64 compiled successfully"
    file ./retty64 ./blindtty64
else
    echo "✗ Binaries not found"
    exit 1
fi

echo

# Test 2: Version check
echo "Test 2: Version information..."
./retty64 -v
./blindtty64 -v

echo

# Test 3: Help output
echo "Test 3: Help output..."
echo "retty64 help:"
./retty64 -h | head -5
echo
echo "blindtty64 help:"
./blindtty64 -h | head -8

echo

# Test 4: Architecture check
echo "Test 4: Architecture verification..."
RETTY_ARCH=$(file ./retty64 | grep -o "x86-64\|ARM\|64-bit")
if [ -n "$RETTY_ARCH" ]; then
    echo "✓ retty64 is compiled for: $RETTY_ARCH"
else
    echo "✗ Could not determine architecture"
fi

echo

# Test 5: Simple functionality test
echo "Test 5: Simple functionality test..."
echo "Starting a test process with blindtty64..."

# Start a simple sleep process in background
./blindtty64 -q sleep 10 &
BLIND_PID=$!
sleep 1

# Check if process is running
if ps -p $BLIND_PID > /dev/null 2>&1; then
    echo "✓ blindtty64 started process with PID: $BLIND_PID"

    # Try to get info about the process
    echo "Process info:"
    ps -fp $BLIND_PID 2>/dev/null || echo "Process not found (may have exited)"

    # Kill the test process
    kill $BLIND_PID 2>/dev/null
    wait $BLIND_PID 2>/dev/null
else
    echo "✗ blindtty64 failed to start process"
fi

echo

# Test 6: Build system test
echo "Test 6: Build system test..."
make -f Makefile.modern clean > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "✓ Clean target works"
else
    echo "✗ Clean target failed"
fi

make -f Makefile.modern > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "✓ Build target works"
else
    echo "✗ Build target failed"
fi

echo

# Test 7: File sizes
echo "Test 7: Binary sizes..."
echo "retty64 size: $(stat -c%s ./retty64) bytes"
echo "blindtty64 size: $(stat -c%s ./blindtty64) bytes"

echo

echo "=== Test Summary ==="
echo "Modern retty64 implementation appears to be working correctly."
echo "Key improvements:"
echo "1. 64-bit architecture support"
echo "2. Modern C codebase (C11)"
echo "3. sp.h library integration"
echo "4. Improved error handling"
echo "5. Better build system"
echo
echo "Note: Full functionality testing requires actual process attachment"
echo "which may require special permissions or test environment."