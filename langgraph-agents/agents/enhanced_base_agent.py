"""
Enhanced BaseAgent with Integrated Retry and Error Handling

This module extends the BaseAgent class with the enhanced retry and error handling
capabilities from Tasks 19.1 and 19.2. It provides seamless integration while
maintaining backward compatibility with existing agent implementations.

Key Enhancements:
- Automatic retry logic for agent operations
- Circuit breaker protection for external services
- Enhanced error handling with context tracking
- Performance monitoring with retry correlation
- Graceful degradation capabilities
"""

import asyncio
import time
from contextlib import asynccontextmanager
from typing import Any, Dict, List, Optional, Union, Callable
import logging

# Import base infrastructure
from .base import (
    BaseAgent, AgentResult, AgentCapability, AgentLifecycleState,
    ValidationResult, ToolCallResult, PerformanceMetrics
)

# Import enhanced error handling
from .error_handler import ErrorContext, ErrorSeverity, ErrorCategory
from .error_integration import get_error_integrator, handle_agent_error
from .retry_manager import (
    EnhancedRetryManager, RetryPolicy, BackoffStrategy,
    create_enhanced_retry_manager
)

# Import existing utilities
try:
    from .utils import ErrorRecoveryManager, AgentLogger, PerformanceMonitor
    from state.document_state import DocumentState
    from langchain_core.messages import BaseMessage
except ImportError:
    # Fallback for testing
    ErrorRecoveryManager = Any
    AgentLogger = Any
    PerformanceMonitor = Any
    DocumentState = Dict[str, Any]
    BaseMessage = Any


class EnhancedBaseAgent(BaseAgent):
    """
    Enhanced BaseAgent with integrated retry and error handling capabilities.
    
    This class extends BaseAgent with:
    - Automatic retry logic for operations
    - Circuit breaker protection
    - Enhanced error context tracking
    - Performance monitoring with error correlation
    - Graceful degradation support
    """
    
    def __init__(self,
                 agent_id: str,
                 capabilities: List[AgentCapability],
                 config: Optional[Dict[str, Any]] = None,
                 tools: Optional[Dict[str, Callable]] = None,
                 enable_retry: bool = True,
                 enable_circuit_breakers: bool = True):
        """
        Initialize enhanced base agent.
        
        Args:
            agent_id: Unique identifier for the agent
            capabilities: List of agent capabilities
            config: Agent configuration
            tools: Tool functions
            enable_retry: Enable automatic retry logic
            enable_circuit_breakers: Enable circuit breaker protection
        """
        # Initialize base agent
        super().__init__(agent_id, capabilities, config, tools)
        
        # Enhanced configuration
        self.enable_retry = enable_retry
        self.enable_circuit_breakers = enable_circuit_breakers
        
        # Initialize enhanced error handling
        self.error_integrator = get_error_integrator()
        
        # Initialize retry manager
        if enable_retry:
            existing_recovery = getattr(self, '_create_error_recovery_manager', lambda: None)()
            self.retry_manager = create_enhanced_retry_manager(
                agent_id=agent_id,
                existing_recovery_manager=existing_recovery
            )
            
            # Configure retry policies for common agent operations
            self._configure_retry_policies()
        else:
            self.retry_manager = None
        
        # Enhanced performance tracking
        self.retry_performance_metrics = {
            "operations_with_retry": 0,
            "successful_retries": 0,
            "failed_after_retry": 0,
            "circuit_breaker_triggers": 0
        }
        
        # Operation categorization for retry policies
        self.operation_categories = {
            "process": "agent_coordination",
            "validate_input": "validation",
            "call_tool": "tool_execution",
            "external_api": "network",
            "file_operation": "file_io"
        }
    
    def _configure_retry_policies(self) -> None:
        """Configure retry policies for agent operations."""
        if not self.retry_manager:
            return
        
        # Agent processing operations
        processing_policy = RetryPolicy(
            operation_type="agent_processing",
            max_attempts=2,
            base_delay=1.0,
            max_delay=5.0,
            backoff_strategy=BackoffStrategy.EXPONENTIAL,
            circuit_breaker_enabled=False  # Don't break agent processing
        )
        self.retry_manager.policy_manager.set_policy(processing_policy)
        
        # Tool execution operations
        tool_policy = RetryPolicy(
            operation_type="tool_execution",
            max_attempts=3,
            base_delay=0.5,
            max_delay=10.0,
            backoff_strategy=BackoffStrategy.EXPONENTIAL_JITTER,
            circuit_breaker_enabled=True,
            circuit_breaker_threshold=5
        )
        self.retry_manager.policy_manager.set_policy(tool_policy)
        
        # External API operations
        api_policy = RetryPolicy(
            operation_type="external_api",
            max_attempts=4,
            base_delay=2.0,
            max_delay=30.0,
            backoff_strategy=BackoffStrategy.EXPONENTIAL_JITTER,
            circuit_breaker_enabled=True,
            circuit_breaker_threshold=3,
            timeout_per_attempt=30.0
        )
        self.retry_manager.policy_manager.set_policy(api_policy)
    
    async def execute_with_monitoring(self,
                                    state: DocumentState,
                                    message: Optional[BaseMessage] = None) -> AgentResult:
        """
        Enhanced execute method with retry and error handling.
        
        Args:
            state: Current document state
            message: Optional message to process
            
        Returns:
            AgentResult: Processing results with enhanced error handling
        """
        operation_id = f"{self.agent_id}_execute_{int(time.time())}"
        
        # Use retry logic if enabled
        if self.retry_manager and self.enable_retry:
            try:
                async with self.retry_manager.retry_context(
                    operation_type="agent_processing",
                    operation_id=operation_id
                ):
                    return await self._execute_with_enhanced_monitoring(state, message, operation_id)
            
            except Exception as e:
                # Handle through enhanced error system
                self.retry_performance_metrics["failed_after_retry"] += 1
                return await self._handle_execution_error(e, state, message, operation_id)
        
        else:
            # Fallback to base implementation with enhanced error handling
            try:
                return await self._execute_with_enhanced_monitoring(state, message, operation_id)
            except Exception as e:
                return await self._handle_execution_error(e, state, message, operation_id)
    
    async def _execute_with_enhanced_monitoring(self,
                                              state: DocumentState,
                                              message: Optional[BaseMessage],
                                              operation_id: str) -> AgentResult:
        """Execute with enhanced monitoring and error context."""
        start_time = time.time()
        self.current_operation_id = operation_id
        
        try:
            self.lifecycle_state = AgentLifecycleState.PROCESSING
            self.last_activity_time = time.time()
            
            # Enhanced validation with error context
            validation_result = await self._enhanced_validate_input(state, message)
            if not validation_result.passed:
                return self._create_validation_error_result(operation_id, validation_result)
            
            # Execute main processing with error context
            result = await self.process(state, message)
            
            # Update performance metrics
            execution_time = time.time() - start_time
            self._update_enhanced_performance_metrics(execution_time, True)
            
            # Enhance result with monitoring data
            result.operation_id = operation_id
            result.execution_time = execution_time
            result.validation_results = [validation_result]
            result.performance_metrics = self.performance_metrics
            
            # Store in operation history
            self.operation_history.append(result)
            
            self.lifecycle_state = AgentLifecycleState.READY
            return result
            
        finally:
            self.current_operation_id = None
    
    async def _enhanced_validate_input(self,
                                     state: DocumentState,
                                     message: Optional[BaseMessage]) -> ValidationResult:
        """Enhanced input validation with retry support."""
        if self.retry_manager and self.enable_retry:
            try:
                async with self.retry_manager.retry_context(
                    operation_type="validation",
                    operation_id=f"{self.agent_id}_validate_{int(time.time())}"
                ):
                    return self.validate_input(state, message)
            except Exception as e:
                # Validation failed even with retries
                return ValidationResult(
                    agent_id=self.agent_id,
                    validation_type="input_validation",
                    passed=False,
                    confidence=0.1,
                    issues=[{"type": "validation_error", "message": f"Validation failed: {str(e)}"}]
                )
        else:
            return self.validate_input(state, message)
    
    async def call_tool(self,
                       tool_name: str,
                       **kwargs) -> ToolCallResult:
        """
        Enhanced tool execution with retry logic.
        
        Args:
            tool_name: Name of the tool to execute
            **kwargs: Tool arguments
            
        Returns:
            ToolCallResult: Tool execution results
        """
        if self.retry_manager and self.enable_retry:
            operation_id = f"{self.agent_id}_tool_{tool_name}_{int(time.time())}"
            
            try:
                async with self.retry_manager.retry_context(
                    operation_type="tool_execution",
                    operation_id=operation_id,
                    service_name=f"tool_{tool_name}"
                ):
                    return await self._execute_tool_with_monitoring(tool_name, **kwargs)
            
            except Exception as e:
                self.retry_performance_metrics["failed_after_retry"] += 1
                return ToolCallResult(
                    tool_name=tool_name,
                    success=False,
                    error=f"Tool execution failed after retries: {str(e)}",
                    metadata={"retry_attempted": True, "operation_id": operation_id}
                )
        
        else:
            return await super().call_tool(tool_name, **kwargs)
    
    async def _execute_tool_with_monitoring(self,
                                          tool_name: str,
                                          **kwargs) -> ToolCallResult:
        """Execute tool with enhanced monitoring."""
        start_time = time.time()
        
        if tool_name not in self.tools:
            raise ValueError(f"Tool '{tool_name}' not available")
        
        try:
            # Execute tool function
            tool_function = self.tools[tool_name]
            result = await self._execute_tool_safely(tool_function, **kwargs)
            
            execution_time = time.time() - start_time
            return ToolCallResult(
                tool_name=tool_name,
                success=True,
                result=result,
                execution_time=execution_time,
                metadata={"enhanced_execution": True}
            )
            
        except Exception as e:
            execution_time = time.time() - start_time
            self.logger.error(f"Enhanced tool {tool_name} execution failed: {e}")
            raise  # Re-raise for retry logic
    
    async def _handle_execution_error(self,
                                     error: Exception,
                                     state: DocumentState,
                                     message: Optional[BaseMessage],
                                     operation_id: str) -> AgentResult:
        """Handle execution errors through enhanced error system."""
        context = {
            "agent_id": self.agent_id,
            "operation_id": operation_id,
            "document_state": state,
            "message": str(message) if message else None,
            "retry_enabled": self.enable_retry,
            "circuit_breakers_enabled": self.enable_circuit_breakers
        }
        
        # Handle through integrated error system
        error_response = await handle_agent_error(self, error, context)
        
        # Create enhanced agent result
        return AgentResult(
            agent_id=self.agent_id,
            operation_id=operation_id,
            success=False,
            error=error_response.technical_message,
            metadata={
                "error_response": error_response.to_dict(),
                "enhanced_error_handling": True,
                "retry_statistics": self.get_retry_statistics()
            }
        )
    
    def _create_validation_error_result(self,
                                      operation_id: str,
                                      validation_result: ValidationResult) -> AgentResult:
        """Create agent result for validation errors."""
        return AgentResult(
            agent_id=self.agent_id,
            operation_id=operation_id,
            success=False,
            error=f"Input validation failed: {validation_result.issues}",
            validation_results=[validation_result],
            metadata={"validation_error": True}
        )
    
    def _update_enhanced_performance_metrics(self, execution_time: float, success: bool) -> None:
        """Update enhanced performance metrics."""
        # Update base metrics
        super()._update_performance_metrics(execution_time, success)
        
        # Update retry-specific metrics
        if self.retry_manager:
            if success:
                self.retry_performance_metrics["successful_retries"] += 1
            else:
                self.retry_performance_metrics["failed_after_retry"] += 1
    
    @asynccontextmanager
    async def external_api_context(self, api_name: str, operation: str):
        """Context manager for external API calls with circuit breaker protection."""
        if self.retry_manager and self.enable_circuit_breakers:
            circuit_breaker = self.retry_manager.get_circuit_breaker(f"api_{api_name}")
            
            if not circuit_breaker.can_proceed():
                self.retry_performance_metrics["circuit_breaker_triggers"] += 1
                raise Exception(f"Circuit breaker open for API: {api_name}")
            
            try:
                yield
                circuit_breaker.record_success()
            except Exception as e:
                circuit_breaker.record_failure()
                raise
        else:
            yield
    
    def get_retry_statistics(self) -> Dict[str, Any]:
        """Get comprehensive retry statistics."""
        base_stats = {
            "retry_enabled": self.enable_retry,
            "circuit_breakers_enabled": self.enable_circuit_breakers,
            "retry_performance_metrics": self.retry_performance_metrics.copy()
        }
        
        if self.retry_manager:
            base_stats.update(self.retry_manager.get_statistics())
        
        return base_stats
    
    def update_retry_policy(self, operation_type: str, **updates) -> bool:
        """Update retry policy for specific operation type."""
        if self.retry_manager:
            return self.retry_manager.update_policy(operation_type, **updates)
        return False
    
    def force_circuit_breaker_state(self, service_name: str, state: str) -> bool:
        """Force circuit breaker state (for testing/maintenance)."""
        if self.retry_manager:
            return self.retry_manager.force_circuit_breaker_state(service_name, state)
        return False
    
    def get_performance_summary(self) -> Dict[str, Any]:
        """Get enhanced performance summary with retry information."""
        base_summary = super().get_performance_summary()
        
        # Add retry-specific metrics
        enhanced_summary = {
            **base_summary,
            "retry_statistics": self.get_retry_statistics(),
            "enhanced_capabilities": {
                "automatic_retry": self.enable_retry,
                "circuit_breaker_protection": self.enable_circuit_breakers,
                "enhanced_error_handling": True
            }
        }
        
        return enhanced_summary


class RetryDecoratorMixin:
    """
    Mixin class providing retry decorators for existing agents.
    Use this to enhance existing agents without full inheritance.
    """
    
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.retry_manager = None
        self._setup_retry_manager()
    
    def _setup_retry_manager(self):
        """Setup retry manager for mixin."""
        if hasattr(self, 'agent_id'):
            self.retry_manager = create_enhanced_retry_manager(self.agent_id)
    
    def with_retry(self, operation_type: str = "default"):
        """Decorator for adding retry logic to methods."""
        def decorator(func):
            async def wrapper(*args, **kwargs):
                if self.retry_manager:
                    return await self.retry_manager.with_retry(
                        operation_type=operation_type,
                        operation_func=func,
                        *args,
                        **kwargs
                    )
                else:
                    return await func(*args, **kwargs)
            return wrapper
        return decorator
    
    def with_circuit_breaker(self, service_name: str):
        """Decorator for adding circuit breaker protection."""
        def decorator(func):
            async def wrapper(*args, **kwargs):
                if self.retry_manager:
                    circuit_breaker = self.retry_manager.get_circuit_breaker(service_name)
                    if not circuit_breaker.can_proceed():
                        raise Exception(f"Circuit breaker open for service: {service_name}")
                    
                    try:
                        result = await func(*args, **kwargs)
                        circuit_breaker.record_success()
                        return result
                    except Exception as e:
                        circuit_breaker.record_failure()
                        raise
                else:
                    return await func(*args, **kwargs)
            return wrapper
        return decorator


# Factory function for enhanced agents
def enhance_existing_agent(agent: BaseAgent,
                          enable_retry: bool = True,
                          enable_circuit_breakers: bool = True) -> EnhancedBaseAgent:
    """
    Factory function to enhance existing agent with retry capabilities.
    
    Args:
        agent: Existing agent to enhance
        enable_retry: Enable automatic retry logic
        enable_circuit_breakers: Enable circuit breaker protection
        
    Returns:
        EnhancedBaseAgent: Enhanced agent with retry capabilities
    """
    # Create enhanced agent with same configuration
    enhanced = EnhancedBaseAgent(
        agent_id=agent.agent_id,
        capabilities=agent.capabilities,
        config=agent.config,
        tools=agent.tools,
        enable_retry=enable_retry,
        enable_circuit_breakers=enable_circuit_breakers
    )
    
    # Transfer state and performance data
    enhanced.lifecycle_state = agent.lifecycle_state
    enhanced.performance_metrics = agent.performance_metrics
    enhanced.operation_history = agent.operation_history
    enhanced.error_history = agent.error_history
    
    return enhanced