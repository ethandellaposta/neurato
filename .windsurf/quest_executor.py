#!/usr/bin/env python3
"""
Quest Workflow Executor for Neurato Project
Standalone executor for the /quest command.
"""

import subprocess
import sys
from pathlib import Path
from typing import Any, Dict, List


class QuestExecutor:
    """Executor for Quest workflow"""

    def __init__(self):
        self.windsurf_dir = Path(".windsurf")
        self.project_root = Path.cwd()

    def execute(self, args: List[str] = None) -> Dict[str, Any]:
        """Execute Quest workflow"""
        if not args:
            return self.show_help()
        elif args[0] == "play":
            return self.play_game(args[1:] if len(args) > 1 else [])
        elif args[0] == "help":
            return self.show_help()
        else:
            return self.show_help()

    def play_game(self, args: List[str]) -> Dict[str, Any]:
        """Launch Neurato Quest game"""
        game_path = self.windsurf_dir / "neurato_quest.py"
        
        if not game_path.exists():
            return {
                "success": False,
                "error": "Game file not found",
                "message": "Neurato Quest game not installed"
            }
        
        try:
            # Run the game
            result = subprocess.run(
                [sys.executable, str(game_path)] + args,
                capture_output=False,
                text=True
            )
            return {
                "success": True,
                "message": "Game completed"
            }
        except Exception as e:
            return {
                "success": False,
                "error": str(e),
                "message": "Failed to launch game"
            }
    
    def show_help(self) -> Dict[str, Any]:
        """Show help for quest command"""
        help_text = """
🎮 Neurato Quest - Interactive Knowledge Game

Usage:
  /quest play     - Launch the game
  /quest help     - Show this help

Game Modes:
  • Timeline Runner - Dodge obstacles on the dev timeline
  • Code Sprint    - Answer questions against the clock  
  • Crossword      - Solve Neurato terminology puzzles
  • Knowledge Quiz - Test your project knowledge

Test your Neurato DAW knowledge while having fun!
"""
        
        print(help_text)
        return {
            "success": True,
            "message": "Help displayed"
        }


def main():
    """CLI interface for quest workflow"""
    executor = QuestExecutor()
    
    # Parse arguments
    import argparse
    parser = argparse.ArgumentParser(description="Neurato Quest Game")
    parser.add_argument("args", nargs="*", help="Command arguments")
    
    parsed = parser.parse_args()
    
    # Execute the workflow
    result = executor.execute(parsed.args)
    
    if not result.get("success", False):
        print(f"❌ {result.get('error', 'Unknown error')}")
        if result.get("message"):
            print(result["message"])
        sys.exit(1)


if __name__ == "__main__":
    main()
