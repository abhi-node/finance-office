"""
Tests for BaseAgent class and common interfaces.

This test suite validates the base agent functionality including lifecycle
management, tool integration, error handling, and performance monitoring.
"""

import pytest
import asyncio
import time
from unittest.mock import Mock, patch, AsyncMock
from datetime import datetime, timezone

# Import agent components
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent.parent.parent))

from agents.base import (
    BaseAgent,
    AgentResult,
    AgentError,
    AgentCapability,
    AgentLifecycleState,
    ToolCallResult,
    ValidationResult,
    PerformanceMetrics
)

from state.document_state import DocumentState, DocumentStateManager

class TestAgent(BaseAgent):
    """Test implementation of BaseAgent for testing purposes."""
    
    def __init__(self, agent_id: str = "test_agent"):
        super().__init__(
            agent_id=agent_id,
            capabilities=[AgentCapability.DOCUMENT_ANALYSIS, AgentCapability.VALIDATION],
            config={"test_config": True}
        )
    
    async def process(self, state: DocumentState, message=None) -> AgentResult:
        """Test implementation of process method."""
        await asyncio.sleep(0.1)  # Simulate processing time
        
        return AgentResult(
            agent_id=self.agent_id,
            success=True,
            result={"processed": True, "message_content": str(message) if message else None},
            state_updates={"agent_status": {self.agent_id: "completed"}}
        )
    
    def validate_input(self, state: DocumentState, message=None) -> ValidationResult:
        """Test implementation of validate_input method."""
        return ValidationResult(
            agent_id=self.agent_id,
            validation_type="input_validation",
            passed=True,
            confidence=1.0
        )
    
    def get_required_tools(self) -> list:
        """Test implementation of get_required_tools method."""
        return ["test_tool_1", "test_tool_2"]

class TestBaseAgent:
    """Test suite for BaseAgent class."""
    
    def test_agent_initialization(self):
        """Test agent initialization and basic properties."""
        agent = TestAgent("test_agent_1")
        
        assert agent.agent_id == "test_agent_1"
        assert AgentCapability.DOCUMENT_ANALYSIS in agent.capabilities
        assert AgentCapability.VALIDATION in agent.capabilities
        assert agent.lifecycle_state == AgentLifecycleState.READY
        assert agent.config["test_config"] == True
        assert agent.initialization_time is not None
    
    def test_agent_capabilities(self):
        """Test agent capability management."""
        agent = TestAgent()
        
        capabilities = agent.get_capabilities()
        assert len(capabilities) == 2
        assert agent.can_handle(AgentCapability.DOCUMENT_ANALYSIS)
        assert agent.can_handle(AgentCapability.VALIDATION)
        assert not agent.can_handle(AgentCapability.CONTENT_GENERATION)
    
    @pytest.mark.asyncio
    async def test_agent_processing(self):
        """Test agent processing workflow."""
        agent = TestAgent()
        
        # Create test state
        test_state = DocumentState(
            current_document={},
            cursor_position={},
            selected_text="test text",
            document_structure={},
            formatting_state={},
            messages=[],
            current_task="test task",
            task_history=[],
            agent_status={},
            content_analysis={},
            generated_content=[],
            content_suggestions=[],
            external_data={},
            research_citations=[],
            api_usage={},
            pending_operations=[],
            completed_operations=[],
            validation_results={},
            last_error=None,
            retry_count=0,
            error_recovery={},
            rollback_points=[],
            user_preferences={},
            interaction_history=[],
            approval_required=[],
            performance_metrics={},
            resource_utilization={},
            optimization_recommendations=[]
        )
        
        # Test processing
        result = await agent.execute_with_monitoring(test_state)
        
        assert isinstance(result, AgentResult)
        assert result.success == True
        assert result.agent_id == agent.agent_id
        assert result.execution_time > 0
        assert result.result["processed"] == True
        assert len(result.validation_results) == 1
        assert result.validation_results[0].passed == True
    
    @pytest.mark.asyncio
    async def test_tool_execution(self):
        """Test tool execution functionality."""
        agent = TestAgent()
        
        # Add test tool
        async def test_tool(param1, param2="default"):
            await asyncio.sleep(0.05)
            return {"result": f"{param1}_{param2}"}
        
        agent.tools["test_tool"] = test_tool
        
        # Execute tool
        result = await agent.call_tool("test_tool", param1="value1", param2="value2")
        
        assert isinstance(result, ToolCallResult)
        assert result.success == True
        assert result.tool_name == "test_tool"
        assert result.result["result"] == "value1_value2"
        assert result.execution_time > 0
    
    @pytest.mark.asyncio
    async def test_tool_execution_error(self):
        """Test tool execution error handling."""
        agent = TestAgent()
        
        # Add failing tool
        async def failing_tool():
            raise ValueError("Tool failed")
        
        agent.tools["failing_tool"] = failing_tool
        
        # Execute failing tool
        result = await agent.call_tool("failing_tool")
        
        assert isinstance(result, ToolCallResult)
        assert result.success == False
        assert result.error is not None
        assert "Tool failed" in result.error
    
    @pytest.mark.asyncio
    async def test_nonexistent_tool(self):
        """Test calling nonexistent tool."""
        agent = TestAgent()
        
        result = await agent.call_tool("nonexistent_tool")
        
        assert isinstance(result, ToolCallResult)
        assert result.success == False
        assert "not available" in result.error
    
    def test_performance_metrics_tracking(self):
        """Test performance metrics tracking."""
        agent = TestAgent()
        
        # Initial metrics
        assert agent.performance_metrics.operation_count == 0
        assert agent.performance_metrics.success_rate == 1.0
        
        # Simulate operations
        agent._update_performance_metrics(1.5, True)
        agent._update_performance_metrics(2.0, True)
        agent._update_performance_metrics(1.0, False)
        
        assert agent.performance_metrics.operation_count == 3
        assert agent.performance_metrics.success_rate == 2.0/3.0
        assert agent.performance_metrics.error_count == 1
        assert agent.performance_metrics.average_execution_time == (1.5 + 2.0 + 1.0) / 3
    
    def test_state_manager_integration(self):
        """Test state manager integration."""
        agent = TestAgent()
        state_manager = Mock(spec=DocumentStateManager)
        
        # Set state manager
        agent.set_state_manager(state_manager)
        assert agent.state_manager == state_manager
        
        # Test status update
        agent.update_agent_status("processing")
        state_manager.update_agent_status.assert_called_once_with(agent.agent_id, "processing")
    
    def test_performance_summary(self):
        """Test performance summary generation."""
        agent = TestAgent()
        
        # Add some operations to history
        result1 = AgentResult(agent_id=agent.agent_id, success=True)
        result2 = AgentResult(agent_id=agent.agent_id, success=False, error="test error")
        agent.operation_history.extend([result1, result2])
        
        summary = agent.get_performance_summary()
        
        assert summary["agent_id"] == agent.agent_id
        assert summary["lifecycle_state"] == AgentLifecycleState.READY.value
        assert len(summary["capabilities"]) == 2
        assert "performance_metrics" in summary
        assert summary["recent_operations"] == 2
    
    def test_operation_context_manager(self):
        """Test operation context manager."""
        agent = TestAgent()
        
        with agent.operation_context("test_operation"):
            time.sleep(0.1)  # Simulate work
        
        # Context manager should complete without error
        assert True
    
    def test_agent_cleanup(self):
        """Test agent resource cleanup."""
        agent = TestAgent()
        
        # Agent should start in READY state
        assert agent.lifecycle_state == AgentLifecycleState.READY
        
        # Cleanup should stop the agent
        agent.cleanup()
        assert agent.lifecycle_state == AgentLifecycleState.STOPPED
    
    def test_agent_context_manager(self):
        """Test agent as context manager."""
        with TestAgent() as agent:
            assert agent.lifecycle_state == AgentLifecycleState.READY
        
        # Agent should be cleaned up after context exit
        assert agent.lifecycle_state == AgentLifecycleState.STOPPED
    
    def test_agent_string_representation(self):
        """Test agent string representations."""
        agent = TestAgent("test_agent_repr")
        
        str_repr = str(agent)
        assert "TestAgent" in str_repr
        assert "test_agent_repr" in str_repr
        assert agent.lifecycle_state.value in str_repr
        
        detailed_repr = repr(agent)
        assert "TestAgent" in detailed_repr
        assert "test_agent_repr" in detailed_repr
        assert "capabilities" in detailed_repr

class TestAgentDataStructures:
    """Test suite for agent data structures."""
    
    def test_agent_result_creation(self):
        """Test AgentResult creation and properties."""
        result = AgentResult(
            agent_id="test_agent",
            success=True,
            result={"data": "test"},
            warnings=["warning1"],
            metadata={"key": "value"}
        )
        
        assert result.agent_id == "test_agent"
        assert result.success == True
        assert result.result["data"] == "test"
        assert len(result.warnings) == 1
        assert result.metadata["key"] == "value"
        assert result.operation_id is not None  # Should be auto-generated
        assert result.timestamp is not None     # Should be auto-generated
    
    def test_agent_error_creation(self):
        """Test AgentError creation and properties."""
        error = AgentError(
            agent_id="test_agent",
            error_type="TestError",
            error_message="Test error message",
            context={"operation": "test_op"}
        )
        
        assert error.agent_id == "test_agent"
        assert error.error_type == "TestError"
        assert error.error_message == "Test error message"
        assert error.context["operation"] == "test_op"
        assert error.error_id is not None  # Should be auto-generated
        assert error.recoverable == True   # Default value
        assert error.retry_count == 0      # Default value
    
    def test_tool_call_result_creation(self):
        """Test ToolCallResult creation and properties."""
        result = ToolCallResult(
            tool_name="test_tool",
            success=True,
            result={"output": "test_output"},
            execution_time=1.5,
            metadata={"tool_version": "1.0"}
        )
        
        assert result.tool_name == "test_tool"
        assert result.success == True
        assert result.result["output"] == "test_output"
        assert result.execution_time == 1.5
        assert result.metadata["tool_version"] == "1.0"
        assert result.timestamp is not None
    
    def test_validation_result_creation(self):
        """Test ValidationResult creation and properties."""
        validation = ValidationResult(
            agent_id="test_agent",
            validation_type="input_validation",
            passed=True,
            confidence=0.95,
            issues=[],
            recommendations=["recommendation1"]
        )
        
        assert validation.agent_id == "test_agent"
        assert validation.validation_type == "input_validation"
        assert validation.passed == True
        assert validation.confidence == 0.95
        assert len(validation.issues) == 0
        assert len(validation.recommendations) == 1
        assert validation.validation_id is not None
    
    def test_performance_metrics_creation(self):
        """Test PerformanceMetrics creation and properties."""
        metrics = PerformanceMetrics(
            agent_id="test_agent",
            operation_count=10,
            total_execution_time=15.5,
            error_count=2
        )
        
        assert metrics.agent_id == "test_agent" 
        assert metrics.operation_count == 10
        assert metrics.total_execution_time == 15.5
        assert metrics.error_count == 2
        # success_rate should be calculated
        assert metrics.success_rate == 1.0  # Default value, would be calculated in real usage

class TestErrorHandlingAgent(BaseAgent):
    """Test agent that simulates various error conditions."""
    
    def __init__(self):
        super().__init__(
            agent_id="error_test_agent",
            capabilities=[AgentCapability.VALIDATION]
        )
        self.should_fail_validation = False
        self.should_fail_processing = False
    
    async def process(self, state: DocumentState, message=None) -> AgentResult:
        """Process that can be configured to fail."""
        if self.should_fail_processing:
            raise RuntimeError("Simulated processing error")
        
        return AgentResult(
            agent_id=self.agent_id,
            success=True,
            result={"processed": True}
        )
    
    def validate_input(self, state: DocumentState, message=None) -> ValidationResult:
        """Validation that can be configured to fail."""
        return ValidationResult(
            agent_id=self.agent_id,
            validation_type="test_validation",
            passed=not self.should_fail_validation,
            confidence=1.0 if not self.should_fail_validation else 0.0
        )
    
    def get_required_tools(self) -> list:
        return []

class TestAgentErrorHandling:
    """Test suite for agent error handling."""
    
    @pytest.mark.asyncio
    async def test_validation_failure(self):
        """Test handling of validation failures."""
        agent = TestErrorHandlingAgent()
        agent.should_fail_validation = True
        
        # Create minimal test state
        test_state = DocumentState(
            current_document={}, cursor_position={}, selected_text="",
            document_structure={}, formatting_state={}, messages=[],
            current_task="", task_history=[], agent_status={},
            content_analysis={}, generated_content=[], content_suggestions=[],
            external_data={}, research_citations=[], api_usage={},
            pending_operations=[], completed_operations=[], validation_results={},
            last_error=None, retry_count=0, error_recovery={}, rollback_points=[],
            user_preferences={}, interaction_history=[], approval_required=[],
            performance_metrics={}, resource_utilization={}, optimization_recommendations=[]
        )
        
        result = await agent.execute_with_monitoring(test_state)
        
        assert result.success == False
        assert "validation failed" in result.error.lower()
        assert agent.performance_metrics.error_count == 1
    
    @pytest.mark.asyncio
    async def test_processing_failure(self):
        """Test handling of processing failures."""
        agent = TestErrorHandlingAgent()
        agent.should_fail_processing = True
        
        # Create minimal test state
        test_state = DocumentState(
            current_document={}, cursor_position={}, selected_text="",
            document_structure={}, formatting_state={}, messages=[],
            current_task="", task_history=[], agent_status={},
            content_analysis={}, generated_content=[], content_suggestions=[],
            external_data={}, research_citations=[], api_usage={},
            pending_operations=[], completed_operations=[], validation_results={},
            last_error=None, retry_count=0, error_recovery={}, rollback_points=[],
            user_preferences={}, interaction_history=[], approval_required=[],
            performance_metrics={}, resource_utilization={}, optimization_recommendations=[]
        )
        
        result = await agent.execute_with_monitoring(test_state)
        
        assert result.success == False
        assert "processing failed" in result.error.lower()
        assert agent.lifecycle_state == AgentLifecycleState.ERROR
        assert len(agent.error_history) == 1

if __name__ == "__main__":
    # Run tests when executed directly
    pytest.main([__file__, "-v"])