"""
LibreOffice AI Writing Assistant: LangGraph Multi-Agent System

This package implements the core LangGraph multi-agent architecture for 
intelligent document processing and manipulation within LibreOffice Writer.
The system provides sophisticated AI capabilities through coordinated 
specialist agents while maintaining optimal performance and user experience.

Architecture Overview:
- DocumentMasterAgent: Intelligent supervisor for request analysis and routing
- ContextAnalysisAgent: Document understanding and semantic analysis
- ContentGenerationAgent: AI-powered content creation and enhancement
- FormattingAgent: Professional document styling and layout
- DataIntegrationAgent: External API integration for financial data
- ValidationAgent: Quality assurance and compliance checking
- ExecutionAgent: LibreOffice UNO service integration for document operations

Key Features:
- Intelligent routing with 3 performance tiers (Simple/Moderate/Complex)
- Shared state management for agent coordination
- Real-time progress tracking and cancellation support
- Comprehensive error handling and recovery mechanisms
- Integration with LibreOffice's UNO service architecture
- Support for financial document creation with real-time data
"""

from .agents import *
from .state import *
from .workflow import *
from .utils import *

__version__ = "1.0.0"
__author__ = "LibreOffice AI Integration Team"
__description__ = "LangGraph Multi-Agent System for LibreOffice Writer AI Assistant"

# Export key components for easy access
__all__ = [
    # Core agent classes
    'DocumentMasterAgent',
    'ContextAnalysisAgent', 
    'ContentGenerationAgent',
    'FormattingAgent',
    'DataIntegrationAgent',
    'ValidationAgent',
    'ExecutionAgent',
    
    # State management
    'DocumentState',
    'AgentResult',
    'OperationContext',
    
    # Workflow configuration  
    'create_agent_graph',
    'WorkflowConfig',
    
    # Utilities
    'AgentError',
    'PerformanceMonitor',
    'ConfigManager'
]