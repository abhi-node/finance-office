"""
State Management Package

This package provides shared state management for coordinating data 
between multiple agents in the LibreOffice AI Writing Assistant system.

Key Components:
- DocumentState: Central state container for all agent coordination (TypedDict)
- DocumentStateManager: Thread-safe state update and validation
- EnhancedDocumentStateManager: Advanced state manager with persistence and monitoring
- StatePersistenceManager: State persistence to disk with auto-save
- StateMonitor: State change monitoring and callbacks
- StateSnapshot: Rollback point for error recovery

The state system uses LangGraph's additive update patterns to preserve
historical context while enabling atomic transactions and rollback support.
"""

# Import core state management classes
from .document_state import (
    DocumentState,
    DocumentStateManager,
    DocumentReference,
    CursorPosition,
    DocumentStructure,
    FormattingState,
    PendingOperation,
    CompletedOperation,
    ValidationResult,
    StateSnapshot,
    PerformanceMetrics,
    AgentStatus,
    OperationStatus
)

from .state_manager import (
    EnhancedDocumentStateManager,
    StatePersistenceManager,
    StateMonitor,
    StateChangeEvent,
    create_state_manager
)

__all__ = [
    # Core state types and classes
    'DocumentState',
    'DocumentStateManager',
    'EnhancedDocumentStateManager',
    
    # Data structures
    'DocumentReference',
    'CursorPosition', 
    'DocumentStructure',
    'FormattingState',
    'PendingOperation',
    'CompletedOperation',
    'ValidationResult',
    'StateSnapshot',
    'PerformanceMetrics',
    
    # Enums
    'AgentStatus',
    'OperationStatus',
    
    # Advanced features
    'StatePersistenceManager',
    'StateMonitor',
    'StateChangeEvent',
    
    # Factory function
    'create_state_manager'
]