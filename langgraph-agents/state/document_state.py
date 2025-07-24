"""
DocumentState Management System for LibreOffice AI Writing Assistant

This module implements the shared state management system that enables all agents 
to coordinate effectively while preserving complete context across complex operations.
The DocumentState serves as the central communication hub between all agents in the
LangGraph multi-agent architecture.

Based on the specifications in diagram.md and agent_architecture.md, this implements
the comprehensive state schema with thread-safe operations, serialization support,
and state history tracking for debugging and rollback capabilities.
"""

import json
import threading
import time
from datetime import datetime, timezone
from typing import Dict, List, Any, Optional, Union, TypedDict
from dataclasses import dataclass, field, asdict
from enum import Enum
from pathlib import Path
import uuid
from copy import deepcopy

# Import LangGraph types (will be available when LangGraph is installed)
try:
    from langchain_core.messages import BaseMessage, HumanMessage, AIMessage
except ImportError:
    # Fallback definitions for development
    BaseMessage = Dict[str, Any]
    HumanMessage = Dict[str, Any] 
    AIMessage = Dict[str, Any]

class AgentStatus(Enum):
    """Status enumeration for agent execution states."""
    IDLE = "idle"
    PROCESSING = "processing"
    COMPLETED = "completed"
    ERROR = "error"
    CANCELLED = "cancelled"

class OperationStatus(Enum):
    """Status enumeration for document operations."""
    PENDING = "pending"
    IN_PROGRESS = "in_progress"
    COMPLETED = "completed"
    FAILED = "failed"
    CANCELLED = "cancelled"

@dataclass
class DocumentReference:
    """Reference to the current LibreOffice document with metadata."""
    title: str = ""
    path: Optional[str] = None
    document_type: str = "text"
    size_bytes: int = 0
    modified_timestamp: Optional[str] = None
    page_count: int = 1
    word_count: int = 0
    language: str = "en-US"

@dataclass
class CursorPosition:
    """Current cursor location and context within the document."""
    paragraph: int = 0
    character: int = 0
    page: int = 1
    section: Optional[str] = None
    table_cell: Optional[Dict[str, Any]] = None
    is_in_table: bool = False
    is_in_header: bool = False
    is_in_footer: bool = False

@dataclass
class DocumentStructure:
    """Complete document organization map and hierarchy."""
    paragraphs: int = 0
    sections: List[Dict[str, Any]] = field(default_factory=list)
    tables: List[Dict[str, Any]] = field(default_factory=list)
    images: List[Dict[str, Any]] = field(default_factory=list)
    charts: List[Dict[str, Any]] = field(default_factory=list)
    headers: List[Dict[str, Any]] = field(default_factory=list)
    footers: List[Dict[str, Any]] = field(default_factory=list)
    styles_used: List[str] = field(default_factory=list)
    outline: List[Dict[str, Any]] = field(default_factory=list)

@dataclass
class FormattingState:
    """Active styles and formatting rules in the document."""
    current_style: Optional[str] = None
    character_formatting: Dict[str, Any] = field(default_factory=dict)
    paragraph_formatting: Dict[str, Any] = field(default_factory=dict)
    page_formatting: Dict[str, Any] = field(default_factory=dict)
    active_templates: List[str] = field(default_factory=list)

@dataclass
class PendingOperation:
    """Individual operation awaiting execution."""
    operation_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    operation_type: str = ""
    parameters: Dict[str, Any] = field(default_factory=dict)
    agent_id: str = ""
    priority: int = 0
    created_timestamp: str = field(default_factory=lambda: datetime.now(timezone.utc).isoformat())
    status: OperationStatus = OperationStatus.PENDING
    dependencies: List[str] = field(default_factory=list)
    estimated_duration: float = 0.0

@dataclass
class CompletedOperation:
    """Historical record of executed operations."""
    operation_id: str
    operation_type: str
    parameters: Dict[str, Any]
    agent_id: str
    result: Dict[str, Any]
    execution_time: float
    completed_timestamp: str
    success: bool
    error_message: Optional[str] = None

@dataclass
class ValidationResult:
    """Quality assessment and validation status."""
    validation_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    validator_agent: str = ""
    validation_type: str = ""  # content, formatting, compliance, etc.
    passed: bool = False
    score: float = 0.0
    issues: List[Dict[str, Any]] = field(default_factory=list)
    recommendations: List[str] = field(default_factory=list)
    timestamp: str = field(default_factory=lambda: datetime.now(timezone.utc).isoformat())

@dataclass
class StateSnapshot:
    """Saved state for error recovery and rollback."""
    snapshot_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    timestamp: str = field(default_factory=lambda: datetime.now(timezone.utc).isoformat())
    description: str = ""
    state_data: Dict[str, Any] = field(default_factory=dict)
    operation_context: Dict[str, Any] = field(default_factory=dict)

@dataclass
class PerformanceMetrics:
    """System performance measurements and resource tracking."""
    response_times: Dict[str, List[float]] = field(default_factory=dict)
    memory_usage_mb: float = 0.0
    cpu_usage_percent: float = 0.0
    api_call_counts: Dict[str, int] = field(default_factory=dict)
    cache_hit_rates: Dict[str, float] = field(default_factory=dict)
    error_rates: Dict[str, float] = field(default_factory=dict)
    throughput_ops_per_second: float = 0.0

class DocumentState(TypedDict):
    """
    Comprehensive shared state for LibreOffice AI Writing Assistant agents.
    
    This TypedDict implementation follows the exact schema specified in 
    agent_architecture.md and provides the central coordination mechanism
    for all agent communication and state management.
    """
    
    # Document Context (from diagram.md lines 112-115)
    current_document: Dict[str, Any]          # LibreOffice document reference and metadata
    cursor_position: Dict[str, Any]           # Current cursor location and context
    selected_text: str                        # Currently selected text content
    document_structure: Dict[str, Any]        # Complete document organization map
    formatting_state: Dict[str, Any]          # Active styles and formatting rules
    
    # Agent Communication (from diagram.md lines 116-117, architecture.md lines 324-327)
    messages: List[BaseMessage]               # Conversation history between user and agents
    current_task: str                         # Active task description and parameters
    task_history: List[Dict[str, Any]]        # Historical task execution log
    agent_status: Dict[str, str]              # Current status of each agent
    
    # Content and Analysis (architecture.md lines 329-332)
    content_analysis: Dict[str, Any]          # Document content semantic analysis
    generated_content: List[Dict[str, Any]]   # Content created by generation agent
    content_suggestions: List[Dict[str, Any]] # Improvement recommendations
    
    # External Integration (diagram.md line 118, architecture.md lines 335-337)
    external_data: Dict[str, Any]             # Data retrieved from external APIs
    research_citations: List[Dict[str, Any]]  # Source attribution and references
    api_usage: Dict[str, Any]                 # External service utilization tracking
    
    # Operations Management (diagram.md lines 119-120, architecture.md lines 340-342)
    pending_operations: List[Dict[str, Any]]  # Queued operations awaiting execution
    completed_operations: List[Dict[str, Any]] # Historical operation log
    validation_results: Dict[str, Any]        # Quality assessment and validation status
    
    # Error Handling and Recovery (architecture.md lines 345-348)
    last_error: Optional[str]                 # Most recent error condition
    retry_count: int                          # Current retry attempt count
    error_recovery: Dict[str, Any]            # Recovery state and options
    rollback_points: List[Dict[str, Any]]     # Saved states for error recovery
    
    # User Interaction (architecture.md lines 351-353)
    user_preferences: Dict[str, Any]          # User configuration and preferences
    interaction_history: List[Dict[str, Any]] # User interaction patterns and feedback
    approval_required: List[Dict[str, Any]]   # Operations requiring user approval
    
    # Performance and Optimization (architecture.md lines 356-358)
    performance_metrics: Dict[str, Any]       # System performance measurements
    resource_utilization: Dict[str, Any]      # Resource usage tracking
    optimization_recommendations: List[str]   # Performance improvement suggestions

class DocumentStateManager:
    """
    Thread-safe manager for DocumentState with serialization, validation,
    and history tracking capabilities.
    
    This class implements the core state management functionality including:
    - Thread-safe state updates using LangGraph's additive patterns
    - State serialization/deserialization for persistence
    - State validation and consistency checking
    - State history tracking for debugging and rollback
    - Performance monitoring and optimization
    """
    
    def __init__(self, initial_state: Optional[DocumentState] = None):
        """Initialize the DocumentStateManager with optional initial state."""
        self._lock = threading.RLock()
        self._state_history: List[StateSnapshot] = []
        self._max_history_size = 50
        self._last_update_time = time.time()
        
        # Initialize state with defaults if not provided
        if initial_state:
            self._state = initial_state
        else:
            self._state = self._create_default_state()
        
        # Create initial snapshot
        self._create_snapshot("Initial state")
    
    def _create_default_state(self) -> DocumentState:
        """Create a default DocumentState with empty values."""
        return DocumentState(
            # Document Context
            current_document=asdict(DocumentReference()),
            cursor_position=asdict(CursorPosition()),
            selected_text="",
            document_structure=asdict(DocumentStructure()),
            formatting_state=asdict(FormattingState()),
            
            # Agent Communication
            messages=[],
            current_task="",
            task_history=[],
            agent_status={},
            
            # Content and Analysis
            content_analysis={},
            generated_content=[],
            content_suggestions=[],
            
            # External Integration
            external_data={},
            research_citations=[],
            api_usage={},
            
            # Operations Management
            pending_operations=[],
            completed_operations=[],
            validation_results={},
            
            # Error Handling and Recovery
            last_error=None,
            retry_count=0,
            error_recovery={},
            rollback_points=[],
            
            # User Interaction
            user_preferences={},
            interaction_history=[],
            approval_required=[],
            
            # Performance and Optimization
            performance_metrics=asdict(PerformanceMetrics()),
            resource_utilization={},
            optimization_recommendations=[]
        )
    
    def get_state(self) -> DocumentState:
        """Get a deep copy of the current state (thread-safe)."""
        with self._lock:
            return deepcopy(self._state)
    
    def update_state(self, updates: Dict[str, Any], create_snapshot: bool = True) -> bool:
        """
        Update state using LangGraph's additive pattern (thread-safe).
        
        Args:
            updates: Dictionary of state updates to apply
            create_snapshot: Whether to create a rollback snapshot
            
        Returns:
            bool: True if update was successful, False otherwise
        """
        with self._lock:
            try:
                # Validate updates before applying
                if not self._validate_updates(updates):
                    return False
                
                # Create snapshot before updating if requested
                if create_snapshot:
                    self._create_snapshot(f"Before update: {list(updates.keys())}")
                
                # Apply updates using additive pattern
                for key, value in updates.items():
                    if key in self._state:
                        if isinstance(self._state[key], list) and isinstance(value, list):
                            # Additive list updates
                            self._state[key].extend(value)
                        elif isinstance(self._state[key], dict) and isinstance(value, dict):
                            # Additive dictionary updates
                            self._state[key].update(value)
                        else:
                            # Direct value replacement
                            self._state[key] = value
                    else:
                        # New key addition
                        self._state[key] = value
                
                self._last_update_time = time.time()
                return True
                
            except Exception as e:
                self._state["last_error"] = f"State update failed: {str(e)}"
                return False
    
    def _validate_updates(self, updates: Dict[str, Any]) -> bool:
        """Validate state updates for consistency and type safety."""
        try:
            # Check for required fields and types
            for key, value in updates.items():
                if key == "messages" and not isinstance(value, list):
                    return False
                elif key == "current_task" and not isinstance(value, str):
                    return False
                elif key == "selected_text" and not isinstance(value, str):
                    return False
                # Add more validation rules as needed
            
            return True
            
        except Exception:
            return False
    
    def add_message(self, message: Union[BaseMessage, Dict[str, Any]]) -> bool:
        """Add a message to the conversation history."""
        return self.update_state({"messages": [message]}, create_snapshot=False)
    
    def add_pending_operation(self, operation: PendingOperation) -> bool:
        """Add a pending operation to the queue."""
        operation_dict = asdict(operation)
        return self.update_state({"pending_operations": [operation_dict]})
    
    def complete_operation(self, operation_id: str, result: Dict[str, Any], 
                          execution_time: float, success: bool = True, 
                          error_message: Optional[str] = None) -> bool:
        """Mark an operation as completed and move to history."""
        with self._lock:
            # Find and remove from pending operations
            pending_ops = self._state["pending_operations"]
            operation = None
            
            for i, op in enumerate(pending_ops):
                if op["operation_id"] == operation_id:
                    operation = pending_ops.pop(i)
                    break
            
            if operation:
                # Create completed operation record
                completed_op = CompletedOperation(
                    operation_id=operation_id,
                    operation_type=operation["operation_type"],
                    parameters=operation["parameters"],
                    agent_id=operation["agent_id"],
                    result=result,
                    execution_time=execution_time,
                    completed_timestamp=datetime.now(timezone.utc).isoformat(),
                    success=success,
                    error_message=error_message
                )
                
                return self.update_state({
                    "completed_operations": [asdict(completed_op)]
                })
            
            return False
    
    def update_agent_status(self, agent_id: str, status: AgentStatus) -> bool:
        """Update the status of a specific agent."""
        return self.update_state({
            "agent_status": {agent_id: status.value}
        }, create_snapshot=False)
    
    def add_validation_result(self, validation: ValidationResult) -> bool:
        """Add a validation result to the state."""
        validation_dict = asdict(validation)
        return self.update_state({
            "validation_results": {validation.validation_id: validation_dict}
        })
    
    def _create_snapshot(self, description: str) -> StateSnapshot:
        """Create a state snapshot for rollback capabilities."""
        snapshot = StateSnapshot(
            description=description,
            state_data=deepcopy(self._state),
            operation_context={
                "timestamp": datetime.now(timezone.utc).isoformat(),
                "pending_ops_count": len(self._state["pending_operations"]),
                "active_agents": len([s for s in self._state["agent_status"].values() 
                                    if s != AgentStatus.IDLE.value])
            }
        )
        
        self._state_history.append(snapshot)
        
        # Maintain history size limit
        if len(self._state_history) > self._max_history_size:
            self._state_history.pop(0)
        
        return snapshot
    
    def rollback_to_snapshot(self, snapshot_id: str) -> bool:
        """Rollback state to a previous snapshot."""
        with self._lock:
            for snapshot in self._state_history:
                if snapshot.snapshot_id == snapshot_id:
                    self._state = deepcopy(snapshot.state_data)
                    self._create_snapshot(f"Rolled back to: {snapshot.description}")
                    return True
            return False
    
    def get_state_history(self) -> List[Dict[str, Any]]:
        """Get the state history for debugging and analysis."""
        with self._lock:
            return [
                {
                    "snapshot_id": snapshot.snapshot_id,
                    "timestamp": snapshot.timestamp,
                    "description": snapshot.description,
                    "operation_context": snapshot.operation_context
                }
                for snapshot in self._state_history
            ]
    
    def serialize_state(self) -> str:
        """Serialize the current state to JSON string."""
        with self._lock:
            try:
                # Create serializable copy
                serializable_state = deepcopy(self._state)
                
                # Handle non-serializable objects
                if "messages" in serializable_state:
                    serializable_state["messages"] = [
                        dict(msg) if hasattr(msg, "__dict__") else msg 
                        for msg in serializable_state["messages"]
                    ]
                
                return json.dumps(serializable_state, indent=2, default=str)
                
            except Exception as e:
                return json.dumps({"error": f"Serialization failed: {str(e)}"})
    
    def deserialize_state(self, state_json: str) -> bool:
        """Deserialize state from JSON string."""
        with self._lock:
            try:
                deserialized_state = json.loads(state_json)
                
                # Validate deserialized state structure
                if self._validate_state_structure(deserialized_state):
                    self._create_snapshot("Before deserialization")
                    self._state = deserialized_state
                    return True
                
                return False
                
            except Exception as e:
                self._state["last_error"] = f"Deserialization failed: {str(e)}"
                return False
    
    def _validate_state_structure(self, state: Dict[str, Any]) -> bool:
        """Validate that a state dictionary has the required structure."""
        required_keys = [
            "current_document", "cursor_position", "selected_text",
            "document_structure", "formatting_state", "messages",
            "current_task", "task_history", "agent_status"
        ]
        
        return all(key in state for key in required_keys)
    
    def get_performance_summary(self) -> Dict[str, Any]:
        """Get a summary of current performance metrics."""
        with self._lock:
            metrics = self._state["performance_metrics"]
            return {
                "memory_usage_mb": metrics.get("memory_usage_mb", 0.0),
                "cpu_usage_percent": metrics.get("cpu_usage_percent", 0.0),
                "active_operations": len(self._state["pending_operations"]),
                "completed_operations": len(self._state["completed_operations"]),
                "active_agents": len([s for s in self._state["agent_status"].values() 
                                    if s != AgentStatus.IDLE.value]),
                "last_update_time": self._last_update_time,
                "state_history_size": len(self._state_history)
            }