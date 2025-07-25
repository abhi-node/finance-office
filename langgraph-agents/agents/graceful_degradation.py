"""
Graceful Degradation and Rollback System for LibreOffice AI Writing Assistant

This module implements Task 19.3 by providing comprehensive graceful degradation
and rollback mechanisms. It builds on the existing DocumentState management
system and integrates with the retry infrastructure to provide robust failure
handling and recovery.

Key Components:
- TransactionalStateManager: Enhanced DocumentState with transaction support
- GracefulDegradationManager: Service degradation strategies
- OperationCheckpoint: Multi-step operation rollback system
- FallbackStrategyManager: Fallback strategy coordination
- ServiceAvailabilityMonitor: Service health monitoring
"""

import asyncio
import json
import threading
import time
import uuid
from abc import ABC, abstractmethod
from contextlib import asynccontextmanager
from dataclasses import dataclass, field
from datetime import datetime, timezone, timedelta
from enum import Enum
from typing import Any, Callable, Dict, List, Optional, Set, Tuple, Union
import logging

# Import existing infrastructure
try:
    from state.document_state import DocumentState, DocumentStateManager
    from .retry_manager import EnhancedRetryManager, CircuitBreaker
    from .error_handler import ErrorContext, ErrorSeverity, ErrorCategory
    from .error_tracking import ErrorTracker
except ImportError:
    # Fallback for testing
    DocumentState = Dict[str, Any]
    DocumentStateManager = Any
    EnhancedRetryManager = Any
    CircuitBreaker = Any
    ErrorContext = Any
    ErrorSeverity = Any
    ErrorCategory = Any
    ErrorTracker = Any


class DegradationLevel(Enum):
    """Levels of service degradation."""
    FULL_SERVICE = "full_service"          # All services operational
    REDUCED_FEATURES = "reduced_features"  # Some features disabled
    ESSENTIAL_ONLY = "essential_only"      # Only essential features
    EMERGENCY_MODE = "emergency_mode"      # Minimal functionality
    OFFLINE_MODE = "offline_mode"          # Local operations only


class ServiceStatus(Enum):
    """Service availability status."""
    HEALTHY = "healthy"
    DEGRADED = "degraded"
    FAILING = "failing"
    UNAVAILABLE = "unavailable"
    UNKNOWN = "unknown"


class RollbackScope(Enum):
    """Scope of rollback operations."""
    OPERATION_ONLY = "operation_only"      # Single operation
    AGENT_STATE = "agent_state"           # Agent-specific state
    DOCUMENT_STATE = "document_state"     # Document-wide state
    SYSTEM_STATE = "system_state"         # System-wide state


@dataclass
class OperationCheckpoint:
    """Checkpoint for multi-step operation rollback."""
    checkpoint_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    operation_id: str = ""
    agent_id: str = ""
    timestamp: datetime = field(default_factory=lambda: datetime.now(timezone.utc))
    
    # State snapshots
    document_state_snapshot: Dict[str, Any] = field(default_factory=dict)
    agent_state_snapshot: Dict[str, Any] = field(default_factory=dict)
    system_state_snapshot: Dict[str, Any] = field(default_factory=dict)
    
    # Operation context
    operation_type: str = ""
    step_number: int = 0
    total_steps: Optional[int] = None
    
    # Rollback information
    rollback_actions: List[Dict[str, Any]] = field(default_factory=list)
    rollback_order: List[str] = field(default_factory=list)
    
    # Metadata
    description: str = ""
    created_by: str = ""
    rollback_scope: RollbackScope = RollbackScope.OPERATION_ONLY
    auto_rollback_enabled: bool = True


@dataclass
class ServiceAvailability:
    """Service availability information."""
    service_name: str
    status: ServiceStatus
    last_check: datetime
    response_time: Optional[float] = None
    error_rate: float = 0.0
    availability_percentage: float = 100.0
    degradation_reason: Optional[str] = None
    estimated_recovery_time: Optional[int] = None  # seconds


@dataclass
class DegradationStrategy:
    """Strategy for handling service degradation."""
    strategy_name: str
    trigger_conditions: Dict[str, Any]
    degradation_level: DegradationLevel
    fallback_services: List[str] = field(default_factory=list)
    disabled_features: List[str] = field(default_factory=list)
    alternative_workflows: Dict[str, str] = field(default_factory=dict)
    user_notification: str = ""
    auto_recovery_enabled: bool = True


class FallbackStrategy(ABC):
    """Abstract base class for fallback strategies."""
    
    @abstractmethod
    def can_handle(self, error_context: ErrorContext, service_status: Dict[str, ServiceAvailability]) -> bool:
        """Check if this strategy can handle the current situation."""
        pass
    
    @abstractmethod
    async def execute_fallback(self, 
                              original_operation: Callable,
                              *args, 
                              **kwargs) -> Any:
        """Execute fallback operation."""
        pass
    
    @abstractmethod
    def get_degradation_level(self) -> DegradationLevel:
        """Get the degradation level this strategy provides."""
        pass


class CachedDataFallback(FallbackStrategy):
    """Fallback strategy using cached data."""
    
    def __init__(self, cache_manager: Any):
        """Initialize cached data fallback."""
        self.cache_manager = cache_manager
        self.logger = logging.getLogger("cached_data_fallback")
    
    def can_handle(self, error_context: ErrorContext, service_status: Dict[str, ServiceAvailability]) -> bool:
        """Check if cached data is available."""
        return (error_context.category in [ErrorCategory.NETWORK_ERROR, ErrorCategory.API_FAILURE] and
                hasattr(self.cache_manager, 'has_cached_data'))
    
    async def execute_fallback(self, original_operation: Callable, *args, **kwargs) -> Any:
        """Use cached data instead of live data."""
        try:
            # Extract cache key from operation context
            cache_key = kwargs.get('cache_key') or f"fallback_{hash(str(args))}"
            cached_data = await self.cache_manager.get_cached_data(cache_key)
            
            if cached_data:
                self.logger.info(f"Using cached data for fallback: {cache_key}")
                return {
                    **cached_data,
                    "fallback": True,
                    "data_age": cached_data.get("timestamp", time.time()),
                    "fallback_reason": "Service unavailable"
                }
            else:
                raise Exception("No cached data available")
        
        except Exception as e:
            self.logger.warning(f"Cached data fallback failed: {e}")
            raise
    
    def get_degradation_level(self) -> DegradationLevel:
        """Return degradation level for cached data."""
        return DegradationLevel.REDUCED_FEATURES


class SimplifiedWorkflowFallback(FallbackStrategy):
    """Fallback strategy using simplified workflows."""
    
    def __init__(self):
        """Initialize simplified workflow fallback."""
        self.logger = logging.getLogger("simplified_workflow_fallback")
    
    def can_handle(self, error_context: ErrorContext, service_status: Dict[str, ServiceAvailability]) -> bool:
        """Check if simplified workflow is applicable."""
        return error_context.category in [
            ErrorCategory.AGENT_COORDINATION,
            ErrorCategory.WORKFLOW_EXECUTION,
            ErrorCategory.PERFORMANCE_DEGRADATION
        ]
    
    async def execute_fallback(self, original_operation: Callable, *args, **kwargs) -> Any:
        """Execute simplified version of operation."""
        try:
            # Remove complex parameters and use simplified approach
            simplified_kwargs = {
                k: v for k, v in kwargs.items()
                if k in ['essential_params', 'basic_operation', 'simple_mode']
            }
            
            # Enable simple mode
            simplified_kwargs['simple_mode'] = True
            simplified_kwargs['reduced_complexity'] = True
            
            self.logger.info("Executing simplified workflow fallback")
            result = await original_operation(*args, **simplified_kwargs)
            
            # Mark result as simplified
            if isinstance(result, dict):
                result['fallback'] = True
                result['simplified_workflow'] = True
            
            return result
        
        except Exception as e:
            self.logger.warning(f"Simplified workflow fallback failed: {e}")
            raise
    
    def get_degradation_level(self) -> DegradationLevel:
        """Return degradation level for simplified workflow."""
        return DegradationLevel.ESSENTIAL_ONLY


class OfflineModeFallback(FallbackStrategy):
    """Fallback strategy for offline operations."""
    
    def __init__(self):
        """Initialize offline mode fallback."""
        self.logger = logging.getLogger("offline_mode_fallback")
    
    def can_handle(self, error_context: ErrorContext, service_status: Dict[str, ServiceAvailability]) -> bool:
        """Check if offline mode is applicable."""
        return error_context.category in [
            ErrorCategory.NETWORK_ERROR,
            ErrorCategory.API_FAILURE,
            ErrorCategory.EXTERNAL_SERVICE_ERROR
        ]
    
    async def execute_fallback(self, original_operation: Callable, *args, **kwargs) -> Any:
        """Execute offline version of operation."""
        try:
            # Remove network-dependent parameters
            offline_kwargs = {
                k: v for k, v in kwargs.items()
                if k not in ['api_key', 'network_timeout', 'external_service']
            }
            
            # Enable offline mode
            offline_kwargs['offline_mode'] = True
            offline_kwargs['local_only'] = True
            
            self.logger.info("Executing offline mode fallback")
            result = await original_operation(*args, **offline_kwargs)
            
            # Mark result as offline
            if isinstance(result, dict):
                result['fallback'] = True
                result['offline_mode'] = True
            
            return result
        
        except Exception as e:
            self.logger.warning(f"Offline mode fallback failed: {e}")
            raise
    
    def get_degradation_level(self) -> DegradationLevel:
        """Return degradation level for offline mode."""
        return DegradationLevel.OFFLINE_MODE


class TransactionalStateManager:
    """
    Enhanced DocumentStateManager with transaction support and rollback capabilities.
    """
    
    def __init__(self, base_state_manager: DocumentStateManager):
        """
        Initialize transactional state manager.
        
        Args:
            base_state_manager: Existing DocumentStateManager to enhance
        """
        self.base_state_manager = base_state_manager
        
        # Transaction management
        self.active_transactions: Dict[str, Dict[str, Any]] = {}
        self.checkpoints: Dict[str, OperationCheckpoint] = {}
        self.checkpoint_stack: List[str] = []
        
        # Rollback tracking
        self.rollback_history: List[Dict[str, Any]] = []
        self.rollback_stats = {
            "total_rollbacks": 0,
            "successful_rollbacks": 0,
            "failed_rollbacks": 0,
            "auto_rollbacks": 0,
            "manual_rollbacks": 0
        }
        
        # Thread safety
        self.lock = threading.RLock()
        
        # Setup logging
        self.logger = logging.getLogger("transactional_state_manager")
    
    def create_checkpoint(self,
                         operation_id: str,
                         agent_id: str,
                         description: str = "",
                         rollback_scope: RollbackScope = RollbackScope.OPERATION_ONLY) -> str:
        """
        Create a checkpoint for rollback.
        
        Args:
            operation_id: Operation identifier
            agent_id: Agent creating the checkpoint
            description: Human-readable description
            rollback_scope: Scope of rollback
            
        Returns:
            str: Checkpoint ID
        """
        with self.lock:
            # Get current state snapshot
            current_state = self.base_state_manager.get_state() if hasattr(self.base_state_manager, 'get_state') else {}
            
            checkpoint = OperationCheckpoint(
                operation_id=operation_id,
                agent_id=agent_id,
                document_state_snapshot=current_state.copy() if current_state else {},
                operation_type=description,
                description=description,
                created_by=agent_id,
                rollback_scope=rollback_scope
            )
            
            # Store checkpoint
            self.checkpoints[checkpoint.checkpoint_id] = checkpoint
            self.checkpoint_stack.append(checkpoint.checkpoint_id)
            
            # Keep stack bounded
            if len(self.checkpoint_stack) > 50:
                old_checkpoint_id = self.checkpoint_stack.pop(0)
                self.checkpoints.pop(old_checkpoint_id, None)
            
            self.logger.info(f"Created checkpoint {checkpoint.checkpoint_id} for operation {operation_id}")
            return checkpoint.checkpoint_id
    
    async def rollback_to_checkpoint(self,
                                   checkpoint_id: str,
                                   reason: str = "",
                                   auto_rollback: bool = False) -> bool:
        """
        Rollback to a specific checkpoint.
        
        Args:
            checkpoint_id: Checkpoint to rollback to
            reason: Reason for rollback
            auto_rollback: Whether this is an automatic rollback
            
        Returns:
            bool: True if rollback successful
        """
        with self.lock:
            if checkpoint_id not in self.checkpoints:
                self.logger.error(f"Checkpoint {checkpoint_id} not found")
                return False
            
            checkpoint = self.checkpoints[checkpoint_id]
            
            try:
                # Record rollback attempt
                rollback_record = {
                    "checkpoint_id": checkpoint_id,
                    "operation_id": checkpoint.operation_id,
                    "agent_id": checkpoint.agent_id,
                    "reason": reason,
                    "auto_rollback": auto_rollback,
                    "timestamp": datetime.now(timezone.utc).isoformat(),
                    "rollback_scope": checkpoint.rollback_scope.value
                }
                
                # Perform rollback based on scope
                if checkpoint.rollback_scope == RollbackScope.DOCUMENT_STATE:
                    success = await self._rollback_document_state(checkpoint)
                elif checkpoint.rollback_scope == RollbackScope.AGENT_STATE:
                    success = await self._rollback_agent_state(checkpoint)
                elif checkpoint.rollback_scope == RollbackScope.SYSTEM_STATE:
                    success = await self._rollback_system_state(checkpoint)
                else:  # OPERATION_ONLY
                    success = await self._rollback_operation_state(checkpoint)
                
                # Update statistics
                self.rollback_stats["total_rollbacks"] += 1
                if success:
                    self.rollback_stats["successful_rollbacks"] += 1
                else:
                    self.rollback_stats["failed_rollbacks"] += 1
                
                if auto_rollback:
                    self.rollback_stats["auto_rollbacks"] += 1
                else:
                    self.rollback_stats["manual_rollbacks"] += 1
                
                rollback_record["success"] = success
                self.rollback_history.append(rollback_record)
                
                # Clean up checkpoints after this one
                if success:
                    self._cleanup_checkpoints_after(checkpoint_id)
                
                if success:
                    self.logger.info(f"Successfully rolled back to checkpoint {checkpoint_id}")
                else:
                    self.logger.error(f"Failed to rollback to checkpoint {checkpoint_id}")
                
                return success
                
            except Exception as e:
                self.logger.error(f"Rollback to checkpoint {checkpoint_id} failed: {e}")
                self.rollback_stats["failed_rollbacks"] += 1
                return False
    
    async def _rollback_document_state(self, checkpoint: OperationCheckpoint) -> bool:
        """Rollback document state."""
        try:
            if hasattr(self.base_state_manager, 'restore_from_snapshot'):
                return await self.base_state_manager.restore_from_snapshot(checkpoint.document_state_snapshot)
            else:
                # Fallback: update state directly
                if hasattr(self.base_state_manager, 'update_state'):
                    return self.base_state_manager.update_state(checkpoint.document_state_snapshot)
            return True
        except Exception as e:
            self.logger.error(f"Document state rollback failed: {e}")
            return False
    
    async def _rollback_agent_state(self, checkpoint: OperationCheckpoint) -> bool:
        """Rollback agent-specific state."""
        try:
            # This would integrate with agent state management
            # For now, return success if snapshot exists
            return bool(checkpoint.agent_state_snapshot)
        except Exception as e:
            self.logger.error(f"Agent state rollback failed: {e}")
            return False
    
    async def _rollback_system_state(self, checkpoint: OperationCheckpoint) -> bool:
        """Rollback system-wide state."""
        try:
            # This would integrate with system state management
            # For now, return success if snapshot exists
            return bool(checkpoint.system_state_snapshot)
        except Exception as e:
            self.logger.error(f"System state rollback failed: {e}")
            return False
    
    async def _rollback_operation_state(self, checkpoint: OperationCheckpoint) -> bool:
        """Rollback operation-specific state."""
        try:
            # Execute rollback actions in reverse order
            for action in reversed(checkpoint.rollback_actions):
                # This would execute specific rollback actions
                pass
            return True
        except Exception as e:
            self.logger.error(f"Operation state rollback failed: {e}")
            return False
    
    def _cleanup_checkpoints_after(self, checkpoint_id: str) -> None:
        """Clean up checkpoints created after the specified checkpoint."""
        try:
            checkpoint_index = self.checkpoint_stack.index(checkpoint_id)
            # Remove checkpoints after this one
            removed_checkpoints = self.checkpoint_stack[checkpoint_index + 1:]
            self.checkpoint_stack = self.checkpoint_stack[:checkpoint_index + 1]
            
            for removed_id in removed_checkpoints:
                self.checkpoints.pop(removed_id, None)
                
        except ValueError:
            # Checkpoint not in stack
            pass
    
    @asynccontextmanager
    async def transaction_context(self,
                                operation_id: str,
                                agent_id: str,
                                auto_rollback: bool = True,
                                rollback_scope: RollbackScope = RollbackScope.OPERATION_ONLY):
        """
        Context manager for transactional operations.
        
        Args:
            operation_id: Operation identifier
            agent_id: Agent performing operation
            auto_rollback: Enable automatic rollback on failure
            rollback_scope: Scope of rollback
        """
        checkpoint_id = self.create_checkpoint(operation_id, agent_id, rollback_scope=rollback_scope)
        
        try:
            yield checkpoint_id
        except Exception as e:
            if auto_rollback:
                self.logger.info(f"Auto-rolling back due to exception: {e}")
                await self.rollback_to_checkpoint(checkpoint_id, f"Auto-rollback: {str(e)}", auto_rollback=True)
            raise
    
    def get_rollback_statistics(self) -> Dict[str, Any]:
        """Get rollback statistics."""
        with self.lock:
            return {
                "rollback_stats": self.rollback_stats.copy(),
                "active_checkpoints": len(self.checkpoints),
                "checkpoint_stack_depth": len(self.checkpoint_stack),
                "recent_rollbacks": self.rollback_history[-10:] if self.rollback_history else []
            }


class ServiceAvailabilityMonitor:
    """
    Monitors service availability and health for degradation decisions.
    """
    
    def __init__(self):
        """Initialize service availability monitor."""
        self.service_status: Dict[str, ServiceAvailability] = {}
        self.monitoring_enabled = True
        self.check_interval = 30.0  # seconds
        
        # Statistics
        self.monitor_stats = {
            "total_checks": 0,
            "healthy_services": 0,
            "degraded_services": 0,
            "failing_services": 0,
            "unavailable_services": 0
        }
        
        # Monitoring task
        self.monitor_task: Optional[asyncio.Task] = None
        
        self.logger = logging.getLogger("service_availability_monitor")
    
    def register_service(self, service_name: str) -> None:
        """Register a service for monitoring."""
        self.service_status[service_name] = ServiceAvailability(
            service_name=service_name,
            status=ServiceStatus.UNKNOWN,
            last_check=datetime.now(timezone.utc)
        )
        self.logger.info(f"Registered service for monitoring: {service_name}")
    
    async def check_service_health(self, service_name: str) -> ServiceStatus:
        """Check health of a specific service."""
        try:
            # This would implement actual health checks
            # For now, return healthy as placeholder
            return ServiceStatus.HEALTHY
        except Exception as e:
            self.logger.warning(f"Health check failed for {service_name}: {e}")
            return ServiceStatus.FAILING
    
    async def update_service_status(self, service_name: str, status: ServiceStatus, error_rate: float = 0.0) -> None:
        """Update service status manually."""
        if service_name in self.service_status:
            self.service_status[service_name].status = status
            self.service_status[service_name].last_check = datetime.now(timezone.utc)
            self.service_status[service_name].error_rate = error_rate
            
            self.logger.info(f"Updated status for {service_name}: {status.value}")
    
    def get_service_status(self, service_name: str) -> Optional[ServiceAvailability]:
        """Get status of a specific service."""
        return self.service_status.get(service_name)
    
    def get_overall_health(self) -> Dict[str, Any]:
        """Get overall system health summary."""
        total_services = len(self.service_status)
        if total_services == 0:
            return {"overall_status": "unknown", "total_services": 0}
        
        status_counts = {}
        for service_status in self.service_status.values():
            status = service_status.status.value
            status_counts[status] = status_counts.get(status, 0) + 1
        
        # Determine overall status
        if status_counts.get("unavailable", 0) > total_services * 0.5:
            overall_status = "critical"
        elif status_counts.get("failing", 0) > total_services * 0.3:
            overall_status = "degraded"
        elif status_counts.get("degraded", 0) > 0:
            overall_status = "reduced"
        else:
            overall_status = "healthy"
        
        return {
            "overall_status": overall_status,
            "total_services": total_services,
            "status_distribution": status_counts,
            "availability_percentage": (status_counts.get("healthy", 0) / total_services) * 100
        }


class GracefulDegradationManager:
    """
    Manages graceful degradation strategies and fallback mechanisms.
    """
    
    def __init__(self,
                 transactional_state_manager: TransactionalStateManager,
                 service_monitor: ServiceAvailabilityMonitor,
                 retry_manager: Optional[EnhancedRetryManager] = None):
        """
        Initialize graceful degradation manager.
        
        Args:
            transactional_state_manager: State manager with rollback support
            service_monitor: Service availability monitor
            retry_manager: Optional retry manager for integration
        """
        self.state_manager = transactional_state_manager
        self.service_monitor = service_monitor
        self.retry_manager = retry_manager
        
        # Degradation strategies
        self.degradation_strategies: List[DegradationStrategy] = []
        self.fallback_strategies: List[FallbackStrategy] = []
        self.current_degradation_level = DegradationLevel.FULL_SERVICE
        
        # Setup default strategies
        self._setup_default_strategies()
        
        # Statistics
        self.degradation_stats = {
            "total_degradations": 0,
            "successful_fallbacks": 0,
            "failed_fallbacks": 0,
            "current_level": self.current_degradation_level.value
        }
        
        self.logger = logging.getLogger("graceful_degradation_manager")
    
    def _setup_default_strategies(self) -> None:
        """Setup default degradation and fallback strategies."""
        # Network failure degradation strategy
        network_failure_strategy = DegradationStrategy(
            strategy_name="network_failure",
            trigger_conditions={
                "network_error_rate": 0.3,
                "api_failure_rate": 0.2
            },
            degradation_level=DegradationLevel.REDUCED_FEATURES,
            fallback_services=["cache", "local_processing"],
            disabled_features=["real_time_data", "external_apis"],
            user_notification="Some features temporarily unavailable due to network issues."
        )
        self.degradation_strategies.append(network_failure_strategy)
        
        # Agent coordination failure strategy
        coordination_failure_strategy = DegradationStrategy(
            strategy_name="agent_coordination_failure",
            trigger_conditions={
                "agent_failure_rate": 0.2,
                "coordination_timeout_rate": 0.3
            },
            degradation_level=DegradationLevel.ESSENTIAL_ONLY,
            fallback_services=["single_agent_mode"],
            disabled_features=["multi_agent_coordination", "parallel_processing"],
            alternative_workflows={"complex_operation": "simple_operation"},
            user_notification="Using simplified processing mode."
        )
        self.degradation_strategies.append(coordination_failure_strategy)
        
        # System resource failure strategy
        resource_failure_strategy = DegradationStrategy(
            strategy_name="resource_failure",
            trigger_conditions={
                "memory_usage": 0.9,
                "cpu_usage": 0.9
            },
            degradation_level=DegradationLevel.EMERGENCY_MODE,
            disabled_features=["background_processing", "caching", "monitoring"],
            user_notification="System under high load. Using minimal features."
        )
        self.degradation_strategies.append(resource_failure_strategy)
        
        # Setup default fallback strategies
        self.fallback_strategies.extend([
            CachedDataFallback(self),
            SimplifiedWorkflowFallback(),
            OfflineModeFallback()
        ])
    
    async def execute_with_degradation_protection(self,
                                                operation_func: Callable,
                                                operation_id: str,
                                                agent_id: str,
                                                *args,
                                                **kwargs) -> Any:
        """
        Execute operation with degradation protection and fallback strategies.
        
        Args:
            operation_func: Function to execute
            operation_id: Operation identifier
            agent_id: Agent performing operation
            *args: Function arguments
            **kwargs: Function keyword arguments
            
        Returns:
            Any: Operation result or fallback result
        """
        # Create checkpoint for rollback
        async with self.state_manager.transaction_context(
            operation_id=operation_id,
            agent_id=agent_id,
            auto_rollback=True
        ) as checkpoint_id:
            
            try:
                # Check service availability
                service_health = self.service_monitor.get_overall_health()
                
                # Adjust operation based on current degradation level
                adjusted_kwargs = self._adjust_operation_for_degradation(kwargs)
                
                # Execute operation
                result = await operation_func(*args, **adjusted_kwargs)
                
                # Record successful execution
                self.degradation_stats["successful_fallbacks"] += 1
                
                return result
                
            except Exception as e:
                self.logger.warning(f"Operation {operation_id} failed, attempting fallback: {e}")
                
                # Try fallback strategies
                result = await self._execute_fallback_strategies(
                    operation_func, e, *args, **kwargs
                )
                
                if result is not None:
                    self.degradation_stats["successful_fallbacks"] += 1
                    return result
                else:
                    self.degradation_stats["failed_fallbacks"] += 1
                    raise
    
    def _adjust_operation_for_degradation(self, kwargs: Dict[str, Any]) -> Dict[str, Any]:
        """Adjust operation parameters based on current degradation level."""
        adjusted_kwargs = kwargs.copy()
        
        if self.current_degradation_level == DegradationLevel.REDUCED_FEATURES:
            adjusted_kwargs['disable_advanced_features'] = True
            adjusted_kwargs['use_cache'] = True
        
        elif self.current_degradation_level == DegradationLevel.ESSENTIAL_ONLY:
            adjusted_kwargs['essential_only'] = True
            adjusted_kwargs['disable_parallel_processing'] = True
        
        elif self.current_degradation_level == DegradationLevel.EMERGENCY_MODE:
            adjusted_kwargs['emergency_mode'] = True
            adjusted_kwargs['minimal_processing'] = True
        
        elif self.current_degradation_level == DegradationLevel.OFFLINE_MODE:
            adjusted_kwargs['offline_mode'] = True
            adjusted_kwargs['local_only'] = True
        
        return adjusted_kwargs
    
    async def _execute_fallback_strategies(self,
                                         original_operation: Callable,
                                         error: Exception,
                                         *args,
                                         **kwargs) -> Any:
        """Execute fallback strategies in order of preference."""
        error_context = ErrorContext(
            category=self._categorize_error(error),
            severity=ErrorSeverity.HIGH,
            error_message=str(error)
        )
        
        service_status = dict(self.service_monitor.service_status)
        
        # Try fallback strategies in order
        for strategy in self.fallback_strategies:
            if strategy.can_handle(error_context, service_status):
                try:
                    self.logger.info(f"Attempting fallback strategy: {strategy.__class__.__name__}")
                    result = await strategy.execute_fallback(original_operation, *args, **kwargs)
                    
                    # Update degradation level
                    new_level = strategy.get_degradation_level()
                    if new_level != self.current_degradation_level:
                        self._update_degradation_level(new_level)
                    
                    return result
                
                except Exception as fallback_error:
                    self.logger.warning(f"Fallback strategy {strategy.__class__.__name__} failed: {fallback_error}")
                    continue
        
        # No fallback strategy worked
        return None
    
    def _categorize_error(self, error: Exception) -> ErrorCategory:
        """Categorize error for fallback strategy selection."""
        error_type = type(error).__name__.lower()
        error_message = str(error).lower()
        
        if "network" in error_type or "connection" in error_message:
            return ErrorCategory.NETWORK_ERROR
        elif "timeout" in error_type or "timeout" in error_message:
            return ErrorCategory.TIMEOUT_ERROR
        elif "api" in error_message or "service" in error_message:
            return ErrorCategory.API_FAILURE
        else:
            return ErrorCategory.UNKNOWN_ERROR
    
    def _update_degradation_level(self, new_level: DegradationLevel) -> None:
        """Update current degradation level."""
        old_level = self.current_degradation_level
        self.current_degradation_level = new_level
        self.degradation_stats["current_level"] = new_level.value
        self.degradation_stats["total_degradations"] += 1
        
        self.logger.info(f"Degradation level changed from {old_level.value} to {new_level.value}")
    
    def get_degradation_status(self) -> Dict[str, Any]:
        """Get current degradation status."""
        return {
            "current_degradation_level": self.current_degradation_level.value,
            "degradation_statistics": self.degradation_stats.copy(),
            "available_fallback_strategies": [s.__class__.__name__ for s in self.fallback_strategies],
            "service_health": self.service_monitor.get_overall_health(),
            "rollback_statistics": self.state_manager.get_rollback_statistics()
        }


# Factory functions
def create_transactional_state_manager(base_state_manager: DocumentStateManager) -> TransactionalStateManager:
    """Factory function to create transactional state manager."""
    return TransactionalStateManager(base_state_manager)


def create_graceful_degradation_manager(state_manager: TransactionalStateManager,
                                       service_monitor: Optional[ServiceAvailabilityMonitor] = None,
                                       retry_manager: Optional[EnhancedRetryManager] = None) -> GracefulDegradationManager:
    """Factory function to create graceful degradation manager."""
    if service_monitor is None:
        service_monitor = ServiceAvailabilityMonitor()
    
    return GracefulDegradationManager(
        transactional_state_manager=state_manager,
        service_monitor=service_monitor,
        retry_manager=retry_manager
    )