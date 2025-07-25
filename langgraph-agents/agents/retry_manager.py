"""
Enhanced Retry and Backoff System for LibreOffice AI Writing Assistant

This module implements Task 19.2 by extending the existing ErrorRecoveryManager
infrastructure with advanced retry logic, circuit breaker patterns, and concurrent
operation support. It integrates seamlessly with the current architecture while
providing sophisticated retry capabilities.

Key Components:
- EnhancedRetryManager: Extends existing ErrorRecoveryManager with concurrent support
- CircuitBreakerManager: Implements circuit breaker patterns for failing services  
- RetryPolicyManager: Configurable retry policies per operation type
- ConcurrentRetryCoordinator: Manages retries across multiple concurrent operations
- BackoffStrategy: Various backoff algorithms (exponential, linear, fibonacci)
"""

import asyncio
import random
import threading
import time
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from datetime import datetime, timezone, timedelta
from enum import Enum
from typing import Any, Callable, Dict, List, Optional, Set, Tuple, Union
from concurrent.futures import ThreadPoolExecutor, Future
from contextlib import asynccontextmanager
import logging
import uuid

# Import existing error infrastructure
try:
    from .utils import ErrorRecoveryManager
    from .error_handler import ErrorCategory, ErrorSeverity, ErrorContext, UnifiedErrorCoordinator
    from .error_tracking import ErrorTracker
except ImportError:
    # Fallback for testing
    ErrorRecoveryManager = Any
    ErrorCategory = Any
    ErrorSeverity = Any
    ErrorContext = Any
    UnifiedErrorCoordinator = Any
    ErrorTracker = Any


class BackoffStrategy(Enum):
    """Backoff strategy algorithms for retry operations."""
    EXPONENTIAL = "exponential"
    LINEAR = "linear"
    FIBONACCI = "fibonacci"
    FIXED = "fixed"
    RANDOM_JITTER = "random_jitter"
    EXPONENTIAL_JITTER = "exponential_jitter"


class CircuitBreakerState(Enum):
    """Circuit breaker states for service failure handling."""
    CLOSED = "closed"        # Normal operation
    OPEN = "open"           # Service failing, block requests
    HALF_OPEN = "half_open" # Testing if service recovered


@dataclass
class RetryPolicy:
    """Configuration for retry behavior per operation type."""
    operation_type: str
    max_attempts: int = 3
    base_delay: float = 1.0
    max_delay: float = 60.0
    backoff_strategy: BackoffStrategy = BackoffStrategy.EXPONENTIAL
    backoff_multiplier: float = 2.0
    jitter_range: float = 0.1
    timeout_per_attempt: Optional[float] = None
    circuit_breaker_enabled: bool = True
    circuit_breaker_threshold: int = 5
    circuit_breaker_timeout: float = 60.0
    should_retry_predicate: Optional[Callable[[Exception], bool]] = None


@dataclass
class RetryAttempt:
    """Information about a single retry attempt."""
    attempt_number: int
    operation_id: str
    operation_type: str
    delay_before_attempt: float
    start_time: datetime
    end_time: Optional[datetime] = None
    success: bool = False
    error: Optional[Exception] = None
    context: Dict[str, Any] = field(default_factory=dict)


@dataclass
class CircuitBreakerStats:
    """Statistics for circuit breaker monitoring."""
    service_name: str
    state: CircuitBreakerState
    failure_count: int = 0
    success_count: int = 0
    last_failure_time: Optional[datetime] = None
    last_success_time: Optional[datetime] = None
    state_changed_time: datetime = field(default_factory=lambda: datetime.now(timezone.utc))
    total_requests: int = 0
    blocked_requests: int = 0


class BackoffCalculator:
    """Calculates backoff delays using various strategies."""
    
    @staticmethod
    def calculate_delay(strategy: BackoffStrategy,
                       attempt: int,
                       base_delay: float,
                       max_delay: float,
                       multiplier: float = 2.0,
                       jitter_range: float = 0.1) -> float:
        """
        Calculate backoff delay based on strategy.
        
        Args:
            strategy: Backoff strategy to use
            attempt: Current attempt number (1-based)
            base_delay: Base delay in seconds
            max_delay: Maximum delay in seconds
            multiplier: Multiplier for exponential backoff
            jitter_range: Jitter range as fraction of delay
            
        Returns:
            float: Calculated delay in seconds
        """
        if strategy == BackoffStrategy.FIXED:
            delay = base_delay
        
        elif strategy == BackoffStrategy.LINEAR:
            delay = base_delay * attempt
        
        elif strategy == BackoffStrategy.EXPONENTIAL:
            delay = base_delay * (multiplier ** (attempt - 1))
        
        elif strategy == BackoffStrategy.FIBONACCI:
            delay = base_delay * BackoffCalculator._fibonacci(attempt)
        
        elif strategy == BackoffStrategy.RANDOM_JITTER:
            jitter = random.uniform(-jitter_range, jitter_range)
            delay = base_delay * (1.0 + jitter)
        
        elif strategy == BackoffStrategy.EXPONENTIAL_JITTER:
            exponential_delay = base_delay * (multiplier ** (attempt - 1))
            jitter = random.uniform(-jitter_range, jitter_range)
            delay = exponential_delay * (1.0 + jitter)
        
        else:
            delay = base_delay
        
        return min(delay, max_delay)
    
    @staticmethod
    def _fibonacci(n: int) -> int:
        """Calculate nth Fibonacci number."""
        if n <= 2:
            return 1
        a, b = 1, 1
        for _ in range(2, n):
            a, b = b, a + b
        return b


class CircuitBreaker:
    """
    Circuit breaker implementation for service failure protection.
    """
    
    def __init__(self,
                 service_name: str,
                 failure_threshold: int = 5,
                 timeout: float = 60.0,
                 success_threshold: int = 2):
        """
        Initialize circuit breaker.
        
        Args:
            service_name: Name of the service
            failure_threshold: Number of failures before opening circuit
            timeout: Timeout before trying half-open state
            success_threshold: Successes needed to close circuit from half-open
        """
        self.service_name = service_name
        self.failure_threshold = failure_threshold
        self.timeout = timeout
        self.success_threshold = success_threshold
        
        self.stats = CircuitBreakerStats(service_name=service_name, state=CircuitBreakerState.CLOSED)
        self.lock = threading.RLock()
        
        # Half-open state tracking
        self.half_open_successes = 0
        
        self.logger = logging.getLogger(f"circuit_breaker.{service_name}")
    
    def can_proceed(self) -> bool:
        """Check if operation can proceed based on circuit breaker state."""
        with self.lock:
            self.stats.total_requests += 1
            
            if self.stats.state == CircuitBreakerState.CLOSED:
                return True
            
            elif self.stats.state == CircuitBreakerState.OPEN:
                # Check if timeout has passed
                if (self.stats.last_failure_time and 
                    datetime.now(timezone.utc) - self.stats.last_failure_time >= timedelta(seconds=self.timeout)):
                    # Transition to half-open
                    self._transition_to_half_open()
                    return True
                else:
                    self.stats.blocked_requests += 1
                    return False
            
            elif self.stats.state == CircuitBreakerState.HALF_OPEN:
                # Allow limited requests to test service
                return True
            
            return False
    
    def record_success(self) -> None:
        """Record successful operation."""
        with self.lock:
            self.stats.success_count += 1
            self.stats.last_success_time = datetime.now(timezone.utc)
            
            if self.stats.state == CircuitBreakerState.HALF_OPEN:
                self.half_open_successes += 1
                if self.half_open_successes >= self.success_threshold:
                    self._transition_to_closed()
            
            elif self.stats.state == CircuitBreakerState.OPEN:
                # Shouldn't happen, but reset if it does
                self._transition_to_closed()
    
    def record_failure(self) -> None:
        """Record failed operation."""
        with self.lock:
            self.stats.failure_count += 1
            self.stats.last_failure_time = datetime.now(timezone.utc)
            
            if self.stats.state == CircuitBreakerState.CLOSED:
                if self.stats.failure_count >= self.failure_threshold:
                    self._transition_to_open()
            
            elif self.stats.state == CircuitBreakerState.HALF_OPEN:
                # Failure in half-open means service still failing
                self._transition_to_open()
    
    def _transition_to_open(self) -> None:
        """Transition to open state."""
        self.stats.state = CircuitBreakerState.OPEN
        self.stats.state_changed_time = datetime.now(timezone.utc)
        self.half_open_successes = 0
        self.logger.warning(f"Circuit breaker OPENED for {self.service_name}")
    
    def _transition_to_half_open(self) -> None:
        """Transition to half-open state."""
        self.stats.state = CircuitBreakerState.HALF_OPEN
        self.stats.state_changed_time = datetime.now(timezone.utc)
        self.half_open_successes = 0
        self.logger.info(f"Circuit breaker HALF-OPEN for {self.service_name}")
    
    def _transition_to_closed(self) -> None:
        """Transition to closed state."""
        self.stats.state = CircuitBreakerState.CLOSED
        self.stats.state_changed_time = datetime.now(timezone.utc)
        self.stats.failure_count = 0  # Reset failure count
        self.half_open_successes = 0
        self.logger.info(f"Circuit breaker CLOSED for {self.service_name}")
    
    def get_stats(self) -> CircuitBreakerStats:
        """Get current circuit breaker statistics."""
        with self.lock:
            return CircuitBreakerStats(
                service_name=self.stats.service_name,
                state=self.stats.state,
                failure_count=self.stats.failure_count,
                success_count=self.stats.success_count,
                last_failure_time=self.stats.last_failure_time,
                last_success_time=self.stats.last_success_time,
                state_changed_time=self.stats.state_changed_time,
                total_requests=self.stats.total_requests,
                blocked_requests=self.stats.blocked_requests
            )
    
    def force_open(self) -> None:
        """Force circuit breaker to open state (for testing/maintenance)."""
        with self.lock:
            self._transition_to_open()
    
    def force_closed(self) -> None:
        """Force circuit breaker to closed state (for recovery)."""
        with self.lock:
            self._transition_to_closed()


class RetryPolicyManager:
    """
    Manages retry policies for different operation types.
    """
    
    def __init__(self):
        """Initialize retry policy manager."""
        self.policies: Dict[str, RetryPolicy] = {}
        self.default_policy = RetryPolicy(
            operation_type="default",
            max_attempts=3,
            base_delay=1.0,
            backoff_strategy=BackoffStrategy.EXPONENTIAL_JITTER
        )
        
        # Setup default policies for common operation types
        self._setup_default_policies()
        
        self.lock = threading.RLock()
    
    def _setup_default_policies(self) -> None:
        """Setup default retry policies for common operation types."""
        # Network/API operations
        self.policies["network"] = RetryPolicy(
            operation_type="network",
            max_attempts=5,
            base_delay=2.0,
            max_delay=30.0,
            backoff_strategy=BackoffStrategy.EXPONENTIAL_JITTER,
            circuit_breaker_enabled=True,
            circuit_breaker_threshold=3
        )
        
        # Database operations
        self.policies["database"] = RetryPolicy(
            operation_type="database",
            max_attempts=3,
            base_delay=0.5,
            max_delay=10.0,
            backoff_strategy=BackoffStrategy.EXPONENTIAL,
            circuit_breaker_enabled=True
        )
        
        # File I/O operations
        self.policies["file_io"] = RetryPolicy(
            operation_type="file_io",
            max_attempts=3,
            base_delay=0.1,
            max_delay=5.0,
            backoff_strategy=BackoffStrategy.LINEAR
        )
        
        # Agent coordination
        self.policies["agent_coordination"] = RetryPolicy(
            operation_type="agent_coordination",
            max_attempts=2,
            base_delay=1.0,
            max_delay=10.0,
            backoff_strategy=BackoffStrategy.FIXED,
            circuit_breaker_enabled=False  # Don't break agent coordination
        )
        
        # UNO service operations
        self.policies["uno_service"] = RetryPolicy(
            operation_type="uno_service",
            max_attempts=2,
            base_delay=0.5,
            max_delay=5.0,
            backoff_strategy=BackoffStrategy.EXPONENTIAL,
            circuit_breaker_enabled=False  # LibreOffice should handle this
        )
        
        # Financial API operations
        self.policies["financial_api"] = RetryPolicy(
            operation_type="financial_api",
            max_attempts=4,
            base_delay=3.0,
            max_delay=60.0,
            backoff_strategy=BackoffStrategy.EXPONENTIAL_JITTER,
            circuit_breaker_enabled=True,
            circuit_breaker_threshold=2,
            timeout_per_attempt=30.0
        )
    
    def get_policy(self, operation_type: str) -> RetryPolicy:
        """Get retry policy for operation type."""
        with self.lock:
            return self.policies.get(operation_type, self.default_policy)
    
    def set_policy(self, policy: RetryPolicy) -> None:
        """Set retry policy for operation type."""
        with self.lock:
            self.policies[policy.operation_type] = policy
    
    def update_policy(self, operation_type: str, **updates) -> bool:
        """Update specific policy parameters."""
        with self.lock:
            if operation_type in self.policies:
                policy = self.policies[operation_type]
                for key, value in updates.items():
                    if hasattr(policy, key):
                        setattr(policy, key, value)
                return True
            return False
    
    def get_all_policies(self) -> Dict[str, RetryPolicy]:
        """Get all configured policies."""
        with self.lock:
            return self.policies.copy()


class ConcurrentRetryCoordinator:
    """
    Coordinates retry operations across multiple concurrent operations.
    """
    
    def __init__(self, max_concurrent_retries: int = 10):
        """
        Initialize concurrent retry coordinator.
        
        Args:
            max_concurrent_retries: Maximum concurrent retry operations
        """
        self.max_concurrent_retries = max_concurrent_retries
        self.active_retries: Dict[str, RetryAttempt] = {}
        self.retry_semaphore = asyncio.Semaphore(max_concurrent_retries)
        self.lock = threading.RLock()
        
        # Statistics
        self.total_operations = 0
        self.successful_operations = 0
        self.failed_operations = 0
        self.total_retry_attempts = 0
        
        self.logger = logging.getLogger("concurrent_retry_coordinator")
    
    async def coordinate_retry(self,
                             operation_id: str,
                             operation_type: str,
                             operation_func: Callable,
                             *args,
                             **kwargs) -> Any:
        """
        Coordinate retry for a single operation.
        
        Args:
            operation_id: Unique operation identifier
            operation_type: Type of operation for policy selection
            operation_func: Function to retry
            *args: Function arguments
            **kwargs: Function keyword arguments
            
        Returns:
            Any: Operation result
        """
        async with self.retry_semaphore:
            self.total_operations += 1
            
            try:
                result = await operation_func(*args, **kwargs)
                self.successful_operations += 1
                return result
                
            except Exception as e:
                self.failed_operations += 1
                self.logger.error(f"Operation {operation_id} failed: {e}")
                raise
            
            finally:
                # Clean up tracking
                with self.lock:
                    self.active_retries.pop(operation_id, None)
    
    def get_statistics(self) -> Dict[str, Any]:
        """Get retry coordination statistics."""
        with self.lock:
            active_count = len(self.active_retries)
            success_rate = (self.successful_operations / max(self.total_operations, 1)) * 100
            
            return {
                "total_operations": self.total_operations,
                "successful_operations": self.successful_operations,
                "failed_operations": self.failed_operations,
                "success_rate_percent": success_rate,
                "active_retries": active_count,
                "max_concurrent": self.max_concurrent_retries,
                "total_retry_attempts": self.total_retry_attempts
            }


class EnhancedRetryManager:
    """
    Enhanced retry manager that extends existing ErrorRecoveryManager
    with advanced retry capabilities, circuit breakers, and concurrent support.
    """
    
    def __init__(self,
                 agent_id: str,
                 existing_recovery_manager: Optional[ErrorRecoveryManager] = None,
                 unified_coordinator: Optional[UnifiedErrorCoordinator] = None,
                 error_tracker: Optional[ErrorTracker] = None):
        """
        Initialize enhanced retry manager.
        
        Args:
            agent_id: Agent identifier
            existing_recovery_manager: Existing ErrorRecoveryManager to enhance
            unified_coordinator: Unified error coordinator for integration
            error_tracker: Error tracker for pattern analysis
        """
        self.agent_id = agent_id
        self.existing_recovery_manager = existing_recovery_manager
        self.unified_coordinator = unified_coordinator
        self.error_tracker = error_tracker
        
        # Core components
        self.policy_manager = RetryPolicyManager()
        self.circuit_breakers: Dict[str, CircuitBreaker] = {}
        self.concurrent_coordinator = ConcurrentRetryCoordinator()
        
        # Operation tracking
        self.active_operations: Dict[str, Dict[str, Any]] = {}
        self.retry_history: List[RetryAttempt] = []
        self.operation_stats: Dict[str, Dict[str, Any]] = {}
        
        # Thread safety
        self.lock = threading.RLock()
        
        # Setup logging
        self.logger = logging.getLogger(f"enhanced_retry_manager.{agent_id}")
        self.logger.setLevel(logging.INFO)
    
    def get_circuit_breaker(self, service_name: str) -> CircuitBreaker:
        """Get or create circuit breaker for service."""
        with self.lock:
            if service_name not in self.circuit_breakers:
                policy = self.policy_manager.get_policy(service_name)
                self.circuit_breakers[service_name] = CircuitBreaker(
                    service_name=service_name,
                    failure_threshold=policy.circuit_breaker_threshold,
                    timeout=policy.circuit_breaker_timeout
                )
            return self.circuit_breakers[service_name]
    
    @asynccontextmanager
    async def retry_context(self,
                           operation_type: str,
                           operation_id: Optional[str] = None,
                           service_name: Optional[str] = None):
        """
        Async context manager for retry operations.
        
        Args:
            operation_type: Type of operation
            operation_id: Optional operation identifier
            service_name: Optional service name for circuit breaker
        """
        operation_id = operation_id or str(uuid.uuid4())
        service_name = service_name or operation_type
        
        policy = self.policy_manager.get_policy(operation_type)
        circuit_breaker = self.get_circuit_breaker(service_name) if policy.circuit_breaker_enabled else None
        
        # Check circuit breaker
        if circuit_breaker and not circuit_breaker.can_proceed():
            raise Exception(f"Circuit breaker open for service: {service_name}")
        
        attempt = 1
        last_exception = None
        
        while attempt <= policy.max_attempts:
            try:
                # Track attempt
                retry_attempt = RetryAttempt(
                    attempt_number=attempt,
                    operation_id=operation_id,
                    operation_type=operation_type,
                    delay_before_attempt=0.0 if attempt == 1 else BackoffCalculator.calculate_delay(
                        policy.backoff_strategy,
                        attempt - 1,
                        policy.base_delay,
                        policy.max_delay,
                        policy.backoff_multiplier,
                        policy.jitter_range
                    ),
                    start_time=datetime.now(timezone.utc)
                )
                
                # Apply delay for retries
                if attempt > 1:
                    await asyncio.sleep(retry_attempt.delay_before_attempt)
                
                # Yield control to operation
                yield
                
                # Success - record and break
                retry_attempt.end_time = datetime.now(timezone.utc)
                retry_attempt.success = True
                
                if circuit_breaker:
                    circuit_breaker.record_success()
                
                self._record_attempt(retry_attempt)
                
                if attempt > 1:
                    self.logger.info(f"Operation {operation_id} succeeded on attempt {attempt}")
                
                return
                
            except Exception as e:
                last_exception = e
                retry_attempt.end_time = datetime.now(timezone.utc)
                retry_attempt.error = e
                
                # Check if we should retry this exception
                if policy.should_retry_predicate and not policy.should_retry_predicate(e):
                    self.logger.info(f"Exception {type(e).__name__} not retryable for operation {operation_id}")
                    break
                
                # Record failure
                if circuit_breaker:
                    circuit_breaker.record_failure()
                
                self._record_attempt(retry_attempt)
                
                # Check if we have more attempts
                if attempt < policy.max_attempts:
                    self.logger.warning(f"Operation {operation_id} failed on attempt {attempt}, retrying: {e}")
                    attempt += 1
                else:
                    self.logger.error(f"Operation {operation_id} failed after {attempt} attempts: {e}")
                    break
        
        # All attempts failed
        if last_exception:
            # Integrate with existing error recovery if available
            if self.existing_recovery_manager:
                self._integrate_with_existing_recovery(operation_id, operation_type, last_exception)
            
            raise last_exception
    
    async def with_retry(self,
                        operation_type: str,
                        operation_func: Callable,
                        *args,
                        operation_id: Optional[str] = None,
                        service_name: Optional[str] = None,
                        **kwargs) -> Any:
        """
        Execute function with retry logic.
        
        Args:
            operation_type: Type of operation
            operation_func: Function to execute
            *args: Function arguments
            operation_id: Optional operation identifier
            service_name: Optional service name
            **kwargs: Function keyword arguments
            
        Returns:
            Any: Function result
        """
        async with self.retry_context(operation_type, operation_id, service_name):
            if asyncio.iscoroutinefunction(operation_func):
                return await operation_func(*args, **kwargs)
            else:
                # Execute sync function in thread pool
                loop = asyncio.get_event_loop()
                return await loop.run_in_executor(None, operation_func, *args, **kwargs)
    
    def _record_attempt(self, attempt: RetryAttempt) -> None:
        """Record retry attempt for statistics."""
        with self.lock:
            self.retry_history.append(attempt)
            
            # Keep history bounded
            if len(self.retry_history) > 1000:
                self.retry_history = self.retry_history[-500:]
            
            # Update statistics
            op_type = attempt.operation_type
            if op_type not in self.operation_stats:
                self.operation_stats[op_type] = {
                    "total_attempts": 0,
                    "successful_attempts": 0,
                    "failed_attempts": 0,
                    "total_operations": 0,
                    "successful_operations": 0
                }
            
            stats = self.operation_stats[op_type]
            stats["total_attempts"] += 1
            
            if attempt.success:
                stats["successful_attempts"] += 1
                if attempt.attempt_number == 1:
                    stats["total_operations"] += 1
                    stats["successful_operations"] += 1
            else:
                stats["failed_attempts"] += 1
                if attempt.attempt_number == 1:
                    stats["total_operations"] += 1
    
    def _integrate_with_existing_recovery(self,
                                        operation_id: str,
                                        operation_type: str,
                                        exception: Exception) -> None:
        """Integrate with existing ErrorRecoveryManager."""
        if self.existing_recovery_manager:
            try:
                context = {
                    "operation_name": operation_id,
                    "operation_type": operation_type,
                    "agent_id": self.agent_id,
                    "enhanced_retry_manager": True
                }
                self.existing_recovery_manager._handle_error(exception, context)
            except Exception as e:
                self.logger.warning(f"Failed to integrate with existing recovery manager: {e}")
    
    def get_statistics(self) -> Dict[str, Any]:
        """Get comprehensive retry statistics."""
        with self.lock:
            circuit_breaker_stats = {
                name: breaker.get_stats() 
                for name, breaker in self.circuit_breakers.items()
            }
            
            return {
                "agent_id": self.agent_id,
                "operation_statistics": self.operation_stats.copy(),
                "circuit_breaker_statistics": circuit_breaker_stats,
                "concurrent_coordinator_stats": self.concurrent_coordinator.get_statistics(),
                "total_retry_attempts": len(self.retry_history),
                "active_operations": len(self.active_operations),
                "configured_policies": list(self.policy_manager.get_all_policies().keys())
            }
    
    def get_recent_attempts(self, limit: int = 50) -> List[RetryAttempt]:
        """Get recent retry attempts."""
        with self.lock:
            return self.retry_history[-limit:] if self.retry_history else []
    
    def update_policy(self, operation_type: str, **updates) -> bool:
        """Update retry policy for operation type."""
        return self.policy_manager.update_policy(operation_type, **updates)
    
    def force_circuit_breaker_state(self, service_name: str, state: str) -> bool:
        """Force circuit breaker state (for testing/maintenance)."""
        with self.lock:
            if service_name in self.circuit_breakers:
                breaker = self.circuit_breakers[service_name]
                if state.lower() == "open":
                    breaker.force_open()
                elif state.lower() == "closed":
                    breaker.force_closed()
                else:
                    return False
                return True
            return False


# Factory functions for easy integration
def create_enhanced_retry_manager(agent_id: str,
                                 existing_recovery_manager: Optional[ErrorRecoveryManager] = None,
                                 config: Optional[Dict[str, Any]] = None) -> EnhancedRetryManager:
    """
    Factory function to create enhanced retry manager.
    
    Args:
        agent_id: Agent identifier
        existing_recovery_manager: Existing ErrorRecoveryManager to enhance
        config: Configuration options
        
    Returns:
        EnhancedRetryManager: Configured retry manager
    """
    return EnhancedRetryManager(
        agent_id=agent_id,
        existing_recovery_manager=existing_recovery_manager
    )


def create_circuit_breaker(service_name: str,
                          failure_threshold: int = 5,
                          timeout: float = 60.0) -> CircuitBreaker:
    """Factory function to create circuit breaker."""
    return CircuitBreaker(
        service_name=service_name,
        failure_threshold=failure_threshold,
        timeout=timeout
    )