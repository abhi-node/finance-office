"""
Base Agent Classes and Common Interfaces

This module provides the foundational base classes and interfaces that all
specialized agents inherit from. It establishes common patterns for agent
lifecycle management, error handling, tool integration, and communication
with the shared DocumentState system.

The BaseAgent class implements the core functionality required by all agents
including state management, performance monitoring, error recovery, and
integration with LangGraph's workflow patterns.
"""

import asyncio
import time
import uuid
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from datetime import datetime, timezone
from enum import Enum
from typing import Any, Dict, List, Optional, Union, Callable, Set
from pathlib import Path
import logging
import traceback
from concurrent.futures import ThreadPoolExecutor, Future
from contextlib import contextmanager

# Import LangGraph types
try:
    from langchain_core.messages import BaseMessage, HumanMessage, AIMessage
    from langgraph.graph import StateGraph
except ImportError:
    # Fallback definitions for development without LangGraph
    BaseMessage = Dict[str, Any]
    HumanMessage = Dict[str, Any]
    AIMessage = Dict[str, Any]
    StateGraph = Any

# Import DocumentState from our state management system
try:
    from state.document_state import DocumentState, DocumentStateManager, AgentStatus
except ImportError:
    # For testing, create mock types
    DocumentState = Dict[str, Any]
    DocumentStateManager = Any
    AgentStatus = Any

class AgentLifecycleState(Enum):
    """Agent lifecycle state enumeration."""
    UNINITIALIZED = "uninitialized"
    INITIALIZING = "initializing"
    READY = "ready"
    PROCESSING = "processing"
    WAITING = "waiting"
    ERROR = "error"
    STOPPED = "stopped"

class AgentCapability(Enum):
    """Agent capability classification for routing and coordination."""
    DOCUMENT_ANALYSIS = "document_analysis"
    CONTENT_GENERATION = "content_generation"
    FORMATTING = "formatting"
    DATA_INTEGRATION = "data_integration"
    VALIDATION = "validation"
    EXECUTION = "execution"
    ORCHESTRATION = "orchestration"

@dataclass
class ToolCallResult:
    """Result of a tool function call."""
    tool_name: str
    success: bool
    result: Any = None
    error: Optional[str] = None
    execution_time: float = 0.0
    metadata: Dict[str, Any] = field(default_factory=dict)
    timestamp: str = field(default_factory=lambda: datetime.now(timezone.utc).isoformat())

@dataclass
class ValidationResult:
    """Validation result for agent operations."""
    validation_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    agent_id: str = ""
    validation_type: str = ""
    passed: bool = False
    confidence: float = 0.0
    issues: List[Dict[str, Any]] = field(default_factory=list)
    recommendations: List[str] = field(default_factory=list)
    metadata: Dict[str, Any] = field(default_factory=dict)
    timestamp: str = field(default_factory=lambda: datetime.now(timezone.utc).isoformat())

@dataclass
class PerformanceMetrics:
    """Agent performance metrics and monitoring data."""
    agent_id: str = ""
    operation_count: int = 0
    total_execution_time: float = 0.0
    average_execution_time: float = 0.0
    error_count: int = 0
    success_rate: float = 1.0
    memory_usage_mb: float = 0.0
    cpu_usage_percent: float = 0.0
    last_operation_time: Optional[str] = None
    performance_trends: Dict[str, List[float]] = field(default_factory=dict)
    resource_utilization: Dict[str, Any] = field(default_factory=dict)

@dataclass
class AgentResult:
    """Standard result format for agent operations."""
    agent_id: str
    operation_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    success: bool = False
    result: Any = None
    error: Optional[str] = None
    warnings: List[str] = field(default_factory=list)
    metadata: Dict[str, Any] = field(default_factory=dict)
    execution_time: float = 0.0
    timestamp: str = field(default_factory=lambda: datetime.now(timezone.utc).isoformat())
    state_updates: Dict[str, Any] = field(default_factory=dict)
    tool_calls: List[ToolCallResult] = field(default_factory=list)
    validation_results: List[ValidationResult] = field(default_factory=list)
    performance_metrics: Optional[PerformanceMetrics] = None

@dataclass
class AgentError:
    """Structured error information for agent operations."""
    agent_id: str
    error_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    error_type: str = "unknown"
    error_message: str = ""
    error_details: Dict[str, Any] = field(default_factory=dict)
    stack_trace: Optional[str] = None
    context: Dict[str, Any] = field(default_factory=dict)
    timestamp: str = field(default_factory=lambda: datetime.now(timezone.utc).isoformat())
    recoverable: bool = True
    retry_count: int = 0
    max_retries: int = 3

class BaseAgent(ABC):
    """
    Abstract base class for all LibreOffice AI Writing Assistant agents.
    
    This class provides common functionality including state management,
    error handling, performance monitoring, and tool integration. All
    specialized agents inherit from this class and implement their
    specific processing logic.
    """
    
    def __init__(self, 
                 agent_id: str,
                 capabilities: List[AgentCapability],
                 config: Optional[Dict[str, Any]] = None,
                 tools: Optional[Dict[str, Callable]] = None):
        """
        Initialize the base agent.
        
        Args:
            agent_id: Unique identifier for this agent instance
            capabilities: List of capabilities this agent provides
            config: Optional configuration dictionary
            tools: Optional dictionary of tool functions
        """
        self.agent_id = agent_id
        self.capabilities = capabilities
        self.config = config or {}
        self.tools = tools or {}
        
        # Lifecycle management
        self.lifecycle_state = AgentLifecycleState.UNINITIALIZED
        self.initialization_time: Optional[datetime] = None
        self.last_activity_time: Optional[datetime] = None
        
        # Performance tracking
        self.performance_metrics = PerformanceMetrics(agent_id=agent_id)
        self.operation_history: List[AgentResult] = []
        self.error_history: List[AgentError] = []
        
        # State management
        self.state_manager: Optional[DocumentStateManager] = None
        self.current_operation_id: Optional[str] = None
        
        # Tool execution
        self.tool_executor = ThreadPoolExecutor(max_workers=2, thread_name_prefix=f"Agent-{agent_id}")
        
        # Logging
        self.logger = logging.getLogger(f"agent.{agent_id}")
        self.logger.setLevel(logging.INFO)
        
        # Error recovery
        self.max_retries = self.config.get("max_retries", 3)
        self.retry_delay = self.config.get("retry_delay", 1.0)
        
        # Initialize agent-specific components
        self._initialize_agent()
    
    def _initialize_agent(self) -> None:
        """Initialize agent-specific components. Override in subclasses."""
        self.lifecycle_state = AgentLifecycleState.INITIALIZING
        self.initialization_time = datetime.now(timezone.utc)
        
        try:
            # Perform any agent-specific initialization
            self._setup_tools()
            self._load_configuration()
            self._initialize_resources()
            
            self.lifecycle_state = AgentLifecycleState.READY
            self.logger.info(f"Agent {self.agent_id} initialized successfully")
            
        except Exception as e:
            self.lifecycle_state = AgentLifecycleState.ERROR
            self.logger.error(f"Agent {self.agent_id} initialization failed: {e}")
            raise
    
    def _setup_tools(self) -> None:
        """Setup agent-specific tools. Override in subclasses."""
        pass
    
    def _load_configuration(self) -> None:
        """Load agent-specific configuration. Override in subclasses."""
        pass
    
    def _initialize_resources(self) -> None:
        """Initialize agent-specific resources. Override in subclasses."""
        pass
    
    @abstractmethod
    async def process(self, 
                     state: DocumentState, 
                     message: Optional[BaseMessage] = None) -> AgentResult:
        """
        Process a user request or state update.
        
        This is the main processing method that each agent must implement.
        It receives the current document state and an optional message,
        performs its specific operations, and returns results.
        
        Args:
            state: Current document state with all context information
            message: Optional message that triggered this processing
            
        Returns:
            AgentResult: Results of the processing operation
        """
        pass
    
    @abstractmethod
    def validate_input(self, 
                      state: DocumentState, 
                      message: Optional[BaseMessage] = None) -> ValidationResult:
        """
        Validate input before processing.
        
        Args:
            state: Document state to validate
            message: Optional message to validate
            
        Returns:
            ValidationResult: Validation outcome and details
        """
        pass
    
    @abstractmethod
    def get_required_tools(self) -> List[str]:
        """
        Get list of required tools for this agent.
        
        Returns:
            List of tool names required by this agent
        """
        pass
    
    async def execute_with_monitoring(self, 
                                    state: DocumentState, 
                                    message: Optional[BaseMessage] = None) -> AgentResult:
        """
        Execute agent processing with comprehensive monitoring and error handling.
        
        Args:
            state: Current document state
            message: Optional message to process
            
        Returns:
            AgentResult: Comprehensive results including performance metrics
        """
        operation_id = str(uuid.uuid4())
        self.current_operation_id = operation_id
        start_time = time.time()
        
        try:
            self.lifecycle_state = AgentLifecycleState.PROCESSING
            self.last_activity_time = datetime.now(timezone.utc)
            
            # Validate input
            validation_result = self.validate_input(state, message)
            if not validation_result.passed:
                return self._create_error_result(
                    operation_id, 
                    f"Input validation failed: {validation_result.issues}",
                    execution_time=time.time() - start_time
                )
            
            # Execute main processing
            result = await self.process(state, message)
            
            # Update performance metrics
            execution_time = time.time() - start_time
            self._update_performance_metrics(execution_time, True)
            
            # Enhance result with monitoring data
            result.operation_id = operation_id
            result.execution_time = execution_time
            result.validation_results = [validation_result]
            result.performance_metrics = self.performance_metrics
            
            # Store in operation history
            self.operation_history.append(result)
            
            self.lifecycle_state = AgentLifecycleState.READY
            return result
            
        except Exception as e:
            execution_time = time.time() - start_time
            self._update_performance_metrics(execution_time, False)
            
            error_result = self._handle_processing_error(e, operation_id, execution_time)
            self.error_history.append(self._create_agent_error(e, operation_id))
            
            self.lifecycle_state = AgentLifecycleState.ERROR
            return error_result
            
        finally:
            self.current_operation_id = None
    
    def _create_error_result(self, 
                           operation_id: str, 
                           error_message: str, 
                           execution_time: float = 0.0) -> AgentResult:
        """Create an error result with proper formatting."""
        return AgentResult(
            agent_id=self.agent_id,
            operation_id=operation_id,
            success=False,
            error=error_message,
            execution_time=execution_time,
            performance_metrics=self.performance_metrics
        )
    
    def _handle_processing_error(self, 
                               error: Exception, 
                               operation_id: str, 
                               execution_time: float) -> AgentResult:
        """Handle processing errors with appropriate logging and formatting."""
        error_message = f"Agent {self.agent_id} processing failed: {str(error)}"
        self.logger.error(error_message, exc_info=True)
        
        return AgentResult(
            agent_id=self.agent_id,
            operation_id=operation_id,
            success=False,
            error=error_message,
            execution_time=execution_time,
            metadata={"exception_type": type(error).__name__},
            performance_metrics=self.performance_metrics
        )
    
    def _create_agent_error(self, error: Exception, operation_id: str) -> AgentError:
        """Create structured error information."""
        return AgentError(
            agent_id=self.agent_id,
            error_type=type(error).__name__,
            error_message=str(error),
            stack_trace=traceback.format_exc(),
            context={"operation_id": operation_id},
            recoverable=not isinstance(error, (SystemExit, KeyboardInterrupt))
        )
    
    def _update_performance_metrics(self, execution_time: float, success: bool) -> None:
        """Update agent performance metrics."""
        self.performance_metrics.operation_count += 1
        self.performance_metrics.total_execution_time += execution_time
        self.performance_metrics.average_execution_time = (
            self.performance_metrics.total_execution_time / self.performance_metrics.operation_count
        )
        
        if not success:
            self.performance_metrics.error_count += 1
        
        self.performance_metrics.success_rate = (
            (self.performance_metrics.operation_count - self.performance_metrics.error_count) /
            self.performance_metrics.operation_count
        )
        
        self.performance_metrics.last_operation_time = datetime.now(timezone.utc).isoformat()
    
    async def call_tool(self, 
                       tool_name: str, 
                       **kwargs) -> ToolCallResult:
        """
        Execute a tool function with error handling and monitoring.
        
        Args:
            tool_name: Name of the tool to execute
            **kwargs: Arguments to pass to the tool
            
        Returns:
            ToolCallResult: Results of the tool execution
        """
        start_time = time.time()
        
        if tool_name not in self.tools:
            return ToolCallResult(
                tool_name=tool_name,
                success=False,
                error=f"Tool '{tool_name}' not available",
                execution_time=time.time() - start_time
            )
        
        try:
            # Execute tool function
            tool_function = self.tools[tool_name]
            result = await self._execute_tool_safely(tool_function, **kwargs)
            
            execution_time = time.time() - start_time
            return ToolCallResult(
                tool_name=tool_name,
                success=True,
                result=result,
                execution_time=execution_time
            )
            
        except Exception as e:
            execution_time = time.time() - start_time
            self.logger.error(f"Tool {tool_name} execution failed: {e}")
            
            return ToolCallResult(
                tool_name=tool_name,
                success=False,
                error=str(e),
                execution_time=execution_time,
                metadata={"exception_type": type(e).__name__}
            )
    
    async def _execute_tool_safely(self, tool_function: Callable, **kwargs) -> Any:
        """Execute tool function with proper async handling."""
        if asyncio.iscoroutinefunction(tool_function):
            return await tool_function(**kwargs)
        else:
            # Execute synchronous function in thread pool
            loop = asyncio.get_event_loop()
            return await loop.run_in_executor(self.tool_executor, tool_function, **kwargs)
    
    def set_state_manager(self, state_manager: DocumentStateManager) -> None:
        """Set the document state manager for this agent."""
        self.state_manager = state_manager
    
    def update_agent_status(self, status: AgentStatus) -> bool:
        """Update this agent's status in the shared state."""
        if self.state_manager:
            return self.state_manager.update_agent_status(self.agent_id, status)
        return False
    
    def get_capabilities(self) -> List[AgentCapability]:
        """Get agent capabilities for routing decisions."""
        return self.capabilities.copy()
    
    def can_handle(self, capability: AgentCapability) -> bool:
        """Check if agent can handle a specific capability."""
        return capability in self.capabilities
    
    def get_performance_summary(self) -> Dict[str, Any]:
        """Get comprehensive performance summary."""
        return {
            "agent_id": self.agent_id,
            "lifecycle_state": self.lifecycle_state.value,
            "capabilities": [cap.value for cap in self.capabilities],
            "performance_metrics": {
                "operation_count": self.performance_metrics.operation_count,
                "success_rate": self.performance_metrics.success_rate,
                "average_execution_time": self.performance_metrics.average_execution_time,
                "error_count": self.performance_metrics.error_count
            },
            "recent_operations": len(self.operation_history[-10:]),
            "recent_errors": len(self.error_history[-5:]),
            "last_activity": self.last_activity_time.isoformat() if self.last_activity_time else None
        }
    
    @contextmanager
    def operation_context(self, operation_name: str):
        """Context manager for operation tracking and cleanup."""
        operation_start = time.time()
        self.logger.info(f"Starting operation: {operation_name}")
        
        try:
            yield
        except Exception as e:
            self.logger.error(f"Operation {operation_name} failed: {e}")
            raise
        finally:
            execution_time = time.time() - operation_start
            self.logger.info(f"Operation {operation_name} completed in {execution_time:.2f}s")
    
    def cleanup(self) -> None:
        """Clean up agent resources."""
        try:
            self.lifecycle_state = AgentLifecycleState.STOPPED
            self.tool_executor.shutdown(wait=True)
            self.logger.info(f"Agent {self.agent_id} cleanup completed")
        except Exception as e:
            self.logger.error(f"Error during agent cleanup: {e}")
    
    def __enter__(self):
        """Context manager entry."""
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit with cleanup."""
        self.cleanup()
        
    def __str__(self) -> str:
        """String representation of the agent."""
        return f"{self.__class__.__name__}(id={self.agent_id}, state={self.lifecycle_state.value})"
    
    def __repr__(self) -> str:
        """Detailed representation of the agent."""
        return (f"{self.__class__.__name__}("
                f"id={self.agent_id}, "
                f"capabilities={[c.value for c in self.capabilities]}, "
                f"state={self.lifecycle_state.value}, "
                f"operations={self.performance_metrics.operation_count})")