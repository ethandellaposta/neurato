#!/usr/bin/env python3
"""
Max Framework Bridge for Neurato
Imports and re-exports Max Framework functionality.
"""

import sys
from pathlib import Path

# Add Max Framework to Python path
# Assumes Max Framework is at ../MAx relative to this file
max_framework_path = Path(__file__).parent / "max-framework"
if max_framework_path.is_symlink():
    # Resolve symlink to actual path
    max_framework_path = max_framework_path.resolve()
if max_framework_path.exists():
    sys.path.insert(0, str(max_framework_path))
else:
    # Fallback to system-wide installation
    import site
    site_packages = site.getsitepackages()[0]
    max_framework_path = Path(site_packages) / "max_framework"
    if max_framework_path.exists():
        sys.path.insert(0, str(max_framework_path))

# Import Max Framework components
try:
    # Import from the new .max submodule structure
    from pin_manager import PinManager, PinnedTask

    # Import working core modules
    from core.cache import MemoryCache, DiskCache, CacheManager, cached
    from core.multi_cache import MultiLevelCache
    from core.max_avatar_renderer import MaxAvatarRenderer
    from core.max_version_manager import MaxVersionManager
    from core.task_complexity_estimator import TaskComplexityEstimator

    # Try to import config_manager if it's not broken
    try:
        from config_manager import ConfigManager, ModelConfig, TestConfig
    except (ImportError, SyntaxError):
        # Fallback if config_manager has syntax errors
        ConfigManager = None
        ModelConfig = None
        TestConfig = None

    # Check for command executor in core
    try:
        from core.main.commands.command_executor import (
            CommandExecutor, CommandResult
        )
    except ImportError:
        # Fallback for backward compatibility
        CommandExecutor = None
        CommandResult = None

    # Re-export for convenience
    __all__ = [
        'PinManager',
        'PinnedTask',
        'MemoryCache',
        'DiskCache',
        'CacheManager',
        'cached',
        'MultiLevelCache',
        'MaxAvatarRenderer',
        'MaxVersionManager',
        'TaskComplexityEstimator',
        'ConfigManager',
        'ModelConfig',
        'TestConfig',
        'CommandExecutor',
        'CommandResult'
    ]

    # For backward compatibility, create aliases
    AICodeQualitySystem = None  # Not available in new structure
    TaskVisualizer = None       # Not available in new structure
    PersonaLoader = None        # Not available in new structure
    WorkflowExecutor = None     # Not available in new structure

except ImportError as e:
    print(f"Warning: Could not import Max Framework: {e}")
    print("Please ensure Max Framework is available at ../MAx/.max")
    print("Run: git submodule update --init --recursive")
    sys.exit(1)
