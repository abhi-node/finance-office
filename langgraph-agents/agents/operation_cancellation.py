"""
Operation Cancellation and Progress Tracking System for LibreOffice AI Writing Assistant

This module implements Task 19.5 by providing comprehensive operation cancellation
capabilities and enhanced progress tracking throughout the multi-agent system.
It integrates with all existing components to enable graceful cancellation of
long-running operations and detailed progress reporting.

Key Components:
- OperationCancellationManager: Main cancellation coordinator
- CancellationToken: Thread-safe cancellation signaling
- ProgressTrackingOrchestrator: Comprehensive progress tracking
- AgentCancellationSupport: Agent-level cancellation integration
- CancellableOperation: Wrapper for cancellable operations
"""

import asyncio
import threading
import time
import uuid
from abc import ABC, abstractmethod
from contextlib import asynccontextmanager
from dataclasses import dataclass, field
from datetime import datetime, timezone, timedelta
from enum import Enum
from typing import Any, Callable, Dict, List, Optional, Set, Union, Awaitable
import logging

# Import existing infrastructure
try:
    from .error_handler import ErrorContext, ErrorSeverity, ErrorCategory
    from .ui_error_propagation import UIErrorPropagator, ProgressState, ProgressUpdate
    from .graceful_degradation import TransactionalStateManager
    from .retry_manager import EnhancedRetryManager
    from bridge import LangGraphBridge
    from state.document_state import DocumentState
except ImportError:
    # Fallback for testing
    ErrorContext = Any
    ErrorSeverity = Any
    ErrorCategory = Any
    UIErrorPropagator = Any
    ProgressState = Any
    ProgressUpdate = Any
    TransactionalStateManager = Any
    EnhancedRetryManager = Any
    LangGraphBridge = Any
    DocumentState = Dict[str, Any]


class CancellationReason(Enum):
    """Reasons for operation cancellation."""
    USER_REQUESTED = "user_requested"         # User explicitly cancelled
    TIMEOUT = "timeout"                       # Operation timed out
    SYSTEM_SHUTDOWN = "system_shutdown"       # System shutting down
    ERROR_THRESHOLD = "error_threshold"       # Too many errors
    RESOURCE_EXHAUSTION = "resource_exhaustion"  # Out of resources
    PRIORITY_OVERRIDE = "priority_override"   # Higher priority operation
    DEPENDENCY_FAILURE = "dependency_failure"  # Required service failed
    PARENT_CANCELLED = "parent_cancelled"     # Parent operation cancelled


class CancellationScope(Enum):
    """Scope of cancellation operation."""
    OPERATION_ONLY = "operation_only"         # Just this operation
    OPERATION_AND_CHILDREN = "operation_and_children"  # Operation and sub-operations
    AGENT_OPERATIONS = "agent_operations"     # All operations for specific agent
    REQUEST_OPERATIONS = "request_operations" # All operations for request
    SYSTEM_WIDE = "system_wide"               # All operations system-wide


class OperationState(Enum):
    """Detailed operation states for tracking."""
    QUEUED = "queued"                         # Operation queued for execution
    INITIALIZING = "initializing"             # Setting up operation
    RUNNING = "running"                       # Operation executing
    AGENT_COORDINATION = "agent_coordination" # Agents coordinating
    DATA_PROCESSING = "data_processing"       # Processing data
    DOCUMENT_UPDATING = "document_updating"   # Updating document
    CANCELLING = "cancelling"                 # Cancellation in progress
    CANCELLED = "cancelled"                   # Successfully cancelled
    COMPLETED = "completed"                   # Successfully completed
    ERROR = "error"                          # Operation failed
    TIMEOUT = "timeout"                      # Operation timed out
    RETRYING = "retrying"                    # Retrying after error


@dataclass
class CancellationToken:
    """Thread-safe cancellation token for operations."""
    token_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    is_cancelled: bool = False
    cancellation_reason: Optional[CancellationReason] = None
    cancellation_message: str = ""
    cancellation_time: Optional[datetime] = None
    cancelled_by: str = ""
    
    # Thread safety
    _lock: threading.RLock = field(default_factory=threading.RLock, init=False)
    
    # Callbacks
    cancellation_callbacks: List[Callable[[], None]] = field(default_factory=list, init=False)
    
    def cancel(self, reason: CancellationReason, message: str = "", cancelled_by: str = "") -> bool:
        """
        Cancel the operation.
        
        Args:
            reason: Reason for cancellation
            message: Human-readable cancellation message
            cancelled_by: Who/what initiated cancellation
            
        Returns:
            bool: True if cancellation was successful (not already cancelled)
        """
        with self._lock:
            if self.is_cancelled:
                return False
            
            self.is_cancelled = True
            self.cancellation_reason = reason
            self.cancellation_message = message
            self.cancellation_time = datetime.now(timezone.utc)
            self.cancelled_by = cancelled_by
            
            # Execute cancellation callbacks
            for callback in self.cancellation_callbacks:
                try:
                    callback()
                except Exception as e:
                    # Log but don't fail cancellation
                    logging.getLogger("cancellation_token").warning(f"Cancellation callback failed: {e}")
            
            return True
    
    def is_cancellation_requested(self) -> bool:
        """Check if cancellation has been requested."""
        with self._lock:
            return self.is_cancelled
    
    def add_cancellation_callback(self, callback: Callable[[], None]) -> None:
        """Add callback to be executed when cancellation occurs."""
        with self._lock:
            if self.is_cancelled:
                # Already cancelled, execute immediately
                try:
                    callback()
                except Exception:
                    pass
            else:
                self.cancellation_callbacks.append(callback)
    
    def get_cancellation_info(self) -> Dict[str, Any]:
        """Get cancellation information."""
        with self._lock:
            return {
                "token_id": self.token_id,
                "is_cancelled": self.is_cancelled,
                "reason": self.cancellation_reason.value if self.cancellation_reason else None,
                "message": self.cancellation_message,
                "cancellation_time": self.cancellation_time.isoformat() if self.cancellation_time else None,
                "cancelled_by": self.cancelled_by
            }


@dataclass
class OperationProgress:
    """Comprehensive progress tracking for operations."""
    operation_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    request_id: str = ""
    parent_operation_id: Optional[str] = None
    
    # Basic progress
    state: OperationState = OperationState.QUEUED
    progress_percentage: float = 0.0
    current_step: str = ""
    current_step_number: int = 0
    total_steps: Optional[int] = None
    
    # Time tracking
    created_time: datetime = field(default_factory=lambda: datetime.now(timezone.utc))
    started_time: Optional[datetime] = None
    completed_time: Optional[datetime] = None
    estimated_completion_time: Optional[datetime] = None
    
    # Agent tracking
    involved_agents: List[str] = field(default_factory=list)
    active_agents: List[str] = field(default_factory=list)
    completed_agents: List[str] = field(default_factory=list)
    failed_agents: List[str] = field(default_factory=list)
    
    # Error and cancellation
    cancellation_token: Optional[CancellationToken] = None
    error_count: int = 0
    retry_count: int = 0
    last_error: Optional[str] = None
    
    # Resource tracking
    memory_usage_mb: float = 0.0
    cpu_time_seconds: float = 0.0
    network_requests: int = 0
    document_modifications: int = 0
    
    # Metadata
    operation_type: str = ""
    complexity_level: str = ""
    user_id: Optional[str] = None
    metadata: Dict[str, Any] = field(default_factory=dict)


class CancellableOperation(ABC):
    """Abstract base class for cancellable operations."""
    
    def __init__(self, operation_id: str, cancellation_token: CancellationToken):
        """
        Initialize cancellable operation.
        
        Args:
            operation_id: Unique operation identifier
            cancellation_token: Cancellation token for this operation
        """
        self.operation_id = operation_id
        self.cancellation_token = cancellation_token
        self.logger = logging.getLogger(f"cancellable_operation.{operation_id}")
    
    @abstractmethod
    async def execute(self) -> Any:
        """Execute the operation with cancellation support."""
        pass
    
    @abstractmethod
    async def cancel(self) -> bool:
        """Cancel the operation gracefully."""
        pass
    
    def check_cancellation(self) -> None:
        """Check if operation should be cancelled and raise exception if so."""
        if self.cancellation_token.is_cancellation_requested():
            cancellation_info = self.cancellation_token.get_cancellation_info()
            raise OperationCancelledException(
                f"Operation {self.operation_id} was cancelled: {cancellation_info['message']}",
                cancellation_info
            )


class OperationCancelledException(Exception):
    """Exception raised when an operation is cancelled."""
    
    def __init__(self, message: str, cancellation_info: Dict[str, Any]):
        super().__init__(message)
        self.cancellation_info = cancellation_info


class ProgressTrackingOrchestrator:
    """Orchestrates comprehensive progress tracking across all operations."""
    
    def __init__(self, ui_error_propagator: Optional[UIErrorPropagator] = None):
        """
        Initialize progress tracking orchestrator.
        
        Args:
            ui_error_propagator: UI error propagator for progress updates
        """
        self.ui_error_propagator = ui_error_propagator
        
        # Operation tracking
        self.active_operations: Dict[str, OperationProgress] = {}
        self.completed_operations: List[OperationProgress] = []
        self.operation_hierarchy: Dict[str, List[str]] = {}  # parent -> children
        
        # Statistics
        self.tracking_stats = {
            "total_operations": 0,
            "active_operations": 0,
            "completed_operations": 0,
            "cancelled_operations": 0,
            "failed_operations": 0,
            "average_completion_time": 0.0
        }
        
        # Thread safety
        self.lock = threading.RLock()
        
        self.logger = logging.getLogger("progress_tracking_orchestrator")
    
    def start_operation(self, 
                       operation_id: str,
                       request_id: str,
                       operation_type: str = "",
                       complexity_level: str = "",
                       parent_operation_id: Optional[str] = None,
                       total_steps: Optional[int] = None,
                       involved_agents: Optional[List[str]] = None) -> OperationProgress:
        """Start tracking a new operation."""
        with self.lock:
            progress = OperationProgress(
                operation_id=operation_id,
                request_id=request_id,
                parent_operation_id=parent_operation_id,
                total_steps=total_steps,
                involved_agents=involved_agents or [],
                operation_type=operation_type,
                complexity_level=complexity_level,
                started_time=datetime.now(timezone.utc)
            )
            
            # Create cancellation token
            progress.cancellation_token = CancellationToken()
            
            self.active_operations[operation_id] = progress
            
            # Track hierarchy
            if parent_operation_id:
                if parent_operation_id not in self.operation_hierarchy:
                    self.operation_hierarchy[parent_operation_id] = []
                self.operation_hierarchy[parent_operation_id].append(operation_id)
            
            # Update statistics
            self.tracking_stats["total_operations"] += 1
            self.tracking_stats["active_operations"] = len(self.active_operations)
            
            self.logger.info(f"Started tracking operation {operation_id} (type: {operation_type})")
            return progress
    
    def update_progress(self,
                       operation_id: str,
                       state: Optional[OperationState] = None,
                       progress_percentage: Optional[float] = None,
                       current_step: Optional[str] = None,
                       step_number: Optional[int] = None,
                       active_agents: Optional[List[str]] = None,
                       metadata_updates: Optional[Dict[str, Any]] = None) -> Optional[OperationProgress]:
        """Update operation progress."""
        with self.lock:
            if operation_id not in self.active_operations:
                self.logger.warning(f"Attempted to update unknown operation {operation_id}")
                return None
            
            progress = self.active_operations[operation_id]
            
            # Update fields
            if state is not None:
                progress.state = state
            if progress_percentage is not None:
                progress.progress_percentage = min(100.0, max(0.0, progress_percentage))
            if current_step is not None:
                progress.current_step = current_step
            if step_number is not None:
                progress.current_step_number = step_number
            if active_agents is not None:
                progress.active_agents = active_agents.copy()
            if metadata_updates:
                progress.metadata.update(metadata_updates)
            
            # Update estimated completion time
            if progress.progress_percentage > 0 and progress.started_time:
                elapsed = datetime.now(timezone.utc) - progress.started_time
                if progress.progress_percentage < 100:
                    estimated_total = elapsed.total_seconds() * (100.0 / progress.progress_percentage)
                    progress.estimated_completion_time = progress.started_time + timedelta(seconds=estimated_total)
            
            # Propagate to UI if available
            if self.ui_error_propagator:
                asyncio.create_task(self._propagate_progress_update(progress))
            
            return progress
    
    def complete_operation(self, operation_id: str, success: bool = True, final_message: str = "") -> Optional[OperationProgress]:
        """Complete operation tracking."""
        with self.lock:
            if operation_id not in self.active_operations:
                return None
            
            progress = self.active_operations[operation_id]
            progress.completed_time = datetime.now(timezone.utc)
            progress.state = OperationState.COMPLETED if success else OperationState.ERROR
            progress.progress_percentage = 100.0
            
            if final_message:
                progress.current_step = final_message
            
            # Move to completed operations
            self.completed_operations.append(progress)
            del self.active_operations[operation_id]
            
            # Update statistics
            self.tracking_stats["active_operations"] = len(self.active_operations)
            self.tracking_stats["completed_operations"] += 1
            
            if not success:
                self.tracking_stats["failed_operations"] += 1
            
            # Update average completion time
            if progress.started_time and progress.completed_time:
                completion_time = (progress.completed_time - progress.started_time).total_seconds()
                current_avg = self.tracking_stats["average_completion_time"]
                total_completed = self.tracking_stats["completed_operations"]
                self.tracking_stats["average_completion_time"] = (
                    (current_avg * (total_completed - 1) + completion_time) / total_completed
                )
            
            # Keep completed operations bounded
            if len(self.completed_operations) > 1000:
                self.completed_operations = self.completed_operations[-500:]
            
            # Clean up hierarchy
            self.operation_hierarchy.pop(operation_id, None)
            
            self.logger.info(f"Completed operation {operation_id} - Success: {success}")
            return progress
    
    def get_operation_progress(self, operation_id: str) -> Optional[OperationProgress]:
        """Get current progress for operation."""
        with self.lock:
            return self.active_operations.get(operation_id)
    
    def get_all_active_operations(self) -> List[OperationProgress]:
        """Get all active operations."""
        with self.lock:
            return list(self.active_operations.values())
    
    def get_operations_by_request(self, request_id: str) -> List[OperationProgress]:
        """Get all operations for a specific request."""
        with self.lock:
            return [op for op in self.active_operations.values() if op.request_id == request_id]
    
    def get_child_operations(self, parent_operation_id: str) -> List[str]:
        """Get child operation IDs for a parent operation."""
        with self.lock:
            return self.operation_hierarchy.get(parent_operation_id, []).copy()
    
    async def _propagate_progress_update(self, progress: OperationProgress) -> None:
        """Propagate progress update to UI."""
        try:
            if self.ui_error_propagator:
                # Convert to UI progress state
                ui_state = self._convert_to_ui_progress_state(progress.state)
                
                await self.ui_error_propagator.update_progress(
                    operation_id=progress.operation_id,
                    progress_percentage=progress.progress_percentage,
                    state=ui_state,
                    step_description=progress.current_step,
                    operation_context={
                        "request_id": progress.request_id,
                        "operation_type": progress.operation_type,
                        "active_agents": progress.active_agents
                    }
                )
        except Exception as e:
            self.logger.warning(f"Failed to propagate progress update: {e}")
    
    def _convert_to_ui_progress_state(self, state: OperationState) -> ProgressState:
        """Convert operation state to UI progress state."""
        mapping = {
            OperationState.QUEUED: ProgressState.INITIALIZING,
            OperationState.INITIALIZING: ProgressState.INITIALIZING,
            OperationState.RUNNING: ProgressState.PROCESSING,
            OperationState.AGENT_COORDINATION: ProgressState.AGENT_COORDINATION,
            OperationState.DATA_PROCESSING: ProgressState.DATA_FETCHING,
            OperationState.DOCUMENT_UPDATING: ProgressState.DOCUMENT_UPDATING,
            OperationState.CANCELLING: ProgressState.CANCELLED,
            OperationState.CANCELLED: ProgressState.CANCELLED,
            OperationState.COMPLETED: ProgressState.COMPLETED,
            OperationState.ERROR: ProgressState.ERROR,
            OperationState.TIMEOUT: ProgressState.ERROR,
            OperationState.RETRYING: ProgressState.RETRYING
        }
        return mapping.get(state, ProgressState.PROCESSING)


class OperationCancellationManager:
    """Main coordinator for operation cancellation across the system."""
    
    def __init__(self,
                 progress_orchestrator: ProgressTrackingOrchestrator,
                 transactional_state_manager: Optional[TransactionalStateManager] = None,
                 retry_manager: Optional[EnhancedRetryManager] = None,
                 ui_error_propagator: Optional[UIErrorPropagator] = None):
        """
        Initialize operation cancellation manager.
        
        Args:
            progress_orchestrator: Progress tracking orchestrator
            transactional_state_manager: State manager for rollback
            retry_manager: Retry manager for cancellation integration
            ui_error_propagator: UI error propagator for notifications
        """
        self.progress_orchestrator = progress_orchestrator
        self.transactional_state_manager = transactional_state_manager
        self.retry_manager = retry_manager
        self.ui_error_propagator = ui_error_propagator
        
        # Cancellation tracking
        self.cancellation_handlers: Dict[str, Callable[[str], Awaitable[bool]]] = {}
        self.global_cancellation_token = CancellationToken()
        
        # Statistics
        self.cancellation_stats = {
            "total_cancellations": 0,
            "successful_cancellations": 0,
            "failed_cancellations": 0,
            "user_initiated": 0,
            "system_initiated": 0,
            "timeout_cancellations": 0
        }
        
        self.logger = logging.getLogger("operation_cancellation_manager")
    
    def register_cancellation_handler(self, operation_type: str, handler: Callable[[str], Awaitable[bool]]) -> None:
        """Register cancellation handler for specific operation type."""
        self.cancellation_handlers[operation_type] = handler
        self.logger.info(f"Registered cancellation handler for operation type: {operation_type}")
    
    async def cancel_operation(self,
                             operation_id: str,
                             reason: CancellationReason,
                             scope: CancellationScope = CancellationScope.OPERATION_ONLY,
                             message: str = "",
                             cancelled_by: str = "") -> bool:
        """
        Cancel operation with specified scope.
        
        Args:
            operation_id: Operation to cancel
            reason: Reason for cancellation
            scope: Scope of cancellation
            message: Human-readable message
            cancelled_by: Who initiated cancellation
            
        Returns:
            bool: True if cancellation was successful
        """
        self.cancellation_stats["total_cancellations"] += 1
        
        if reason == CancellationReason.USER_REQUESTED:
            self.cancellation_stats["user_initiated"] += 1
        else:
            self.cancellation_stats["system_initiated"] += 1
        
        if reason == CancellationReason.TIMEOUT:
            self.cancellation_stats["timeout_cancellations"] += 1
        
        self.logger.info(f"Cancelling operation {operation_id} with scope {scope.value}: {message}")
        
        # Get operations to cancel based on scope
        operations_to_cancel = self._get_operations_to_cancel(operation_id, scope)
        
        cancellation_results = []
        
        for op_id in operations_to_cancel:
            try:
                result = await self._cancel_single_operation(op_id, reason, message, cancelled_by)
                cancellation_results.append(result)
                
                if result:
                    self.cancellation_stats["successful_cancellations"] += 1
                else:
                    self.cancellation_stats["failed_cancellations"] += 1
                    
            except Exception as e:
                self.logger.error(f"Failed to cancel operation {op_id}: {e}")
                self.cancellation_stats["failed_cancellations"] += 1
                cancellation_results.append(False)
        
        # Overall success if at least the main operation was cancelled
        overall_success = len(cancellation_results) > 0 and cancellation_results[0]
        
        # Notify UI of cancellation
        if self.ui_error_propagator and overall_success:
            await self._notify_ui_of_cancellation(operation_id, reason, message)
        
        return overall_success
    
    async def _cancel_single_operation(self,
                                     operation_id: str,
                                     reason: CancellationReason,
                                     message: str,
                                     cancelled_by: str) -> bool:
        """Cancel a single operation."""
        # Get operation progress
        progress = self.progress_orchestrator.get_operation_progress(operation_id)
        if not progress:
            self.logger.warning(f"Cannot cancel unknown operation {operation_id}")
            return False
        
        # Check if already cancelled/completed
        if progress.state in [OperationState.CANCELLED, OperationState.COMPLETED]:
            return True
        
        # Signal cancellation
        if progress.cancellation_token:
            success = progress.cancellation_token.cancel(reason, message, cancelled_by)
            if not success:
                return False
        
        # Update progress state
        self.progress_orchestrator.update_progress(
            operation_id,
            state=OperationState.CANCELLING,
            current_step=f"Cancelling: {message}"
        )
        
        # Execute operation-specific cancellation if handler exists
        operation_type = progress.operation_type
        if operation_type in self.cancellation_handlers:
            try:
                handler_success = await self.cancellation_handlers[operation_type](operation_id)
                if not handler_success:
                    self.logger.warning(f"Operation-specific cancellation failed for {operation_id}")
            except Exception as e:
                self.logger.error(f"Cancellation handler failed for {operation_id}: {e}")
        
        # Rollback state if transactional state manager is available
        if self.transactional_state_manager:
            try:
                # Create rollback checkpoint if not exists
                checkpoint_id = f"cancel_{operation_id}_{int(time.time())}"
                await self.transactional_state_manager.rollback_to_checkpoint(
                    checkpoint_id, f"Operation cancelled: {message}", auto_rollback=True
                )
            except Exception as e:
                self.logger.warning(f"State rollback failed for cancelled operation {operation_id}: {e}")
        
        # Complete operation as cancelled
        self.progress_orchestrator.complete_operation(operation_id, success=False, final_message="Operation cancelled")
        
        return True
    
    def _get_operations_to_cancel(self, operation_id: str, scope: CancellationScope) -> List[str]:
        """Get list of operations to cancel based on scope."""
        operations = [operation_id]
        
        if scope == CancellationScope.OPERATION_AND_CHILDREN:
            # Add child operations
            children = self.progress_orchestrator.get_child_operations(operation_id)
            operations.extend(children)
        
        elif scope == CancellationScope.AGENT_OPERATIONS:
            # Find all operations for the same agents
            progress = self.progress_orchestrator.get_operation_progress(operation_id)
            if progress and progress.involved_agents:
                for op in self.progress_orchestrator.get_all_active_operations():
                    if any(agent in progress.involved_agents for agent in op.involved_agents):
                        if op.operation_id not in operations:
                            operations.append(op.operation_id)
        
        elif scope == CancellationScope.REQUEST_OPERATIONS:
            # Find all operations for the same request
            progress = self.progress_orchestrator.get_operation_progress(operation_id)
            if progress:
                request_operations = self.progress_orchestrator.get_operations_by_request(progress.request_id)
                for op in request_operations:
                    if op.operation_id not in operations:
                        operations.append(op.operation_id)
        
        elif scope == CancellationScope.SYSTEM_WIDE:
            # Cancel all active operations
            all_operations = self.progress_orchestrator.get_all_active_operations()
            for op in all_operations:
                if op.operation_id not in operations:
                    operations.append(op.operation_id)
        
        return operations
    
    async def _notify_ui_of_cancellation(self, operation_id: str, reason: CancellationReason, message: str) -> None:
        """Notify UI of operation cancellation."""
        try:
            if self.ui_error_propagator:
                await self.ui_error_propagator.update_progress(
                    operation_id=operation_id,
                    progress_percentage=100.0,
                    state=ProgressState.CANCELLED,
                    step_description=f"Cancelled: {message}",
                    operation_context={"cancellation_reason": reason.value}
                )
        except Exception as e:
            self.logger.warning(f"Failed to notify UI of cancellation: {e}")
    
    async def cancel_all_operations(self, reason: CancellationReason = CancellationReason.SYSTEM_SHUTDOWN) -> bool:
        """Cancel all active operations."""
        active_operations = self.progress_orchestrator.get_all_active_operations()
        if not active_operations:
            return True
        
        self.logger.info(f"Cancelling all {len(active_operations)} active operations")
        
        # Signal global cancellation
        self.global_cancellation_token.cancel(reason, "System-wide cancellation", "system")
        
        # Cancel each operation
        cancellation_tasks = []
        for operation in active_operations:
            task = self.cancel_operation(
                operation.operation_id,
                reason,
                CancellationScope.OPERATION_ONLY,
                "System shutdown",
                "system"
            )
            cancellation_tasks.append(task)
        
        # Wait for all cancellations
        if cancellation_tasks:
            results = await asyncio.gather(*cancellation_tasks, return_exceptions=True)
            successful_cancellations = sum(1 for result in results if result is True)
            
            self.logger.info(f"Cancelled {successful_cancellations}/{len(cancellation_tasks)} operations")
            return successful_cancellations == len(cancellation_tasks)
        
        return True
    
    @asynccontextmanager
    async def cancellable_operation(self,
                                  operation_id: str,
                                  request_id: str,
                                  operation_type: str = "",
                                  timeout_seconds: Optional[float] = None):
        """Context manager for cancellable operations."""
        # Start progress tracking
        progress = self.progress_orchestrator.start_operation(
            operation_id=operation_id,
            request_id=request_id,
            operation_type=operation_type
        )
        
        cancellation_token = progress.cancellation_token
        timeout_task = None
        
        try:
            # Setup timeout if specified
            if timeout_seconds:
                timeout_task = asyncio.create_task(self._handle_timeout(operation_id, timeout_seconds))
            
            # Update state to running
            self.progress_orchestrator.update_progress(operation_id, state=OperationState.RUNNING)
            
            yield cancellation_token
            
            # Complete successfully
            self.progress_orchestrator.complete_operation(operation_id, success=True)
            
        except OperationCancelledException:
            # Handle cancellation
            self.progress_orchestrator.complete_operation(operation_id, success=False, final_message="Operation cancelled")
            raise
            
        except Exception as e:
            # Handle other errors
            self.progress_orchestrator.complete_operation(operation_id, success=False, final_message=f"Operation failed: {str(e)}")
            raise
            
        finally:
            # Cancel timeout task
            if timeout_task and not timeout_task.done():
                timeout_task.cancel()
    
    async def _handle_timeout(self, operation_id: str, timeout_seconds: float) -> None:
        """Handle operation timeout."""
        await asyncio.sleep(timeout_seconds)
        
        # Cancel operation due to timeout
        await self.cancel_operation(
            operation_id=operation_id,
            reason=CancellationReason.TIMEOUT,
            message=f"Operation timed out after {timeout_seconds} seconds",
            cancelled_by="timeout_handler"
        )
    
    def get_cancellation_statistics(self) -> Dict[str, Any]:
        """Get cancellation statistics."""
        return {
            "cancellation_stats": self.cancellation_stats.copy(),
            "global_cancellation_requested": self.global_cancellation_token.is_cancellation_requested(),
            "registered_handlers": list(self.cancellation_handlers.keys()),
            "active_operations": len(self.progress_orchestrator.get_all_active_operations())
        }


# Factory functions
def create_operation_cancellation_manager(
    progress_orchestrator: ProgressTrackingOrchestrator,
    transactional_state_manager: Optional[TransactionalStateManager] = None,
    retry_manager: Optional[EnhancedRetryManager] = None,
    ui_error_propagator: Optional[UIErrorPropagator] = None
) -> OperationCancellationManager:
    """Factory function to create operation cancellation manager."""
    return OperationCancellationManager(
        progress_orchestrator=progress_orchestrator,
        transactional_state_manager=transactional_state_manager,
        retry_manager=retry_manager,
        ui_error_propagator=ui_error_propagator
    )


def create_progress_tracking_orchestrator(ui_error_propagator: Optional[UIErrorPropagator] = None) -> ProgressTrackingOrchestrator:
    """Factory function to create progress tracking orchestrator."""
    return ProgressTrackingOrchestrator(ui_error_propagator=ui_error_propagator)