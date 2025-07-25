#!/usr/bin/env python3
"""
Test script for DocumentMasterAgent orchestration system.

This script validates the intelligent routing, workflow orchestration,
and agent coordination capabilities of the DocumentMasterAgent.
"""

import asyncio
import time
from agents import DocumentMasterAgent, OperationComplexity, WorkflowPath
from agents.base import BaseAgent, AgentResult, AgentCapability, ValidationResult

# Mock DocumentState for testing
MockDocumentState = {
    "current_document": {"title": "Test Document"},
    "cursor_position": {"paragraph": 1, "character": 0},
    "selected_text": "test text",
    "document_structure": {"paragraphs": 25, "sections": [], "tables": []},
    "formatting_state": {},
    "messages": [],
    "current_task": "",
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

class MockAgent(BaseAgent):
    """Mock agent for testing orchestration."""
    
    def __init__(self, agent_id: str, capabilities: list):
        super().__init__(agent_id, capabilities)
        self.execution_delay = 0.1  # Simulate processing time
    
    async def process(self, state, message=None):
        await asyncio.sleep(self.execution_delay)
        return AgentResult(
            agent_id=self.agent_id,
            success=True,
            result={"mock_processed": True, "agent": self.agent_id},
            execution_time=self.execution_delay,
            state_updates={"agent_status": {self.agent_id: "completed"}}
        )
    
    def validate_input(self, state, message=None):
        return ValidationResult(
            agent_id=self.agent_id,
            validation_type="mock_validation",
            passed=True,
            confidence=1.0
        )
    
    def get_required_tools(self):
        return []

async def test_request_analysis():
    """Test request analysis and complexity assessment."""
    print("Testing request analysis and complexity assessment...")
    
    master = DocumentMasterAgent()
    
    # Test simple requests
    simple_requests = [
        "create a simple chart",
        "make this text bold", 
        "insert a table with 3 rows",
        "add a page break"
    ]
    
    for request in simple_requests:
        analysis = await master._analyze_request(request, MockDocumentState)
        assert analysis.complexity == OperationComplexity.SIMPLE, f"Expected SIMPLE for '{request}', got {analysis.complexity}"
        print(f"  ✓ '{request}' correctly classified as SIMPLE")
    
    # Test moderate requests
    moderate_requests = [
        "write a summary of this section",
        "format this document professionally",
        "explain what machine learning is",
        "improve the organization of this content"
    ]
    
    for request in moderate_requests:
        analysis = await master._analyze_request(request, MockDocumentState)
        assert analysis.complexity == OperationComplexity.MODERATE, f"Expected MODERATE for '{request}', got {analysis.complexity}"
        print(f"  ✓ '{request}' correctly classified as MODERATE")
    
    # Test complex requests
    complex_requests = [
        "create a comprehensive financial report with market data",
        "research and write a detailed analysis of AI trends", 
        "restructure entire document with professional formatting",
        "comprehensive analysis of stock performance with charts"
    ]
    
    for request in complex_requests:
        analysis = await master._analyze_request(request, MockDocumentState)
        assert analysis.complexity == OperationComplexity.COMPLEX, f"Expected COMPLEX for '{request}', got {analysis.complexity}"
        print(f"  ✓ '{request}' correctly classified as COMPLEX")
    
    print("✅ Request analysis test passed")

async def test_workflow_routing():
    """Test workflow routing and execution plans."""
    print("Testing workflow routing and execution plans...")
    
    master = DocumentMasterAgent()
    
    # Test simple workflow routing
    simple_analysis = await master._analyze_request("create a simple chart", MockDocumentState)
    simple_plan = await master._create_execution_plan(simple_analysis, MockDocumentState)
    
    assert simple_plan.complexity == OperationComplexity.SIMPLE
    assert simple_plan.workflow_path == WorkflowPath.DIRECT
    assert len(simple_plan.required_agents) <= 4  # Minimal agents for simple operations
    print(f"  ✓ Simple workflow: {simple_plan.workflow_path.value} with {len(simple_plan.required_agents)} agents")
    
    # Test moderate workflow routing
    moderate_analysis = await master._analyze_request("write a summary of this section", MockDocumentState)
    moderate_plan = await master._create_execution_plan(moderate_analysis, MockDocumentState)
    
    assert moderate_plan.complexity == OperationComplexity.MODERATE
    assert moderate_plan.workflow_path == WorkflowPath.FOCUSED
    print(f"  ✓ Moderate workflow: {moderate_plan.workflow_path.value} with {len(moderate_plan.required_agents)} agents")
    
    # Test complex workflow routing
    complex_analysis = await master._analyze_request("create comprehensive financial analysis", MockDocumentState)
    complex_plan = await master._create_execution_plan(complex_analysis, MockDocumentState)
    
    assert complex_plan.complexity == OperationComplexity.COMPLEX
    assert complex_plan.workflow_path in [WorkflowPath.ORCHESTRATED, WorkflowPath.PARALLEL]
    print(f"  ✓ Complex workflow: {complex_plan.workflow_path.value} with {len(complex_plan.required_agents)} agents")
    
    print("✅ Workflow routing test passed")

async def test_agent_registration():
    """Test agent registration and management."""
    print("Testing agent registration and management...")
    
    master = DocumentMasterAgent()
    
    # Create mock agents
    context_agent = MockAgent("context_analysis", [AgentCapability.DOCUMENT_ANALYSIS])
    formatting_agent = MockAgent("formatting", [AgentCapability.FORMATTING])
    
    # Register agents
    assert master.register_agent(context_agent) == True
    assert master.register_agent(formatting_agent) == True
    
    # Check registration
    registered = master.get_registered_agents()
    assert "context_analysis" in registered
    assert "formatting" in registered
    assert AgentCapability.DOCUMENT_ANALYSIS in registered["context_analysis"]
    assert AgentCapability.FORMATTING in registered["formatting"]
    
    print(f"  ✓ Registered {len(registered)} agents successfully")
    
    # Test unregistration
    assert master.unregister_agent("context_analysis") == True
    assert master.unregister_agent("nonexistent_agent") == False
    
    updated_registered = master.get_registered_agents()
    assert "context_analysis" not in updated_registered
    assert "formatting" in updated_registered
    
    print("  ✓ Agent unregistration works correctly")
    
    print("✅ Agent registration test passed")

async def test_simple_workflow_execution():
    """Test simple workflow execution."""
    print("Testing simple workflow execution...")
    
    master = DocumentMasterAgent()
    
    # Register mock agents
    context_agent = MockAgent("context_analysis", [AgentCapability.DOCUMENT_ANALYSIS])
    formatting_agent = MockAgent("formatting", [AgentCapability.FORMATTING])
    execution_agent = MockAgent("execution", [AgentCapability.EXECUTION])
    
    master.register_agent(context_agent)
    master.register_agent(formatting_agent)
    master.register_agent(execution_agent)
    
    # Create test message
    test_message = {"content": "create a simple chart"}
    
    # Execute workflow
    start_time = time.time()
    result = await master.execute_with_monitoring(MockDocumentState, test_message)
    execution_time = time.time() - start_time
    
    # Validate results
    assert result.success == True, f"Simple workflow failed: {result.error}"
    assert execution_time <= 3.0, f"Simple workflow took too long: {execution_time}s"
    assert "orchestration" in result.metadata
    assert result.metadata["orchestration"]["complexity"] == "simple"
    
    print(f"  ✓ Simple workflow completed successfully in {execution_time:.2f}s")
    print(f"  ✓ Used workflow path: {result.metadata['orchestration']['workflow_path']}")
    
    print("✅ Simple workflow execution test passed")

async def test_moderate_workflow_execution():
    """Test moderate workflow execution."""
    print("Testing moderate workflow execution...")
    
    master = DocumentMasterAgent()
    
    # Register mock agents for moderate workflow
    agents = [
        MockAgent("context_analysis", [AgentCapability.DOCUMENT_ANALYSIS]),
        MockAgent("content_generation", [AgentCapability.CONTENT_GENERATION]),
        MockAgent("formatting", [AgentCapability.FORMATTING]),
        MockAgent("validation", [AgentCapability.VALIDATION]),
        MockAgent("execution", [AgentCapability.EXECUTION])
    ]
    
    for agent in agents:
        master.register_agent(agent)
    
    # Create test message for moderate complexity
    test_message = {"content": "write a summary of this section"}
    
    # Execute workflow
    start_time = time.time()
    result = await master.execute_with_monitoring(MockDocumentState, test_message)
    execution_time = time.time() - start_time
    
    # Validate results
    assert result.success == True, f"Moderate workflow failed: {result.error}"
    assert execution_time <= 5.0, f"Moderate workflow took too long: {execution_time}s"
    assert result.metadata["orchestration"]["complexity"] == "moderate"
    
    print(f"  ✓ Moderate workflow completed successfully in {execution_time:.2f}s")
    print(f"  ✓ Used workflow path: {result.metadata['orchestration']['workflow_path']}")
    
    print("✅ Moderate workflow execution test passed")

async def test_complex_workflow_execution():
    """Test complex workflow execution."""
    print("Testing complex workflow execution...")
    
    master = DocumentMasterAgent()
    
    # Register all mock agents for complex workflow
    agents = [
        MockAgent("context_analysis", [AgentCapability.DOCUMENT_ANALYSIS]),
        MockAgent("data_integration", [AgentCapability.DATA_INTEGRATION]),
        MockAgent("content_generation", [AgentCapability.CONTENT_GENERATION]),
        MockAgent("formatting", [AgentCapability.FORMATTING]),
        MockAgent("validation", [AgentCapability.VALIDATION]),
        MockAgent("execution", [AgentCapability.EXECUTION])
    ]
    
    for agent in agents:
        master.register_agent(agent)
    
    # Create test message for complex operation
    test_message = {"content": "create comprehensive financial report with market data"}
    
    # Execute workflow
    start_time = time.time()
    result = await master.execute_with_monitoring(MockDocumentState, test_message)
    execution_time = time.time() - start_time
    
    # Validate results
    assert result.success == True, f"Complex workflow failed: {result.error}"
    assert execution_time <= 7.0, f"Complex workflow took too long: {execution_time}s"
    assert result.metadata["orchestration"]["complexity"] == "complex"
    
    print(f"  ✓ Complex workflow completed successfully in {execution_time:.2f}s")
    print(f"  ✓ Used workflow path: {result.metadata['orchestration']['workflow_path']}")
    
    # Check if parallel processing was used for appropriate requests
    if result.metadata["orchestration"]["workflow_path"] == "parallel":
        print("  ✓ Parallel processing was correctly utilized")
    
    print("✅ Complex workflow execution test passed")

async def test_performance_tracking():
    """Test performance tracking and optimization."""
    print("Testing performance tracking and optimization...")
    
    master = DocumentMasterAgent()
    
    # Register mock agents
    context_agent = MockAgent("context_analysis", [AgentCapability.DOCUMENT_ANALYSIS])
    master.register_agent(context_agent)
    
    # Execute multiple operations to build performance history
    for i in range(5):
        test_message = {"content": f"create simple chart {i}"}
        result = await master.execute_with_monitoring(MockDocumentState, test_message)
        assert result.success == True
    
    # Check performance history
    assert "context_analysis" in master.agent_performance_history
    assert len(master.agent_performance_history["context_analysis"]) == 5
    
    # Get orchestration summary
    summary = master.get_orchestration_summary()
    assert summary["registered_agents"] == 1
    assert master.performance_metrics.operation_count == 5
    assert "performance_targets" in summary
    
    print(f"  ✓ Performance history tracked for {len(master.agent_performance_history)} agents")
    print(f"  ✓ Orchestration summary includes {len(summary)} metrics")
    
    print("✅ Performance tracking test passed")

async def test_error_handling():
    """Test error handling and fallback strategies."""
    print("Testing error handling and fallback strategies...")
    
    master = DocumentMasterAgent()
    
    # Test with empty request
    empty_result = await master.execute_with_monitoring(MockDocumentState, None)
    assert empty_result.success == False
    assert "No user request" in empty_result.error
    print("  ✓ Empty request handled correctly")
    
    # Test with invalid state
    invalid_state = {}
    test_message = {"content": "create chart"}
    result = await master.execute_with_monitoring(invalid_state, test_message)
    # Should handle invalid state gracefully with appropriate error
    assert result.success == False
    assert "validation failed" in result.error.lower()
    print("  ✓ Invalid state handled gracefully")
    
    print("✅ Error handling test passed")

async def main():
    """Run all DocumentMasterAgent tests."""
    print("🚀 Starting DocumentMasterAgent orchestration tests...\n")
    
    try:
        # Run all tests
        await test_request_analysis()
        print()
        
        await test_workflow_routing()
        print()
        
        await test_agent_registration()
        print()
        
        await test_simple_workflow_execution()
        print()
        
        await test_moderate_workflow_execution()
        print()
        
        await test_complex_workflow_execution()
        print()
        
        await test_performance_tracking()
        print()
        
        await test_error_handling()
        print()
        
        print("🎉 All DocumentMasterAgent tests passed!")
        print("\nThe DocumentMasterAgent orchestration system is working correctly:")
        print("✅ Intelligent request analysis and complexity assessment")
        print("✅ Adaptive workflow routing (Simple/Moderate/Complex)")
        print("✅ Agent registration and coordination management") 
        print("✅ Performance-optimized execution paths")
        print("✅ Comprehensive error handling and fallback strategies")
        print("✅ Performance tracking and continuous optimization")
        
        return True
        
    except Exception as e:
        print(f"\n❌ Test failed: {e}")
        import traceback
        traceback.print_exc()
        return False

if __name__ == "__main__":
    success = asyncio.run(main())
    exit(0 if success else 1)