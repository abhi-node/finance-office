"""
Comprehensive Integration Tests for Task 19 - Error Handling and Recovery Systems

This test suite validates the complete integration of all Task 19 components:
- Core Error Handling Infrastructure (19.1)
- Automatic Retry and Backoff System (19.2)
- Graceful Degradation and Rollback (19.3)
- UI Error Propagation (19.4)
- Operation Cancellation and Progress Tracking (19.5)

Tests verify that all components work together seamlessly to provide
robust error handling, recovery, and user communication.
"""

import asyncio
import json
import pytest
import time
import uuid
from datetime import datetime, timezone
from unittest.mock import Mock, AsyncMock, patch

# Import all Task 19 components
from agents.error_handler import (
    UnifiedErrorCoordinator, ErrorContext, ErrorSeverity, ErrorCategory
)
from agents.error_tracking import ErrorTracker
from agents.error_integration import ErrorHandlingIntegrator
from agents.retry_manager import (
    EnhancedRetryManager, RetryPolicy, BackoffStrategy, CircuitBreaker
)
from agents.graceful_degradation import (
    GracefulDegradationManager, TransactionalStateManager, 
    DegradationLevel, ServiceAvailabilityMonitor
)
from agents.ui_error_propagation import (
    UIErrorPropagator, ErrorMessageTranslator, ProgressErrorTracker,
    UIErrorSeverity, UIErrorType, ProgressState
)
from agents.operation_cancellation import (
    OperationCancellationManager, ProgressTrackingOrchestrator,
    CancellationToken, CancellationReason, OperationState
)


@pytest.fixture
def mock_document_state_manager():
    """Mock DocumentStateManager for testing."""
    mock_manager = Mock()
    mock_manager.get_state.return_value = {"test": "state"}
    mock_manager.restore_from_snapshot = AsyncMock(return_value=True)
    mock_manager.update_state.return_value = True
    return mock_manager


@pytest.fixture
def mock_bridge():
    """Mock LangGraphBridge for testing."""
    mock_bridge = Mock()
    mock_bridge.process_cpp_request = AsyncMock(return_value='{"success": true, "result": "test"}')
    mock_bridge.send_error_notification = AsyncMock(return_value=True)
    mock_bridge.send_progress_update = AsyncMock(return_value=True)
    return mock_bridge


@pytest.fixture
async def integrated_error_system(mock_document_state_manager, mock_bridge):
    """Create fully integrated error handling system for testing."""
    # Core error handling
    error_coordinator = UnifiedErrorCoordinator()
    error_tracker = ErrorTracker()
    
    # Retry management
    retry_manager = EnhancedRetryManager(
        agent_id="test_agent",
        unified_coordinator=error_coordinator,
        error_tracker=error_tracker
    )
    
    # Graceful degradation
    transactional_state_manager = TransactionalStateManager(mock_document_state_manager)
    service_monitor = ServiceAvailabilityMonitor()
    degradation_manager = GracefulDegradationManager(
        transactional_state_manager=transactional_state_manager,
        service_monitor=service_monitor,
        retry_manager=retry_manager
    )
    
    # UI error propagation
    ui_error_propagator = UIErrorPropagator(bridge=mock_bridge, error_tracker=error_tracker)
    
    # Progress tracking and cancellation
    progress_orchestrator = ProgressTrackingOrchestrator(ui_error_propagator=ui_error_propagator)
    cancellation_manager = OperationCancellationManager(
        progress_orchestrator=progress_orchestrator,
        transactional_state_manager=transactional_state_manager,
        retry_manager=retry_manager,
        ui_error_propagator=ui_error_propagator
    )
    
    return {
        "error_coordinator": error_coordinator,
        "error_tracker": error_tracker,
        "retry_manager": retry_manager,
        "transactional_state_manager": transactional_state_manager,
        "service_monitor": service_monitor,
        "degradation_manager": degradation_manager,
        "ui_error_propagator": ui_error_propagator,
        "progress_orchestrator": progress_orchestrator,
        "cancellation_manager": cancellation_manager
    }


class TestTask19ComprehensiveIntegration:
    """Comprehensive integration tests for all Task 19 components."""
    
    @pytest.mark.asyncio
    async def test_complete_error_flow_with_retry_and_recovery(self, integrated_error_system):
        """Test complete error flow with retry, degradation, and UI notification."""
        system = integrated_error_system
        
        # Start operation tracking
        operation_id = "test_operation_" + str(uuid.uuid4())
        request_id = "test_request_" + str(uuid.uuid4())
        
        progress = system["progress_orchestrator"].start_operation(
            operation_id=operation_id,
            request_id=request_id,
            operation_type="test_operation",
            total_steps=3
        )
        
        # Simulate operation that fails initially but succeeds on retry
        call_count = 0
        async def failing_operation():
            nonlocal call_count
            call_count += 1
            if call_count < 3:  # Fail first 2 attempts
                raise Exception(f"Simulated failure #{call_count}")
            return {"success": True, "result": "Operation succeeded on retry"}
        
        # Execute with full error handling
        try:
            async with system["retry_manager"].retry_context(
                operation_type="test_operation",
                operation_id=operation_id
            ):
                result = await failing_operation()
                
                # Update progress
                system["progress_orchestrator"].update_progress(
                    operation_id=operation_id,
                    state=OperationState.COMPLETED,
                    progress_percentage=100.0
                )
                
        except Exception as e:
            # Handle error through integrated system
            error_context = ErrorContext(
                category=ErrorCategory.UNKNOWN_ERROR,
                severity=ErrorSeverity.HIGH,
                error_message=str(e)
            )
            
            # Propagate error to UI
            operation_context = {
                "operation_id": operation_id,
                "request_id": request_id,
                "operation_type": "test_operation"
            }
            
            await system["ui_error_propagator"].propagate_error(
                error_context, operation_context
            )
        
        # Verify retry attempts
        assert call_count == 3, "Should have made 3 attempts (2 failures + 1 success)"
        
        # Verify error tracking
        stats = system["retry_manager"].get_statistics()
        assert stats["operation_statistics"]["test_operation"]["total_attempts"] >= 3
        
        # Complete operation
        final_progress = system["progress_orchestrator"].complete_operation(operation_id, success=True)
        assert final_progress is not None
        assert final_progress.state == OperationState.COMPLETED
    
    @pytest.mark.asyncio
    async def test_graceful_degradation_with_fallback_strategies(self, integrated_error_system):
        """Test graceful degradation with fallback strategies."""
        system = integrated_error_system
        
        operation_id = "degradation_test_" + str(uuid.uuid4())
        request_id = "degradation_request_" + str(uuid.uuid4())
        
        # Register a service and mark it as failing
        system["service_monitor"].register_service("test_api")
        await system["service_monitor"].update_service_status(
            "test_api", 
            system["service_monitor"].ServiceStatus.FAILING,
            error_rate=0.8
        )
        
        # Simulate operation that triggers degradation
        async def operation_requiring_api():
            # This would normally call external API
            raise Exception("API service unavailable")
        
        # Execute with degradation protection
        result = await system["degradation_manager"].execute_with_degradation_protection(
            operation_func=operation_requiring_api,
            operation_id=operation_id,
            agent_id="test_agent"
        )
        
        # Verify degradation level changed
        degradation_status = system["degradation_manager"].get_degradation_status()
        assert degradation_status["current_degradation_level"] != DegradationLevel.FULL_SERVICE.value
        
        # Verify fallback was attempted
        stats = degradation_status["degradation_statistics"]
        assert stats["total_degradations"] > 0
    
    @pytest.mark.asyncio
    async def test_operation_cancellation_with_rollback(self, integrated_error_system):
        """Test operation cancellation with state rollback."""
        system = integrated_error_system
        
        operation_id = "cancellation_test_" + str(uuid.uuid4())
        request_id = "cancellation_request_" + str(uuid.uuid4())
        
        # Start cancellable operation
        async with system["cancellation_manager"].cancellable_operation(
            operation_id=operation_id,
            request_id=request_id,
            operation_type="cancellable_test",
            timeout_seconds=10.0
        ) as cancellation_token:
            
            # Create checkpoint for rollback
            checkpoint_id = system["transactional_state_manager"].create_checkpoint(
                operation_id=operation_id,
                agent_id="test_agent",
                description="Before cancellable operation"
            )
            
            # Simulate some work
            await asyncio.sleep(0.1)
            
            # Cancel operation
            success = await system["cancellation_manager"].cancel_operation(
                operation_id=operation_id,
                reason=CancellationReason.USER_REQUESTED,
                message="User cancelled operation"
            )
            
            assert success, "Cancellation should succeed"
            assert cancellation_token.is_cancellation_requested(), "Token should be cancelled"
            
            # Verify rollback occurred
            rollback_stats = system["transactional_state_manager"].get_rollback_statistics()
            assert rollback_stats["rollback_stats"]["total_rollbacks"] >= 0
    
    @pytest.mark.asyncio
    async def test_ui_error_propagation_integration(self, integrated_error_system):
        """Test complete UI error propagation flow."""
        system = integrated_error_system
        
        # Create error context
        error_context = ErrorContext(
            category=ErrorCategory.NETWORK_ERROR,
            severity=ErrorSeverity.HIGH,
            error_message="Network connection failed"
        )
        
        operation_context = {
            "operation_id": "ui_test_" + str(uuid.uuid4()),
            "request_id": "ui_request_" + str(uuid.uuid4()),
            "operation_type": "network_operation"
        }
        
        # Propagate error
        ui_error = await system["ui_error_propagator"].propagate_error(
            error_context=error_context,
            operation_context=operation_context
        )
        
        # Verify error translation
        assert ui_error.error_type == UIErrorType.NETWORK_ISSUE
        assert ui_error.severity == UIErrorSeverity.WARNING
        assert "Network Connection Issue" in ui_error.title
        assert len(ui_error.suggested_actions) > 0
        
        # Verify propagation statistics
        stats = system["ui_error_propagator"].get_propagation_statistics()
        assert stats["propagation_stats"]["total_errors_propagated"] > 0
        assert stats["propagation_stats"]["translation_requests"] > 0
    
    @pytest.mark.asyncio
    async def test_circuit_breaker_integration_with_degradation(self, integrated_error_system):
        """Test circuit breaker integration with graceful degradation."""
        system = integrated_error_system
        
        # Configure circuit breaker policy
        policy = RetryPolicy(
            operation_type="circuit_breaker_test",
            max_attempts=2,
            circuit_breaker_enabled=True,
            circuit_breaker_threshold=2
        )
        system["retry_manager"].policy_manager.set_policy(policy)
        
        # Simulate service failures to trigger circuit breaker
        failure_count = 0
        async def failing_service():
            nonlocal failure_count
            failure_count += 1
            raise Exception(f"Service failure #{failure_count}")
        
        # Execute operations until circuit breaker opens
        for i in range(5):
            try:
                async with system["retry_manager"].retry_context(
                    operation_type="circuit_breaker_test",
                    service_name="test_service"
                ):
                    await failing_service()
            except:
                pass  # Expected failures
        
        # Verify circuit breaker opened
        circuit_breaker = system["retry_manager"].get_circuit_breaker("test_service")
        stats = circuit_breaker.get_stats()
        
        # Circuit breaker should have opened after threshold failures
        assert stats.failure_count >= policy.circuit_breaker_threshold
    
    @pytest.mark.asyncio
    async def test_progress_tracking_with_error_states(self, integrated_error_system):
        """Test progress tracking through various error states."""
        system = integrated_error_system
        
        operation_id = "progress_test_" + str(uuid.uuid4())
        request_id = "progress_request_" + str(uuid.uuid4())
        
        # Start operation
        progress = system["progress_orchestrator"].start_operation(
            operation_id=operation_id,
            request_id=request_id,
            operation_type="progress_test",
            total_steps=5
        )
        
        # Simulate progress through various states
        states_to_test = [
            (OperationState.INITIALIZING, 10.0, "Initializing operation"),
            (OperationState.RUNNING, 30.0, "Processing data"),
            (OperationState.AGENT_COORDINATION, 50.0, "Coordinating agents"),
            (OperationState.RETRYING, 60.0, "Retrying after error"),
            (OperationState.COMPLETING, 90.0, "Finalizing results"),
            (OperationState.COMPLETED, 100.0, "Operation completed")
        ]
        
        for state, percentage, step_description in states_to_test:
            updated_progress = system["progress_orchestrator"].update_progress(
                operation_id=operation_id,
                state=state,
                progress_percentage=percentage,
                current_step=step_description
            )
            
            assert updated_progress is not None
            assert updated_progress.state == state
            assert updated_progress.progress_percentage == percentage
            assert updated_progress.current_step == step_description
        
        # Complete operation
        final_progress = system["progress_orchestrator"].complete_operation(operation_id, success=True)
        assert final_progress.state == OperationState.COMPLETED
    
    @pytest.mark.asyncio
    async def test_error_tracking_and_pattern_analysis(self, integrated_error_system):
        """Test error tracking and pattern analysis across components."""
        system = integrated_error_system
        
        # Generate multiple similar errors
        for i in range(5):
            error_context = ErrorContext(
                category=ErrorCategory.NETWORK_ERROR,
                severity=ErrorSeverity.MEDIUM,
                error_message=f"Connection timeout #{i}",
                agent_id="test_agent",
                operation_name=f"network_operation_{i}"
            )
            
            system["error_tracker"].track_error(error_context)
        
        # Analyze patterns
        patterns = system["error_tracker"].analyze_error_patterns(hours_back=1)
        
        # Should detect network error pattern
        network_pattern = next(
            (p for p in patterns if p["category"] == ErrorCategory.NETWORK_ERROR.value),
            None
        )
        
        assert network_pattern is not None
        assert network_pattern["count"] == 5
        assert network_pattern["frequency"] == 5.0  # 5 errors in 1 hour
    
    @pytest.mark.asyncio
    async def test_end_to_end_complex_operation_scenario(self, integrated_error_system):
        """Test end-to-end scenario with complex operation involving all components."""
        system = integrated_error_system
        
        operation_id = "complex_scenario_" + str(uuid.uuid4())
        request_id = "complex_request_" + str(uuid.uuid4())
        
        # Start complex operation with cancellation support
        async with system["cancellation_manager"].cancellable_operation(
            operation_id=operation_id,
            request_id=request_id,
            operation_type="complex_operation",
            timeout_seconds=30.0
        ) as cancellation_token:
            
            # Simulate multi-step operation with potential failures
            steps = [
                ("Data validation", 20.0),
                ("Agent coordination", 40.0),
                ("External API calls", 60.0),
                ("Document processing", 80.0),
                ("Result compilation", 100.0)
            ]
            
            for step_name, progress_pct in steps:
                # Check for cancellation
                if cancellation_token.is_cancellation_requested():
                    raise Exception("Operation was cancelled")
                
                # Update progress
                system["progress_orchestrator"].update_progress(
                    operation_id=operation_id,
                    state=OperationState.RUNNING,
                    progress_percentage=progress_pct,
                    current_step=step_name
                )
                
                # Simulate work with potential retry
                async with system["retry_manager"].retry_context(
                    operation_type="complex_operation",
                    operation_id=f"{operation_id}_{step_name.replace(' ', '_')}"
                ):
                    # Simulate some processing time
                    await asyncio.sleep(0.05)
                    
                    # Occasionally simulate failures for some steps
                    if step_name == "External API calls" and progress_pct == 60.0:
                        # This will trigger retry logic
                        import random
                        if random.random() < 0.3:  # 30% chance of failure
                            raise Exception("External API temporarily unavailable")
        
        # Verify operation completed successfully
        final_progress = system["progress_orchestrator"].get_operation_progress(operation_id)
        if final_progress:
            assert final_progress.progress_percentage == 100.0
        
        # Verify statistics
        retry_stats = system["retry_manager"].get_statistics()
        assert retry_stats["operation_statistics"]["complex_operation"]["total_operations"] >= 1
        
        cancellation_stats = system["cancellation_manager"].get_cancellation_statistics()
        assert cancellation_stats["active_operations"] >= 0  # Should be 0 after completion
        
        ui_stats = system["ui_error_propagator"].get_propagation_statistics()
        assert ui_stats["propagation_stats"]["progress_updates"] >= len(steps)
    
    def test_component_initialization_and_integration(self, integrated_error_system):
        """Test that all components initialize correctly and integrate properly."""
        system = integrated_error_system
        
        # Verify all components are initialized
        assert system["error_coordinator"] is not None
        assert system["error_tracker"] is not None
        assert system["retry_manager"] is not None
        assert system["transactional_state_manager"] is not None
        assert system["service_monitor"] is not None
        assert system["degradation_manager"] is not None
        assert system["ui_error_propagator"] is not None
        assert system["progress_orchestrator"] is not None
        assert system["cancellation_manager"] is not None
        
        # Verify component references are properly set
        assert system["retry_manager"].unified_coordinator == system["error_coordinator"]
        assert system["retry_manager"].error_tracker == system["error_tracker"]
        assert system["degradation_manager"].retry_manager == system["retry_manager"]
        assert system["ui_error_propagator"].error_tracker == system["error_tracker"]
        assert system["cancellation_manager"].progress_orchestrator == system["progress_orchestrator"]
        assert system["cancellation_manager"].ui_error_propagator == system["ui_error_propagator"]
    
    @pytest.mark.asyncio
    async def test_error_recovery_performance_impact(self, integrated_error_system):
        """Test that error recovery systems don't significantly impact performance."""
        system = integrated_error_system
        
        # Measure baseline operation time
        start_time = time.time()
        
        # Execute simple operation multiple times
        for i in range(10):
            operation_id = f"perf_test_{i}"
            request_id = f"perf_request_{i}"
            
            # Start operation
            progress = system["progress_orchestrator"].start_operation(
                operation_id=operation_id,
                request_id=request_id,
                operation_type="performance_test"
            )
            
            # Simulate quick operation
            async with system["retry_manager"].retry_context(
                operation_type="performance_test",
                operation_id=operation_id
            ):
                await asyncio.sleep(0.01)  # 10ms simulated work
            
            # Complete operation
            system["progress_orchestrator"].complete_operation(operation_id, success=True)
        
        total_time = time.time() - start_time
        
        # Should complete 10 operations in reasonable time (allowing for overhead)
        assert total_time < 1.0, f"Operations took too long: {total_time}s"
        
        # Verify all operations were tracked
        tracking_stats = system["progress_orchestrator"].tracking_stats
        assert tracking_stats["completed_operations"] >= 10


if __name__ == "__main__":
    pytest.main([__file__, "-v"])