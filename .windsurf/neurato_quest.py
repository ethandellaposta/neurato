#!/usr/bin/env python3
"""
Neurato Quest - An Interactive CLI Knowledge Game
Test your knowledge of the Neurato DAW project while having fun!
"""

import os
import random
import sys
import time
import threading
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Tuple

# ANSI color codes
class Colors:
    RESET = '\033[0m'
    RED = '\033[91m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    MAGENTA = '\033[95m'
    CYAN = '\033[96m'
    WHITE = '\033[97m'
    BOLD = '\033[1m'
    DIM = '\033[2m'

# Game sprites
SPRITES = {
    'player': {
        'idle': ['  O  ', ' /|\\ ', ' / \\ '],
        'jump': ['  O  ', ' /|\\ ', '     '],
        'slide': [' O___', '/|\\  ', '     '],
        'think': ['  O? ', ' /|\\ ', ' /\\ '],
        'code': ['  💻 ', ' /|\\ ', ' / \\ ']
    },
    'obstacles': {
        'bug': ['🐛', '🪲', '🦟'],
        'feature': ['⭐', '🚀', '💡'],
        'deadline': ['⏰', '📅', '⚡'],
        'question': ['❓', '❔', '🤔']
    },
    'collectibles': ['💎', '🏆', '⚡', '🎯', '🌟'],
    'terrain': ['_', '═', '─', '░']
}

class NeuratoQuest:
    """Main game class for Neurato Quest"""
    
    def __init__(self):
        self.score = 0
        self.lives = 3
        self.level = 1
        self.game_mode = None
        self.player_pos = 5
        self.is_jumping = False
        self.is_sliding = False
        self.game_speed = 0.1
        self.running = True
        self.project_knowledge = self.load_project_knowledge()
        
    def load_project_knowledge(self) -> Dict:
        """Load project context for questions"""
        return {
            'milestones': {
                'Milestone 1': 'Audio Host Skeleton - Native app window, audio device selection, transport, metronome',
                'Milestone 2': 'Timeline & Non-Destructive Edits - Session model, clip editing, CommandManager',
                'Milestone 3': 'Project Persistence & Offline Bounce - JSON save/load, WAV bounce',
                'Milestone 4': 'Multi-Track & Mixer - MixerPanel, ChannelStrip, master bus',
                'Milestone 4.5': 'MIDI Tracks & UI Modernization - MIDI synth, PianoRollEditor, Theme system',
                'Milestone 5': 'Audio Graph & Automation - Processing nodes, latency compensation, automation lanes'
            },
            'tech_stack': {
                'Language': 'C++20',
                'Framework': 'JUCE 8.0.12',
                'Build System': 'CMake 3.22+',
                'Platforms': 'macOS, Windows, Linux (native)',
                'Architecture': 'Audio thread (RT-safe) + UI thread model'
            },
            'key_components': {
                'AudioEngine': 'Handles audio processing and device management',
                'SessionRenderer': 'Real-time audio rendering with lock-free queues',
                'CommandManager': 'Undo/redo system for all edits',
                'TimelineView': 'Waveform rendering, playhead, clip editing',
                'MixerPanel': 'Channel strips with gain, pan, mute/solo',
                'PianoRollEditor': 'MIDI note editing with piano keyboard'
            },
            'principles': [
                'Audio thread is sacred (zero alloc/lock/IO)',
                'UI thread owns the Session model',
                'All edits go through CommandManager',
                'Cross-platform from day one',
                'Non-destructive editing philosophy'
            ]
        }
    
    def clear_screen(self):
        """Clear the terminal screen"""
        os.system('clear' if os.name == 'posix' else 'cls')
    
    def print_header(self):
        """Print game header"""
        print(f"""
{Colors.CYAN}╔══════════════════════════════════════════════════════════════╗
║                    {Colors.BOLD}NEURATO QUEST{Colors.RESET}{Colors.CYAN} - Knowledge Adventure!     ║
║  Test your Neurato DAW knowledge while dodging obstacles!         ║
╚══════════════════════════════════════════════════════════════╝{Colors.RESET}
""")
        print(f"Score: {Colors.YELLOW}{self.score}{Colors.RESET} | Lives: {Colors.RED}{'❤' * self.lives}{Colors.RESET} | Level: {Colors.GREEN}{self.level}{Colors.RESET} | Speed: {self.game_speed:.1f}s")
    
    def print_game_area(self, obstacles: List, collectibles: List, ground: str):
        """Print the main game area"""
        # Build the game field
        field = [[' ' for _ in range(80)] for _ in range(10)]
        
        # Add ground
        for x in range(80):
            field[9][x] = random.choice(SPRITES['terrain'])
        
        # Add player
        player_sprite = SPRITES['player']['jump'] if self.is_jumping else SPRITES['player']['slide'] if self.is_sliding else SPRITES['player']['idle']
        for i, line in enumerate(player_sprite):
            if 0 <= self.player_pos + i < 10:
                for j, char in enumerate(line):
                    if 0 <= 10 + j < 80:
                        field[self.player_pos + i][10 + j] = char
        
        # Add obstacles and collectibles
        for obs in obstacles:
            x, obs_type, obs_sprite = obs
            if 0 <= x < 80:
                field[7][x] = obs_sprite
        
        for col in collectibles:
            x, col_sprite = col
            if 0 <= x < 80:
                field[5][x] = col_sprite
        
        # Print the field
        for row in field:
            print(''.join(row))
    
    def get_question(self, category: str = None) -> Tuple[str, str]:
        """Get a random question from project knowledge"""
        questions = [
            ('milestone', 'What was the main achievement of Milestone 1?', 'Audio Host Skeleton'),
            ('milestone', 'Which milestone introduced MIDI tracks?', 'Milestone 4.5'),
            ('tech', 'What programming language is Neurato built with?', 'C++20'),
            ('tech', 'Which audio framework does Neurato use?', 'JUCE 8.0.12'),
            ('component', 'What handles undo/redo operations?', 'CommandManager'),
            ('component', 'Which component renders waveforms on the timeline?', 'TimelineView'),
            ('principle', 'What must the audio thread never do?', 'Allocations, locks, or I/O'),
            ('principle', 'Which thread owns the Session model?', 'UI thread')
        ]
        
        if category:
            questions = [q for q in questions if q[0] == category]
        
        q = random.choice(questions)
        return q[1], q[2]
    
    def crossword_mode(self):
        """Crossword puzzle mode"""
        self.clear_screen()
        print(f"""
{Colors.CYAN}╔══════════════════════════════════════════════════════════════╗
║                        {Colors.BOLD}CROSSWORD PUZZLE MODE{Colors.RESET}{Colors.CYAN}              ║
║  Fill in the crossword with Neurato DAW terminology!              ║
╚══════════════════════════════════════════════════════════════╝{Colors.RESET}
""")
        
        # Simple crossword layout
        crossword = [
            ['J', 'U', 'C', 'E', '#', '#', '#', '#', '#', '#'],
            ['U', '#', '#', 'O', '#', 'C', 'M', 'A', 'N', 'D'],
            ['N', '#', '#', 'D', '#', 'O', '#', '#', '#', 'E'],
            ['E', '#', '#', 'E', '#', 'D', '#', '#', '#', 'R'],
            ['#', '#', '#', '#', '#', 'E', '#', '#', '#', '#'],
            ['#', 'A', 'U', 'D', 'I', 'O', '#', '#', '#', '#'],
            ['#', '#', '#', '#', '#', '#', '#', '#', '#', '#'],
            ['#', 'S', 'E', 'S', 'S', 'I', 'O', 'N', '#', '#'],
            ['#', '#', '#', '#', '#', '#', '#', '#', '#', '#'],
            ['#', 'T', 'R', 'A', 'C', 'K', '#', '#', '#', '#']
        ]
        
        clues = {
            'Across': [
                '1. Audio framework used (4)',
                '5. Manages commands (5)',
                '6. What we process (5)',
                '8. Project data structure (7)',
                '10. Timeline element (5)'
            ],
            'Down': [
                '2. Programming language (4)',
                '3. Timeline unit (4)',
                '4. Code execution unit (5)',
                '7. What we edit (4)',
                '9. MIDI editing tool (10)'
            ]
        }
        
        # Print crossword
        print("\nCrossword Grid:")
        for row in crossword:
            print(' '.join(row))
        
        print("\n" + Colors.YELLOW + "CLUES:" + Colors.RESET)
        for direction, clue_list in clues.items():
            print(f"\n{Colors.BOLD}{direction}:{Colors.RESET}")
            for clue in clue_list:
                print(f"  {clue}")
        
        print(f"\n{Colors.GREEN}Press any key to return to menu...{Colors.RESET}")
        input()
    
    def timeline_runner(self):
        """Timeline runner game mode - jump over obstacles representing timeline events"""
        self.clear_screen()
        print(f"""
{Colors.CYAN}╔══════════════════════════════════════════════════════════════╗
║                     {Colors.BOLD}TIMELINE RUNNER MODE{Colors.RESET}{Colors.CYAN}                ║
║  Jump over bugs, slide under features, collect knowledge!       ║
║  Controls: SPACE=Jump, S=Slide, Q=Quit                        ║
╚══════════════════════════════════════════════════════════════╝{Colors.RESET}
""")
        
        obstacles = []
        collectibles = []
        frame = 0
        
        while self.running and self.lives > 0:
            self.clear_screen()
            self.print_header()
            
            # Update game state
            frame += 1
            
            # Generate obstacles
            if frame % 20 == 0:
                obs_type = random.choice(['bug', 'feature', 'deadline'])
                obs_sprite = random.choice(SPRITES['obstacles'][obs_type])
                obstacles.append([79, obs_type, obs_sprite])
            
            # Generate collectibles
            if frame % 30 == 0:
                col_sprite = random.choice(SPRITES['collectibles'])
                collectibles.append([79, col_sprite])
            
            # Move obstacles and collectibles
            obstacles = [[x-1, t, s] for x, t, s in obstacles if x > 0]
            collectibles = [[x-1, s] for x, s in collectibles if x > 0]
            
            # Check collisions
            for obs in obstacles[:]:
                if 8 <= obs[0] <= 15 and not self.is_jumping:
                    if obs[1] == 'deadline':
                        # Need to slide
                        if not self.is_sliding:
                            self.lives -= 1
                            obstacles.remove(obs)
                            print(f"{Colors.RED}Hit a deadline! Lives: {self.lives}{Colors.RESET}")
                    else:
                        self.lives -= 1
                        obstacles.remove(obs)
                        print(f"{Colors.RED}Hit a {obs[1]}! Lives: {self.lives}{Colors.RESET}")
            
            for col in collectibles[:]:
                if 8 <= col[0] <= 15:
                    self.score += 10
                    collectibles.remove(col)
                    print(f"{Colors.GREEN}Collected {col[1]}! +10 points{Colors.RESET}")
            
            # Update player position
            if self.is_jumping:
                self.player_pos = 3
                self.is_jumping = False
            elif self.is_sliding:
                self.player_pos = 7
                self.is_sliding = False
            else:
                self.player_pos = 5
            
            # Print game
            self.print_game_area(obstacles, collectibles, '_' * 80)
            
            # Handle input
            try:
                # Non-blocking input check
                import select
                if select.select([sys.stdin], [], [], 0)[0]:
                    key = sys.stdin.read(1).upper()
                    if key == ' ':
                        self.is_jumping = True
                    elif key == 'S':
                        self.is_sliding = True
                    elif key == 'Q':
                        self.running = False
            except:
                pass
            
            time.sleep(self.game_speed)
            
            # Increase difficulty
            if frame % 100 == 0:
                self.game_speed = max(0.05, self.game_speed - 0.01)
                self.level += 1
    
    def code_sprint(self):
        """Code sprint mode - answer questions quickly"""
        self.clear_screen()
        print(f"""
{Colors.CYAN}╔══════════════════════════════════════════════════════════════╗
║                      {Colors.BOLD}CODE SPRINT MODE{Colors.RESET}{Colors.CYAN}                     ║
║  Answer Neurato questions as fast as you can!                   ║
║  Type your answer and press ENTER                             ║
╚══════════════════════════════════════════════════════════════╝{Colors.RESET}
""")
        
        time_limit = 60  # seconds
        start_time = time.time()
        questions_answered = 0
        
        while time.time() - start_time < time_limit:
            question, answer = self.get_question()
            
            print(f"\n{Colors.YELLOW}Time left: {int(time_limit - (time.time() - start_time))}s{Colors.RESET}")
            print(f"\n{Colors.CYAN}Question: {Colors.WHITE}{question}{Colors.RESET}")
            
            user_answer = input(f"\n{Colors.GREEN}Your answer: {Colors.RESET}").strip()
            
            if user_answer.lower() == answer.lower():
                print(f"{Colors.GREEN}✓ Correct! +10 points{Colors.RESET}")
                self.score += 10
                questions_answered += 1
            else:
                print(f"{Colors.RED}✗ Wrong! Answer: {answer}{Colors.RESET}")
            
            time.sleep(1)
        
        print(f"\n{Colors.BOLD}Time's up!{Colors.RESET}")
        print(f"Questions answered: {questions_answered}")
        print(f"Final score: {self.score}")
    
    def main_menu(self):
        """Main game menu"""
        while self.running:
            self.clear_screen()
            print(f"""
{Colors.CYAN}╔══════════════════════════════════════════════════════════════╗
║                    {Colors.BOLD}NEURATO QUEST - Main Menu{Colors.RESET}{Colors.CYAN}              ║
╚══════════════════════════════════════════════════════════════╝{Colors.RESET}

{Colors.YELLOW}Choose your game mode:{Colors.RESET}

1) {Colors.GREEN}Timeline Runner{Colors.RESET} - Dodge obstacles on the development timeline!
2) {Colors.BLUE}Code Sprint{Colors.RESET} - Answer questions against the clock!
3) {Colors.MAGENTA}Crossword Puzzle{Colors.RESET} - Solve the Neurato terminology crossword!
4) {Colors.CYAN}Knowledge Quiz{Colors.RESET} - Test your project knowledge!
5) {Colors.RED}Quit{Colors.RESET}

{Colors.DIM}Current Score: {self.score} | Best Score: {self.score}{Colors.RESET}
""")
            
            choice = input(f"\n{Colors.GREEN}Select mode (1-5): {Colors.RESET}").strip()
            
            if choice == '1':
                self.timeline_runner()
            elif choice == '2':
                self.code_sprint()
            elif choice == '3':
                self.crossword_mode()
            elif choice == '4':
                self.knowledge_quiz()
            elif choice == '5':
                print(f"\n{Colors.YELLOW}Thanks for playing Neurato Quest!{Colors.RESET}")
                print(f"Final score: {self.score}")
                break
            else:
                print(f"{Colors.RED}Invalid choice!{Colors.RESET}")
                time.sleep(1)
    
    def knowledge_quiz(self):
        """Knowledge quiz mode"""
        self.clear_screen()
        print(f"""
{Colors.CYAN}╔══════════════════════════════════════════════════════════════╗
║                      {Colors.BOLD}KNOWLEDGE QUIZ MODE{Colors.RESET}{Colors.CYAN}                   ║
║  Test your knowledge of Neurato DAW development!               ║
╚══════════════════════════════════════════════════════════════╝{Colors.RESET}
""")
        
        categories = ['milestone', 'tech', 'component', 'principle']
        score = 0
        total = 10
        
        for i in range(total):
            category = random.choice(categories)
            question, answer = self.get_question(category)
            
            print(f"\n{Colors.YELLOW}Question {i+1}/{total} ({category}):{Colors.RESET}")
            print(f"{Colors.WHITE}{question}{Colors.RESET}")
            
            # Multiple choice
            wrong_answers = ['Audio processing', 'MIDI support', 'File I/O', 'UI rendering', 'Memory management']
            wrong_answers = [w for w in wrong_answers if w != answer][:3]
            options = [answer] + wrong_answers
            random.shuffle(options)
            
            for j, opt in enumerate(options, 1):
                print(f"  {j}) {opt}")
            
            try:
                choice = int(input(f"\n{Colors.GREEN}Your choice (1-4): {Colors.RESET}"))
                if 1 <= choice <= 4 and options[choice-1] == answer:
                    print(f"{Colors.GREEN}✓ Correct!{Colors.RESET}")
                    score += 1
                else:
                    print(f"{Colors.RED}✗ Wrong! Answer: {answer}{Colors.RESET}")
            except:
                print(f"{Colors.RED}Invalid choice!{Colors.RESET}")
            
            time.sleep(1)
        
        print(f"\n{Colors.BOLD}Quiz Complete!{Colors.RESET}")
        print(f"Score: {score}/{total}")
        self.score += score * 5
        print(f"Added {score * 5} points to total score!")
        
        input(f"\n{Colors.GREEN}Press Enter to continue...{Colors.RESET}")

def main():
    """Main entry point"""
    game = NeuratoQuest()
    
    # Set up terminal for non-blocking input
    if os.name == 'posix':
        import termios, tty
        old_settings = termios.tcgetattr(sys.stdin)
        try:
            tty.setcbreak(sys.stdin.fileno())
            game.main_menu()
        finally:
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_settings)
    else:
        game.main_menu()

if __name__ == "__main__":
    main()
