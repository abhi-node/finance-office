# LibreOffice AI Writing Assistant: LangGraph Multi-Agent System

## Overview

This package implements a sophisticated multi-agent architecture using LangGraph for the LibreOffice AI Writing Assistant. The system provides intelligent document processing capabilities through coordinated specialist agents while maintaining optimal performance and user experience.

## Architecture

### Core Components

- **DocumentMasterAgent**: Intelligent supervisor that analyzes user requests and routes to appropriate specialist agents
- **ContextAnalysisAgent**: Document understanding and semantic analysis 
- **ContentGenerationAgent**: AI-powered content creation and enhancement
- **FormattingAgent**: Professional document styling and layout
- **DataIntegrationAgent**: External API integration for financial data
- **ValidationAgent**: Quality assurance and compliance checking
- **ExecutionAgent**: LibreOffice UNO service integration for document operations

### Performance Optimization

The system implements three performance-optimized workflow paths:

- **Simple Operations (1-2 seconds)**: Lightweight agent chains for basic document operations
- **Moderate Operations (2-4 seconds)**: Focused agent subsets for content generation and formatting
- **Complex Operations (3-5 seconds)**: Full agent orchestration with parallel processing

## Directory Structure

```
langgraph-agents/
├── agents/              # Agent implementations
├── state/              # Shared state management  
├── workflow/           # LangGraph configurations
├── utils/              # Common utilities
├── tests/              # Test suite
├── config.py           # Configuration management
├── requirements.txt    # Dependencies
└── README.md          # This file
```

## Installation

1. Install Python dependencies:
```bash
pip install -r requirements.txt
```

2. Configure environment variables:
```bash
export OPENAI_API_KEY="your_openai_key"
export ANTHROPIC_API_KEY="your_anthropic_key"  
# Alpha Vantage integration removed - using Yahoo Finance as default
```

3. Optional performance tuning:
```bash
export AGENT_MAX_MEMORY_MB="200"
export AGENT_MAX_CPU_PERCENT="10.0"
export AGENT_SIMPLE_TARGET="2.0"
export AGENT_MODERATE_TARGET="4.0" 
export AGENT_COMPLEX_TARGET="5.0"
```

## Integration with LibreOffice

This agent system integrates with the existing LibreOffice C++ infrastructure:

- **C++ AgentCoordinator**: Located in `sw/source/core/ai/AgentCoordinator.cxx`
- **UI Integration**: AIPanel in `sw/source/ui/sidebar/ai/AIPanel.cxx`
- **UNO Services**: Bridge operations in `sw/source/core/ai/operations/`

The Python agents communicate with LibreOffice through the PyUNO bridge, converting document context and operations between C++ and Python formats.

## Development Status

This is the foundational directory structure for Task 11.1. Subsequent tasks will implement:

- Task 11.2: Shared DocumentState management system
- Task 11.3: Agent base classes and common interfaces  
- Task 11.4: DocumentMasterAgent orchestration system
- Task 11.5: LangGraph workflow configuration and state transitions

## Testing

Run the test suite:
```bash
pytest tests/ -v --cov=langgraph-agents
```

Performance benchmarks:
```bash
pytest tests/performance/ -v --benchmark-only
```

## Configuration

The system uses the `ConfigManager` class for centralized configuration management. Configuration can be customized through:

- Environment variables (see Installation section)
- JSON configuration files  
- Runtime configuration updates

## Contributing

When adding new agents or utilities:

1. Follow the established patterns in existing agent implementations
2. Add comprehensive unit tests and integration tests
3. Update the appropriate `__init__.py` files to export new components
4. Document any new configuration options or environment variables
5. Ensure performance targets are met for your agent's operations

## License

This code is part of the LibreOffice project and follows the LibreOffice licensing terms.