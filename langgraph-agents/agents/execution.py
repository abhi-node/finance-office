"""
ExecutionAgent - LibreOffice UNO Service Bridge and Document Modification Specialist

This agent serves as the final bridge between validated agent operations and actual 
LibreOffice document modifications. It executes all validated operations through 
UNO service interfaces while ensuring proper integration with LibreOffice's 
undo/redo system, threading model, and resource management.

Key Capabilities:
- Execute validated document modifications through UNO service interfaces
- Manage UNO service connections and resource utilization efficiently
- Coordinate with LibreOffice's undo/redo system for operation tracking
- Handle thread safety and operation atomicity for document integrity
- Provide error recovery and rollback capabilities for failed operations
- Integrate with SwEditShell, SwDoc model, and SwWrtShell systems
- Support complex multi-step operations with transaction management
"""

import asyncio
import time
import logging
import json
from datetime import datetime, timezone
from typing import Any, Dict, List, Optional, Union, Tuple, Set
from dataclasses import dataclass, field
from enum import Enum
from abc import ABC, abstractmethod
import uuid
import traceback
import threading
from contextlib import contextmanager
from concurrent.futures import ThreadPoolExecutor, Future

# Import base agent classes
from .base import (
    BaseAgent, 
    AgentCapability, 
    AgentResult, 
    AgentError,
    ValidationResult,
    PerformanceMetrics,
    ToolCallResult
)

# Import shared caching system
from .shared_cache import SharedCacheMixin, CacheType, InvalidationTrigger

# Import state management
try:
    from state.document_state import DocumentState, DocumentStateManager
    from langchain_core.messages import BaseMessage, HumanMessage, AIMessage
except ImportError:
    # Fallback for development
    DocumentState = Dict[str, Any]
    DocumentStateManager = Any
    BaseMessage = Dict[str, Any]
    HumanMessage = Dict[str, Any]
    AIMessage = Dict[str, Any]


class OperationType(Enum):
    """Types of document operations that can be executed"""
    TEXT_INSERT = "text_insert"
    TEXT_FORMAT = "text_format"
    TEXT_DELETE = "text_delete"
    TEXT_REPLACE = "text_replace"
    TABLE_CREATE = "table_create"
    TABLE_MODIFY = "table_modify"
    CHART_CREATE = "chart_create"
    CHART_MODIFY = "chart_modify"
    STYLE_APPLY = "style_apply"
    PAGE_BREAK = "page_break"
    IMAGE_INSERT = "image_insert"
    CUSTOM_OPERATION = "custom_operation"


class OperationPriority(Enum):
    """Priority levels for operation execution"""
    CRITICAL = "critical"      # Must execute immediately
    HIGH = "high"             # Execute before normal operations
    NORMAL = "normal"         # Standard priority
    LOW = "low"              # Execute when system resources available
    BACKGROUND = "background" # Execute during idle time


class OperationStatus(Enum):
    """Status of operation execution"""
    PENDING = "pending"           # Queued for execution
    VALIDATING = "validating"     # Being validated
    VALIDATED = "validated"       # Validation passed
    EXECUTING = "executing"       # Currently being executed
    COMPLETED = "completed"       # Successfully completed
    FAILED = "failed"            # Execution failed
    ROLLED_BACK = "rolled_back"  # Operation was rolled back
    CANCELLED = "cancelled"      # Operation was cancelled


@dataclass
class UnoServiceConnection:
    """Represents a connection to LibreOffice UNO services"""
    service_name: str
    connection_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    created_at: datetime = field(default_factory=lambda: datetime.now(timezone.utc))
    last_used: datetime = field(default_factory=lambda: datetime.now(timezone.utc))
    is_active: bool = True
    connection_object: Any = None
    use_count: int = 0


@dataclass
class OperationContext:
    """Context information for operation execution"""
    document_id: str
    cursor_position: Dict[str, Any] = field(default_factory=dict)
    selection_range: Optional[Dict[str, Any]] = None
    formatting_state: Dict[str, Any] = field(default_factory=dict)
    undo_group_id: Optional[str] = None
    transaction_id: Optional[str] = None
    metadata: Dict[str, Any] = field(default_factory=dict)


@dataclass
class ExecutionOperation:
    """Represents a document operation to be executed"""
    operation_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    operation_type: OperationType = OperationType.CUSTOM_OPERATION
    priority: OperationPriority = OperationPriority.NORMAL
    status: OperationStatus = OperationStatus.PENDING
    
    # Operation parameters
    parameters: Dict[str, Any] = field(default_factory=dict)
    context: Optional[OperationContext] = None
    
    # Execution tracking
    created_at: datetime = field(default_factory=lambda: datetime.now(timezone.utc))
    started_at: Optional[datetime] = None
    completed_at: Optional[datetime] = None
    execution_time_ms: float = 0.0
    
    # Results and error handling
    result: Optional[Dict[str, Any]] = None
    error_message: Optional[str] = None
    rollback_data: Optional[Dict[str, Any]] = None
    
    # Dependencies and relationships
    depends_on: List[str] = field(default_factory=list)  # Operation IDs this depends on
    blocks: List[str] = field(default_factory=list)      # Operation IDs blocked by this
    
    # Validation and approval
    validation_required: bool = True
    validation_passed: bool = False
    approval_required: bool = False
    approval_granted: bool = False


@dataclass
class ExecutionResult:
    """Result of operation execution"""
    operation_id: str
    success: bool
    result_data: Optional[Dict[str, Any]] = None
    error_message: Optional[str] = None
    execution_time_ms: float = 0.0
    undo_information: Optional[Dict[str, Any]] = None
    side_effects: List[Dict[str, Any]] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)
    metadata: Dict[str, Any] = field(default_factory=dict)


class UnoServiceManager:
    """Manages LibreOffice UNO service connections and operations"""
    
    def __init__(self):
        self.connections: Dict[str, UnoServiceConnection] = {}
        self.connection_pool_size = 5
        self.connection_timeout = 300  # 5 minutes
        self.lock = threading.RLock()
        self.executor = ThreadPoolExecutor(max_workers=3, thread_name_prefix="UNO")
    
    @contextmanager
    def get_connection(self, service_name: str):
        """Get a UNO service connection with automatic cleanup"""
        connection = None
        try:
            connection = self._acquire_connection(service_name)
            yield connection.connection_object
        finally:
            if connection:
                self._release_connection(connection)
    
    def _acquire_connection(self, service_name: str) -> UnoServiceConnection:
        """Acquire a connection to the specified UNO service"""
        with self.lock:
            # Look for existing available connection
            for conn in self.connections.values():
                if (conn.service_name == service_name and 
                    conn.is_active and 
                    conn.use_count == 0):
                    conn.use_count += 1
                    conn.last_used = datetime.now(timezone.utc)
                    return conn
            
            # Create new connection if under pool limit
            if len(self.connections) < self.connection_pool_size:
                connection = self._create_connection(service_name)
                self.connections[connection.connection_id] = connection
                connection.use_count = 1
                return connection
            
            # Wait for available connection (simplified)
            # In a real implementation, this would have proper waiting logic
            raise Exception(f"No available connections for service: {service_name}")
    
    def _release_connection(self, connection: UnoServiceConnection):
        """Release a connection back to the pool"""
        with self.lock:
            connection.use_count = max(0, connection.use_count - 1)
            connection.last_used = datetime.now(timezone.utc)
    
    def _create_connection(self, service_name: str) -> UnoServiceConnection:
        """Create a new UNO service connection"""
        # This is a placeholder - in a real implementation, this would
        # establish actual UNO connections to LibreOffice
        connection = UnoServiceConnection(
            service_name=service_name,
            connection_object=f"MockUnoService_{service_name}_{uuid.uuid4()}"
        )
        logging.info(f"Created UNO connection for service: {service_name}")
        return connection
    
    def cleanup_expired_connections(self):
        """Remove expired connections from the pool"""
        with self.lock:
            current_time = datetime.now(timezone.utc)
            expired_connections = []
            
            for conn_id, conn in self.connections.items():
                time_since_use = (current_time - conn.last_used).total_seconds()
                if time_since_use > self.connection_timeout and conn.use_count == 0:
                    expired_connections.append(conn_id)
            
            for conn_id in expired_connections:
                del self.connections[conn_id]
                logging.debug(f"Removed expired UNO connection: {conn_id}")
    
    def shutdown(self):
        """Shutdown the UNO service manager"""
        with self.lock:
            self.connections.clear()
            self.executor.shutdown(wait=True)


class OperationExecutor(ABC):
    """Abstract base class for operation executors"""
    
    def __init__(self, executor_id: str, uno_manager: UnoServiceManager):
        self.executor_id = executor_id
        self.uno_manager = uno_manager
    
    @abstractmethod
    async def execute(self, operation: ExecutionOperation) -> ExecutionResult:
        """Execute the operation and return result"""
        pass
    
    @abstractmethod
    def can_execute(self, operation: ExecutionOperation) -> bool:
        """Check if this executor can handle the operation"""
        pass
    
    @abstractmethod
    def get_undo_information(self, operation: ExecutionOperation) -> Dict[str, Any]:
        """Get information needed for undo operation"""
        pass


class TextOperationExecutor(OperationExecutor):
    """Executor for text-related operations"""
    
    def __init__(self, uno_manager: UnoServiceManager):
        super().__init__("text_executor", uno_manager)
        self.supported_operations = {
            OperationType.TEXT_INSERT,
            OperationType.TEXT_FORMAT,
            OperationType.TEXT_DELETE,
            OperationType.TEXT_REPLACE
        }
    
    def can_execute(self, operation: ExecutionOperation) -> bool:
        """Check if this executor can handle text operations"""
        return operation.operation_type in self.supported_operations
    
    async def execute(self, operation: ExecutionOperation) -> ExecutionResult:
        """Execute text operation through UNO services"""
        start_time = time.time()
        
        try:
            if operation.operation_type == OperationType.TEXT_INSERT:
                result = await self._execute_text_insert(operation)
            elif operation.operation_type == OperationType.TEXT_FORMAT:
                result = await self._execute_text_format(operation)
            elif operation.operation_type == OperationType.TEXT_DELETE:
                result = await self._execute_text_delete(operation)
            elif operation.operation_type == OperationType.TEXT_REPLACE:
                result = await self._execute_text_replace(operation)
            else:
                raise ValueError(f"Unsupported operation type: {operation.operation_type}")
            
            execution_time = (time.time() - start_time) * 1000
            
            return ExecutionResult(
                operation_id=operation.operation_id,
                success=True,
                result_data=result,
                execution_time_ms=execution_time,
                undo_information=self.get_undo_information(operation)
            )
            
        except Exception as e:
            execution_time = (time.time() - start_time) * 1000
            logging.error(f"Text operation execution failed: {e}")
            
            return ExecutionResult(
                operation_id=operation.operation_id,
                success=False,
                error_message=str(e),
                execution_time_ms=execution_time
            )
    
    async def _execute_text_insert(self, operation: ExecutionOperation) -> Dict[str, Any]:
        """Execute text insertion operation"""
        text = operation.parameters.get('text', '')
        position = operation.parameters.get('position', {})
        
        # Simulate UNO service call
        with self.uno_manager.get_connection('com.sun.star.text.TextDocument') as text_service:
            # In a real implementation, this would use actual UNO APIs
            # like textService.getCursor(), cursor.gotoEnd(), cursor.insertString()
            
            logging.info(f"Inserting text: '{text}' at position: {position}")
            
            # Simulate some processing time
            await asyncio.sleep(0.01)
            
            return {
                'operation': 'text_insert',
                'text_inserted': text,
                'insertion_position': position,
                'characters_inserted': len(text)
            }
    
    async def _execute_text_format(self, operation: ExecutionOperation) -> Dict[str, Any]:
        """Execute text formatting operation"""
        formatting = operation.parameters.get('formatting', {})
        range_spec = operation.parameters.get('range', {})
        
        with self.uno_manager.get_connection('com.sun.star.text.TextDocument') as text_service:
            logging.info(f"Applying formatting: {formatting} to range: {range_spec}")
            
            await asyncio.sleep(0.01)
            
            return {
                'operation': 'text_format',
                'formatting_applied': formatting,
                'range_affected': range_spec,
                'format_count': len(formatting)
            }
    
    async def _execute_text_delete(self, operation: ExecutionOperation) -> Dict[str, Any]:
        """Execute text deletion operation"""
        range_spec = operation.parameters.get('range', {})
        
        with self.uno_manager.get_connection('com.sun.star.text.TextDocument') as text_service:
            logging.info(f"Deleting text in range: {range_spec}")
            
            await asyncio.sleep(0.01)
            
            return {
                'operation': 'text_delete',
                'range_deleted': range_spec,
                'characters_deleted': range_spec.get('length', 0)
            }
    
    async def _execute_text_replace(self, operation: ExecutionOperation) -> Dict[str, Any]:
        """Execute text replacement operation"""
        old_text = operation.parameters.get('old_text', '')
        new_text = operation.parameters.get('new_text', '')
        range_spec = operation.parameters.get('range', {})
        
        with self.uno_manager.get_connection('com.sun.star.text.TextDocument') as text_service:
            logging.info(f"Replacing '{old_text}' with '{new_text}' in range: {range_spec}")
            
            await asyncio.sleep(0.01)
            
            return {
                'operation': 'text_replace',
                'old_text': old_text,
                'new_text': new_text,
                'range_affected': range_spec,
                'replacements_made': 1
            }
    
    def get_undo_information(self, operation: ExecutionOperation) -> Dict[str, Any]:
        """Get undo information for text operations"""
        return {
            'operation_type': operation.operation_type.value,
            'reverse_operation': self._get_reverse_operation(operation),
            'original_state': operation.parameters.get('original_state', {}),
            'undo_group_id': operation.context.undo_group_id if operation.context else None
        }
    
    def _get_reverse_operation(self, operation: ExecutionOperation) -> Dict[str, Any]:
        """Determine the reverse operation for undo"""
        if operation.operation_type == OperationType.TEXT_INSERT:
            return {
                'type': 'text_delete',
                'range': operation.parameters.get('insertion_range', {})
            }
        elif operation.operation_type == OperationType.TEXT_DELETE:
            return {
                'type': 'text_insert',
                'text': operation.parameters.get('deleted_text', ''),
                'position': operation.parameters.get('deletion_position', {})
            }
        elif operation.operation_type == OperationType.TEXT_REPLACE:
            return {
                'type': 'text_replace',
                'old_text': operation.parameters.get('new_text', ''),
                'new_text': operation.parameters.get('old_text', ''),
                'range': operation.parameters.get('range', {})
            }
        return {}


class TableOperationExecutor(OperationExecutor):
    """Executor for table-related operations"""
    
    def __init__(self, uno_manager: UnoServiceManager):
        super().__init__("table_executor", uno_manager)
        self.supported_operations = {
            OperationType.TABLE_CREATE,
            OperationType.TABLE_MODIFY
        }
    
    def can_execute(self, operation: ExecutionOperation) -> bool:
        """Check if this executor can handle table operations"""
        return operation.operation_type in self.supported_operations
    
    async def execute(self, operation: ExecutionOperation) -> ExecutionResult:
        """Execute table operation through UNO services"""
        start_time = time.time()
        
        try:
            if operation.operation_type == OperationType.TABLE_CREATE:
                result = await self._execute_table_create(operation)
            elif operation.operation_type == OperationType.TABLE_MODIFY:
                result = await self._execute_table_modify(operation)
            else:
                raise ValueError(f"Unsupported operation type: {operation.operation_type}")
            
            execution_time = (time.time() - start_time) * 1000
            
            return ExecutionResult(
                operation_id=operation.operation_id,
                success=True,
                result_data=result,
                execution_time_ms=execution_time,
                undo_information=self.get_undo_information(operation)
            )
            
        except Exception as e:
            execution_time = (time.time() - start_time) * 1000
            logging.error(f"Table operation execution failed: {e}")
            
            return ExecutionResult(
                operation_id=operation.operation_id,
                success=False,
                error_message=str(e),
                execution_time_ms=execution_time
            )
    
    async def _execute_table_create(self, operation: ExecutionOperation) -> Dict[str, Any]:
        """Execute table creation operation"""
        rows = operation.parameters.get('rows', 2)
        columns = operation.parameters.get('columns', 2)
        data = operation.parameters.get('data', [])
        formatting = operation.parameters.get('formatting', {})
        
        with self.uno_manager.get_connection('com.sun.star.text.TextDocument') as text_service:
            logging.info(f"Creating table: {rows}x{columns} with data")
            
            await asyncio.sleep(0.02)  # Simulate table creation time
            
            return {
                'operation': 'table_create',
                'rows': rows,
                'columns': columns,
                'data_entries': len(data),
                'formatting_applied': len(formatting),
                'table_id': f"table_{uuid.uuid4().hex[:8]}"
            }
    
    async def _execute_table_modify(self, operation: ExecutionOperation) -> Dict[str, Any]:
        """Execute table modification operation"""
        table_id = operation.parameters.get('table_id', '')
        modifications = operation.parameters.get('modifications', {})
        
        with self.uno_manager.get_connection('com.sun.star.text.TextDocument') as text_service:
            logging.info(f"Modifying table: {table_id} with modifications: {modifications}")
            
            await asyncio.sleep(0.01)
            
            return {
                'operation': 'table_modify',
                'table_id': table_id,
                'modifications_applied': len(modifications),
                'modification_types': list(modifications.keys())
            }
    
    def get_undo_information(self, operation: ExecutionOperation) -> Dict[str, Any]:
        """Get undo information for table operations"""
        return {
            'operation_type': operation.operation_type.value,
            'table_snapshot': operation.parameters.get('table_snapshot', {}),
            'undo_group_id': operation.context.undo_group_id if operation.context else None
        }


@dataclass
class UndoRedoSnapshot:
    """Represents a snapshot for undo/redo operations"""
    snapshot_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    operation_id: str = ""
    document_state: Dict[str, Any] = field(default_factory=dict)
    cursor_position: Dict[str, Any] = field(default_factory=dict)
    selection_state: Optional[Dict[str, Any]] = None
    formatting_state: Dict[str, Any] = field(default_factory=dict)
    content_before: Optional[str] = None
    content_after: Optional[str] = None
    timestamp: datetime = field(default_factory=lambda: datetime.now(timezone.utc))
    operation_metadata: Dict[str, Any] = field(default_factory=dict)


@dataclass
class UndoRedoGroup:
    """Groups related operations for undo/redo"""
    group_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    group_name: str = ""
    operations: List[str] = field(default_factory=list)  # operation_ids
    snapshots: List[UndoRedoSnapshot] = field(default_factory=list)
    created_at: datetime = field(default_factory=lambda: datetime.now(timezone.utc))
    is_atomic: bool = True  # If true, all operations must be undone together


class UndoRedoManager:
    """Manages undo/redo operations for document modifications"""
    
    def __init__(self, max_undo_levels: int = 100):
        self.max_undo_levels = max_undo_levels
        self.undo_stack: List[UndoRedoGroup] = []
        self.redo_stack: List[UndoRedoGroup] = []
        self.current_group: Optional[UndoRedoGroup] = None
        self.lock = threading.RLock()
        
        # Document state tracking
        self.document_snapshots: Dict[str, UndoRedoSnapshot] = {}
        self.snapshot_lock = threading.RLock()
    
    def begin_undo_group(self, group_name: str = "", is_atomic: bool = True) -> str:
        """Begin a new undo group for related operations"""
        with self.lock:
            if self.current_group:
                self.end_undo_group()
            
            self.current_group = UndoRedoGroup(
                group_name=group_name or f"Operation Group {len(self.undo_stack) + 1}",
                is_atomic=is_atomic
            )
            
            logging.debug(f"Started undo group: {self.current_group.group_name}")
            return self.current_group.group_id
    
    def end_undo_group(self):
        """End the current undo group and add it to the undo stack"""
        with self.lock:
            if not self.current_group:
                return
            
            if self.current_group.operations:
                # Add to undo stack
                self.undo_stack.append(self.current_group)
                
                # Limit undo stack size
                if len(self.undo_stack) > self.max_undo_levels:
                    removed_group = self.undo_stack.pop(0)
                    self._cleanup_group_snapshots(removed_group)
                
                # Clear redo stack (new operation invalidates redo)
                for group in self.redo_stack:
                    self._cleanup_group_snapshots(group)
                self.redo_stack.clear()
                
                logging.debug(f"Ended undo group: {self.current_group.group_name} "
                            f"with {len(self.current_group.operations)} operations")
            
            self.current_group = None
    
    def create_snapshot(self, operation: ExecutionOperation, content_before: Optional[str] = None) -> UndoRedoSnapshot:
        """Create a snapshot for undo/redo"""
        snapshot = UndoRedoSnapshot(
            operation_id=operation.operation_id,
            document_state={
                'document_id': operation.context.document_id if operation.context else 'unknown',
                'operation_type': operation.operation_type.value,
                'parameters': operation.parameters.copy()
            },
            cursor_position=operation.context.cursor_position.copy() if operation.context else {},
            selection_state=operation.context.selection_range.copy() if operation.context and operation.context.selection_range else None,
            formatting_state=operation.context.formatting_state.copy() if operation.context else {},
            content_before=content_before,
            operation_metadata={
                'priority': operation.priority.value,
                'timestamp': operation.created_at.isoformat()
            }
        )
        
        # Store snapshot
        with self.snapshot_lock:
            self.document_snapshots[snapshot.snapshot_id] = snapshot
        
        # Add to current group
        with self.lock:
            if self.current_group:
                self.current_group.operations.append(operation.operation_id)
                self.current_group.snapshots.append(snapshot)
        
        return snapshot
    
    def update_snapshot_after_execution(self, snapshot_id: str, content_after: Optional[str] = None, result_data: Optional[Dict[str, Any]] = None):
        """Update snapshot with post-execution data"""
        with self.snapshot_lock:
            if snapshot_id in self.document_snapshots:
                snapshot = self.document_snapshots[snapshot_id]
                snapshot.content_after = content_after
                if result_data:
                    snapshot.operation_metadata.update({
                        'execution_result': result_data,
                        'completed_at': datetime.now(timezone.utc).isoformat()
                    })
    
    def can_undo(self) -> bool:
        """Check if undo is possible"""
        with self.lock:
            return len(self.undo_stack) > 0
    
    def can_redo(self) -> bool:
        """Check if redo is possible"""
        with self.lock:
            return len(self.redo_stack) > 0
    
    def get_undo_description(self) -> Optional[str]:
        """Get description of the next undo operation"""
        with self.lock:
            if self.undo_stack:
                return f"Undo {self.undo_stack[-1].group_name}"
            return None
    
    def get_redo_description(self) -> Optional[str]:
        """Get description of the next redo operation"""
        with self.lock:
            if self.redo_stack:
                return f"Redo {self.redo_stack[-1].group_name}"
            return None
    
    def prepare_undo(self) -> Optional[UndoRedoGroup]:
        """Prepare the next undo group"""
        with self.lock:
            if not self.undo_stack:
                return None
            
            group = self.undo_stack.pop()
            self.redo_stack.append(group)
            return group
    
    def prepare_redo(self) -> Optional[UndoRedoGroup]:
        """Prepare the next redo group"""
        with self.lock:
            if not self.redo_stack:
                return None
            
            group = self.redo_stack.pop()
            self.undo_stack.append(group)
            return group
    
    def _cleanup_group_snapshots(self, group: UndoRedoGroup):
        """Clean up snapshots for a removed group"""
        with self.snapshot_lock:
            for snapshot in group.snapshots:
                if snapshot.snapshot_id in self.document_snapshots:
                    del self.document_snapshots[snapshot.snapshot_id]
    
    def get_undo_redo_stats(self) -> Dict[str, Any]:
        """Get statistics about undo/redo state"""
        with self.lock:
            return {
                'undo_levels_available': len(self.undo_stack),
                'redo_levels_available': len(self.redo_stack),
                'max_undo_levels': self.max_undo_levels,
                'current_group_active': self.current_group is not None,
                'total_snapshots': len(self.document_snapshots)
            }


class ExecutionAgent(BaseAgent, SharedCacheMixin):
    """
    ExecutionAgent serves as the UNO service bridge for actual document modifications.
    It executes validated operations while ensuring proper integration with LibreOffice's
    systems including undo/redo, threading, and resource management.
    """
    
    def __init__(self, agent_id: str = "execution_agent", config: Optional[Dict[str, Any]] = None):
        super().__init__(agent_id, config)
        
        # Initialize UNO service management
        self.uno_manager = UnoServiceManager()
        
        # Initialize operation management
        self.operation_queue: List[ExecutionOperation] = []
        self.active_operations: Dict[str, ExecutionOperation] = {}
        self.completed_operations: Dict[str, ExecutionOperation] = {}
        self.operation_lock = threading.RLock()
        
        # Initialize executors
        self.executors: List[OperationExecutor] = []
        self._register_default_executors()
        
        # Initialize undo/redo system
        max_undo_levels = config.get('max_undo_levels', 100) if config else 100
        self.undo_redo_manager = UndoRedoManager(max_undo_levels)
        
        # Performance tracking
        self.performance_metrics = {
            'operations_executed': 0,
            'operations_failed': 0,
            'operations_undone': 0,
            'operations_redone': 0,
            'total_execution_time': 0.0,
            'average_execution_time': 0.0
        }
        
        # Transaction management
        self.active_transactions: Dict[str, List[str]] = {}  # transaction_id -> operation_ids
        self.transaction_lock = threading.RLock()
        
        logging.info(f"ExecutionAgent initialized with {len(self.executors)} executors and undo/redo system")
    
    def _register_default_executors(self):
        """Register default operation executors"""
        self.executors = [
            TextOperationExecutor(self.uno_manager),
            TableOperationExecutor(self.uno_manager)
        ]
    
    async def execute_operation(self, operation: ExecutionOperation, create_undo_snapshot: bool = True) -> ExecutionResult:
        """
        Execute a single operation through appropriate executor with undo/redo support
        
        Args:
            operation: ExecutionOperation to be executed
            create_undo_snapshot: Whether to create an undo snapshot
            
        Returns:
            ExecutionResult with execution details and results
        """
        start_time = time.time()
        snapshot = None
        
        try:
            # Find appropriate executor
            executor = self._find_executor(operation)
            if not executor:
                return ExecutionResult(
                    operation_id=operation.operation_id,
                    success=False,
                    error_message=f"No executor found for operation type: {operation.operation_type}"
                )
            
            # Create undo snapshot before execution
            if create_undo_snapshot:
                content_before = await self._capture_content_before_operation(operation)
                snapshot = self.undo_redo_manager.create_snapshot(operation, content_before)
            
            # Update operation status
            operation.status = OperationStatus.EXECUTING
            operation.started_at = datetime.now(timezone.utc)
            
            with self.operation_lock:
                self.active_operations[operation.operation_id] = operation
            
            # Execute operation
            result = await executor.execute(operation)
            
            # Update undo snapshot with post-execution data
            if snapshot and result.success:
                content_after = await self._capture_content_after_operation(operation, result)
                self.undo_redo_manager.update_snapshot_after_execution(
                    snapshot.snapshot_id, content_after, result.result_data
                )
            
            # Update operation status and metrics
            operation.status = OperationStatus.COMPLETED if result.success else OperationStatus.FAILED
            operation.completed_at = datetime.now(timezone.utc)
            operation.execution_time_ms = result.execution_time_ms
            operation.result = result.result_data
            operation.error_message = result.error_message
            
            # Store rollback data if needed
            if result.success and result.undo_information:
                operation.rollback_data = result.undo_information
            
            # Move to completed operations
            with self.operation_lock:
                if operation.operation_id in self.active_operations:
                    del self.active_operations[operation.operation_id]
                self.completed_operations[operation.operation_id] = operation
            
            # Update performance metrics
            self._update_performance_metrics(result.success, result.execution_time_ms)
            
            logging.info(f"Operation {operation.operation_id} completed: success={result.success}")
            return result
            
        except Exception as e:
            execution_time = (time.time() - start_time) * 1000
            logging.error(f"Operation execution failed: {e}")
            traceback.print_exc()
            
            # Update operation status
            operation.status = OperationStatus.FAILED
            operation.completed_at = datetime.now(timezone.utc)
            operation.execution_time_ms = execution_time
            operation.error_message = str(e)
            
            # Clean up active operations
            with self.operation_lock:
                if operation.operation_id in self.active_operations:
                    del self.active_operations[operation.operation_id]
                self.completed_operations[operation.operation_id] = operation
            
            self._update_performance_metrics(False, execution_time)
            
            return ExecutionResult(
                operation_id=operation.operation_id,
                success=False,
                error_message=str(e),
                execution_time_ms=execution_time
            )
    
    async def execute_batch_operations(self, operations: List[ExecutionOperation]) -> List[ExecutionResult]:
        """
        Execute multiple operations, handling dependencies and transactions
        
        Args:
            operations: List of ExecutionOperations to execute
            
        Returns:
            List of ExecutionResults for each operation
        """
        results = []
        
        # Sort operations by priority and dependencies
        sorted_operations = self._sort_operations_by_dependencies(operations)
        
        # Execute operations in order
        for operation in sorted_operations:
            # Check if dependencies are satisfied
            if not self._dependencies_satisfied(operation, results):
                result = ExecutionResult(
                    operation_id=operation.operation_id,
                    success=False,
                    error_message="Operation dependencies not satisfied"
                )
                results.append(result)
                continue
            
            # Execute operation
            result = await self.execute_operation(operation)
            results.append(result)
            
            # If operation failed and it's critical, stop batch execution
            if (not result.success and 
                operation.priority in [OperationPriority.CRITICAL, OperationPriority.HIGH]):
                logging.warning(f"Critical/High priority operation failed, stopping batch execution")
                break
        
        return results
    
    def _find_executor(self, operation: ExecutionOperation) -> Optional[OperationExecutor]:
        """Find an executor that can handle the operation"""
        for executor in self.executors:
            if executor.can_execute(operation):
                return executor
        return None
    
    def _sort_operations_by_dependencies(self, operations: List[ExecutionOperation]) -> List[ExecutionOperation]:
        """Sort operations based on dependencies and priority"""
        # Simple topological sort implementation
        # In a full implementation, this would handle complex dependency graphs
        return sorted(operations, key=lambda op: (
            op.priority.value,
            len(op.depends_on),
            op.created_at
        ))
    
    def _dependencies_satisfied(self, operation: ExecutionOperation, completed_results: List[ExecutionResult]) -> bool:
        """Check if operation dependencies are satisfied"""
        if not operation.depends_on:
            return True
        
        completed_ids = {result.operation_id for result in completed_results if result.success}
        return all(dep_id in completed_ids for dep_id in operation.depends_on)
    
    def _update_performance_metrics(self, success: bool, execution_time_ms: float):
        """Update performance metrics"""
        self.performance_metrics['operations_executed'] += 1
        if not success:
            self.performance_metrics['operations_failed'] += 1
        
        self.performance_metrics['total_execution_time'] += execution_time_ms
        self.performance_metrics['average_execution_time'] = (
            self.performance_metrics['total_execution_time'] / 
            self.performance_metrics['operations_executed']
        )
    
    # Transaction management
    
    def begin_transaction(self, transaction_id: Optional[str] = None) -> str:
        """Begin a new transaction for atomic operations"""
        if not transaction_id:
            transaction_id = str(uuid.uuid4())
        
        with self.transaction_lock:
            self.active_transactions[transaction_id] = []
        
        logging.debug(f"Started transaction: {transaction_id}")
        return transaction_id
    
    def add_operation_to_transaction(self, transaction_id: str, operation_id: str):
        """Add an operation to a transaction"""
        with self.transaction_lock:
            if transaction_id in self.active_transactions:
                self.active_transactions[transaction_id].append(operation_id)
    
    async def commit_transaction(self, transaction_id: str) -> bool:
        """Commit all operations in a transaction"""
        with self.transaction_lock:
            if transaction_id not in self.active_transactions:
                logging.warning(f"Transaction not found: {transaction_id}")
                return False
            
            operation_ids = self.active_transactions[transaction_id]
        
        # Execute all operations in the transaction
        operations = []
        for op_id in operation_ids:
            if op_id in self.completed_operations:
                operations.append(self.completed_operations[op_id])
        
        results = await self.execute_batch_operations(operations)
        
        # Check if all operations succeeded
        all_succeeded = all(result.success for result in results)
        
        if all_succeeded:
            with self.transaction_lock:
                del self.active_transactions[transaction_id]
            logging.info(f"Transaction committed successfully: {transaction_id}")
        else:
            logging.error(f"Transaction failed, rolling back: {transaction_id}")
            await self.rollback_transaction(transaction_id)
        
        return all_succeeded
    
    async def rollback_transaction(self, transaction_id: str) -> bool:
        """Rollback all operations in a transaction"""
        with self.transaction_lock:
            if transaction_id not in self.active_transactions:
                logging.warning(f"Transaction not found for rollback: {transaction_id}")
                return False
            
            operation_ids = self.active_transactions[transaction_id]
            del self.active_transactions[transaction_id]
        
        # Rollback operations in reverse order
        rollback_success = True
        for op_id in reversed(operation_ids):
            if op_id in self.completed_operations:
                operation = self.completed_operations[op_id]
                if not await self._rollback_operation(operation):
                    rollback_success = False
        
        logging.info(f"Transaction rollback {'successful' if rollback_success else 'partially failed'}: {transaction_id}")
        return rollback_success
    
    # Undo/Redo Operations
    
    async def undo_last_operation(self) -> bool:
        """Undo the last operation or operation group"""
        if not self.undo_redo_manager.can_undo():
            logging.warning("No operations available to undo")
            return False
        
        try:
            group = self.undo_redo_manager.prepare_undo()
            if not group:
                return False
            
            # Undo operations in reverse order
            success = True
            for snapshot in reversed(group.snapshots):
                operation_id = snapshot.operation_id
                if operation_id in self.completed_operations:
                    operation = self.completed_operations[operation_id]
                    if not await self._undo_operation_from_snapshot(snapshot, operation):
                        success = False
                        break
            
            if success:
                self.performance_metrics['operations_undone'] += len(group.operations)
                logging.info(f"Successfully undid operation group: {group.group_name}")
            else:
                # Move the group back to undo stack if undo failed
                self.undo_redo_manager.redo_stack.remove(group)
                self.undo_redo_manager.undo_stack.append(group)
                logging.error(f"Failed to undo operation group: {group.group_name}")
            
            return success
            
        except Exception as e:
            logging.error(f"Undo operation failed: {e}")
            return False
    
    async def redo_last_operation(self) -> bool:
        """Redo the last undone operation or operation group"""
        if not self.undo_redo_manager.can_redo():
            logging.warning("No operations available to redo")
            return False
        
        try:
            group = self.undo_redo_manager.prepare_redo()
            if not group:
                return False
            
            # Redo operations in original order
            success = True
            for snapshot in group.snapshots:
                operation_id = snapshot.operation_id
                if operation_id in self.completed_operations:
                    operation = self.completed_operations[operation_id]
                    if not await self._redo_operation_from_snapshot(snapshot, operation):
                        success = False
                        break
            
            if success:
                self.performance_metrics['operations_redone'] += len(group.operations)
                logging.info(f"Successfully redid operation group: {group.group_name}")
            else:
                # Move the group back to redo stack if redo failed
                self.undo_redo_manager.undo_stack.remove(group)
                self.undo_redo_manager.redo_stack.append(group)
                logging.error(f"Failed to redo operation group: {group.group_name}")
            
            return success
            
        except Exception as e:
            logging.error(f"Redo operation failed: {e}")
            return False
    
    async def _capture_content_before_operation(self, operation: ExecutionOperation) -> Optional[str]:
        """Capture document content before operation for undo purposes"""
        try:
            # This would interface with actual UNO services to capture content
            # For now, return a placeholder
            if operation.operation_type in [OperationType.TEXT_INSERT, OperationType.TEXT_REPLACE, OperationType.TEXT_DELETE]:
                return f"content_before_operation_{operation.operation_id}"
            return None
        except Exception as e:
            logging.warning(f"Failed to capture content before operation: {e}")
            return None
    
    async def _capture_content_after_operation(self, operation: ExecutionOperation, result: ExecutionResult) -> Optional[str]:
        """Capture document content after operation for redo purposes"""
        try:
            # This would interface with actual UNO services to capture content
            # For now, return a placeholder based on operation result
            if result.result_data and operation.operation_type in [OperationType.TEXT_INSERT, OperationType.TEXT_REPLACE]:
                return f"content_after_operation_{operation.operation_id}_{result.result_data}"
            return None
        except Exception as e:
            logging.warning(f"Failed to capture content after operation: {e}")
            return None
    
    async def _undo_operation_from_snapshot(self, snapshot: UndoRedoSnapshot, operation: ExecutionOperation) -> bool:
        """Undo an operation using its snapshot data"""
        try:
            # Create reverse operation based on snapshot
            reverse_operation = self._create_reverse_operation(snapshot, operation)
            if not reverse_operation:
                logging.warning(f"Could not create reverse operation for {operation.operation_id}")
                return False
            
            # Execute reverse operation without creating new undo snapshot
            result = await self.execute_operation(reverse_operation, create_undo_snapshot=False)
            
            if result.success:
                logging.debug(f"Successfully undid operation: {operation.operation_id}")
                operation.status = OperationStatus.ROLLED_BACK
                return True
            else:
                logging.error(f"Failed to undo operation {operation.operation_id}: {result.error_message}")
                return False
                
        except Exception as e:
            logging.error(f"Error during undo of operation {operation.operation_id}: {e}")
            return False
    
    async def _redo_operation_from_snapshot(self, snapshot: UndoRedoSnapshot, operation: ExecutionOperation) -> bool:
        """Redo an operation using its snapshot data"""
        try:
            # Recreate the original operation
            redo_operation = self._recreate_operation_from_snapshot(snapshot, operation)
            if not redo_operation:
                logging.warning(f"Could not recreate operation for {operation.operation_id}")
                return False
            
            # Execute operation without creating new undo snapshot (it will be managed by the redo system)
            result = await self.execute_operation(redo_operation, create_undo_snapshot=False)
            
            if result.success:
                logging.debug(f"Successfully redid operation: {operation.operation_id}")
                operation.status = OperationStatus.COMPLETED
                return True
            else:
                logging.error(f"Failed to redo operation {operation.operation_id}: {result.error_message}")
                return False
                
        except Exception as e:
            logging.error(f"Error during redo of operation {operation.operation_id}: {e}")
            return False
    
    def _create_reverse_operation(self, snapshot: UndoRedoSnapshot, original_operation: ExecutionOperation) -> Optional[ExecutionOperation]:
        """Create a reverse operation from a snapshot"""
        try:
            operation_type = OperationType(snapshot.document_state['operation_type'])
            
            # Create reverse operation based on type
            if operation_type == OperationType.TEXT_INSERT:
                # Reverse of insert is delete
                reverse_params = {
                    'range': snapshot.operation_metadata.get('insertion_range', {}),
                    'length': len(snapshot.content_after or '')
                }
                return ExecutionOperation(
                    operation_type=OperationType.TEXT_DELETE,
                    parameters=reverse_params,
                    context=self._recreate_context_from_snapshot(snapshot)
                )
            
            elif operation_type == OperationType.TEXT_DELETE:
                # Reverse of delete is insert
                reverse_params = {
                    'text': snapshot.content_before or '',
                    'position': snapshot.cursor_position
                }
                return ExecutionOperation(
                    operation_type=OperationType.TEXT_INSERT,
                    parameters=reverse_params,
                    context=self._recreate_context_from_snapshot(snapshot)
                )
            
            elif operation_type == OperationType.TEXT_REPLACE:
                # Reverse of replace is replace with original text
                original_params = snapshot.document_state['parameters']
                reverse_params = {
                    'old_text': original_params.get('new_text', ''),
                    'new_text': original_params.get('old_text', ''),
                    'range': original_params.get('range', {})
                }
                return ExecutionOperation(
                    operation_type=OperationType.TEXT_REPLACE,
                    parameters=reverse_params,
                    context=self._recreate_context_from_snapshot(snapshot)
                )
            
            # For other operations, use the original rollback data if available
            if original_operation.rollback_data:
                return ExecutionOperation(
                    operation_type=OperationType.CUSTOM_OPERATION,
                    parameters=original_operation.rollback_data,
                    context=self._recreate_context_from_snapshot(snapshot)
                )
            
            return None
            
        except Exception as e:
            logging.error(f"Failed to create reverse operation: {e}")
            return None
    
    def _recreate_operation_from_snapshot(self, snapshot: UndoRedoSnapshot, original_operation: ExecutionOperation) -> Optional[ExecutionOperation]:
        """Recreate an operation from its snapshot for redo"""
        try:
            operation_type = OperationType(snapshot.document_state['operation_type'])
            original_params = snapshot.document_state['parameters']
            
            return ExecutionOperation(
                operation_type=operation_type,
                parameters=original_params,
                context=self._recreate_context_from_snapshot(snapshot),
                priority=OperationPriority(snapshot.operation_metadata['priority'])
            )
            
        except Exception as e:
            logging.error(f"Failed to recreate operation from snapshot: {e}")
            return None
    
    def _recreate_context_from_snapshot(self, snapshot: UndoRedoSnapshot) -> OperationContext:
        """Recreate operation context from snapshot"""
        return OperationContext(
            document_id=snapshot.document_state.get('document_id', 'unknown'),
            cursor_position=snapshot.cursor_position.copy(),
            selection_range=snapshot.selection_state.copy() if snapshot.selection_state else None,
            formatting_state=snapshot.formatting_state.copy(),
            metadata={'recreated_from_snapshot': True}
        )
    
    def get_undo_redo_status(self) -> Dict[str, Any]:
        """Get current undo/redo status"""
        stats = self.undo_redo_manager.get_undo_redo_stats()
        stats.update({
            'can_undo': self.undo_redo_manager.can_undo(),
            'can_redo': self.undo_redo_manager.can_redo(),
            'undo_description': self.undo_redo_manager.get_undo_description(),
            'redo_description': self.undo_redo_manager.get_redo_description(),
            'performance_metrics': {
                'operations_undone': self.performance_metrics['operations_undone'],
                'operations_redone': self.performance_metrics['operations_redone']
            }
        })
        return stats
    
    async def _rollback_operation(self, operation: ExecutionOperation) -> bool:
        """Rollback a single operation"""
        try:
            # This would implement actual rollback logic using undo information
            logging.info(f"Rolling back operation: {operation.operation_id}")
            
            # Update operation status
            operation.status = OperationStatus.ROLLED_BACK
            
            return True
            
        except Exception as e:
            logging.error(f"Failed to rollback operation {operation.operation_id}: {e}")
            return False
    
    # BaseAgent abstract method implementations
    
    async def process(self, state: DocumentState, message: Optional[BaseMessage] = None) -> AgentResult:
        """Process execution request through BaseAgent interface"""
        try:
            # Extract pending operations from state
            pending_operations = []
            
            if isinstance(state, dict):
                pending_ops = state.get('pending_operations', [])
                
                # Convert pending operations to ExecutionOperations
                for op_data in pending_ops:
                    if isinstance(op_data, dict):
                        operation = self._create_operation_from_dict(op_data)
                        pending_operations.append(operation)
            
            if not pending_operations:
                return AgentResult(
                    agent_id=self.agent_id,
                    success=True,
                    result={'message': 'No operations to execute'},
                    execution_time=0
                )
            
            # Execute operations
            results = await self.execute_batch_operations(pending_operations)
            
            # Aggregate results
            successful_operations = sum(1 for r in results if r.success)
            total_execution_time = sum(r.execution_time_ms for r in results)
            
            return AgentResult(
                agent_id=self.agent_id,
                success=all(r.success for r in results),
                result={
                    'operations_executed': len(results),
                    'successful_operations': successful_operations,
                    'failed_operations': len(results) - successful_operations,
                    'total_execution_time_ms': total_execution_time,
                    'operation_results': [
                        {
                            'operation_id': r.operation_id,
                            'success': r.success,
                            'error_message': r.error_message
                        }
                        for r in results
                    ]
                },
                execution_time=total_execution_time / 1000.0  # Convert to seconds
            )
            
        except Exception as e:
            logging.error(f"Execution processing failed: {e}")
            return AgentResult(
                agent_id=self.agent_id,
                success=False,
                result={},
                error=f"Execution processing error: {str(e)}",
                execution_time=0
            )
    
    def validate_input(self, state: DocumentState, message: Optional[BaseMessage] = None) -> ValidationResult:
        """Validate input parameters for execution operations"""
        issues = []
        
        # Basic input validation
        if not isinstance(state, dict):
            issues.append({'type': 'invalid_state', 'message': 'State must be a dictionary'})
        
        # Check for pending operations
        if isinstance(state, dict):
            pending_ops = state.get('pending_operations', [])
            if not pending_ops:
                issues.append({'type': 'no_operations', 'message': 'No pending operations to execute'})
            
            # Validate operation format
            for i, op in enumerate(pending_ops):
                if not isinstance(op, dict):
                    issues.append({
                        'type': 'invalid_operation',
                        'message': f'Operation {i} is not a valid dictionary'
                    })
                elif 'type' not in op:
                    issues.append({
                        'type': 'missing_operation_type',
                        'message': f'Operation {i} missing type field'
                    })
        
        return ValidationResult(
            agent_id=self.agent_id,
            validation_type="execution_input_validation",
            passed=len(issues) == 0,
            confidence=1.0 if len(issues) == 0 else 0.0,
            issues=issues
        )
    
    def get_required_tools(self) -> List[str]:
        """Get list of required tools for execution agent"""
        return [
            'uno_text_service',
            'uno_table_service',
            'uno_chart_service',
            'undo_manager',
            'document_model_access'
        ]
    
    def _create_operation_from_dict(self, op_data: Dict[str, Any]) -> ExecutionOperation:
        """Create ExecutionOperation from dictionary data"""
        # Map operation type string to enum
        operation_type_map = {
            'text_insert': OperationType.TEXT_INSERT,
            'text_format': OperationType.TEXT_FORMAT,
            'text_delete': OperationType.TEXT_DELETE,
            'text_replace': OperationType.TEXT_REPLACE,
            'table_create': OperationType.TABLE_CREATE,
            'table_modify': OperationType.TABLE_MODIFY,
            'chart_create': OperationType.CHART_CREATE,
            'chart_modify': OperationType.CHART_MODIFY,
            'style_apply': OperationType.STYLE_APPLY,
            'page_break': OperationType.PAGE_BREAK,
            'image_insert': OperationType.IMAGE_INSERT
        }
        
        op_type_str = op_data.get('type', 'custom_operation')
        operation_type = operation_type_map.get(op_type_str, OperationType.CUSTOM_OPERATION)
        
        # Create operation context if available
        context = None
        if 'context' in op_data:
            context_data = op_data['context']
            context = OperationContext(
                document_id=context_data.get('document_id', 'unknown'),
                cursor_position=context_data.get('cursor_position', {}),
                selection_range=context_data.get('selection_range'),
                formatting_state=context_data.get('formatting_state', {}),
                metadata=context_data.get('metadata', {})
            )
        
        return ExecutionOperation(
            operation_type=operation_type,
            parameters=op_data.get('parameters', {}),
            context=context,
            priority=OperationPriority(op_data.get('priority', 'normal')),
            validation_required=op_data.get('validation_required', True),
            approval_required=op_data.get('approval_required', False)
        )
    
    def get_capabilities(self) -> List[AgentCapability]:
        """Return agent capabilities"""
        return [
            AgentCapability.EXECUTION,
            AgentCapability.DOCUMENT_ANALYSIS
        ]
    
    async def cleanup(self):
        """Cleanup resources"""
        # Shutdown UNO service manager
        self.uno_manager.shutdown()
        
        # Clear operation queues
        with self.operation_lock:
            self.operation_queue.clear()
            self.active_operations.clear()
            self.completed_operations.clear()
        
        # Clear transactions
        with self.transaction_lock:
            self.active_transactions.clear()
        
        await super().cleanup()