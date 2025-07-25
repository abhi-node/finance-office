"""
LibreOffice AI Writing Assistant - Agent Package

This package contains all agent implementations for the LibreOffice AI Writing
Assistant, including the enhanced error handling infrastructure implemented
in Task 19.1.

Enhanced Error Handling and Recovery System (Task 19):
- Task 19.1: Core Error Handling Infrastructure
  - UnifiedErrorCoordinator for centralized error management
  - ErrorTracker for comprehensive error tracking and pattern analysis
  - AuditTrail for compliance and debugging support
  - ErrorHandlingIntegrator for seamless integration with existing infrastructure

- Task 19.2: Automatic Retry and Backoff System
  - EnhancedRetryManager with concurrent operation support
  - CircuitBreaker patterns for failing services
  - RetryPolicyManager with configurable strategies
  - Integration with existing ErrorRecoveryManager

- Task 19.3: Graceful Degradation and Rollback
  - TransactionalStateManager with checkpoint-based rollback
  - GracefulDegradationManager with fallback strategies
  - ServiceAvailabilityMonitor for health tracking
  - Multi-level degradation support

- Task 19.4: UI Error Propagation
  - UIErrorPropagator for multi-channel error communication
  - ErrorMessageTranslator for user-friendly messages
  - WebSocketErrorNotifier for real-time updates
  - BridgeErrorCommunicator for C++ integration

- Task 19.5: Operation Cancellation and Progress Tracking
  - OperationCancellationManager with graceful cancellation
  - ProgressTrackingOrchestrator for comprehensive progress monitoring
  - CancellationToken for thread-safe cancellation signaling
  - Integration with all error handling components

Agent Hierarchy:
- BaseAgent: Foundation class with error handling integration
- DocumentMasterAgent: Central orchestrator (original implementation)
- EnhancedDocumentMasterAgent: Enhanced with Task 19.1 error handling
- ContextAnalysisAgent: Document analysis with error tracking
- ContentGenerationAgent: Content creation with error recovery
- FormattingAgent: Document formatting with error handling
- DataIntegrationAgent: External API integration with circuit breakers
- ValidationAgent: Quality assurance with error validation
- ExecutionAgent: Document operations with error recovery

Error Handling Components:
- error_handler: Core error handling infrastructure
- error_tracking: Error tracking and pattern analysis
- error_integration: Integration layer for existing systems
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
    ErrorRecoveryManager,
    create_agent_logger,
    create_performance_monitor,
    create_error_recovery_manager
)

# Enhanced Error Handling Infrastructure (Task 19.1)
from .error_handler import (
    UnifiedErrorCoordinator,
    ErrorContext,
    ErrorSeverity,
    ErrorCategory,
    ErrorResponse,
    ErrorHandler,
    NetworkErrorHandler,
    AgentCoordinationErrorHandler,
    ValidationErrorHandler,
    SystemErrorHandler,
    create_unified_error_coordinator,
    create_error_context
)

from .error_tracking import (
    ErrorTracker,
    AuditTrail,
    ErrorPattern,
    AuditEntry,
    DebugSnapshot,
    ErrorPatternAnalyzer,
    DebugContextCollector,
    create_error_tracker,
    create_audit_trail
)

from .error_integration import (
    ErrorHandlingIntegrator,
    ErrorIntegrationConfig,
    AgentErrorEnhancer,
    StateErrorHandler,
    BridgeErrorCoordinator,
    WorkflowErrorManager,
    get_error_integrator,
    initialize_error_integration,
    handle_agent_error,
    handle_state_error,
    enhance_agent_with_error_handling
)

# Task 19.2: Automatic Retry and Backoff System
from .retry_manager import (
    EnhancedRetryManager,
    RetryPolicy,
    BackoffStrategy,
    CircuitBreaker,
    RetryPolicyManager,
    ConcurrentRetryCoordinator,
    BackoffCalculator,
    create_enhanced_retry_manager,
    create_circuit_breaker
)

from .enhanced_base_agent import (
    EnhancedBaseAgent,
    RetryDecoratorMixin,
    enhance_existing_agent
)

from .retry_integration import (
    RetryAwareBridge,
    RetryAwareDataIntegration,
    NetworkRetryWrapper,
    create_retry_aware_bridge,
    create_retry_aware_data_integration,
    create_network_retry_wrapper,
    with_retry
)

# Task 19.3: Graceful Degradation and Rollback
from .graceful_degradation import (
    TransactionalStateManager,
    GracefulDegradationManager,
    ServiceAvailabilityMonitor,
    DegradationLevel,
    ServiceStatus,
    OperationCheckpoint,
    ServiceAvailability,
    DegradationStrategy,
    FallbackStrategy,
    CachedDataFallback,
    SimplifiedWorkflowFallback,
    OfflineModeFallback,
    create_transactional_state_manager,
    create_graceful_degradation_manager
)

# Task 19.4: UI Error Propagation
from .ui_error_propagation import (
    UIErrorPropagator,
    ErrorMessageTranslator,
    ProgressErrorTracker,
    WebSocketErrorNotifier,
    BridgeErrorCommunicator,
    UIErrorSeverity,
    UIErrorType,
    UIErrorMessage,
    ProgressState,
    ProgressUpdate,
    create_ui_error_propagator,
    create_error_message_translator,
    create_progress_error_tracker
)

# Task 19.5: Operation Cancellation and Progress Tracking
from .operation_cancellation import (
    OperationCancellationManager,
    ProgressTrackingOrchestrator,
    CancellationToken,
    CancellationReason,
    CancellationScope,
    OperationState,
    OperationProgress,
    CancellableOperation,
    OperationCancelledException,
    create_operation_cancellation_manager,
    create_progress_tracking_orchestrator
)

# Import specialized agent implementations
from .document_master import DocumentMasterAgent, OperationComplexity, WorkflowPath
from .enhanced_document_master import (
    EnhancedDocumentMasterAgent,
    EnhancedRequestAnalysis,
    create_enhanced_document_master
)
from .context_analysis import ContextAnalysisAgent, AnalysisMode, ContextType, AnalysisRequest, AnalysisResult

# Import implemented agents
from .content_generation import ContentGenerationAgent
from .formatting import FormattingAgent
from .data_integration import DataIntegrationAgent
from .validation import (
    ValidationAgent, ValidationLevel, ValidationCategory, ValidationSeverity,
    ValidationIssue, ValidationRequest, ValidationResponse, ValidationRule
)

# Import ExecutionAgent
from .execution import (
    ExecutionAgent, OperationType, OperationPriority, OperationStatus,
    ExecutionOperation, ExecutionResult, UnoServiceManager,
    UndoRedoManager, UndoRedoSnapshot, UndoRedoGroup
)

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
    
    # Implemented agents
    'ContentGenerationAgent', 
    'FormattingAgent',
    'DataIntegrationAgent',
    'ValidationAgent',
    'ValidationLevel',
    'ValidationCategory', 
    'ValidationSeverity',
    'ValidationIssue',
    'ValidationRequest',
    'ValidationResponse',
    'ValidationRule',
    
    # ExecutionAgent components
    'ExecutionAgent',
    'OperationType',
    'OperationPriority',
    'OperationStatus',
    'ExecutionOperation',
    'ExecutionResult',
    'UnoServiceManager',
    'UndoRedoManager',
    'UndoRedoSnapshot',
    'UndoRedoGroup'
]

# Package version and metadata
__version__ = "1.0.0"
__author__ = "LibreOffice AI Writing Assistant Team"
__description__ = "Multi-agent system for intelligent document manipulation"