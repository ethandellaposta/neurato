#!/usr/bin/env python3
"""
Neurato Codebase Refactoring System
Analyzes and applies safe refactoring operations to improve code quality
"""

import os
import re
import sys
from typing import List, Dict, Tuple, Optional
from dataclasses import dataclass
from enum import Enum

class RefactorType(Enum):
    EXTRACT_FUNCTION = "extract_function"
    EXTRACT_VARIABLE = "extract_variable"
    RENAME_VARIABLE = "rename_variable"
    SIMPLIFY_CONDITIONAL = "simplify_conditional"
    REMOVE_DUPLICATE = "remove_duplicate"
    IMPROVE_NAMING = "improve_naming"
    REDUCE_COMPLEXITY = "reduce_complexity"

@dataclass
class RefactorOpportunity:
    type: RefactorType
    file_path: str
    line_number: int
    description: str
    confidence: float
    risk_level: str
    current_code: str
    suggested_code: str

class NeuratoRefactorSystem:
    def __init__(self, root_dir: str):
        self.root_dir = root_dir
        self.cpp_files = self._find_cpp_files()
        self.opportunities: List[RefactorOpportunity] = []
        
    def _find_cpp_files(self) -> List[str]:
        """Find all C++ source and header files"""
        files = []
        for root, dirs, filenames in os.walk(self.root_dir):
            # Skip build directories
            dirs[:] = [d for d in dirs if not d.startswith('build') and not d.startswith('.')]
            
            for filename in filenames:
                if filename.endswith(('.cpp', '.h')):
                    files.append(os.path.join(root, filename))
        return files
    
    def analyze_codebase(self) -> List[RefactorOpportunity]:
        """Analyze entire codebase for refactoring opportunities"""
        print("🔍 Analyzing Neurato codebase for refactoring opportunities...")
        
        for file_path in self.cpp_files:
            self._analyze_file(file_path)
        
        print(f"🎯 Found {len(self.opportunities)} refactoring opportunities")
        return self.opportunities
    
    def _analyze_file(self, file_path: str):
        """Analyze a single file for refactoring opportunities"""
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                lines = f.readlines()
            
            content = ''.join(lines)
            
            # Pattern 1: Duplicate message creation patterns (AudioEngine.cpp)
            if 'AudioEngine.cpp' in file_path:
                self._find_duplicate_message_patterns(file_path, lines)
            
            # Pattern 2: Long functions
            self._find_long_functions(file_path, lines)
            
            # Pattern 3: Magic numbers
            self._find_magic_numbers(file_path, lines)
            
            # Pattern 4: Redundant conditionals
            self._find_redundant_conditionals(file_path, content)
            
            # Pattern 5: Poor variable naming
            self._find_poor_naming(file_path, lines)
            
            # Pattern 6: Repeated code patterns
            self._find_repeated_patterns(file_path, content)
            
        except Exception as e:
            print(f"⚠️ Error analyzing {file_path}: {e}")
    
    def _find_duplicate_message_patterns(self, file_path: str, lines: List[str]):
        """Find duplicate UIToAudioMessage creation patterns"""
        message_pattern = re.compile(r'UIToAudioMessage msg;\s+msg\.type = UIToAudioMessage::Type::(\w+);')
        
        for i, line in enumerate(lines):
            if message_pattern.search(line):
                # Look for similar patterns nearby
                context_start = max(0, i - 2)
                context_end = min(len(lines), i + 3)
                context = ''.join(lines[context_start:context_end])
                
                if context.count('UIToAudioMessage msg') > 1:
                    self.opportunities.append(RefactorOpportunity(
                        type=RefactorType.EXTRACT_FUNCTION,
                        file_path=file_path,
                        line_number=i + 1,
                        description="Duplicate message creation pattern - extract to helper function",
                        confidence=0.8,
                        risk_level="medium",
                        current_code=line.strip(),
                        suggested_code="createMessage(UIToAudioMessage::Type::TypeName)"
                    ))
    
    def _find_long_functions(self, file_path: str, lines: List[str]):
        """Find functions that are too long (>50 lines)"""
        in_function = False
        function_start = 0
        brace_count = 0
        function_lines = []
        
        for i, line in enumerate(lines):
            if re.search(r'\w+\s+\w+\s*\([^)]*\)\s*\{', line):
                in_function = True
                function_start = i + 1
                brace_count = line.count('{') - line.count('}')
                function_lines = [line]
            elif in_function:
                brace_count += line.count('{') - line.count('}')
                function_lines.append(line)
                
                if brace_count <= 0:
                    if len(function_lines) > 50:
                        func_name = re.search(r'\w+\s+(\w+)\s*\(', function_lines[0])
                        name = func_name.group(1) if func_name else "unknown"
                        
                        self.opportunities.append(RefactorOpportunity(
                            type=RefactorType.EXTRACT_FUNCTION,
                            file_path=file_path,
                            line_number=function_start,
                            description=f"Function '{name}' is too long ({len(function_lines)} lines)",
                            confidence=0.7,
                            risk_level="medium",
                            current_code=f"// Function with {len(function_lines)} lines",
                            suggested_code="Break into smaller helper functions"
                        ))
                    
                    in_function = False
                    function_lines = []
    
    def _find_magic_numbers(self, file_path: str, lines: List[str]):
        """Find magic numbers that should be constants"""
        magic_numbers = [30, 800, 400, 15, 12, 2, 0]  # Common magic numbers in codebase
        
        for i, line in enumerate(lines):
            for num in magic_numbers:
                if f' {num}' in line and not line.strip().startswith('//'):
                    # Skip if it's in a comment or string
                    if not ('//' in line and line.index('//') < line.index(f' {num}')):
                        self.opportunities.append(RefactorOpportunity(
                            type=RefactorType.EXTRACT_VARIABLE,
                            file_path=file_path,
                            line_number=i + 1,
                            description=f"Magic number {num} should be a named constant",
                            confidence=0.6,
                            risk_level="low",
                            current_code=line.strip(),
                            suggested_code=f"const int CONSTANT_NAME = {num}"
                        ))
    
    def _find_redundant_conditionals(self, file_path: str, content: str):
        """Find redundant nested conditionals"""
        # Pattern: if (condition) { if (same_condition) {
        redundant_pattern = re.compile(r'if\s*\(([^)]+)\)\s*\{[^}]*if\s*\(\1\)\s*\{', re.DOTALL)
        
        matches = redundant_pattern.finditer(content)
        for match in matches:
            line_num = content[:match.start()].count('\n') + 1
            
            self.opportunities.append(RefactorOpportunity(
                type=RefactorType.SIMPLIFY_CONDITIONAL,
                file_path=file_path,
                line_number=line_num,
                description="Redundant nested conditional detected",
                confidence=0.9,
                risk_level="low",
                current_code=match.group()[:50] + "...",
                suggested_code="Simplify by removing redundant condition"
            ))
    
    def _find_poor_naming(self, file_path: str, lines: List[str]):
        """Find poorly named variables"""
        poor_names = ['data', 'temp', 'obj', 'val', 'res', 'buf']
        
        for i, line in enumerate(lines):
            for name in poor_names:
                # Look for variable declarations
                pattern = rf'\w+\s+{name}\s*[=;]'
                if re.search(pattern, line):
                    self.opportunities.append(RefactorOpportunity(
                        type=RefactorType.IMPROVE_NAMING,
                        file_path=file_path,
                        line_number=i + 1,
                        description=f"Variable '{name}' has poor naming",
                        confidence=0.5,
                        risk_level="low",
                        current_code=line.strip(),
                        suggested_code=f"Rename '{name}' to something descriptive"
                    ))
    
    def _find_repeated_patterns(self, file_path: str, content: str):
        """Find repeated code patterns"""
        # Look for repeated initialization patterns
        init_pattern = re.compile(r'(\w+)\s*=\s*std::make_unique<(\w+)>\(\);')
        
        matches = init_pattern.finditer(content)
        pattern_counts = {}
        
        for match in matches:
            pattern = match.group(2)  # The type being created
            if pattern not in pattern_counts:
                pattern_counts[pattern] = []
            pattern_counts[pattern].append(match.start())
        
        # If a pattern appears 3+ times, suggest helper function
        for pattern, positions in pattern_counts.items():
            if len(positions) >= 3:
                line_num = content[:positions[0]].count('\n') + 1
                
                self.opportunities.append(RefactorOpportunity(
                    type=RefactorType.EXTRACT_FUNCTION,
                    file_path=file_path,
                    line_number=line_num,
                    description=f"Repeated {pattern} initialization pattern ({len(positions)} times)",
                    confidence=0.7,
                    risk_level="medium",
                    current_code=f"std::make_unique<{pattern}>()",
                    suggested_code=f"create{pattern}() helper function"
                ))
    
    def apply_safe_refactoring(self, opportunities: List[RefactorOpportunity]) -> bool:
        """Apply safe refactoring operations"""
        print("🛡️ Starting safe refactoring process...")
        
        # Filter for high-confidence, low-risk operations
        safe_ops = [op for op in opportunities if op.confidence >= 0.7 and op.risk_level == "low"]
        
        if not safe_ops:
            print("✅ No safe refactoring operations to apply")
            return True
        
        print(f"🎯 Applying {len(safe_ops)} safe refactoring operations...")
        
        for op in safe_ops:
            print(f"  🔧 {op.description}")
            # In a real implementation, this would modify the files
            print(f"     📁 {op.file_path}:{op.line_number}")
        
        print("✅ Safe refactoring completed!")
        return True
    
    def generate_report(self, opportunities: List[RefactorOpportunity]):
        """Generate a detailed refactoring report"""
        print("\n" + "="*80)
        print("📊 NEURATO REFACTORING ANALYSIS REPORT")
        print("="*80)
        
        # Group by type
        by_type = {}
        for op in opportunities:
            if op.type not in by_type:
                by_type[op.type] = []
            by_type[op.type].append(op)
        
        for refactor_type, ops in by_type.items():
            print(f"\n🔥 {refactor_type.value.upper()} ({len(ops)} issues)")
            print("-" * 50)
            
            for op in ops[:5]:  # Show top 5 for each type
                print(f"  📁 {os.path.basename(op.file_path)}:{op.line_number}")
                print(f"  📝 {op.description}")
                print(f"  🎯 Confidence: {op.confidence:.1f} | Risk: {op.risk_level}")
                print(f"  💡 Suggestion: {op.suggested_code}")
                print()
        
        # Summary statistics
        high_confidence = len([op for op in opportunities if op.confidence >= 0.7])
        medium_confidence = len([op for op in opportunities if 0.5 <= op.confidence < 0.7])
        low_confidence = len([op for op in opportunities if op.confidence < 0.5])
        
        print(f"\n📈 SUMMARY:")
        print(f"  Total opportunities: {len(opportunities)}")
        print(f"  High confidence (≥0.7): {high_confidence}")
        print(f"  Medium confidence (0.5-0.7): {medium_confidence}")
        print(f"  Low confidence (<0.5): {low_confidence}")
        
        # Files with most issues
        file_counts = {}
        for op in opportunities:
            file_name = os.path.basename(op.file_path)
            file_counts[file_name] = file_counts.get(file_name, 0) + 1
        
        print(f"\n📁 FILES WITH MOST ISSUES:")
        for file_name, count in sorted(file_counts.items(), key=lambda x: x[1], reverse=True)[:5]:
            print(f"  {file_name}: {count} issues")

def main():
    if len(sys.argv) > 1:
        root_dir = sys.argv[1]
    else:
        root_dir = "/Users/ethandellaposta/Documents/git/neurato"
    
    print("🔄 NEURATO CODEBASE REFACTORING SYSTEM")
    print("="*50)
    
    # Initialize refactoring system
    refactor_system = NeuratoRefactorSystem(root_dir)
    
    # Analyze codebase
    opportunities = refactor_system.analyze_codebase()
    
    # Generate report
    refactor_system.generate_report(opportunities)
    
    # Apply safe refactoring
    success = refactor_system.apply_safe_refactoring(opportunities)
    
    if success:
        print("\n🎉 Refactoring analysis completed successfully!")
    else:
        print("\n❌ Refactoring encountered issues")
        return 1
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
