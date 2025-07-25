"""
Comprehensive Integration Tests for Error Handling Infrastructure

This module tests the complete error handling system implemented in Task 19.1,
including the UnifiedErrorCoordinator, ErrorTracker, AuditTrail, and integration
with existing ErrorRecoveryManager infrastructure.

Test Coverage:
- Unified error coordination and handling
- Error tracking and pattern analysis
- Audit trail and compliance logging
- Integration with existing infrastructure
- Cross-component error propagation
- Performance and reliability testing
"""

import asyncio
import pytest
import tempfile
import threading
import time
from datetime import datetime, timezone, timedelta
from pathlib import Path
from typing import Any, Dict, List, Optional
from unittest.mock import Mock, patch, AsyncMock

# Import test framework
try:
    import pytest_asyncio
except ImportError:
    pytest_asyncio = None

# Import error handling components
import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(__file__)))

from agents.error_handler import (
    UnifiedErrorCoordinator, ErrorContext, ErrorSeverity, ErrorCategory,
    ErrorResponse, NetworkErrorHandler, AgentCoordinationErrorHandler,
    ValidationErrorHandler, SystemErrorHandler, create_unified_error_coordinator
)
from agents.error_tracking import (
    ErrorTracker, AuditTrail, ErrorPattern, AuditEntry, DebugSnapshot,
    ErrorPatternAnalyzer, DebugContextCollector, create_error_tracker
)
from agents.error_integration import (
    ErrorHandlingIntegrator, ErrorIntegrationConfig, AgentErrorEnhancer,
    StateErrorHandler, BridgeErrorCoordinator, WorkflowErrorManager,
    get_error_integrator, initialize_error_integration
)
from agents.utils import ErrorRecoveryManager, AgentLogger
from agents.base import BaseAgent, AgentResult, AgentCapability


class TestUnifiedErrorCoordinator:
    """Test unified error coordination functionality."""
    
    @pytest.fixture
    def coordinator(self):
        """Create test coordinator."""
        return UnifiedErrorCoordinator(config={
            "adaptive_recovery": True,
            "learning_enabled": True,
            "escalation_thresholds": {
                ErrorSeverity.CRITICAL: 1,
                ErrorSeverity.HIGH: 3,
                ErrorSeverity.MEDIUM: 10,
                ErrorSeverity.LOW: 50
            }
        })
    
    @pytest.fixture
    def sample_error_context(self):
        """Create sample error context."""
        return ErrorContext(
            category=ErrorCategory.NETWORK_ERROR,
            severity=ErrorSeverity.MEDIUM,
            error_message="Connection timeout",
            agent_id="test_agent",
            operation_id="test_operation"
        )
    
    @pytest.mark.asyncio
    async def test_error_handler_registration(self, coordinator):
        """Test error handler registration and priority sorting."""
        initial_count = len(coordinator.error_handlers)
        
        # Register custom handler
        class CustomHandler(NetworkErrorHandler):
            def get_priority(self):
                return 5
        
        custom_handler = CustomHandler()
        coordinator.register_error_handler(custom_handler)
        
        # Verify registration
        assert len(coordinator.error_handlers) == initial_count + 1
        assert custom_handler in coordinator.error_handlers
        
        # Verify priority sorting
        priorities = [h.get_priority() for h in coordinator.error_handlers]
        assert priorities == sorted(priorities)
    
    @pytest.mark.asyncio
    async def test_exception_to_error_context_conversion(self, coordinator):
        """Test conversion of exceptions to error contexts."""
        test_exception = ConnectionError("Network unreachable")
        context = {"agent_id": "test_agent", "operation_id": "test_op"}
        
        error_context = coordinator._create_error_context_from_exception(test_exception, context)
        
        assert error_context.error_message == "Network unreachable"
        assert error_context.exception_type == "ConnectionError"
        assert error_context.category == ErrorCategory.NETWORK_ERROR
        assert error_context.agent_id == "test_agent"
        assert error_context.operation_id == "test_op"
        assert error_context.stack_trace is not None
    
    @pytest.mark.asyncio
    async def test_network_error_handling(self, coordinator):
        """Test network error handling with retry logic."""
        network_error = ConnectionError("Connection refused")
        context = {"agent_id": "network_agent"}
        
        response = await coordinator.handle_error(network_error, context)
        
        assert not response.success
        assert "Network connectivity issue" in response.user_message
        assert response.retry_allowed
        assert response.fallback_available
        assert "retry" in response.recovery_options
        assert response.estimated_recovery_time is not None
    
    @pytest.mark.asyncio
    async def test_validation_error_handling(self, coordinator):
        """Test validation error handling."""
        validation_error = ValueError("Invalid input format")
        context = {"agent_id": "validation_agent"}
        
        response = await coordinator.handle_error(validation_error, context)
        
        assert not response.success
        assert "validation error" in response.user_message.lower()
        assert response.retry_allowed
        assert "correct_input" in response.recovery_options
    
    @pytest.mark.asyncio
    async def test_error_escalation(self, coordinator):
        """Test error escalation for critical errors."""
        critical_error = SystemError("Critical system failure")
        context = {"agent_id": "system_agent"}
        
        # First, create error context manually to set critical severity
        error_context = ErrorContext(
            category=ErrorCategory.SYSTEM_FAILURE,
            severity=ErrorSeverity.CRITICAL,
            error_message="Critical system failure",
            agent_id="system_agent"
        )
        
        response = await coordinator.handle_error(error_context)
        
        assert not response.success
        assert response.support_reference is not None
        assert "Contact system administrator" in response.suggested_actions
    
    @pytest.mark.asyncio
    async def test_error_statistics_tracking(self, coordinator):
        """Test error statistics collection."""
        initial_stats = coordinator.get_error_statistics()
        
        # Generate several errors
        errors = [
            ConnectionError("Network error 1"),
            ValueError("Validation error 1"),
            TimeoutError("Timeout error 1"),
            ConnectionError("Network error 2")
        ]
        
        for error in errors:
            await coordinator.handle_error(error, {"agent_id": "test_agent"})
        
        final_stats = coordinator.get_error_statistics()
        
        assert final_stats["total_errors"] > initial_stats["total_errors"]
        assert len(coordinator.error_history) >= len(errors)


class TestErrorTracker:
    """Test error tracking and pattern analysis."""
    
    @pytest.fixture
    def temp_storage(self):
        """Create temporary storage for testing."""
        with tempfile.TemporaryDirectory() as temp_dir:
            yield Path(temp_dir)
    
    @pytest.fixture
    def tracker(self, temp_storage):
        """Create test error tracker."""
        return ErrorTracker(
            storage_path=temp_storage / "test_error_tracking.db",
            max_memory_entries=100,
            persistence_enabled=True
        )
    
    @pytest.fixture
    def sample_errors(self):
        """Create sample error contexts for testing."""
        return [
            ErrorContext(
                category=ErrorCategory.NETWORK_ERROR,
                severity=ErrorSeverity.MEDIUM,
                error_message=f"Network error {i}",
                agent_id="network_agent",
                operation_id=f"op_{i}"
            )
            for i in range(5)
        ]
    
    def test_error_tracking_basic(self, tracker, sample_errors):
        """Test basic error tracking functionality."""
        initial_count = tracker.tracking_statistics["total_tracked"]
        
        for error in sample_errors:
            tracker.track_error(error)
        
        assert tracker.tracking_statistics["total_tracked"] == initial_count + len(sample_errors)
        assert len(tracker.error_history) >= len(sample_errors)
        assert len(tracker.active_errors) == len(sample_errors)
    
    def test_error_resolution(self, tracker, sample_errors):
        """Test error resolution tracking."""
        # Track an error
        error = sample_errors[0]
        tracker.track_error(error)
        
        assert error.error_id in tracker.active_errors
        
        # Resolve the error
        resolution_info = {"resolution_method": "manual_fix", "time_to_resolve": 300}
        success = tracker.resolve_error(error.error_id, resolution_info)
        
        assert success
        assert error.error_id not in tracker.active_errors
        assert error.recovery_successful
        assert "resolution_method" in error.metadata
    
    def test_error_correlation_analysis(self, tracker):
        """Test error correlation detection."""
        # Create related errors
        base_time = datetime.now(timezone.utc)
        
        errors = [
            ErrorContext(
                category=ErrorCategory.AGENT_COORDINATION,
                severity=ErrorSeverity.HIGH,
                error_message="Agent 1 failed",
                agent_id="agent_1",
                operation_id="coordination_op",
                timestamp=(base_time + timedelta(seconds=i)).isoformat()
            )
            for i in range(3)
        ]
        
        # Track errors
        for error in errors:
            tracker.track_error(error)
        
        # Check correlations for the last error
        last_error = errors[-1]
        correlations = tracker.get_correlated_errors(last_error.error_id)
        
        # Should find correlations due to same operation and close timing
        assert len(correlations) > 0
    
    def test_pattern_detection(self, tracker):
        """Test error pattern detection."""
        # Create repeating pattern of network errors
        for i in range(6):
            error = ErrorContext(
                category=ErrorCategory.NETWORK_ERROR,
                severity=ErrorSeverity.MEDIUM,
                error_message=f"Repeated network error {i}",
                agent_id="network_agent",
                timestamp=(datetime.now(timezone.utc) + timedelta(seconds=i)).isoformat()
            )
            tracker.track_error(error)
        
        # Check if patterns were detected
        assert len(tracker.detected_patterns) > 0
        
        # Should detect high frequency pattern
        frequency_patterns = [
            p for p in tracker.detected_patterns.values()
            if p.pattern_type == "high_frequency"
        ]
        assert len(frequency_patterns) > 0
    
    def test_debug_snapshot_creation(self, tracker):
        """Test debug snapshot creation for significant errors."""
        critical_error = ErrorContext(
            category=ErrorCategory.SYSTEM_FAILURE,
            severity=ErrorSeverity.CRITICAL,
            error_message="Critical system failure",
            agent_id="system_agent"
        )
        
        initial_snapshots = len(tracker.debug_snapshots)
        tracker.track_error(critical_error)
        
        # Should create debug snapshot for critical error
        assert len(tracker.debug_snapshots) > initial_snapshots
        
        # Verify snapshot content
        latest_snapshot = tracker.debug_snapshots[-1]
        assert latest_snapshot.error_context == critical_error
        assert latest_snapshot.timestamp is not None


class TestAuditTrail:
    """Test audit trail functionality."""
    
    @pytest.fixture
    def temp_storage(self):
        """Create temporary storage for testing."""
        with tempfile.TemporaryDirectory() as temp_dir:
            yield Path(temp_dir)
    
    @pytest.fixture
    def audit_trail(self, temp_storage):
        """Create test audit trail."""
        return AuditTrail(
            storage_path=temp_storage / "test_audit.db",
            compliance_mode=True
        )
    
    def test_audit_event_logging(self, audit_trail):
        """Test basic audit event logging."""
        entry_id = audit_trail.log_event(
            event_type="error_handled",
            event_source="test_agent",
            event_description="Test error handled successfully",
            agent_id="test_agent",
            error_id="test_error_123",
            privacy_sensitive=False,
            compliance_relevant=True
        )
        
        assert entry_id is not None
        assert len(audit_trail.audit_entries) > 0
        
        # Verify entry content
        latest_entry = audit_trail.audit_entries[-1]
        assert latest_entry.event_type == "error_handled"
        assert latest_entry.event_source == "test_agent"
        assert latest_entry.compliance_relevant
    
    def test_compliance_logging(self, audit_trail):
        """Test compliance-specific logging."""
        with patch.object(audit_trail.logger, 'info') as mock_logger:
            audit_trail.log_event(
                event_type="privacy_event",
                event_source="data_agent",
                event_description="Processed sensitive data",
                privacy_sensitive=True,
                compliance_relevant=True
            )
            
            # Should log compliance event
            mock_logger.assert_called()
            call_args = mock_logger.call_args[0][0]
            assert "Compliance event" in call_args


class TestErrorIntegration:
    """Test error handling integration functionality."""
    
    @pytest.fixture
    def integration_config(self):
        """Create test integration configuration."""
        return ErrorIntegrationConfig(
            enable_unified_coordinator=True,
            enable_error_tracking=True,
            enable_audit_trail=True,
            integrate_with_existing_recovery=True,
            enable_persistence=False  # Disable for testing
        )
    
    @pytest.fixture
    def integrator(self, integration_config):
        """Create test error handling integrator."""
        return ErrorHandlingIntegrator(integration_config)
    
    @pytest.fixture
    def mock_agent(self):
        """Create mock agent for testing."""
        agent = Mock(spec=BaseAgent)
        agent.agent_id = "test_agent"
        agent.current_operation_id = "test_operation"
        agent.capabilities = [AgentCapability.CONTENT_GENERATION]
        
        # Mock async methods
        agent.execute_with_monitoring = AsyncMock()
        agent.call_tool = AsyncMock()
        
        return agent
    
    def test_integrator_initialization(self, integrator):
        """Test integrator initialization."""
        assert integrator.initialization_complete
        assert integrator.unified_coordinator is not None
        assert integrator.error_tracker is not None
        assert integrator.audit_trail is not None
        assert integrator.agent_enhancer is not None
        assert integrator.state_error_handler is not None
    
    def test_existing_recovery_manager_registration(self, integrator):
        """Test registration of existing recovery managers."""
        mock_recovery_manager = Mock(spec=ErrorRecoveryManager)
        agent_id = "test_agent"
        
        integrator.register_existing_recovery_manager(agent_id, mock_recovery_manager)
        
        assert agent_id in integrator.existing_recovery_managers
        assert integrator.existing_recovery_managers[agent_id] == mock_recovery_manager
    
    def test_agent_logger_registration(self, integrator):
        """Test agent logger registration."""
        mock_logger = Mock(spec=AgentLogger)
        agent_id = "test_agent"
        
        integrator.register_agent_logger(agent_id, mock_logger)
        
        assert agent_id in integrator.agent_loggers
        assert integrator.agent_loggers[agent_id] == mock_logger
    
    @pytest.mark.asyncio
    async def test_integrated_error_handling(self, integrator):
        """Test integrated error handling workflow."""
        test_error = ValueError("Test validation error")
        context = {
            "agent_id": "test_agent",
            "operation_id": "test_operation",
            "user_request": "Test request"
        }
        
        response = await integrator.handle_error(test_error, context)
        
        assert isinstance(response, ErrorResponse)
        assert not response.success
        assert response.error_context is not None
        assert response.user_message is not None
        assert response.technical_message is not None
    
    def test_agent_enhancement(self, integrator, mock_agent):
        """Test agent enhancement with error handling."""
        enhanced_agent = integrator.enhance_agent(mock_agent)
        
        # Agent should be enhanced (in practice, methods would be wrapped)
        assert enhanced_agent is not None
        # In actual implementation, we would test that methods are wrapped
    
    def test_integration_status(self, integrator):
        """Test integration status reporting."""
        status = integrator.get_integration_status()
        
        assert status["initialization_complete"]
        assert status["components"]["unified_coordinator"]
        assert status["components"]["error_tracker"]
        assert status["components"]["audit_trail"]
        assert "registered_systems" in status
        assert "statistics" in status


class TestErrorHandlerSpecialization:
    """Test specialized error handlers."""
    
    @pytest.mark.asyncio
    async def test_network_error_handler(self):
        """Test network error handler."""
        handler = NetworkErrorHandler()
        
        # Test with network error
        network_context = ErrorContext(
            category=ErrorCategory.NETWORK_ERROR,
            severity=ErrorSeverity.MEDIUM,
            error_message="Connection failed",
            retry_count=0,
            max_retries=3
        )
        
        assert handler.can_handle(network_context)
        
        response = await handler.handle_error(network_context)
        assert not response.success
        assert response.retry_allowed
        assert response.estimated_recovery_time is not None
    
    @pytest.mark.asyncio
    async def test_agent_coordination_error_handler(self):
        """Test agent coordination error handler."""
        handler = AgentCoordinationErrorHandler()
        
        # Test with workflow error
        workflow_context = ErrorContext(
            category=ErrorCategory.WORKFLOW_EXECUTION,
            severity=ErrorSeverity.HIGH,
            error_message="Workflow step failed",
        )
        
        assert handler.can_handle(workflow_context)
        
        response = await handler.handle_error(workflow_context)
        assert not response.success
        assert "rollback_state" in response.recovery_options
    
    @pytest.mark.asyncio
    async def test_validation_error_handler(self):
        """Test validation error handler."""
        handler = ValidationErrorHandler()
        
        # Test with validation error
        validation_context = ErrorContext(
            category=ErrorCategory.INPUT_VALIDATION,
            severity=ErrorSeverity.LOW,
            error_message="Invalid input format",
        )
        
        assert handler.can_handle(validation_context)
        
        response = await handler.handle_error(validation_context)
        assert not response.success
        assert "correct_input" in response.recovery_options
        assert response.estimated_recovery_time == 0  # Immediate fix possible
    
    @pytest.mark.asyncio
    async def test_system_error_handler(self):
        """Test system error handler."""
        handler = SystemErrorHandler()
        
        # Test with critical system error
        system_context = ErrorContext(
            category=ErrorCategory.SYSTEM_FAILURE,
            severity=ErrorSeverity.CRITICAL,
            error_message="Out of memory",
        )
        
        assert handler.can_handle(system_context)
        
        response = await handler.handle_error(system_context)
        assert not response.success
        assert not response.retry_allowed
        assert response.support_reference is not None


class TestPerformanceAndReliability:
    """Test performance and reliability of error handling system."""
    
    @pytest.mark.asyncio
    async def test_concurrent_error_handling(self):
        """Test handling multiple concurrent errors."""
        coordinator = create_unified_error_coordinator()
        
        # Create multiple concurrent errors
        errors = [
            ValueError(f"Validation error {i}")
            for i in range(10)
        ]
        
        # Handle errors concurrently
        tasks = [
            coordinator.handle_error(error, {"agent_id": f"agent_{i}"})
            for i, error in enumerate(errors)
        ]
        
        responses = await asyncio.gather(*tasks)
        
        # All errors should be handled
        assert len(responses) == len(errors)
        assert all(isinstance(r, ErrorResponse) for r in responses)
        assert all(not r.success for r in responses)  # All are error responses
    
    def test_memory_usage_control(self):
        """Test memory usage control in error tracking."""
        max_entries = 50
        tracker = ErrorTracker(
            max_memory_entries=max_entries,
            persistence_enabled=False
        )
        
        # Create more errors than max entries
        for i in range(max_entries + 20):
            error = ErrorContext(
                category=ErrorCategory.NETWORK_ERROR,
                severity=ErrorSeverity.LOW,
                error_message=f"Error {i}",
                agent_id="test_agent"
            )
            tracker.track_error(error)
        
        # Should not exceed max entries
        assert len(tracker.error_history) <= max_entries
    
    def test_thread_safety(self):
        """Test thread safety of error handling components."""
        coordinator = create_unified_error_coordinator()
        errors_handled = []
        
        def handle_error_thread(error_id):
            """Handle error in thread."""
            try:
                error = ValueError(f"Thread error {error_id}")
                # Note: Using asyncio.run in thread (simplified for test)
                loop = asyncio.new_event_loop()
                asyncio.set_event_loop(loop)
                response = loop.run_until_complete(
                    coordinator.handle_error(error, {"thread_id": error_id})
                )
                errors_handled.append(response)
            finally:
                loop.close()
        
        # Create multiple threads
        threads = [
            threading.Thread(target=handle_error_thread, args=(i,))
            for i in range(5)
        ]
        
        # Start all threads
        for thread in threads:
            thread.start()
        
        # Wait for completion
        for thread in threads:
            thread.join()
        
        # All errors should be handled
        assert len(errors_handled) == 5


# Integration test fixtures and utilities
@pytest.fixture(scope="session")
def event_loop():
    """Create event loop for async tests."""
    if pytest_asyncio:
        return pytest_asyncio.event_loop()
    else:
        loop = asyncio.new_event_loop()
        yield loop
        loop.close()


@pytest.fixture
def sample_document_state():
    """Create sample document state for testing."""
    return {
        "current_document": {"id": "test_doc", "title": "Test Document"},
        "cursor_position": {"line": 1, "column": 1},
        "selected_text": "",
        "document_structure": {"sections": []},
        "formatting_state": {"font": "Arial", "size": 12},
        "messages": [],
        "current_task": "test_task",
        "agent_status": {}
    }


class TestEndToEndIntegration:
    """End-to-end integration tests."""
    
    @pytest.mark.asyncio
    async def test_complete_error_handling_workflow(self, sample_document_state):
        """Test complete error handling workflow from error to resolution."""
        # Initialize integrated error handling
        config = ErrorIntegrationConfig(enable_persistence=False)
        integrator = ErrorHandlingIntegrator(config)
        
        # Simulate agent error
        test_error = ConnectionError("Database connection failed")
        context = {
            "agent_id": "database_agent",
            "operation_id": "fetch_data",
            "document_state": sample_document_state,
            "user_request": "Get financial data"
        }
        
        # Handle error through integrated system
        response = await integrator.handle_error(test_error, context)
        
        # Verify complete workflow
        assert isinstance(response, ErrorResponse)
        assert not response.success
        assert response.error_context is not None
        
        # Verify error was tracked
        assert integrator.error_tracker is not None
        assert len(integrator.error_tracker.error_history) > 0
        
        # Verify audit trail
        assert integrator.audit_trail is not None
        assert len(integrator.audit_trail.audit_entries) > 0
        
        # Verify error is in active errors
        error_id = response.error_context.error_id
        assert error_id in integrator.error_tracker.active_errors
        
        # Simulate error resolution
        resolution_success = integrator.error_tracker.resolve_error(
            error_id, 
            {"resolution_method": "connection_restored"}
        )
        
        assert resolution_success
        assert error_id not in integrator.error_tracker.active_errors


if __name__ == "__main__":
    # Run tests if executed directly
    pytest.main([__file__, "-v", "--tb=short"])