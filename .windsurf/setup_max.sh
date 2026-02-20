#!/bin/bash

# Setup Max Framework for Neurato
# This script sets up the Max Framework dependency

set -e

echo "Setting up Max Framework for Neurato..."

# Check if Max Framework exists at the expected location
MAX_FRAMEWORK_PATH="../MAx"
if [ ! -d "$MAX_FRAMEWORK_PATH" ]; then
    echo "Error: Max Framework not found at $MAX_FRAMEWORK_PATH"
    echo "Please clone Max Framework first:"
    echo "  cd .."
    echo "  git clone <max-framework-repo-url> MAx"
    exit 1
fi

# Check if .max directory exists in Max Framework
if [ ! -d "$MAX_FRAMEWORK_PATH/.max" ]; then
    echo "Error: Max Framework .max directory not found"
    echo "Please ensure .max submodule is initialized:"
    echo "  cd ../MAx"
    echo "  git submodule update --init --recursive"
    exit 1
fi

# Create symlink in .windsurf to Max's .max directory
if [ ! -L ".windsurf/max-framework" ]; then
    ln -sf "$MAX_FRAMEWORK_PATH/.max" ".windsurf/max-framework"
    echo "Created symlink to Max Framework .max directory"
fi

# Test import
echo "Testing Max Framework import..."
python3 -c "
import sys
sys.path.insert(0, '.windsurf/max-framework')
try:
    from pin_manager import PinManager, PinnedTask
    print('✅ Max Framework imported successfully')
    print('Available components:')
    print('  - PinManager: Task pinning system')
    print('  - PinnedTask: Task data model')
except ImportError as e:
    print(f'❌ Import failed: {e}')
    sys.exit(1)
"

echo "✅ Max Framework setup complete!"
echo ""
echo "Usage:"
echo "  Import in Python: from .windsurf.max_bridge import PinManager, PinnedTask"
echo "  Or directly: import sys; sys.path.insert(0, '.windsurf/max-framework'); from pin_manager import PinManager"
