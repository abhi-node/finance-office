"""
Error Handling Integration Layer for LibreOffice AI Writing Assistant

This module provides the integration layer that connects the new unified error 
handling system with existing ErrorRecoveryManager infrastructure, agent base 
classes, and the broader system architecture. It ensures seamless operation
while enhancing error handling capabilities.

Key Components:
- ErrorHandlingIntegrator: Main integration coordinator
- AgentErrorEnhancer: Enhanced error handling for BaseAgent classes
- StateErrorHandler: Error handling integration with DocumentState
- BridgeErrorCoordinator: Error coordination across Python-C++ bridge
- WorkflowErrorManager: Error handling within LangGraph workflows
"""

import asyncio
import threading
import time
from contextlib import asynccontextmanager, contextmanager
from dataclasses import dataclass, field
from datetime import datetime, timezone
from typing import Any, Dict, List, Optional, Union, Callable, Type
import logging
import traceback

# Import existing infrastructure
try:
    from .base import BaseAgent, AgentResult, AgentError
    from .utils import ErrorRecoveryManager, AgentLogger, PerformanceMonitor
    from .error_handler import (
        UnifiedErrorCoordinator, ErrorContext, ErrorSeverity, 
        ErrorCategory, ErrorResponse, ErrorHandler
    )
    from .error_tracking import ErrorTracker, AuditTrail, DebugSnapshot
    from state.document_state import DocumentState, DocumentStateManager
    from bridge import LangGraphBridge
except ImportError:
    # Fallback for testing
    BaseAgent = Any
    AgentResult = Any
    AgentError = Any
    ErrorRecoveryManager = Any
    AgentLogger = Any
    PerformanceMonitor = Any
    UnifiedErrorCoordinator = Any
    ErrorContext = Any
    ErrorSeverity = Any
    ErrorCategory = Any
    ErrorResponse = Any
    ErrorHandler = Any
    ErrorTracker = Any
    AuditTrail = Any
    DebugSnapshot = Any
    DocumentState = Dict[str, Any]
    DocumentStateManager = Any
    LangGraphBridge = Any


@dataclass
class ErrorIntegrationConfig:
    """Configuration for error handling integration."""
    # Unified coordinator settings
    enable_unified_coordinator: bool = True
    enable_error_tracking: bool = True
    enable_audit_trail: bool = True
    
    # Integration settings
    integrate_with_existing_recovery: bool = True
    enhance_agent_error_handling: bool = True
    enable_bridge_error_coordination: bool = True
    enable_workflow_error_management: bool = True
    
    # Performance settings
    max_concurrent_error_handling: int = 10
    error_handling_timeout: float = 30.0
    enable_async_error_processing: bool = True
    
    # Monitoring settings
    enable_error_metrics: bool = True
    enable_performance_tracking: bool = True
    enable_pattern_analysis: bool = True
    
    # Storage settings
    error_storage_path: str = "error_data"
    audit_storage_path: str = "audit_data"
    enable_persistence: bool = True
    
    # Escalation settings
    enable_automatic_escalation: bool = True
    escalation_thresholds: Dict[str, int] = field(default_factory=lambda: {
        "critical": 1,
        "high": 3,
        "medium": 10,
        "low": 50
    })


class ErrorHandlingIntegrator:
    """
    Main integration coordinator that connects all error handling components
    and ensures seamless operation with existing infrastructure.
    """
    
    def __init__(self, config: Optional[ErrorIntegrationConfig] = None):
        """
        Initialize error handling integrator.
        
        Args:
            config: Error integration configuration
        """
        self.config = config or ErrorIntegrationConfig()
        
        # Core error handling components
        self.unified_coordinator: Optional[UnifiedErrorCoordinator] = None
        self.error_tracker: Optional[ErrorTracker] = None
        self.audit_trail: Optional[AuditTrail] = None
        
        # Integration components
        self.agent_enhancer: Optional[AgentErrorEnhancer] = None
        self.state_error_handler: Optional[StateErrorHandler] = None
        self.bridge_coordinator: Optional[BridgeErrorCoordinator] = None
        self.workflow_manager: Optional[WorkflowErrorManager] = None
        
        # Existing systems integration
        self.existing_recovery_managers: Dict[str, ErrorRecoveryManager] = {}
        self.agent_loggers: Dict[str, AgentLogger] = {}
        self.performance_monitors: Dict[str, PerformanceMonitor] = {}
        
        # State management
        self.initialization_complete = False
        self.active_error_handlers: Dict[str, Any] = {}
        self.lock = threading.RLock()
        
        # Setup logging
        self.logger = logging.getLogger("error_integration")
        self.logger.setLevel(logging.INFO)
        
        # Initialize components
        self._initialize_components()
    
    def _initialize_components(self) -> None:
        """Initialize all error handling components."""
        try:
            # Initialize unified coordinator
            if self.config.enable_unified_coordinator:
                self.unified_coordinator = UnifiedErrorCoordinator(
                    config={
                        "adaptive_recovery": True,
                        "learning_enabled": True,
                        "escalation_thresholds": self.config.escalation_thresholds
                    }
                )
            
            # Initialize error tracker
            if self.config.enable_error_tracking:
                from pathlib import Path
                self.error_tracker = ErrorTracker(
                    storage_path=Path(self.config.error_storage_path) / "error_tracking.db",
                    persistence_enabled=self.config.enable_persistence
                )
            
            # Initialize audit trail
            if self.config.enable_audit_trail:
                from pathlib import Path
                self.audit_trail = AuditTrail(
                    storage_path=Path(self.config.audit_storage_path) / "audit_trail.db"
                )
            
            # Initialize integration components
            self._initialize_integration_components()
            
            self.initialization_complete = True
            self.logger.info("Error handling integration initialized successfully")
            
        except Exception as e:
            self.logger.error(f"Failed to initialize error handling integration: {e}")
            raise
    
    def _initialize_integration_components(self) -> None:
        """Initialize integration-specific components."""
        # Agent error enhancer
        if self.config.enhance_agent_error_handling:
            self.agent_enhancer = AgentErrorEnhancer(
                unified_coordinator=self.unified_coordinator,
                error_tracker=self.error_tracker,
                audit_trail=self.audit_trail
            )
        
        # State error handler
        self.state_error_handler = StateErrorHandler(
            unified_coordinator=self.unified_coordinator,
            error_tracker=self.error_tracker
        )
        
        # Bridge error coordinator
        if self.config.enable_bridge_error_coordination:
            self.bridge_coordinator = BridgeErrorCoordinator(
                unified_coordinator=self.unified_coordinator,
                error_tracker=self.error_tracker
            )
        
        # Workflow error manager
        if self.config.enable_workflow_error_management:
            self.workflow_manager = WorkflowErrorManager(
                unified_coordinator=self.unified_coordinator,
                error_tracker=self.error_tracker
            )
    
    def register_existing_recovery_manager(self, 
                                         agent_id: str, 
                                         recovery_manager: ErrorRecoveryManager) -> None:
        """Register existing ErrorRecoveryManager for integration."""
        with self.lock:
            self.existing_recovery_managers[agent_id] = recovery_manager
            
            # Integrate with unified coordinator
            if self.unified_coordinator:
                self.unified_coordinator.existing_recovery_manager = recovery_manager
            
            self.logger.info(f"Registered existing recovery manager for agent: {agent_id}")
    
    def register_agent_logger(self, agent_id: str, logger: AgentLogger) -> None:
        """Register agent logger for integrated error tracking."""
        with self.lock:
            self.agent_loggers[agent_id] = logger
            
            # Register with unified coordinator
            if self.unified_coordinator:
                self.unified_coordinator.register_agent_logger(agent_id, logger)
            
            self.logger.info(f"Registered agent logger for agent: {agent_id}")
    
    def register_performance_monitor(self, agent_id: str, monitor: PerformanceMonitor) -> None:
        """Register performance monitor for error correlation."""
        with self.lock:
            self.performance_monitors[agent_id] = monitor
            self.logger.info(f"Registered performance monitor for agent: {agent_id}")
    
    def enhance_agent(self, agent: BaseAgent) -> BaseAgent:
        """
        Enhance an existing agent with unified error handling capabilities.
        
        Args:
            agent: Agent to enhance
            
        Returns:
            BaseAgent: Enhanced agent with integrated error handling
        """
        if not self.initialization_complete:
            self.logger.warning("Integration not complete, cannot enhance agent")
            return agent
        
        if self.agent_enhancer:
            return self.agent_enhancer.enhance_agent(agent)
        
        return agent
    
    async def handle_error(self, 
                          error: Union[Exception, ErrorContext],
                          context: Optional[Dict[str, Any]] = None) -> ErrorResponse:
        """
        Main error handling entry point for the integrated system.
        
        Args:
            error: Error to handle
            context: Additional context
            
        Returns:
            ErrorResponse: Comprehensive error response
        """
        if not self.unified_coordinator:
            # Fallback to basic error handling
            return self._create_fallback_response(error, context)
        
        # Enhance context with integration information
        enhanced_context = self._enhance_error_context(context or {})
        
        # Handle through unified coordinator
        response = await self.unified_coordinator.handle_error(error, enhanced_context)
        
        # Track error if tracker is available
        if self.error_tracker and isinstance(error, ErrorContext):
            self.error_tracker.track_error(error)
        
        # Log audit event if audit trail is available
        if self.audit_trail:
            self.audit_trail.log_event(
                event_type="error_handled",
                event_source="error_integration",
                event_description=f"Handled error: {response.error_context.error_id if response.error_context else 'unknown'}",
                error_id=response.error_context.error_id if response.error_context else None,
                agent_id=enhanced_context.get("agent_id"),
                operation_id=enhanced_context.get("operation_id")
            )
        
        return response
    
    def _enhance_error_context(self, context: Dict[str, Any]) -> Dict[str, Any]:
        """Enhance error context with integration information."""
        enhanced = context.copy()
        
        # Add system-wide context
        enhanced["integration_version"] = "1.0"
        enhanced["integration_components"] = {
            "unified_coordinator": self.unified_coordinator is not None,
            "error_tracker": self.error_tracker is not None,
            "audit_trail": self.audit_trail is not None
        }
        
        # Add performance context if available
        agent_id = context.get("agent_id")
        if agent_id and agent_id in self.performance_monitors:
            monitor = self.performance_monitors[agent_id]
            enhanced["performance_metrics"] = monitor.get_performance_summary()
        
        return enhanced
    
    def _create_fallback_response(self, 
                                error: Union[Exception, ErrorContext], 
                                context: Optional[Dict[str, Any]]) -> ErrorResponse:
        """Create fallback response when unified coordinator is not available."""
        if isinstance(error, Exception):
            error_message = str(error)
        else:
            error_message = error.error_message
        
        return ErrorResponse(
            success=False,
            user_message="An error occurred. Please try again.",
            technical_message=f"Fallback error handling: {error_message}",
            suggested_actions=["Try again", "Contact support if problem persists"],
            recovery_options=["retry"],
            retry_allowed=True
        )
    
    @asynccontextmanager
    async def error_handling_context(self, context: Dict[str, Any]):
        """Async context manager for automatic error handling."""
        try:
            yield
        except Exception as e:
            response = await self.handle_error(e, context)
            # Log response but don't suppress the exception
            self.logger.info(f"Handled error in context: {response.error_context.error_id if response.error_context else 'unknown'}")
            raise
    
    def get_integration_status(self) -> Dict[str, Any]:
        """Get comprehensive integration status."""
        return {
            "initialization_complete": self.initialization_complete,
            "components": {
                "unified_coordinator": self.unified_coordinator is not None,
                "error_tracker": self.error_tracker is not None,
                "audit_trail": self.audit_trail is not None,
                "agent_enhancer": self.agent_enhancer is not None,
                "state_error_handler": self.state_error_handler is not None,
                "bridge_coordinator": self.bridge_coordinator is not None,
                "workflow_manager": self.workflow_manager is not None
            },
            "registered_systems": {
                "recovery_managers": len(self.existing_recovery_managers),
                "agent_loggers": len(self.agent_loggers),
                "performance_monitors": len(self.performance_monitors)
            },
            "statistics": {
                "total_errors": self.error_tracker.get_tracking_statistics() if self.error_tracker else {},
                "coordinator_stats": self.unified_coordinator.get_error_statistics() if self.unified_coordinator else {}
            }
        }


class AgentErrorEnhancer:
    """
    Enhances BaseAgent classes with integrated error handling capabilities.
    """
    
    def __init__(self, 
                 unified_coordinator: Optional[UnifiedErrorCoordinator] = None,
                 error_tracker: Optional[ErrorTracker] = None,
                 audit_trail: Optional[AuditTrail] = None):
        """Initialize agent error enhancer."""
        self.unified_coordinator = unified_coordinator
        self.error_tracker = error_tracker
        self.audit_trail = audit_trail
        self.logger = logging.getLogger("agent_enhancer")
    
    def enhance_agent(self, agent: BaseAgent) -> BaseAgent:
        """
        Enhance agent with integrated error handling.
        
        Args:
            agent: Agent to enhance
            
        Returns:
            BaseAgent: Enhanced agent
        """
        # Store original methods
        original_execute = agent.execute_with_monitoring
        original_call_tool = agent.call_tool
        
        # Create enhanced methods
        async def enhanced_execute_with_monitoring(state: DocumentState, message=None):
            try:
                return await original_execute(state, message)
            except Exception as e:
                # Handle error through integrated system
                context = {
                    "agent_id": agent.agent_id,
                    "operation_id": agent.current_operation_id,
                    "document_state": state,
                    "message": message
                }
                
                if self.unified_coordinator:
                    response = await self.unified_coordinator.handle_error(e, context)
                    
                    # Create enhanced agent result with error information
                    result = AgentResult(
                        agent_id=agent.agent_id,
                        success=False,
                        error=response.technical_message,
                        metadata={"error_response": response.to_dict()}
                    )
                    return result
                else:
                    # Fallback to original error handling
                    raise
        
        async def enhanced_call_tool(tool_name: str, **kwargs):
            try:
                return await original_call_tool(tool_name, **kwargs)
            except Exception as e:
                # Handle tool error
                context = {
                    "agent_id": agent.agent_id,
                    "tool_name": tool_name,
                    "tool_kwargs": kwargs
                }
                
                if self.audit_trail:
                    self.audit_trail.log_event(
                        event_type="tool_error",
                        event_source=f"agent_{agent.agent_id}",
                        event_description=f"Tool {tool_name} failed: {str(e)}",
                        agent_id=agent.agent_id,
                        tool_name=tool_name
                    )
                
                raise
        
        # Replace methods
        agent.execute_with_monitoring = enhanced_execute_with_monitoring
        agent.call_tool = enhanced_call_tool
        
        return agent


class StateErrorHandler:
    """
    Handles errors related to DocumentState management and transitions.
    """
    
    def __init__(self, 
                 unified_coordinator: Optional[UnifiedErrorCoordinator] = None,
                 error_tracker: Optional[ErrorTracker] = None):
        """Initialize state error handler."""
        self.unified_coordinator = unified_coordinator
        self.error_tracker = error_tracker
        self.logger = logging.getLogger("state_error_handler")
    
    async def handle_state_error(self, 
                                error: Exception, 
                                state: DocumentState,
                                operation: str) -> ErrorResponse:
        """Handle state-related errors."""
        context = {
            "error_category": "state_management",
            "document_state": state,
            "operation": operation,
            "timestamp": datetime.now(timezone.utc).isoformat()
        }
        
        if self.unified_coordinator:
            return await self.unified_coordinator.handle_error(error, context)
        else:
            # Fallback error response
            return ErrorResponse(
                success=False,
                user_message="Document state error occurred.",
                technical_message=f"State error in {operation}: {str(error)}",
                suggested_actions=["Try again", "Restart operation"],
                recovery_options=["retry", "rollback_state"]
            )


class BridgeErrorCoordinator:
    """
    Coordinates error handling across the Python-C++ bridge.
    """
    
    def __init__(self, 
                 unified_coordinator: Optional[UnifiedErrorCoordinator] = None,
                 error_tracker: Optional[ErrorTracker] = None):
        """Initialize bridge error coordinator."""
        self.unified_coordinator = unified_coordinator
        self.error_tracker = error_tracker
        self.logger = logging.getLogger("bridge_error_coordinator")
    
    async def handle_bridge_error(self, 
                                 error: Exception,
                                 bridge_context: Dict[str, Any]) -> ErrorResponse:
        """Handle bridge communication errors."""
        context = {
            "error_category": "bridge_communication",
            "bridge_context": bridge_context,
            "timestamp": datetime.now(timezone.utc).isoformat()
        }
        
        if self.unified_coordinator:
            return await self.unified_coordinator.handle_error(error, context)
        else:
            return ErrorResponse(
                success=False,
                user_message="Communication error with LibreOffice.",
                technical_message=f"Bridge error: {str(error)}",
                suggested_actions=["Retry operation", "Restart LibreOffice"],
                recovery_options=["retry", "restart_bridge"]
            )


class WorkflowErrorManager:
    """
    Manages errors within LangGraph workflows.
    """
    
    def __init__(self, 
                 unified_coordinator: Optional[UnifiedErrorCoordinator] = None,
                 error_tracker: Optional[ErrorTracker] = None):
        """Initialize workflow error manager."""
        self.unified_coordinator = unified_coordinator
        self.error_tracker = error_tracker
        self.logger = logging.getLogger("workflow_error_manager")
    
    async def handle_workflow_error(self, 
                                   error: Exception,
                                   workflow_context: Dict[str, Any]) -> ErrorResponse:
        """Handle workflow execution errors."""
        context = {
            "error_category": "workflow_execution",
            "workflow_context": workflow_context,
            "timestamp": datetime.now(timezone.utc).isoformat()
        }
        
        if self.unified_coordinator:
            return await self.unified_coordinator.handle_error(error, context)
        else:
            return ErrorResponse(
                success=False,
                user_message="Workflow execution error.",
                technical_message=f"Workflow error: {str(error)}",
                suggested_actions=["Retry with simpler request", "Try again later"],
                recovery_options=["retry", "simplified_workflow"]
            )


# Global integrator instance
_global_integrator: Optional[ErrorHandlingIntegrator] = None


def get_error_integrator(config: Optional[ErrorIntegrationConfig] = None) -> ErrorHandlingIntegrator:
    """Get global error handling integrator instance."""
    global _global_integrator
    
    if _global_integrator is None:
        _global_integrator = ErrorHandlingIntegrator(config)
    
    return _global_integrator


def initialize_error_integration(config: Optional[ErrorIntegrationConfig] = None) -> ErrorHandlingIntegrator:
    """Initialize error handling integration."""
    return get_error_integrator(config)


# Convenience functions for common integrations
async def handle_agent_error(agent: BaseAgent, 
                            error: Exception, 
                            context: Optional[Dict[str, Any]] = None) -> ErrorResponse:
    """Handle error for a specific agent."""
    integrator = get_error_integrator()
    enhanced_context = context or {}
    enhanced_context["agent_id"] = agent.agent_id
    
    return await integrator.handle_error(error, enhanced_context)


async def handle_state_error(error: Exception, 
                            state: DocumentState, 
                            operation: str) -> ErrorResponse:
    """Handle state-related error."""
    integrator = get_error_integrator()
    
    if integrator.state_error_handler:
        return await integrator.state_error_handler.handle_state_error(error, state, operation)
    else:
        return await integrator.handle_error(error, {
            "document_state": state,
            "operation": operation
        })


def enhance_agent_with_error_handling(agent: BaseAgent) -> BaseAgent:
    """Enhance agent with integrated error handling."""
    integrator = get_error_integrator()
    return integrator.enhance_agent(agent)