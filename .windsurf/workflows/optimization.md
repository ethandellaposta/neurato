---
description: Max optimization strategies for zero-cost and maximum performance
---

# Max Optimization Strategies

## 🎯 Goal: Zero Cost, Maximum Power

Max helps you achieve both:
1. **Zero-cost development** - Use SWE-1.5 (FREE) for 80% of tasks
2. **Maximum model power** - Extract exponential value from expensive models when needed

## 🆓 Zero-Cost Strategy (Primary)

### The 80/20 Rule with SWE-1.5

**Use SWE-1.5 (FREE) for:**
- Single-file edits and bug fixes
- Clear, scoped tasks
- Boilerplate and repetitive code
- Questions about existing codebase
- Config changes and build issues
- "Make X do Y" with no architectural judgment

**SWE-1.5 Optimization Techniques:**
- **Break down complex tasks** into multiple single-file tasks
- **Use iterative refinement** - "Fix this" → "Now add this" → "Now optimize"
- **Leverage context** - Keep relevant code in the conversation window
- **Use specific prompts** - "Add error handling to line 42-56" vs "Improve error handling"

### When to Upgrade (Spend Credits)

Only upgrade to Sonnet/Opus for:
- **Multi-file refactors** (affects 3+ files)
- **Architecture decisions** (hard to reverse)
- **Complex debugging** (root cause not obvious)
- **New systems** (from scratch)
- **When SWE-1.5 fails 2+ times** on the same problem

## 🚀 Maximum Power Strategy (Secondary)

### Exponential Model Performance

When you DO spend credits, maximize their value:

#### 1. **Prompt Stacking Technique**
```
// Don't do this (wastes credits):
Prompt 1: "Build auth system"
Prompt 2: "Add JWT"
Prompt 3: "Add refresh tokens"

// Do this (maximizes each expensive prompt):
Prompt 1: "Build complete auth system with JWT access tokens, refresh tokens,
         middleware, error handling, rate limiting, and tests. Consider
         security best practices and scalability."
```

#### 2. **Context Loading**
Before using expensive models:
- Load all relevant files into context
- Provide complete system overview
- Include constraints and requirements
- Give examples of desired patterns

#### 3. **Parallel Execution**
Ask for multiple solutions in one prompt:
```
"Design the user auth system. Provide:
1. Database schema
2. API endpoints
3. Frontend components
4. Security considerations
5. Test cases
"
```

#### 4. **Future-Proofing Requests**
```
"Build this feature considering:
- Future scaling to 1M users
- Internationalization
- A/B testing framework
- Analytics integration
- Mobile app compatibility
"
```

### The Third Dimension: Model Power × Context × Technique

| Model | Base Power | With Max Technique | Exponential Factor |
|-------|------------|------------------|-------------------|
| SWE-1.5 | 1x | Iterative refinement | ~3x |
| Sonnet | 2x | Context loading + prompt stacking | ~8x |
| Opus | 2x (promo) | Full technique suite | ~20x |

## 💡 Cost Optimization Patterns

### 1. **The SWE-1.5 Funnel**
```
Complex Task → Break down → SWE-1.5 × 5 → FREE
vs
Complex Task → Single prompt → Sonnet × 1 → $0.06
```

### 2. **The Hybrid Approach**
- Use SWE-1.5 to explore and prototype
- Use Sonnet/Opus for final implementation
- Use expensive models for decisions, not implementation

### 3. **The Batch Technique**
Save up multiple small tasks, then:
```
"Handle these 5 bugs in one go:
1. Fix null pointer in user service
2. Add validation to email field
3. Update deprecated API calls
4. Fix memory leak in cache
5. Add logging to auth flow
"
```

## 🎪 Advanced Optimization

### Meta-Prompts for Model Maximization

#### For Sonnet (2x credits):
```
"You're a senior architect. I need a production-ready solution.
Consider edge cases, performance, security, and maintainability.
Provide the complete implementation with explanations."
```

#### For Opus (2x credits promo):
```
"You're a principal engineer with 10+ years experience.
Design this system as if it's for a unicorn startup.
Consider scalability from day 1, team collaboration, and future evolution.
Include tradeoff analysis and decision rationale."
```

### The Power User Workflow

1. **SWE-1.5** - Explore, prototype, iterate (FREE)
2. **Sonnet** - Architecture, patterns, decisions (~$0.06)
3. **Opus** - Critical decisions, complex design (~$0.06)
4. **SWE-1.5** - Implementation, testing, refinement (FREE)

## 📊 Credit Budget Planning

### Free User (500 credits/month):
- 250 Sonnet prompts = $15/month
- Or 250 Opus prompts (promo) = $15/month
- Or mix: 125 Sonnet + 125 Opus = $15/month

### Optimization Target:
- **80% SWE-1.5** = FREE
- **20% paid models** = 100 prompts = $6/month
- **Total**: $6/month vs $15/month = **60% savings**

## 🛠️ Max Features for Optimization

### Built-in Cost Awareness
- Session scoreboard tracks credits in real-time
- Model tier recommendations prevent overspending
- "Heavy session" warnings at 400 credits

### Rabbithole Prevention
- Stops you from wasting credits on tangents
- "Is this on the milestone?" keeps focus
- 30-min rule prevents endless debugging

### /humanify for Learning
- Extract insights from expensive sessions
- Reinforce learning to avoid repeat mistakes
- Build knowledge to reduce future expensive prompts

## 🎯 The Ultimate Goal

**Zero cost for 80% of work, maximum power for 20%**

1. **Master SWE-1.5** - Break tasks down, iterate, refine
2. **Strategic upgrades** - Use expensive models for high-leverage decisions
3. **Extract maximum value** - Prompt stacking, context loading, parallel execution
4. **Learn and reinforce** - Use /humanify to avoid repeating expensive mistakes

## 🔄 Continuous Improvement

Track your optimization:
- Weekly credit spend analysis
- Task complexity vs model choice
- Success rate of SWE-1.5 attempts
- ROI of expensive model usage

The goal: Get better at using SWE-1.5, smarter about when to upgrade, and maximize value when you do.
