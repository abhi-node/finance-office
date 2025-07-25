"""
Centralized Error Handling Infrastructure for LibreOffice AI Writing Assistant

This module implements the unified error handling system for Task 19.1, providing
centralized error coordination, standardized error types, severity levels, and
comprehensive error context tracking. It integrates with existing ErrorRecoveryManager
infrastructure while extending capabilities for cross-agent error management.

Key Components:
- UnifiedErrorCoordinator: Central error management across Python and C++ components
- StandardizedErrorTypes: Comprehensive error classification system
- ErrorSeverity: Multi-level severity classification with response strategies
- ErrorContext: Rich context tracking for debugging and audit trails
- CrossLanguageErrorPropagation: Error handling across Python-C++ bridge
"""

import asyncio
import threading
import time
import uuid
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from datetime import datetime, timezone, timedelta
from enum import Enum
from typing import Any, Dict, List, Optional, Union, Callable, Set, Tuple
from pathlib import Path
import logging
import traceback
import json
from contextlib import contextmanager
from collections import defaultdict, deque

# Import existing error handling infrastructure
try:
    from .utils import ErrorRecoveryManager, AgentLogger
    from .base import AgentError, BaseAgent
    from state.document_state import DocumentState
except ImportError:
    # Fallback for testing
    ErrorRecoveryManager = Any
    AgentLogger = Any
    AgentError = Any
    BaseAgent = Any
    DocumentState = Dict[str, Any]


class ErrorSeverity(Enum):
    """
    Standardized error severity levels with associated response strategies.
    """
    CRITICAL = "critical"      # System failure, immediate attention required
    HIGH = "high"              # Major functionality impacted, urgent resolution needed
    MEDIUM = "medium"          # Moderate impact, should be addressed soon
    LOW = "low"               # Minor issues, can be deferred
    INFO = "info"             # Informational, for logging/monitoring only


class ErrorCategory(Enum):
    """
    Comprehensive error categorization for unified handling strategies.
    """
    # System-level errors
    SYSTEM_FAILURE = "system_failure"
    RESOURCE_EXHAUSTION = "resource_exhaustion"
    CONFIGURATION_ERROR = "configuration_error"
    
    # Communication errors
    NETWORK_ERROR = "network_error"
    CONNECTION_TIMEOUT = "connection_timeout"
    BRIDGE_COMMUNICATION = "bridge_communication"
    API_FAILURE = "api_failure"
    
    # Agent-specific errors
    AGENT_INITIALIZATION = "agent_initialization"
    AGENT_COORDINATION = "agent_coordination"
    WORKFLOW_EXECUTION = "workflow_execution"
    STATE_MANAGEMENT = "state_management"
    
    # Data and validation errors
    DATA_VALIDATION = "data_validation"
    CONTENT_VALIDATION = "content_validation"
    INPUT_VALIDATION = "input_validation"
    FORMAT_VALIDATION = "format_validation"
    
    # External service errors
    FINANCIAL_API_ERROR = "financial_api_error"
    LIBREOFFICE_UNO_ERROR = "libreoffice_uno_error"
    EXTERNAL_SERVICE_ERROR = "external_service_error"
    
    # Security and compliance errors
    AUTHENTICATION_ERROR = "authentication_error"
    AUTHORIZATION_ERROR = "authorization_error"
    COMPLIANCE_VIOLATION = "compliance_violation"
    PRIVACY_VIOLATION = "privacy_violation"
    
    # Performance and resource errors
    PERFORMANCE_DEGRADATION = "performance_degradation"
    TIMEOUT_ERROR = "timeout_error"
    MEMORY_ERROR = "memory_error"
    CPU_EXHAUSTION = "cpu_exhaustion"
    
    # Unknown and miscellaneous
    UNKNOWN_ERROR = "unknown_error"
    USER_ERROR = "user_error"


class ErrorRecoveryStrategy(Enum):
    """
    Standardized recovery strategies for different error types.
    """
    IMMEDIATE_RETRY = "immediate_retry"
    EXPONENTIAL_BACKOFF = "exponential_backoff"
    CIRCUIT_BREAKER = "circuit_breaker"
    FALLBACK_SERVICE = "fallback_service"
    GRACEFUL_DEGRADATION = "graceful_degradation"
    USER_INTERVENTION = "user_intervention"
    SYSTEM_RESTART = "system_restart"
    ROLLBACK_STATE = "rollback_state"
    ESCALATE_ERROR = "escalate_error"
    IGNORE_ERROR = "ignore_error"


@dataclass
class ErrorContext:
    """
    Comprehensive error context information for debugging and audit trails.
    """
    # Core identification
    error_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    timestamp: str = field(default_factory=lambda: datetime.now(timezone.utc).isoformat())
    
    # Error classification
    category: ErrorCategory = ErrorCategory.UNKNOWN_ERROR
    severity: ErrorSeverity = ErrorSeverity.MEDIUM
    error_code: Optional[str] = None
    error_message: str = ""
    
    # Context information
    agent_id: Optional[str] = None
    operation_id: Optional[str] = None
    user_request: Optional[str] = None
    document_context: Dict[str, Any] = field(default_factory=dict)
    
    # Technical details
    exception_type: Optional[str] = None
    stack_trace: Optional[str] = None
    system_state: Dict[str, Any] = field(default_factory=dict)
    performance_metrics: Dict[str, Any] = field(default_factory=dict)
    
    # Recovery information
    recovery_strategy: Optional[ErrorRecoveryStrategy] = None
    retry_count: int = 0
    max_retries: int = 3
    recovery_attempted: bool = False
    recovery_successful: bool = False
    
    # Correlation and causality
    related_errors: List[str] = field(default_factory=list)
    root_cause_analysis: Optional[str] = None
    impact_assessment: Dict[str, Any] = field(default_factory=dict)
    
    # User and session context
    user_session_id: Optional[str] = None
    document_id: Optional[str] = None
    workflow_step: Optional[str] = None
    
    # Audit and compliance
    privacy_impact: bool = False
    compliance_implications: List[str] = field(default_factory=list)
    escalation_required: bool = False
    
    # Additional metadata
    metadata: Dict[str, Any] = field(default_factory=dict)


@dataclass
class ErrorResponse:
    """
    Standardized error response format across all system components.
    """
    success: bool = False
    error_context: Optional[ErrorContext] = None
    user_message: str = ""
    technical_message: str = ""
    suggested_actions: List[str] = field(default_factory=list)
    recovery_options: List[str] = field(default_factory=list)
    support_reference: Optional[str] = None
    retry_allowed: bool = True
    fallback_available: bool = False
    estimated_recovery_time: Optional[int] = None  # seconds
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for serialization."""
        return {
            "success": self.success,
            "error_id": self.error_context.error_id if self.error_context else None,
            "severity": self.error_context.severity.value if self.error_context else None,
            "user_message": self.user_message,
            "technical_message": self.technical_message,
            "suggested_actions": self.suggested_actions,
            "recovery_options": self.recovery_options,
            "support_reference": self.support_reference,
            "retry_allowed": self.retry_allowed,
            "fallback_available": self.fallback_available,
            "estimated_recovery_time": self.estimated_recovery_time
        }


class ErrorHandler(ABC):
    """
    Abstract base class for specialized error handlers.
    """
    
    @abstractmethod
    def can_handle(self, error_context: ErrorContext) -> bool:
        """Check if this handler can process the error."""
        pass
    
    @abstractmethod
    async def handle_error(self, error_context: ErrorContext) -> ErrorResponse:
        """Handle the error and return appropriate response."""
        pass
    
    @abstractmethod
    def get_priority(self) -> int:
        """Get handler priority (lower numbers = higher priority)."""
        pass


class NetworkErrorHandler(ErrorHandler):
    """Specialized handler for network and API-related errors."""
    
    def can_handle(self, error_context: ErrorContext) -> bool:
        return error_context.category in [
            ErrorCategory.NETWORK_ERROR,
            ErrorCategory.CONNECTION_TIMEOUT,
            ErrorCategory.API_FAILURE,
            ErrorCategory.FINANCIAL_API_ERROR,
            ErrorCategory.EXTERNAL_SERVICE_ERROR
        ]
    
    async def handle_error(self, error_context: ErrorContext) -> ErrorResponse:
        """Handle network-related errors with appropriate retry strategies."""
        if error_context.retry_count < error_context.max_retries:
            # Implement exponential backoff for network errors
            delay = min(2 ** error_context.retry_count, 30)  # Max 30 seconds
            
            return ErrorResponse(
                success=False,
                error_context=error_context,
                user_message="Network connectivity issue detected. Retrying...",
                technical_message=f"Network error: {error_context.error_message}",
                suggested_actions=[
                    "Check internet connection",
                    f"Automatic retry in {delay} seconds",
                    "Try again later if problem persists"
                ],
                recovery_options=["retry", "use_cached_data", "offline_mode"],
                retry_allowed=True,
                fallback_available=True,
                estimated_recovery_time=delay
            )
        else:
            return ErrorResponse(
                success=False,
                error_context=error_context,
                user_message="Network service unavailable. Please try again later.",
                technical_message=f"Network error after {error_context.retry_count} retries",
                suggested_actions=[
                    "Check internet connection",
                    "Verify service status",
                    "Contact support if problem persists"
                ],
                recovery_options=["use_cached_data", "offline_mode"],
                retry_allowed=False,
                fallback_available=True
            )
    
    def get_priority(self) -> int:
        return 10


class AgentCoordinationErrorHandler(ErrorHandler):
    """Specialized handler for agent coordination and workflow errors."""
    
    def can_handle(self, error_context: ErrorContext) -> bool:
        return error_context.category in [
            ErrorCategory.AGENT_COORDINATION,
            ErrorCategory.WORKFLOW_EXECUTION,
            ErrorCategory.STATE_MANAGEMENT,
            ErrorCategory.AGENT_INITIALIZATION
        ]
    
    async def handle_error(self, error_context: ErrorContext) -> ErrorResponse:
        """Handle agent coordination errors with workflow recovery."""
        if error_context.category == ErrorCategory.AGENT_INITIALIZATION:
            return ErrorResponse(
                success=False,
                error_context=error_context,
                user_message="AI service initialization error. Please wait...",
                technical_message=f"Agent initialization failed: {error_context.error_message}",
                suggested_actions=[
                    "Restarting AI service",
                    "Please wait for initialization to complete"
                ],
                recovery_options=["restart_agent", "fallback_mode"],
                retry_allowed=True,
                fallback_available=True,
                estimated_recovery_time=10
            )
        
        elif error_context.category == ErrorCategory.WORKFLOW_EXECUTION:
            return ErrorResponse(
                success=False,
                error_context=error_context,
                user_message="Processing workflow error. Attempting recovery...",
                technical_message=f"Workflow execution failed: {error_context.error_message}",
                suggested_actions=[
                    "Rolling back to last stable state",
                    "Retrying with simplified workflow"
                ],
                recovery_options=["rollback_state", "simplified_workflow", "manual_intervention"],
                retry_allowed=True,
                fallback_available=True,
                estimated_recovery_time=5
            )
        
        else:
            return ErrorResponse(
                success=False,
                error_context=error_context,
                user_message="AI coordination error. Please try again.",
                technical_message=f"Agent coordination failed: {error_context.error_message}",
                suggested_actions=[
                    "Try a simpler request",
                    "Wait a moment and retry"
                ],
                recovery_options=["retry", "simplified_request"],
                retry_allowed=True,
                fallback_available=False,
                estimated_recovery_time=3
            )
    
    def get_priority(self) -> int:
        return 20


class ValidationErrorHandler(ErrorHandler):
    """Specialized handler for data and content validation errors."""
    
    def can_handle(self, error_context: ErrorContext) -> bool:
        return error_context.category in [
            ErrorCategory.DATA_VALIDATION,
            ErrorCategory.CONTENT_VALIDATION,
            ErrorCategory.INPUT_VALIDATION,
            ErrorCategory.FORMAT_VALIDATION
        ]
    
    async def handle_error(self, error_context: ErrorContext) -> ErrorResponse:
        """Handle validation errors with corrective guidance."""
        return ErrorResponse(
            success=False,
            error_context=error_context,
            user_message="Input validation error. Please check your request.",
            technical_message=f"Validation failed: {error_context.error_message}",
            suggested_actions=[
                "Review and correct your input",
                "Check for required information",
                "Ensure data format is correct"
            ],
            recovery_options=["correct_input", "provide_example", "simplify_request"],
            retry_allowed=True,
            fallback_available=False,
            estimated_recovery_time=0
        )
    
    def get_priority(self) -> int:
        return 5


class SystemErrorHandler(ErrorHandler):
    """Specialized handler for critical system errors."""
    
    def can_handle(self, error_context: ErrorContext) -> bool:
        return error_context.category in [
            ErrorCategory.SYSTEM_FAILURE,
            ErrorCategory.RESOURCE_EXHAUSTION,
            ErrorCategory.MEMORY_ERROR,
            ErrorCategory.CPU_EXHAUSTION
        ]
    
    async def handle_error(self, error_context: ErrorContext) -> ErrorResponse:
        """Handle critical system errors with escalation."""
        return ErrorResponse(
            success=False,
            error_context=error_context,
            user_message="System error encountered. Please contact support.",
            technical_message=f"Critical system error: {error_context.error_message}",
            suggested_actions=[
                "Contact system administrator",
                "Save your work and restart",
                "Report this issue"
            ],
            recovery_options=["escalate", "restart_system", "emergency_backup"],
            retry_allowed=False,
            fallback_available=False,
            support_reference=error_context.error_id
        )
    
    def get_priority(self) -> int:
        return 1


class UnifiedErrorCoordinator:
    """
    Central error coordination system that manages errors across all components
    of the LibreOffice AI Writing Assistant. Integrates with existing 
    ErrorRecoveryManager infrastructure while providing enhanced capabilities.
    """
    
    def __init__(self, 
                 config: Optional[Dict[str, Any]] = None,
                 existing_recovery_manager: Optional[ErrorRecoveryManager] = None):
        """
        Initialize the unified error coordinator.
        
        Args:
            config: Configuration dictionary
            existing_recovery_manager: Existing ErrorRecoveryManager to integrate with
        """
        self.config = config or {}
        self.existing_recovery_manager = existing_recovery_manager
        
        # Error tracking and management
        self.error_history: deque = deque(maxlen=1000)
        self.active_errors: Dict[str, ErrorContext] = {}
        self.error_patterns: Dict[str, List[ErrorContext]] = defaultdict(list)
        self.lock = threading.RLock()
        
        # Error handlers registry
        self.error_handlers: List[ErrorHandler] = []
        self.setup_default_handlers()
        
        # Performance and monitoring
        self.error_statistics: Dict[str, Any] = defaultdict(int)
        self.recovery_success_rate: Dict[ErrorCategory, float] = {}
        
        # Circuit breaker state for different error categories
        self.circuit_breakers: Dict[ErrorCategory, Dict[str, Any]] = {}
        
        # Integration with existing systems
        self.agent_loggers: Dict[str, AgentLogger] = {}
        
        # User notification and escalation
        self.escalation_thresholds = self.config.get("escalation_thresholds", {
            ErrorSeverity.CRITICAL: 1,
            ErrorSeverity.HIGH: 3,
            ErrorSeverity.MEDIUM: 10,
            ErrorSeverity.LOW: 50
        })
        
        # Recovery optimization
        self.adaptive_recovery = self.config.get("adaptive_recovery", True)
        self.learning_enabled = self.config.get("learning_enabled", True)
        
        # Setup logging
        self.logger = logging.getLogger("error_coordinator")
        self.logger.setLevel(logging.INFO)
    
    def setup_default_handlers(self) -> None:
        """Set up default error handlers."""
        default_handlers = [
            SystemErrorHandler(),
            ValidationErrorHandler(),
            NetworkErrorHandler(),
            AgentCoordinationErrorHandler()
        ]
        
        for handler in default_handlers:
            self.register_error_handler(handler)
    
    def register_error_handler(self, handler: ErrorHandler) -> None:
        """Register a new error handler."""
        with self.lock:
            self.error_handlers.append(handler)
            # Sort by priority (lower numbers first)
            self.error_handlers.sort(key=lambda h: h.get_priority())
    
    def register_agent_logger(self, agent_id: str, logger: AgentLogger) -> None:
        """Register an agent logger for integrated error tracking."""
        with self.lock:
            self.agent_loggers[agent_id] = logger
    
    async def handle_error(self, 
                          error: Union[Exception, ErrorContext],
                          context: Optional[Dict[str, Any]] = None) -> ErrorResponse:
        """
        Main error handling entry point for all system components.
        
        Args:
            error: Exception or ErrorContext to handle
            context: Additional context information
            
        Returns:
            ErrorResponse: Comprehensive error response with recovery options
        """
        # Convert to ErrorContext if needed
        if isinstance(error, Exception):
            error_context = self._create_error_context_from_exception(error, context)
        else:
            error_context = error
        
        # Update error tracking
        with self.lock:
            self.error_history.append(error_context)
            self.active_errors[error_context.error_id] = error_context
            self._update_error_statistics(error_context)
        
        # Check circuit breaker state
        if self._is_circuit_breaker_open(error_context.category):
            return self._create_circuit_breaker_response(error_context)
        
        # Find appropriate handler
        handler = self._find_error_handler(error_context)
        if not handler:
            handler = self._get_default_handler()
        
        # Handle error with selected handler
        try:
            response = await handler.handle_error(error_context)
            
            # Update recovery tracking
            self._track_recovery_attempt(error_context, response)
            
            # Integrate with existing ErrorRecoveryManager if available
            if self.existing_recovery_manager:
                self._integrate_with_existing_recovery(error_context, response)
            
            # Check for escalation requirements
            if self._should_escalate_error(error_context):
                response = self._escalate_error(error_context, response)
            
            return response
            
        except Exception as handling_error:
            # Error occurred while handling error - use fallback
            self.logger.error(f"Error handling failed: {handling_error}")
            return self._create_fallback_response(error_context, handling_error)
    
    def _create_error_context_from_exception(self, 
                                           error: Exception, 
                                           context: Optional[Dict[str, Any]] = None) -> ErrorContext:
        """Create ErrorContext from Exception and additional context."""
        context = context or {}
        
        # Categorize error based on type and message
        category = self._categorize_exception(error)
        severity = self._determine_severity(error, category)
        
        error_context = ErrorContext(
            category=category,
            severity=severity,
            error_message=str(error),
            exception_type=type(error).__name__,
            stack_trace=traceback.format_exc(),
            agent_id=context.get("agent_id"),
            operation_id=context.get("operation_id"),
            user_request=context.get("user_request"),
            document_context=context.get("document_context", {}),
            system_state=context.get("system_state", {}),
            performance_metrics=context.get("performance_metrics", {}),
            user_session_id=context.get("user_session_id"),
            document_id=context.get("document_id"),
            workflow_step=context.get("workflow_step"),
            metadata=context.get("metadata", {})
        )
        
        return error_context
    
    def _categorize_exception(self, error: Exception) -> ErrorCategory:
        """Categorize exception based on type and message."""
        error_type = type(error).__name__.lower()
        error_message = str(error).lower()
        
        # Network and connection errors
        if any(term in error_type for term in ['connection', 'network', 'timeout']):
            if 'timeout' in error_type or 'timeout' in error_message:
                return ErrorCategory.CONNECTION_TIMEOUT
            return ErrorCategory.NETWORK_ERROR
        
        # Validation errors
        if any(term in error_type for term in ['validation', 'invalid', 'format']):
            return ErrorCategory.DATA_VALIDATION
        
        # Memory and resource errors
        if any(term in error_type for term in ['memory', 'resource']):
            return ErrorCategory.RESOURCE_EXHAUSTION
        
        # API and external service errors
        if any(term in error_message for term in ['api', 'service', 'external']):
            return ErrorCategory.API_FAILURE
        
        # Default categorization
        return ErrorCategory.UNKNOWN_ERROR
    
    def _determine_severity(self, error: Exception, category: ErrorCategory) -> ErrorSeverity:
        """Determine error severity based on type and category."""
        error_type = type(error).__name__
        
        # Critical errors
        if category in [ErrorCategory.SYSTEM_FAILURE, ErrorCategory.RESOURCE_EXHAUSTION]:
            return ErrorSeverity.CRITICAL
        
        # High severity errors
        if category in [ErrorCategory.AGENT_COORDINATION, ErrorCategory.BRIDGE_COMMUNICATION]:
            return ErrorSeverity.HIGH
        
        # Medium severity errors
        if category in [ErrorCategory.NETWORK_ERROR, ErrorCategory.API_FAILURE]:
            return ErrorSeverity.MEDIUM
        
        # Low severity errors
        if category in [ErrorCategory.DATA_VALIDATION, ErrorCategory.INPUT_VALIDATION]:
            return ErrorSeverity.LOW
        
        # Default to medium
        return ErrorSeverity.MEDIUM
    
    def _find_error_handler(self, error_context: ErrorContext) -> Optional[ErrorHandler]:
        """Find the most appropriate error handler."""
        for handler in self.error_handlers:
            if handler.can_handle(error_context):
                return handler
        return None
    
    def _get_default_handler(self) -> ErrorHandler:
        """Get default error handler for unhandled errors."""
        return DefaultErrorHandler()
    
    def _is_circuit_breaker_open(self, category: ErrorCategory) -> bool:
        """Check if circuit breaker is open for error category."""
        if category not in self.circuit_breakers:
            return False
        
        breaker = self.circuit_breakers[category]
        if breaker.get("state") == "open":
            # Check if enough time has passed to try again
            if time.time() - breaker.get("last_failure", 0) > breaker.get("timeout", 60):
                breaker["state"] = "half_open"
                return False
            return True
        
        return False
    
    def _create_circuit_breaker_response(self, error_context: ErrorContext) -> ErrorResponse:
        """Create response when circuit breaker is open."""
        return ErrorResponse(
            success=False,
            error_context=error_context,
            user_message="Service temporarily unavailable due to repeated failures.",
            technical_message=f"Circuit breaker open for {error_context.category.value}",
            suggested_actions=["Wait and try again later", "Use alternative approach"],
            recovery_options=["wait", "fallback_mode"],
            retry_allowed=False,
            fallback_available=True
        )
    
    def _update_error_statistics(self, error_context: ErrorContext) -> None:
        """Update error statistics for monitoring and optimization."""
        self.error_statistics["total_errors"] += 1
        self.error_statistics[f"category_{error_context.category.value}"] += 1
        self.error_statistics[f"severity_{error_context.severity.value}"] += 1
        
        if error_context.agent_id:
            self.error_statistics[f"agent_{error_context.agent_id}"] += 1
    
    def _track_recovery_attempt(self, error_context: ErrorContext, response: ErrorResponse) -> None:
        """Track recovery attempt for success rate monitoring."""
        category = error_context.category
        if category not in self.recovery_success_rate:
            self.recovery_success_rate[category] = 0.0
        
        # This would be updated when recovery is actually attempted and completed
        # For now, just track that a recovery was provided
        error_context.recovery_attempted = True
    
    def _integrate_with_existing_recovery(self, 
                                        error_context: ErrorContext, 
                                        response: ErrorResponse) -> None:
        """Integrate with existing ErrorRecoveryManager system."""
        if self.existing_recovery_manager and error_context.agent_id:
            # Create compatible error information for existing system
            try:
                # Add error to existing recovery manager's tracking
                self.existing_recovery_manager._handle_error(
                    Exception(error_context.error_message),
                    {
                        "operation_name": error_context.operation_id or "unknown",
                        "agent_id": error_context.agent_id,
                        "error_category": error_context.category.value
                    }
                )
            except Exception as e:
                self.logger.warning(f"Failed to integrate with existing recovery manager: {e}")
    
    def _should_escalate_error(self, error_context: ErrorContext) -> bool:
        """Determine if error should be escalated."""
        # Check escalation thresholds
        threshold = self.escalation_thresholds.get(error_context.severity, float('inf'))
        category_errors = len([e for e in self.error_history 
                              if e.category == error_context.category])
        
        if category_errors >= threshold:
            return True
        
        # Always escalate critical errors
        if error_context.severity == ErrorSeverity.CRITICAL:
            return True
        
        # Check if error has privacy or compliance implications
        if error_context.privacy_impact or error_context.compliance_implications:
            return True
        
        return error_context.escalation_required
    
    def _escalate_error(self, error_context: ErrorContext, response: ErrorResponse) -> ErrorResponse:
        """Escalate error with appropriate modifications."""
        response.support_reference = error_context.error_id
        response.suggested_actions.append("Contact system administrator")
        response.suggested_actions.append(f"Reference ID: {error_context.error_id}")
        
        # Log escalation
        self.logger.critical(f"Error escalated: {error_context.error_id} - {error_context.error_message}")
        
        return response
    
    def _create_fallback_response(self, 
                                error_context: ErrorContext, 
                                handling_error: Exception) -> ErrorResponse:
        """Create fallback response when error handling fails."""
        return ErrorResponse(
            success=False,
            error_context=error_context,
            user_message="An unexpected error occurred. Please try again.",
            technical_message=f"Error handling failed: {str(handling_error)}",
            suggested_actions=["Try again", "Contact support if problem persists"],
            recovery_options=["retry"],
            retry_allowed=True,
            support_reference=error_context.error_id
        )
    
    def get_error_statistics(self) -> Dict[str, Any]:
        """Get comprehensive error statistics."""
        with self.lock:
            recent_errors = [e for e in self.error_history 
                           if datetime.fromisoformat(e.timestamp) > 
                           datetime.now(timezone.utc) - timedelta(hours=1)]
            
            return {
                "total_errors": len(self.error_history),
                "active_errors": len(self.active_errors),
                "recent_errors_1h": len(recent_errors),
                "error_statistics": dict(self.error_statistics),
                "recovery_success_rates": {k.value: v for k, v in self.recovery_success_rate.items()},
                "circuit_breaker_states": {k.value: v for k, v in self.circuit_breakers.items()},
                "handler_count": len(self.error_handlers)
            }
    
    def get_recent_errors(self, count: int = 50) -> List[ErrorContext]:
        """Get recent error contexts."""
        with self.lock:
            return list(self.error_history)[-count:]
    
    @contextmanager
    def error_handling_context(self, context: Dict[str, Any]):
        """Context manager for automatic error handling."""
        try:
            yield
        except Exception as e:
            # Handle error automatically
            asyncio.create_task(self.handle_error(e, context))
            raise


class DefaultErrorHandler(ErrorHandler):
    """Default handler for unhandled errors."""
    
    def can_handle(self, error_context: ErrorContext) -> bool:
        return True
    
    async def handle_error(self, error_context: ErrorContext) -> ErrorResponse:
        return ErrorResponse(
            success=False,
            error_context=error_context,
            user_message="An unexpected error occurred. Please try again.",
            technical_message=f"Unhandled error: {error_context.error_message}",
            suggested_actions=["Try again", "Simplify your request"],
            recovery_options=["retry"],
            retry_allowed=True
        )
    
    def get_priority(self) -> int:
        return 1000  # Lowest priority


# Factory functions for easy integration
def create_unified_error_coordinator(config: Optional[Dict[str, Any]] = None) -> UnifiedErrorCoordinator:
    """Factory function to create unified error coordinator."""
    return UnifiedErrorCoordinator(config)


def create_error_context(error: Exception, **kwargs) -> ErrorContext:
    """Factory function to create error context from exception."""
    coordinator = UnifiedErrorCoordinator()
    return coordinator._create_error_context_from_exception(error, kwargs)