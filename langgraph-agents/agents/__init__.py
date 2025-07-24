"""
Agent System Package

This package contains the specialized agent implementations for the LibreOffice 
AI Writing Assistant system. All agents inherit from the BaseAgent class and 
implement specific functionality for document manipulation, content generation,
formatting, data integration, validation, and execution.

Agent Types:
- DocumentMasterAgent: Orchestrates all agent operations and user interaction
- ContextAnalysisAgent: Analyzes document structure and provides context
- ContentGenerationAgent: Generates and enhances document content
- FormattingAgent: Handles document styling and layout operations
- DataIntegrationAgent: Integrates external data sources and APIs
- ValidationAgent: Ensures quality and compliance of operations
- ExecutionAgent: Executes validated operations through UNO services

All agents communicate through the shared DocumentState management system
and coordinate operations through LangGraph's workflow patterns.
"""

# Import base classes and common interfaces
from .base import (
    BaseAgent,
    AgentResult,
    AgentError,
    AgentCapability,
    AgentLifecycleState,
    ToolCallResult,
    ValidationResult as AgentValidationResult,
    PerformanceMetrics as AgentPerformanceMetrics
)

# Import tool integration interfaces
from .tools import (
    DocumentToolkit,
    FinancialDataToolkit,
    ResearchToolkit,
    ValidationToolkit,
    UnoServiceManager
)

# Import utility classes
from .utils import (
    AgentLogger,
    AgentConfigManager,
    PerformanceMonitor,
    StateValidator,
    ErrorRecoveryManager
)

# Import specialized agent implementations
from .document_master import DocumentMasterAgent, OperationComplexity, WorkflowPath
from .context_analysis import ContextAnalysisAgent, AnalysisMode, ContextType, AnalysisRequest, AnalysisResult

# Other agents will be implemented in subsequent tasks  
# from .content_generation import ContentGenerationAgent
# from .formatting import FormattingAgent
# from .data_integration import DataIntegrationAgent
# from .validation import ValidationAgent
# from .execution import ExecutionAgent

__all__ = [
    # Base agent infrastructure
    'BaseAgent',
    'AgentResult',
    'AgentError',
    'AgentCapability',
    'AgentLifecycleState',
    'ToolCallResult',
    'AgentValidationResult',
    'AgentPerformanceMetrics',
    
    # Tool integration
    'DocumentToolkit',
    'FinancialDataToolkit', 
    'ResearchToolkit',
    'ValidationToolkit',
    'UnoServiceManager',
    
    # Utilities
    'AgentLogger',
    'AgentConfigManager',
    'PerformanceMonitor',
    'StateValidator',
    'ErrorRecoveryManager',
    
    # Specialized agents
    'DocumentMasterAgent',
    'OperationComplexity',
    'WorkflowPath',
    'ContextAnalysisAgent',
    'AnalysisMode',
    'ContextType',
    'AnalysisRequest',
    'AnalysisResult',
    
    # Other agents (will be uncommented as implemented)
    # 'ContentGenerationAgent', 
    # 'FormattingAgent',
    # 'DataIntegrationAgent',
    # 'ValidationAgent',
    # 'ExecutionAgent'
]

# Package version and metadata
__version__ = "1.0.0"
__author__ = "LibreOffice AI Writing Assistant Team"
__description__ = "Multi-agent system for intelligent document manipulation"