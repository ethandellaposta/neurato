# Neurato DAW - .windsurf Configuration

This directory contains Neurato-specific configuration for the Max Framework.

## Structure

```
.windsurf/
├── project-config.md      # Neurato project configuration
├── max_bridge.py          # Bridge to Max Framework
├── setup_max.sh           # Setup script for Max Framework
├── neurato_quest.py       # Neurato-specific quest game
├── quest_executor.py      # Quest executor
├── pinned_tasks.json      # Current pinned tasks
├── plan_state.json        # Current plan state
├── context-cache/         # Session context cache
├── insights/              # Memory insights
└── max-framework/         # Symlink to Max Framework (created by setup_max.sh)
```

## Max Framework Integration

Neurato uses Max Framework for AI assistance and workflow automation. The Max Framework is located outside this repository to avoid git tracking issues.

### Setup

1. Clone Max Framework outside this repository:
   ```bash
   cd ..  # Go to parent directory
   git clone <max-framework-repo-url> max
   ```

2. Run the setup script:
   ```bash
   ./.windsurf/setup_max.sh
   ```

3. Verify installation:
   ```python
   from .windsurf.max_bridge import CommandExecutor
   ```

### Usage

```python
# Import Max Framework components
from .windsurf.max_bridge import CommandExecutor

# Use commands
executor = CommandExecutor()
result = executor.execute("task::pin", ["Implement audio graph"])
```

## Neurato-Specific Files

- **project-config.md**: Neurato project configuration, milestones, and stack
- **neurato_quest.py**: Interactive CLI game for learning Neurato
- **pinned_tasks.json**: Current task pins (managed by Max Framework)
- **plan_state.json**: Current planning state (managed by Max Framework)

## Max Framework Files

The Max Framework provides:
- Command execution with namespace support
- Persona system for domain-specific AI assistance
- Task management and planning tools
- AI code quality analysis
- Workflow automation

See the Max Framework repository for full documentation.
