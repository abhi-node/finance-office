"""
Retry Integration Layer for Existing Components

This module provides integration wrappers that add retry capabilities to existing
components like Bridge communication, DataIntegrationAgent, and other system
components without requiring major refactoring.

Key Components:
- RetryAwareBridge: Wrapper for bridge.py with retry logic
- RetryAwareDataIntegration: Enhanced DataIntegrationAgent with circuit breakers
- RetryAwareUnoService: UNO service wrapper with retry logic
- NetworkRetryWrapper: Generic network operation wrapper
"""

import asyncio
import functools
import time
from contextlib import asynccontextmanager
from typing import Any, Callable, Dict, List, Optional, Union
import logging

# Import retry infrastructure
from .retry_manager import (
    EnhancedRetryManager, RetryPolicy, BackoffStrategy,
    CircuitBreaker, create_enhanced_retry_manager, create_circuit_breaker
)

# Import existing components to enhance
try:
    from bridge import LangGraphBridge
    from .data_integration import DataIntegrationAgent
    from .financial_apis import BaseFinancialAPIClient
    from state.document_state import DocumentState
except ImportError:
    # Fallback for testing
    LangGraphBridge = Any
    DataIntegrationAgent = Any
    BaseFinancialAPIClient = Any
    DocumentState = Dict[str, Any]


class RetryAwareBridge:
    """
    Wrapper for LangGraphBridge that adds retry logic to bridge communications.
    """
    
    def __init__(self, 
                 bridge: LangGraphBridge,
                 enable_retry: bool = True,
                 max_retries: int = 3):
        """
        Initialize retry-aware bridge wrapper.
        
        Args:
            bridge: Existing LangGraphBridge instance
            enable_retry: Enable retry logic
            max_retries: Maximum retry attempts
        """
        self.bridge = bridge
        self.enable_retry = enable_retry
        
        # Initialize retry manager
        if enable_retry:
            self.retry_manager = create_enhanced_retry_manager("bridge_wrapper")
            
            # Configure bridge-specific retry policies
            self._configure_bridge_retry_policies(max_retries)
        else:
            self.retry_manager = None
        
        # Statistics
        self.bridge_stats = {
            "total_requests": 0,
            "successful_requests": 0,
            "failed_requests": 0,
            "retried_requests": 0,
            "circuit_breaker_triggers": 0
        }
        
        self.logger = logging.getLogger("retry_aware_bridge")
    
    def _configure_bridge_retry_policies(self, max_retries: int) -> None:
        """Configure retry policies for bridge operations."""
        # C++ communication policy
        cpp_policy = RetryPolicy(
            operation_type="cpp_communication",
            max_attempts=max_retries,
            base_delay=0.5,
            max_delay=5.0,
            backoff_strategy=BackoffStrategy.EXPONENTIAL,
            circuit_breaker_enabled=True,
            circuit_breaker_threshold=5
        )
        self.retry_manager.policy_manager.set_policy(cpp_policy)
        
        # Agent processing policy
        agent_policy = RetryPolicy(
            operation_type="agent_processing",
            max_attempts=2,  # Fewer retries for agent processing
            base_delay=1.0,
            max_delay=10.0,
            backoff_strategy=BackoffStrategy.EXPONENTIAL_JITTER,
            circuit_breaker_enabled=False  # Don't break agent processing
        )
        self.retry_manager.policy_manager.set_policy(agent_policy)
        
        # Progress update policy
        progress_policy = RetryPolicy(
            operation_type="progress_update",
            max_attempts=2,
            base_delay=0.1,
            max_delay=1.0,
            backoff_strategy=BackoffStrategy.LINEAR,
            circuit_breaker_enabled=False
        )
        self.retry_manager.policy_manager.set_policy(progress_policy)
    
    async def process_cpp_request(self,
                                 request_id: str,
                                 user_message: str,
                                 document_context: Any) -> str:
        """Process C++ request with retry logic."""
        self.bridge_stats["total_requests"] += 1
        
        if self.retry_manager and self.enable_retry:
            try:
                async with self.retry_manager.retry_context(
                    operation_type="cpp_communication",
                    operation_id=request_id,
                    service_name="cpp_bridge"
                ):
                    result = await self.bridge.process_cpp_request(request_id, user_message, document_context)
                    self.bridge_stats["successful_requests"] += 1
                    return result
            
            except Exception as e:
                self.bridge_stats["failed_requests"] += 1
                self.logger.error(f"Bridge request {request_id} failed after retries: {e}")
                raise
        
        else:
            # Fallback to direct call
            try:
                result = await self.bridge.process_cpp_request(request_id, user_message, document_context)
                self.bridge_stats["successful_requests"] += 1
                return result
            except Exception as e:
                self.bridge_stats["failed_requests"] += 1
                raise
    
    async def update_progress(self,
                             request_id: str,
                             progress_data: Dict[str, Any]) -> bool:
        """Update progress with retry logic."""
        if hasattr(self.bridge, 'update_progress'):
            if self.retry_manager and self.enable_retry:
                try:
                    async with self.retry_manager.retry_context(
                        operation_type="progress_update",
                        operation_id=f"{request_id}_progress",
                        service_name="progress_service"
                    ):
                        return await self.bridge.update_progress(request_id, progress_data)
                
                except Exception as e:
                    self.logger.warning(f"Progress update failed for {request_id}: {e}")
                    return False
            else:
                try:
                    return await self.bridge.update_progress(request_id, progress_data)
                except Exception:
                    return False
        return True
    
    async def cancel_operation(self, request_id: str) -> bool:
        """Cancel operation (no retry needed for cancellation)."""
        if hasattr(self.bridge, 'cancel_operation'):
            try:
                return await self.bridge.cancel_operation(request_id)
            except Exception as e:
                self.logger.error(f"Failed to cancel operation {request_id}: {e}")
                return False
        return True
    
    def get_status(self) -> Dict[str, Any]:
        """Get bridge status with retry statistics."""
        base_status = self.bridge.get_status() if hasattr(self.bridge, 'get_status') else {}
        
        retry_status = {
            "retry_enabled": self.enable_retry,
            "bridge_statistics": self.bridge_stats.copy(),
        }
        
        if self.retry_manager:
            retry_status["retry_manager_stats"] = self.retry_manager.get_statistics()
        
        return {**base_status, **retry_status}


class RetryAwareDataIntegration:
    """
    Enhanced DataIntegrationAgent with retry logic and circuit breaker protection.
    """
    
    def __init__(self,
                 data_agent: DataIntegrationAgent,
                 enable_retry: bool = True,
                 enable_circuit_breakers: bool = True):
        """
        Initialize retry-aware data integration wrapper.
        
        Args:
            data_agent: Existing DataIntegrationAgent
            enable_retry: Enable retry logic
            enable_circuit_breakers: Enable circuit breaker protection
        """
        self.data_agent = data_agent
        self.enable_retry = enable_retry
        self.enable_circuit_breakers = enable_circuit_breakers
        
        # Initialize retry manager
        if enable_retry:
            self.retry_manager = create_enhanced_retry_manager("data_integration_wrapper")
            self._configure_data_retry_policies()
        else:
            self.retry_manager = None
        
        # API-specific circuit breakers
        self.api_circuit_breakers = {}
        if enable_circuit_breakers:
            self._setup_api_circuit_breakers()
        
        # Statistics
        self.data_stats = {
            "total_api_calls": 0,
            "successful_api_calls": 0,
            "failed_api_calls": 0,
            "circuit_breaker_blocks": 0,
            "cache_hits": 0,
            "fallback_usage": 0
        }
        
        self.logger = logging.getLogger("retry_aware_data_integration")
    
    def _configure_data_retry_policies(self) -> None:
        """Configure retry policies for data integration operations."""
        # Financial API policy
        financial_policy = RetryPolicy(
            operation_type="financial_api",
            max_attempts=4,
            base_delay=2.0,
            max_delay=30.0,
            backoff_strategy=BackoffStrategy.EXPONENTIAL_JITTER,
            circuit_breaker_enabled=True,
            circuit_breaker_threshold=3,
            timeout_per_attempt=30.0
        )
        self.retry_manager.policy_manager.set_policy(financial_policy)
        
        # Data validation policy
        validation_policy = RetryPolicy(
            operation_type="data_validation",
            max_attempts=2,
            base_delay=0.1,
            max_delay=1.0,
            backoff_strategy=BackoffStrategy.FIXED,
            circuit_breaker_enabled=False
        )
        self.retry_manager.policy_manager.set_policy(validation_policy)
        
        # Cache operations policy
        cache_policy = RetryPolicy(
            operation_type="cache_operation",
            max_attempts=2,
            base_delay=0.05,
            max_delay=0.5,
            backoff_strategy=BackoffStrategy.LINEAR,
            circuit_breaker_enabled=False
        )
        self.retry_manager.policy_manager.set_policy(cache_policy)
    
    def _setup_api_circuit_breakers(self) -> None:
        """Setup circuit breakers for different APIs."""
        api_names = ["yahoo_finance", "alpha_vantage", "iex_cloud", "bloomberg"]
        
        for api_name in api_names:
            self.api_circuit_breakers[api_name] = create_circuit_breaker(
                service_name=api_name,
                failure_threshold=3,
                timeout=60.0
            )
    
    async def fetch_financial_data(self,
                                  symbols: List[str],
                                  data_types: List[str],
                                  api_preference: Optional[str] = None) -> Dict[str, Any]:
        """Fetch financial data with retry logic and circuit breaker protection."""
        self.data_stats["total_api_calls"] += 1
        
        if self.retry_manager and self.enable_retry:
            try:
                async with self.retry_manager.retry_context(
                    operation_type="financial_api",
                    operation_id=f"fetch_{'-'.join(symbols)}",
                    service_name=api_preference or "financial_api"
                ):
                    return await self._fetch_with_circuit_breaker_protection(
                        symbols, data_types, api_preference
                    )
            
            except Exception as e:
                self.data_stats["failed_api_calls"] += 1
                # Try fallback strategies
                return await self._handle_fetch_failure(symbols, data_types, e)
        
        else:
            # Direct call without retry
            try:
                result = await self.data_agent.fetch_financial_data(symbols, data_types, api_preference)
                self.data_stats["successful_api_calls"] += 1
                return result
            except Exception as e:
                self.data_stats["failed_api_calls"] += 1
                return await self._handle_fetch_failure(symbols, data_types, e)
    
    async def _fetch_with_circuit_breaker_protection(self,
                                                   symbols: List[str],
                                                   data_types: List[str],
                                                   api_preference: Optional[str]) -> Dict[str, Any]:
        """Fetch data with circuit breaker protection."""
        if api_preference and self.enable_circuit_breakers:
            circuit_breaker = self.api_circuit_breakers.get(api_preference)
            if circuit_breaker and not circuit_breaker.can_proceed():
                self.data_stats["circuit_breaker_blocks"] += 1
                raise Exception(f"Circuit breaker open for API: {api_preference}")
        
        try:
            # Call original data agent method
            result = await self.data_agent.fetch_financial_data(symbols, data_types, api_preference)
            
            # Record success in circuit breaker
            if api_preference and self.enable_circuit_breakers:
                circuit_breaker = self.api_circuit_breakers.get(api_preference)
                if circuit_breaker:
                    circuit_breaker.record_success()
            
            self.data_stats["successful_api_calls"] += 1
            return result
        
        except Exception as e:
            # Record failure in circuit breaker
            if api_preference and self.enable_circuit_breakers:
                circuit_breaker = self.api_circuit_breakers.get(api_preference)
                if circuit_breaker:
                    circuit_breaker.record_failure()
            
            raise
    
    async def _handle_fetch_failure(self,
                                   symbols: List[str],
                                   data_types: List[str],
                                   error: Exception) -> Dict[str, Any]:
        """Handle fetch failure with fallback strategies."""
        self.logger.warning(f"Financial data fetch failed: {error}")
        
        # Try cached data
        if hasattr(self.data_agent, 'get_cached_data'):
            try:
                cached_data = await self.data_agent.get_cached_data(symbols, data_types)
                if cached_data:
                    self.data_stats["cache_hits"] += 1
                    self.data_stats["fallback_usage"] += 1
                    return cached_data
            except Exception as cache_error:
                self.logger.warning(f"Cache fallback failed: {cache_error}")
        
        # Return minimal error response
        self.data_stats["fallback_usage"] += 1
        return {
            "symbols": symbols,
            "data_types": data_types,
            "error": str(error),
            "fallback": True,
            "timestamp": time.time()
        }
    
    def get_statistics(self) -> Dict[str, Any]:
        """Get comprehensive data integration statistics."""
        base_stats = {
            "data_integration_stats": self.data_stats.copy(),
            "retry_enabled": self.enable_retry,
            "circuit_breakers_enabled": self.enable_circuit_breakers
        }
        
        if self.retry_manager:
            base_stats["retry_manager_stats"] = self.retry_manager.get_statistics()
        
        if self.enable_circuit_breakers:
            base_stats["circuit_breaker_stats"] = {
                name: breaker.get_stats()
                for name, breaker in self.api_circuit_breakers.items()
            }
        
        return base_stats


class NetworkRetryWrapper:
    """
    Generic wrapper for adding retry logic to network operations.
    """
    
    def __init__(self,
                 service_name: str,
                 retry_manager: Optional[EnhancedRetryManager] = None):
        """
        Initialize network retry wrapper.
        
        Args:
            service_name: Name of the service for circuit breaker
            retry_manager: Optional retry manager (creates one if not provided)
        """
        self.service_name = service_name
        self.retry_manager = retry_manager or create_enhanced_retry_manager(f"network_{service_name}")
        
        # Configure network retry policy
        network_policy = RetryPolicy(
            operation_type="network_operation",
            max_attempts=4,
            base_delay=1.0,
            max_delay=30.0,
            backoff_strategy=BackoffStrategy.EXPONENTIAL_JITTER,
            circuit_breaker_enabled=True,
            circuit_breaker_threshold=5,
            timeout_per_attempt=30.0
        )
        self.retry_manager.policy_manager.set_policy(network_policy)
        
        self.logger = logging.getLogger(f"network_retry_wrapper.{service_name}")
    
    def with_retry(self, operation_name: str = "network_call"):
        """Decorator for adding retry logic to network functions."""
        def decorator(func: Callable) -> Callable:
            @functools.wraps(func)
            async def wrapper(*args, **kwargs) -> Any:
                operation_id = f"{self.service_name}_{operation_name}_{int(time.time())}"
                
                try:
                    async with self.retry_manager.retry_context(
                        operation_type="network_operation",
                        operation_id=operation_id,
                        service_name=self.service_name
                    ):
                        if asyncio.iscoroutinefunction(func):
                            return await func(*args, **kwargs)
                        else:
                            return func(*args, **kwargs)
                
                except Exception as e:
                    self.logger.error(f"Network operation {operation_name} failed after retries: {e}")
                    raise
            
            return wrapper
        return decorator
    
    @asynccontextmanager
    async def circuit_breaker_context(self):
        """Context manager for circuit breaker protection."""
        circuit_breaker = self.retry_manager.get_circuit_breaker(self.service_name)
        
        if not circuit_breaker.can_proceed():
            raise Exception(f"Circuit breaker open for service: {self.service_name}")
        
        try:
            yield
            circuit_breaker.record_success()
        except Exception as e:
            circuit_breaker.record_failure()
            raise


# Integration factory functions
def create_retry_aware_bridge(bridge: LangGraphBridge,
                             enable_retry: bool = True,
                             max_retries: int = 3) -> RetryAwareBridge:
    """Factory function to create retry-aware bridge wrapper."""
    return RetryAwareBridge(
        bridge=bridge,
        enable_retry=enable_retry,
        max_retries=max_retries
    )


def create_retry_aware_data_integration(data_agent: DataIntegrationAgent,
                                      enable_retry: bool = True,
                                      enable_circuit_breakers: bool = True) -> RetryAwareDataIntegration:
    """Factory function to create retry-aware data integration wrapper."""
    return RetryAwareDataIntegration(
        data_agent=data_agent,
        enable_retry=enable_retry,
        enable_circuit_breakers=enable_circuit_breakers
    )


def create_network_retry_wrapper(service_name: str,
                                retry_manager: Optional[EnhancedRetryManager] = None) -> NetworkRetryWrapper:
    """Factory function to create network retry wrapper."""
    return NetworkRetryWrapper(
        service_name=service_name,
        retry_manager=retry_manager
    )


# Convenience decorator for adding retry to existing functions
def with_retry(operation_type: str = "default",
              service_name: Optional[str] = None,
              max_attempts: int = 3):
    """
    Convenience decorator for adding retry logic to any function.
    
    Args:
        operation_type: Type of operation for retry policy
        service_name: Service name for circuit breaker
        max_attempts: Maximum retry attempts
    """
    def decorator(func: Callable) -> Callable:
        # Create a retry manager for this function
        retry_manager = create_enhanced_retry_manager(f"decorator_{func.__name__}")
        
        # Configure retry policy
        policy = RetryPolicy(
            operation_type=operation_type,
            max_attempts=max_attempts,
            base_delay=1.0,
            backoff_strategy=BackoffStrategy.EXPONENTIAL_JITTER
        )
        retry_manager.policy_manager.set_policy(policy)
        
        @functools.wraps(func)
        async def wrapper(*args, **kwargs) -> Any:
            return await retry_manager.with_retry(
                operation_type=operation_type,
                operation_func=func,
                *args,
                service_name=service_name,
                **kwargs
            )
        
        return wrapper
    return decorator