"""
Workflow Configuration Package

This package handles LangGraph workflow configurations, state transitions,
and conditional routing logic for the LibreOffice AI Writing Assistant.

Key Components:
- GraphConfig: LangGraph StateGraph configuration and setup
- RoutingLogic: Intelligent routing based on operation complexity
- StateTransitions: Agent-to-agent state transition management
- WorkflowOptimizer: Performance optimization and parallel execution

The workflow system implements three performance-optimized paths:
- Simple Operations (1-2 seconds): Lightweight agent chains
- Moderate Operations (2-4 seconds): Focused agent subsets  
- Complex Operations (3-5 seconds): Full agent orchestration
"""

# Import implemented workflow configuration classes
from .graph_config import (
    LangGraphWorkflowManager,
    WorkflowConfig,
    WorkflowState,
    NodeExecutionResult,
    create_workflow_manager,
    DEFAULT_WORKFLOW_CONFIG
)

# Future imports (will be implemented in subsequent tasks)
# from .routing_logic import RoutingEngine, ComplexityAnalyzer
# from .state_transitions import TransitionManager
# from .workflow_optimizer import PerformanceOptimizer

__all__ = [
    # Implemented workflow classes
    'LangGraphWorkflowManager',
    'WorkflowConfig', 
    'WorkflowState',
    'NodeExecutionResult',
    'create_workflow_manager',
    'DEFAULT_WORKFLOW_CONFIG',
    
    # Will be uncommented as workflow classes are implemented
    # 'RoutingEngine',
    # 'ComplexityAnalyzer',
    # 'TransitionManager', 
    # 'PerformanceOptimizer'
]