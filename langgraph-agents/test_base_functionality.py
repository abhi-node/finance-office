#!/usr/bin/env python3
"""
Test script to validate base agent functionality.

This script tests the core functionality of the agent base classes
and interfaces to ensure they work correctly before implementing
specialized agents.
"""

import asyncio
import time
from agents.base import (
    BaseAgent, 
    AgentResult, 
    AgentCapability, 
    AgentLifecycleState,
    ValidationResult
)
from agents.tools import DocumentToolkit, create_toolkit
from agents.utils import AgentLogger, create_agent_logger

# Mock DocumentState for testing
MockDocumentState = {
    "current_document": {},
    "cursor_position": {},
    "selected_text": "test text",
    "document_structure": {},
    "formatting_state": {},
    "messages": [],
    "current_task": "test task",
    "task_history": [],
    "agent_status": {},
    "content_analysis": {},
    "generated_content": [],
    "content_suggestions": [],
    "external_data": {},
    "research_citations": [],
    "api_usage": {},
    "pending_operations": [],
    "completed_operations": [],
    "validation_results": {},
    "last_error": None,
    "retry_count": 0,
    "error_recovery": {},
    "rollback_points": [],
    "user_preferences": {},
    "interaction_history": [],
    "approval_required": [],
    "performance_metrics": {},
    "resource_utilization": {},
    "optimization_recommendations": []
}

class TestAgent(BaseAgent):
    """Simple test implementation of BaseAgent."""
    
    def __init__(self):
        super().__init__(
            agent_id="test_agent",
            capabilities=[AgentCapability.DOCUMENT_ANALYSIS],
            config={"test_mode": True}
        )
    
    async def process(self, state, message=None):
        """Test processing implementation."""
        await asyncio.sleep(0.1)  # Simulate work
        
        return AgentResult(
            agent_id=self.agent_id,
            success=True,
            result={"processed": True, "timestamp": time.time()},
            state_updates={"agent_status": {self.agent_id: "completed"}}
        )
    
    def validate_input(self, state, message=None):
        """Test validation implementation."""
        return ValidationResult(
            agent_id=self.agent_id,
            validation_type="input_test",
            passed=True,
            confidence=1.0
        )
    
    def get_required_tools(self):
        """Return required tools."""
        return ["document_toolkit"]

async def test_base_agent():
    """Test basic agent functionality."""
    print("Testing BaseAgent functionality...")
    
    agent = TestAgent()
    
    # Test basic properties
    assert agent.agent_id == "test_agent"
    assert agent.lifecycle_state == AgentLifecycleState.READY
    assert AgentCapability.DOCUMENT_ANALYSIS in agent.capabilities
    
    # Test processing
    result = await agent.execute_with_monitoring(MockDocumentState)
    assert result.success == True
    assert result.agent_id == agent.agent_id
    assert result.execution_time > 0
    
    print("✓ BaseAgent functionality test passed")
    return True

def test_tools():
    """Test tool integration."""
    print("Testing tool integration...")
    
    # Test toolkit creation
    toolkit = create_toolkit("document", {"test_config": True})
    assert isinstance(toolkit, DocumentToolkit)
    assert toolkit.tool_name == "document_toolkit"
    
    # Test UNO service manager
    from agents.tools import UnoServiceManager
    uno_manager = UnoServiceManager()
    assert uno_manager is not None
    
    print("✓ Tool integration test passed")
    return True

def test_utilities():
    """Test utility classes."""
    print("Testing utility classes...")
    
    # Test logger creation
    logger = create_agent_logger("test_agent", {"log_level": "INFO"})
    assert logger.agent_id == "test_agent"
    
    # Test logging
    logger.info("Test log message", context={"test": True})
    recent_logs = logger.get_recent_logs(5)
    assert len(recent_logs) > 0
    assert recent_logs[-1].message == "Test log message"
    
    print("✓ Utility classes test passed")
    return True

def test_data_structures():
    """Test agent data structures."""
    print("Testing data structures...")
    
    # Test AgentResult
    result = AgentResult(
        agent_id="test",
        success=True,
        result={"data": "test"},
        warnings=["test warning"]
    )
    assert result.agent_id == "test"
    assert result.success == True
    assert result.operation_id is not None
    
    # Test ValidationResult
    validation = ValidationResult(
        agent_id="test",
        validation_type="test",
        passed=True,
        confidence=0.9
    )
    assert validation.passed == True
    assert validation.confidence == 0.9
    
    print("✓ Data structures test passed")
    return True

async def main():
    """Run all tests."""
    print("Starting base functionality tests...\n")
    
    try:
        # Run tests
        await test_base_agent()
        test_tools()
        test_utilities()
        test_data_structures()
        
        print("\n🎉 All tests passed! Base agent functionality is working correctly.")
        print("\nBase classes and interfaces are ready for specialized agent implementation.")
        
    except Exception as e:
        print(f"\n❌ Test failed: {e}")
        import traceback
        traceback.print_exc()
        return False
    
    return True

if __name__ == "__main__":
    # Run the tests
    success = asyncio.run(main())
    exit(0 if success else 1)