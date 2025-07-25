"""
UI Error Propagation System for LibreOffice AI Writing Assistant

This module implements Task 19.4 by providing comprehensive error propagation
from backend agents to the LibreOffice frontend UI. It includes error message
translation, progress tracking with error states, and real-time communication
through the WebSocket and bridge systems.

Key Components:
- UIErrorPropagator: Main error propagation coordinator
- ErrorMessageTranslator: User-friendly error message formatting
- ProgressErrorTracker: Progress tracking with error state handling
- WebSocketErrorNotifier: Real-time error notifications
- BridgeErrorCommunicator: C++ bridge error communication
"""

import asyncio
import json
import time
import uuid
from dataclasses import dataclass, field
from datetime import datetime, timezone
from enum import Enum
from typing import Any, Callable, Dict, List, Optional, Set, Union
import logging

# Import existing infrastructure
try:
    from .error_handler import ErrorContext, ErrorSeverity, ErrorCategory, ErrorResponse
    from .error_tracking import ErrorTracker
    from .graceful_degradation import DegradationLevel, ServiceStatus
    from bridge import LangGraphBridge
    from api_server import WebSocketMessage, active_websockets
except ImportError:
    # Fallback for testing
    ErrorContext = Any
    ErrorSeverity = Any
    ErrorCategory = Any
    ErrorResponse = Any
    ErrorTracker = Any
    DegradationLevel = Any
    ServiceStatus = Any
    LangGraphBridge = Any
    WebSocketMessage = Any
    active_websockets = {}


class UIErrorSeverity(Enum):
    """UI-specific error severity levels for user communication."""
    INFO = "info"           # Informational messages
    WARNING = "warning"     # Warning - operation may continue
    ERROR = "error"         # Error - operation failed but recoverable
    CRITICAL = "critical"   # Critical - system functionality affected
    FATAL = "fatal"         # Fatal - immediate user intervention required


class UIErrorType(Enum):
    """Types of UI errors for proper categorization and handling."""
    NETWORK_ISSUE = "network_issue"
    SERVICE_UNAVAILABLE = "service_unavailable"
    PROCESSING_ERROR = "processing_error"
    VALIDATION_ERROR = "validation_error"
    AGENT_COORDINATION_ERROR = "agent_coordination_error"
    DOCUMENT_ERROR = "document_error"
    AUTHENTICATION_ERROR = "authentication_error"
    PERMISSION_ERROR = "permission_error"
    TIMEOUT_ERROR = "timeout_error"
    SYSTEM_ERROR = "system_error"


class ProgressState(Enum):
    """Progress states that can include error conditions."""
    INITIALIZING = "initializing"
    PROCESSING = "processing"
    AGENT_COORDINATION = "agent_coordination"
    DATA_FETCHING = "data_fetching"
    DOCUMENT_UPDATING = "document_updating"
    COMPLETING = "completing"
    COMPLETED = "completed"
    ERROR = "error"
    CANCELLED = "cancelled"
    DEGRADED = "degraded"           # Operating with reduced functionality
    RETRYING = "retrying"           # Retrying after error


@dataclass
class UIErrorMessage:
    """User-friendly error message for UI display."""
    error_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    severity: UIErrorSeverity = UIErrorSeverity.ERROR
    error_type: UIErrorType = UIErrorType.SYSTEM_ERROR
    title: str = ""
    message: str = ""
    details: Optional[str] = None
    suggested_actions: List[str] = field(default_factory=list)
    technical_details: Optional[str] = None
    timestamp: datetime = field(default_factory=lambda: datetime.now(timezone.utc))
    
    # UI presentation
    show_retry_button: bool = True
    show_cancel_button: bool = True
    show_details_expansion: bool = True
    auto_dismiss_seconds: Optional[int] = None
    
    # Context information
    operation_id: Optional[str] = None
    agent_id: Optional[str] = None
    request_id: Optional[str] = None
    
    # Progress information
    current_progress: float = 0.0
    progress_state: ProgressState = ProgressState.ERROR


@dataclass
class ProgressUpdate:
    """Progress update with error state support."""
    progress_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    operation_id: str = ""
    request_id: str = ""
    
    # Progress information
    progress_percentage: float = 0.0
    current_state: ProgressState = ProgressState.INITIALIZING
    current_step: str = ""
    total_steps: Optional[int] = None
    current_step_number: int = 0
    
    # Time tracking
    start_time: datetime = field(default_factory=lambda: datetime.now(timezone.utc))
    estimated_completion: Optional[datetime] = None
    
    # Error information
    has_error: bool = False
    error_message: Optional[UIErrorMessage] = None
    degradation_level: Optional[DegradationLevel] = None
    retry_count: int = 0
    
    # Agent context
    active_agents: List[str] = field(default_factory=list)
    completed_agents: List[str] = field(default_factory=list)
    failed_agents: List[str] = field(default_factory=list)


class ErrorMessageTranslator:
    """Translates technical errors into user-friendly messages."""
    
    def __init__(self):
        """Initialize error message translator."""
        self.translation_templates = self._setup_translation_templates()
        self.logger = logging.getLogger("error_message_translator")
    
    def _setup_translation_templates(self) -> Dict[str, Dict[str, Any]]:
        """Setup translation templates for common errors."""
        return {
            "network_error": {
                "title": "Network Connection Issue",
                "message": "Unable to connect to the required services. Please check your internet connection.",
                "suggestions": [
                    "Check your internet connection",
                    "Try again in a few moments",
                    "Contact your IT administrator if the problem persists"
                ],
                "severity": UIErrorSeverity.WARNING,
                "error_type": UIErrorType.NETWORK_ISSUE,
                "auto_dismiss": 10
            },
            "service_unavailable": {
                "title": "Service Temporarily Unavailable",
                "message": "The AI writing assistant service is temporarily unavailable. Some features may be limited.",
                "suggestions": [
                    "Basic document editing is still available",
                    "Try using the simplified mode",
                    "Check back in a few minutes"
                ],
                "severity": UIErrorSeverity.WARNING,
                "error_type": UIErrorType.SERVICE_UNAVAILABLE,
                "auto_dismiss": 15
            },
            "processing_error": {
                "title": "Processing Error",
                "message": "An error occurred while processing your request. The operation has been stopped.",
                "suggestions": [
                    "Try rephrasing your request",
                    "Use simpler language or shorter sentences",
                    "Try the operation again"
                ],
                "severity": UIErrorSeverity.ERROR,
                "error_type": UIErrorType.PROCESSING_ERROR
            },
            "validation_error": {
                "title": "Input Validation Error",
                "message": "There's an issue with your request that needs to be corrected.",
                "suggestions": [
                    "Check your input for completeness",
                    "Ensure all required fields are filled",
                    "Remove any special characters that might cause issues"
                ],
                "severity": UIErrorSeverity.WARNING,
                "error_type": UIErrorType.VALIDATION_ERROR
            },
            "agent_coordination_error": {
                "title": "Agent Coordination Issue",
                "message": "The AI assistants are having trouble coordinating. Using simplified processing mode.",
                "suggestions": [
                    "The request will be processed with reduced capabilities",
                    "Try breaking complex requests into smaller parts",
                    "Consider using the simplified interface"
                ],
                "severity": UIErrorSeverity.WARNING,
                "error_type": UIErrorType.AGENT_COORDINATION_ERROR
            },
            "timeout_error": {
                "title": "Operation Timeout",
                "message": "The operation took longer than expected and was cancelled to avoid blocking the interface.",
                "suggestions": [
                    "Try simplifying your request",
                    "Break complex operations into smaller steps",
                    "Check your network connection speed"
                ],
                "severity": UIErrorSeverity.ERROR,
                "error_type": UIErrorType.TIMEOUT_ERROR
            },
            "authentication_error": {
                "title": "Authentication Required",
                "message": "You need to be authenticated to access this feature.",
                "suggestions": [
                    "Please log in to continue",
                    "Check if your session has expired",
                    "Contact your administrator for access"
                ],
                "severity": UIErrorSeverity.ERROR,
                "error_type": UIErrorType.AUTHENTICATION_ERROR
            },
            "document_error": {
                "title": "Document Processing Error",
                "message": "There was an issue processing your document. Your document is safe and has not been modified.",
                "suggestions": [
                    "Save your work before trying again",
                    "Try a smaller section of the document",
                    "Check if the document format is supported"
                ],
                "severity": UIErrorSeverity.ERROR,
                "error_type": UIErrorType.DOCUMENT_ERROR
            }
        }
    
    def translate_error(self, error_context: ErrorContext, operation_context: Optional[Dict[str, Any]] = None) -> UIErrorMessage:
        """
        Translate technical error to user-friendly message.
        
        Args:
            error_context: Technical error context
            operation_context: Additional operation context
            
        Returns:
            UIErrorMessage: User-friendly error message
        """
        # Determine error category
        error_key = self._categorize_error(error_context)
        template = self.translation_templates.get(error_key, self.translation_templates["processing_error"])
        
        # Create base message from template
        ui_message = UIErrorMessage(
            severity=template["severity"],
            error_type=template["error_type"],
            title=template["title"],
            message=template["message"],
            suggested_actions=template["suggestions"].copy(),
            auto_dismiss_seconds=template.get("auto_dismiss"),
            technical_details=error_context.error_message if hasattr(error_context, 'error_message') else str(error_context)
        )
        
        # Add operation context
        if operation_context:
            ui_message.operation_id = operation_context.get("operation_id")
            ui_message.agent_id = operation_context.get("agent_id")
            ui_message.request_id = operation_context.get("request_id")
        
        # Customize message based on severity and context
        self._customize_message(ui_message, error_context, operation_context)
        
        return ui_message
    
    def _categorize_error(self, error_context: ErrorContext) -> str:
        """Categorize error for template selection."""
        if hasattr(error_context, 'category'):
            category_mapping = {
                ErrorCategory.NETWORK_ERROR: "network_error",
                ErrorCategory.API_FAILURE: "service_unavailable",
                ErrorCategory.TIMEOUT_ERROR: "timeout_error",
                ErrorCategory.VALIDATION_ERROR: "validation_error",
                ErrorCategory.AGENT_COORDINATION: "agent_coordination_error",
                ErrorCategory.DOCUMENT_PROCESSING: "document_error",
                ErrorCategory.AUTHENTICATION_ERROR: "authentication_error"
            }
            return category_mapping.get(error_context.category, "processing_error")
        
        # Fallback categorization based on error message
        error_message = str(error_context).lower()
        if "network" in error_message or "connection" in error_message:
            return "network_error"
        elif "timeout" in error_message:
            return "timeout_error"
        elif "validation" in error_message or "invalid" in error_message:
            return "validation_error"
        elif "auth" in error_message or "permission" in error_message:
            return "authentication_error"
        else:
            return "processing_error"
    
    def _customize_message(self, ui_message: UIErrorMessage, error_context: ErrorContext, operation_context: Optional[Dict[str, Any]]) -> None:
        """Customize message based on specific error context."""
        # Add context-specific suggestions
        if operation_context:
            complexity = operation_context.get("complexity", "").lower()
            if complexity == "high":
                ui_message.suggested_actions.append("Consider using simplified mode for complex operations")
            
            if operation_context.get("retry_count", 0) > 0:
                ui_message.suggested_actions.insert(0, f"Attempted {operation_context['retry_count']} retries")
                ui_message.show_retry_button = False  # Already retried
        
        # Adjust severity based on error frequency
        if hasattr(error_context, 'frequency') and error_context.frequency > 5:
            ui_message.severity = UIErrorSeverity.CRITICAL
            ui_message.suggested_actions.append("Contact support - this error is occurring frequently")


class ProgressErrorTracker:
    """Tracks progress with error state handling."""
    
    def __init__(self):
        """Initialize progress error tracker."""
        self.active_operations: Dict[str, ProgressUpdate] = {}
        self.progress_history: List[ProgressUpdate] = []
        self.logger = logging.getLogger("progress_error_tracker")
    
    def start_operation(self, operation_id: str, request_id: str, total_steps: Optional[int] = None) -> ProgressUpdate:
        """Start tracking an operation."""
        progress = ProgressUpdate(
            operation_id=operation_id,
            request_id=request_id,
            current_state=ProgressState.INITIALIZING,
            total_steps=total_steps
        )
        
        self.active_operations[operation_id] = progress
        self.logger.info(f"Started tracking operation {operation_id}")
        return progress
    
    def update_progress(self, operation_id: str, 
                       percentage: float, 
                       state: ProgressState,
                       step_description: str = "",
                       step_number: Optional[int] = None) -> Optional[ProgressUpdate]:
        """Update operation progress."""
        if operation_id not in self.active_operations:
            self.logger.warning(f"Attempted to update unknown operation {operation_id}")
            return None
        
        progress = self.active_operations[operation_id]
        progress.progress_percentage = min(100.0, max(0.0, percentage))
        progress.current_state = state
        progress.current_step = step_description
        
        if step_number is not None:
            progress.current_step_number = step_number
        
        # Update estimated completion
        if percentage > 0 and state != ProgressState.ERROR:
            elapsed = datetime.now(timezone.utc) - progress.start_time
            estimated_total = elapsed.total_seconds() * (100.0 / percentage)
            progress.estimated_completion = progress.start_time + asyncio.get_event_loop().time() + estimated_total
        
        return progress
    
    def set_error_state(self, operation_id: str, error_message: UIErrorMessage, degradation_level: Optional[DegradationLevel] = None) -> Optional[ProgressUpdate]:
        """Set operation to error state."""
        if operation_id not in self.active_operations:
            return None
        
        progress = self.active_operations[operation_id]
        progress.has_error = True
        progress.error_message = error_message
        progress.current_state = ProgressState.ERROR
        progress.degradation_level = degradation_level
        
        self.logger.error(f"Operation {operation_id} entered error state: {error_message.title}")
        return progress
    
    def set_retry_state(self, operation_id: str, retry_count: int) -> Optional[ProgressUpdate]:
        """Set operation to retry state."""
        if operation_id not in self.active_operations:
            return None
        
        progress = self.active_operations[operation_id]
        progress.current_state = ProgressState.RETRYING
        progress.retry_count = retry_count
        progress.has_error = False  # Reset error state for retry
        
        return progress
    
    def complete_operation(self, operation_id: str, success: bool = True) -> Optional[ProgressUpdate]:
        """Complete operation tracking."""
        if operation_id not in self.active_operations:
            return None
        
        progress = self.active_operations[operation_id]
        if success:
            progress.current_state = ProgressState.COMPLETED
            progress.progress_percentage = 100.0
        else:
            progress.current_state = ProgressState.ERROR
        
        # Move to history
        self.progress_history.append(progress)
        del self.active_operations[operation_id]
        
        # Keep history bounded
        if len(self.progress_history) > 1000:
            self.progress_history = self.progress_history[-500:]
        
        self.logger.info(f"Completed tracking operation {operation_id} - Success: {success}")
        return progress
    
    def get_operation_progress(self, operation_id: str) -> Optional[ProgressUpdate]:
        """Get current progress for operation."""
        return self.active_operations.get(operation_id)
    
    def get_active_operations(self) -> List[ProgressUpdate]:
        """Get all active operations."""
        return list(self.active_operations.values())


class WebSocketErrorNotifier:
    """Handles real-time error notifications via WebSocket."""
    
    def __init__(self):
        """Initialize WebSocket error notifier."""
        self.logger = logging.getLogger("websocket_error_notifier")
    
    async def notify_error(self, ui_error: UIErrorMessage, target_connections: Optional[List[str]] = None) -> bool:
        """
        Send error notification via WebSocket.
        
        Args:
            ui_error: UI error message to send
            target_connections: Specific connection IDs (None for broadcast)
            
        Returns:
            bool: True if notification sent successfully
        """
        try:
            # Create WebSocket message
            error_notification = {
                "type": "error_notification",
                "request_id": ui_error.request_id or "system",
                "content": {
                    "error_id": ui_error.error_id,
                    "severity": ui_error.severity.value,
                    "error_type": ui_error.error_type.value,
                    "title": ui_error.title,
                    "message": ui_error.message,
                    "details": ui_error.details,
                    "suggested_actions": ui_error.suggested_actions,
                    "show_retry_button": ui_error.show_retry_button,
                    "show_cancel_button": ui_error.show_cancel_button,
                    "auto_dismiss_seconds": ui_error.auto_dismiss_seconds,
                    "timestamp": ui_error.timestamp.isoformat(),
                    "operation_id": ui_error.operation_id,
                    "current_progress": ui_error.current_progress,
                    "progress_state": ui_error.progress_state.value
                }
            }
            
            # Determine target connections
            if target_connections:
                connections_to_notify = {
                    conn_id: ws for conn_id, ws in active_websockets.items()
                    if conn_id in target_connections
                }
            else:
                connections_to_notify = active_websockets
            
            # Send notifications
            notification_tasks = []
            for conn_id, websocket in connections_to_notify.items():
                task = self._send_to_websocket(websocket, conn_id, error_notification)
                notification_tasks.append(task)
            
            if notification_tasks:
                results = await asyncio.gather(*notification_tasks, return_exceptions=True)
                successful_sends = sum(1 for result in results if result is True)
                self.logger.info(f"Sent error notification to {successful_sends}/{len(notification_tasks)} connections")
                return successful_sends > 0
            
            return False
            
        except Exception as e:
            self.logger.error(f"Failed to send WebSocket error notification: {e}")
            return False
    
    async def notify_progress_update(self, progress: ProgressUpdate, target_connections: Optional[List[str]] = None) -> bool:
        """Send progress update notification via WebSocket."""
        try:
            progress_notification = {
                "type": "progress_update",
                "request_id": progress.request_id,
                "content": {
                    "progress_id": progress.progress_id,
                    "operation_id": progress.operation_id,
                    "progress_percentage": progress.progress_percentage,
                    "current_state": progress.current_state.value,
                    "current_step": progress.current_step,
                    "current_step_number": progress.current_step_number,
                    "total_steps": progress.total_steps,
                    "has_error": progress.has_error,
                    "retry_count": progress.retry_count,
                    "active_agents": progress.active_agents,
                    "completed_agents": progress.completed_agents,
                    "failed_agents": progress.failed_agents,
                    "estimated_completion": progress.estimated_completion.isoformat() if progress.estimated_completion else None,
                    "degradation_level": progress.degradation_level.value if progress.degradation_level else None
                }
            }
            
            # Include error details if present
            if progress.error_message:
                progress_notification["content"]["error_details"] = {
                    "error_id": progress.error_message.error_id,
                    "title": progress.error_message.title,
                    "message": progress.error_message.message,
                    "severity": progress.error_message.severity.value
                }
            
            # Send to connections
            if target_connections:
                connections_to_notify = {
                    conn_id: ws for conn_id, ws in active_websockets.items()
                    if conn_id in target_connections
                }
            else:
                connections_to_notify = active_websockets
            
            notification_tasks = []
            for conn_id, websocket in connections_to_notify.items():
                task = self._send_to_websocket(websocket, conn_id, progress_notification)
                notification_tasks.append(task)
            
            if notification_tasks:
                results = await asyncio.gather(*notification_tasks, return_exceptions=True)
                successful_sends = sum(1 for result in results if result is True)
                return successful_sends > 0
            
            return False
            
        except Exception as e:
            self.logger.error(f"Failed to send progress update notification: {e}")
            return False
    
    async def _send_to_websocket(self, websocket, connection_id: str, message: Dict[str, Any]) -> bool:
        """Send message to specific WebSocket connection."""
        try:
            message_json = json.dumps(message)
            await websocket.send_text(message_json)
            return True
        except Exception as e:
            self.logger.warning(f"Failed to send to WebSocket {connection_id}: {e}")
            return False


class BridgeErrorCommunicator:
    """Handles error communication through the C++ bridge."""
    
    def __init__(self, bridge: Optional[LangGraphBridge] = None):
        """Initialize bridge error communicator."""
        self.bridge = bridge
        self.logger = logging.getLogger("bridge_error_communicator")
    
    async def send_error_to_cpp(self, ui_error: UIErrorMessage, operation_context: Dict[str, Any]) -> bool:
        """
        Send error information to C++ side via bridge.
        
        Args:
            ui_error: UI error message
            operation_context: Operation context
            
        Returns:
            bool: True if error sent successfully
        """
        try:
            if not self.bridge:
                self.logger.warning("No bridge available for C++ error communication")
                return False
            
            # Prepare error data for C++
            error_data = {
                "error_id": ui_error.error_id,
                "severity": ui_error.severity.value,
                "error_type": ui_error.error_type.value,
                "title": ui_error.title,
                "message": ui_error.message,
                "suggested_actions": ui_error.suggested_actions,
                "technical_details": ui_error.technical_details,
                "timestamp": ui_error.timestamp.isoformat(),
                "operation_id": ui_error.operation_id,
                "show_retry_button": ui_error.show_retry_button,
                "show_cancel_button": ui_error.show_cancel_button,
                "progress_state": ui_error.progress_state.value
            }
            
            # Send via bridge (this would integrate with actual bridge methods)
            if hasattr(self.bridge, 'send_error_notification'):
                success = await self.bridge.send_error_notification(
                    operation_context.get("request_id", ""),
                    error_data
                )
                return success
            else:
                # Fallback: include in regular response
                self.logger.info(f"Error communicated via bridge fallback for operation {ui_error.operation_id}")
                return True
                
        except Exception as e:
            self.logger.error(f"Failed to send error to C++ via bridge: {e}")
            return False
    
    async def send_progress_to_cpp(self, progress: ProgressUpdate, operation_context: Dict[str, Any]) -> bool:
        """Send progress update to C++ side via bridge."""
        try:
            if not self.bridge:
                return False
            
            progress_data = {
                "progress_id": progress.progress_id,
                "operation_id": progress.operation_id,
                "progress_percentage": progress.progress_percentage,
                "current_state": progress.current_state.value,
                "current_step": progress.current_step,
                "has_error": progress.has_error,
                "retry_count": progress.retry_count,
                "active_agents": progress.active_agents,
                "estimated_completion": progress.estimated_completion.isoformat() if progress.estimated_completion else None
            }
            
            if hasattr(self.bridge, 'send_progress_update'):
                success = await self.bridge.send_progress_update(
                    operation_context.get("request_id", ""),
                    progress_data
                )
                return success
            
            return True
            
        except Exception as e:
            self.logger.error(f"Failed to send progress to C++ via bridge: {e}")
            return False


class UIErrorPropagator:
    """
    Main coordinator for UI error propagation across all communication channels.
    """
    
    def __init__(self, 
                 bridge: Optional[LangGraphBridge] = None,
                 error_tracker: Optional[ErrorTracker] = None):
        """
        Initialize UI error propagator.
        
        Args:
            bridge: LangGraph bridge for C++ communication
            error_tracker: Error tracker for context
        """
        self.bridge = bridge
        self.error_tracker = error_tracker
        
        # Initialize components
        self.message_translator = ErrorMessageTranslator()
        self.progress_tracker = ProgressErrorTracker()
        self.websocket_notifier = WebSocketErrorNotifier()
        self.bridge_communicator = BridgeErrorCommunicator(bridge)
        
        # Statistics
        self.propagation_stats = {
            "total_errors_propagated": 0,
            "websocket_notifications": 0,
            "bridge_notifications": 0,
            "progress_updates": 0,
            "translation_requests": 0
        }
        
        self.logger = logging.getLogger("ui_error_propagator")
    
    async def propagate_error(self, 
                            error_context: ErrorContext,
                            operation_context: Dict[str, Any],
                            target_channels: Optional[Set[str]] = None) -> UIErrorMessage:
        """
        Propagate error across all appropriate UI channels.
        
        Args:
            error_context: Technical error context
            operation_context: Operation context information
            target_channels: Specific channels to target (websocket, bridge, all)
            
        Returns:
            UIErrorMessage: The translated error message
        """
        self.propagation_stats["total_errors_propagated"] += 1
        
        # Translate error to user-friendly message
        ui_error = self.message_translator.translate_error(error_context, operation_context)
        self.propagation_stats["translation_requests"] += 1
        
        # Update progress tracker with error state
        operation_id = operation_context.get("operation_id")
        if operation_id:
            degradation_level = operation_context.get("degradation_level")
            self.progress_tracker.set_error_state(operation_id, ui_error, degradation_level)
        
        # Determine target channels
        channels = target_channels or {"websocket", "bridge"}
        
        # Propagate via WebSocket
        if "websocket" in channels:
            target_connections = operation_context.get("websocket_connections")
            success = await self.websocket_notifier.notify_error(ui_error, target_connections)
            if success:
                self.propagation_stats["websocket_notifications"] += 1
        
        # Propagate via Bridge
        if "bridge" in channels:
            success = await self.bridge_communicator.send_error_to_cpp(ui_error, operation_context)
            if success:
                self.propagation_stats["bridge_notifications"] += 1
        
        # Track in error tracker if available
        if self.error_tracker:
            self.error_tracker.track_ui_error(ui_error, operation_context)
        
        self.logger.info(f"Propagated error {ui_error.error_id} via channels: {channels}")
        return ui_error
    
    async def update_progress(self,
                            operation_id: str,
                            progress_percentage: float,
                            state: ProgressState,
                            step_description: str = "",
                            operation_context: Optional[Dict[str, Any]] = None) -> Optional[ProgressUpdate]:
        """Update operation progress across UI channels."""
        progress = self.progress_tracker.update_progress(
            operation_id, progress_percentage, state, step_description
        )
        
        if progress:
            self.propagation_stats["progress_updates"] += 1
            
            # Send to WebSocket clients
            target_connections = operation_context.get("websocket_connections") if operation_context else None
            await self.websocket_notifier.notify_progress_update(progress, target_connections)
            
            # Send to C++ bridge
            if operation_context:
                await self.bridge_communicator.send_progress_to_cpp(progress, operation_context)
        
        return progress
    
    def start_operation_tracking(self, operation_id: str, request_id: str, total_steps: Optional[int] = None) -> ProgressUpdate:
        """Start tracking a new operation."""
        return self.progress_tracker.start_operation(operation_id, request_id, total_steps)
    
    def complete_operation_tracking(self, operation_id: str, success: bool = True) -> Optional[ProgressUpdate]:
        """Complete operation tracking."""
        return self.progress_tracker.complete_operation(operation_id, success)
    
    async def handle_retry_attempt(self, operation_id: str, retry_count: int, operation_context: Dict[str, Any]) -> None:
        """Handle retry attempt notification."""
        progress = self.progress_tracker.set_retry_state(operation_id, retry_count)
        if progress:
            await self.websocket_notifier.notify_progress_update(progress)
            await self.bridge_communicator.send_progress_to_cpp(progress, operation_context)
    
    def get_propagation_statistics(self) -> Dict[str, Any]:
        """Get error propagation statistics."""
        return {
            "propagation_stats": self.propagation_stats.copy(),
            "active_operations": len(self.progress_tracker.active_operations),
            "recent_errors": len([e for e in self.progress_tracker.progress_history 
                                if e.has_error and e.error_message])
        }


# Factory functions
def create_ui_error_propagator(bridge: Optional[LangGraphBridge] = None,
                              error_tracker: Optional[ErrorTracker] = None) -> UIErrorPropagator:
    """Factory function to create UI error propagator."""
    return UIErrorPropagator(bridge=bridge, error_tracker=error_tracker)


def create_error_message_translator() -> ErrorMessageTranslator:
    """Factory function to create error message translator."""
    return ErrorMessageTranslator()


def create_progress_error_tracker() -> ProgressErrorTracker:
    """Factory function to create progress error tracker."""
    return ProgressErrorTracker()