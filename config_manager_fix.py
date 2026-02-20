#!/usr/bin/env python3
"""
Configuration Manager for Max Framework
Centralized configuration system for all components.
"""

import json
import os
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Dict, Optional

try:
    import yaml  # type: ignore
    HAS_YAML = True
except ImportError:
    HAS_YAML = False


@dataclass
class ModelConfig:
    """Model configuration"""

    default_model: str = "SWE-1.5"
    cost_optimization: bool = True
    auto_upgrade_threshold: float = 0.8  # Upgrade when confidence > 80%

    # Model tiers
    eco_models: list = None
    standard_models: list = None
    power_models: list = None

    def __post_init__(self):
        """Initialize default values for lists"""
        if self.eco_models is None:
            self.eco_models = ["SWE-1.5", "SWE-1-lite", "SWE-1-mini"]
        if self.standard_models is None:
            self.standard_models = ["Sonnet", "SWE-1"]
        if self.power_models is None:
            self.power_models = ["Opus"]


@dataclass
class TestConfig:
    """Test configuration"""

    auto_generate: bool = True
    coverage_threshold: float = 0.8
    parallel_execution: bool = True
    timeout_seconds: int = 30


@dataclass
class TaskConfig:
    """Task configuration"""

    max_complexity: str = "Medium"
    auto_split: bool = True
    progress_tracking: bool = True


@dataclass
class QualityConfig:
    """Quality configuration"""

    min_quality_score: float = 0.7
    auto_refactor: bool = True
    style_check: bool = True


@dataclass
class UIConfig:
    """UI configuration"""

    theme: str = "dark"
    auto_save: bool = True
    show_line_numbers: bool = True


@dataclass
class ProjectConfig:
    """Project configuration"""

    name: str = ""
    description: str = ""
    version: str = "1.0.0"
    author: str = ""


@dataclass
class MaxConfig:
    """Master configuration for Max Framework"""

    model: ModelConfig = None
    test: TestConfig = None
    task: TaskConfig = None
    quality: QualityConfig = None
    ui: UIConfig = None
    project: ProjectConfig = None

    def __post_init__(self):
        """Initialize default values for nested configs"""
        if self.model is None:
            self.model = ModelConfig()
        if self.test is None:
            self.test = TestConfig()
        if self.task is None:
            self.task = TaskConfig()
        if self.quality is None:
            self.quality = QualityConfig()
        if self.ui is None:
            self.ui = UIConfig()
        if self.project is None:
            self.project = ProjectConfig()


class ConfigManager:
    """Manages all configuration for Max Framework"""

    def __init__(self, config_dir: Optional[str] = None):
        """Initialize the configuration manager"""
        if config_dir is None:
            self.config_dir = Path.cwd() / ".max"
        else:
            self.config_dir = Path(config_dir)

        self.config_file = self.config_dir / "config.json"
        self.user_config_file = Path.home() / ".max" / "config.json"

        # Ensure config directory exists
        self.config_dir.mkdir(parents=True, exist_ok=True)

        # Load configuration
        self.config = self._load_config()

    def _load_config(self) -> MaxConfig:
        """Load configuration from files"""
        # Start with defaults
        config = MaxConfig()

        # Load project config
        if self.config_file.exists():
            try:
                with open(self.config_file, "r", encoding="utf-8") as f:
                    data = json.load(f)
                    self._update_config(config, data)
            except Exception as e:
                print(f"Warning: Could not load project config: {e}")

        # Load user config (overrides project config)
        if self.user_config_file.exists():
            try:
                with open(self.user_config_file, "r", encoding="utf-8") as f:
                    data = json.load(f)
                    self._update_config(config, data)
            except Exception as e:
                print(f"Warning: Could not load user config: {e}")

        return config

    def _update_config(self, config: MaxConfig, data: Dict[str, Any]):
        """Update config with data from dict"""
        if "model" in data and isinstance(data["model"], dict):
            self._update_dataclass(config.model, data["model"])
        if "test" in data and isinstance(data["test"], dict):
            self._update_dataclass(config.test, data["test"])
        if "task" in data and isinstance(data["task"], dict):
            self._update_dataclass(config.task, data["task"])
        if "quality" in data and isinstance(data["quality"], dict):
            self._update_dataclass(config.quality, data["quality"])
        if "ui" in data and isinstance(data["ui"], dict):
            self._update_dataclass(config.ui, data["ui"])
        if "project" in data and isinstance(data["project"], dict):
            self._update_dataclass(config.project, data["project"])

    def _update_dataclass(self, obj, data: Dict[str, Any]):
        """Update dataclass fields from dict"""
        for key, value in data.items():
            if hasattr(obj, key):
                setattr(obj, key, value)

    def save_config(self, user_config: bool = False):
        """Save configuration to file"""
        config_file = self.user_config_file if user_config else self.config_file
        config_file.parent.mkdir(parents=True, exist_ok=True)

        try:
            with open(config_file, "w", encoding="utf-8") as f:
                json.dump(asdict(self.config), f, indent=2)
        except Exception as e:
            print(f"Error saving config: {e}")

    def get_model_config(self) -> ModelConfig:
        """Get model configuration"""
        return self.config.model

    def get_test_config(self) -> TestConfig:
        """Get test configuration"""
        return self.config.test

    def get_task_config(self) -> TaskConfig:
        """Get task configuration"""
        return self.config.task

    def get_quality_config(self) -> QualityConfig:
        """Get quality configuration"""
        return self.config.quality

    def get_ui_config(self) -> UIConfig:
        """Get UI configuration"""
        return self.config.ui

    def get_project_config(self) -> ProjectConfig:
        """Get project configuration"""
        return self.config.project

    def update_model_config(self, **kwargs):
        """Update model configuration"""
        self._update_dataclass(self.config.model, kwargs)

    def update_test_config(self, **kwargs):
        """Update test configuration"""
        self._update_dataclass(self.config.test, kwargs)

    def update_task_config(self, **kwargs):
        """Update task configuration"""
        self._update_dataclass(self.config.task, kwargs)

    def update_quality_config(self, **kwargs):
        """Update quality configuration"""
        self._update_dataclass(self.config.quality, kwargs)

    def update_ui_config(self, **kwargs):
        """Update UI configuration"""
        self._update_dataclass(self.config.ui, kwargs)

    def update_project_config(self, **kwargs):
        """Update project configuration"""
        self._update_dataclass(self.config.project, kwargs)


def main():
    """Main function for testing"""
    config_manager = ConfigManager()
    print("Max Framework Configuration Manager")
    print(f"Config directory: {config_manager.config_dir}")
    print(f"Model: {config_manager.get_model_config().default_model}")
    print(f"Test coverage threshold: {config_manager.get_test_config().coverage_threshold}")


if __name__ == "__main__":
    main()
