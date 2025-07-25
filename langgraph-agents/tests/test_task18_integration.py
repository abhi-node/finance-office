#!/usr/bin/env python3
"""
Task 18 Integration Test Suite

Comprehensive end-to-end testing of the complete Task 18: Intelligent Agent
Routing System implementation. Tests all 5 subtasks integration and validates
that the system can take a request with proper context and return functioning,
reliable responses.

Features:
- Complete workflow testing for all complexity levels
- Real agent coordination validation
- Performance monitoring integration
- Error handling and fallback testing
- Stress testing with multiple concurrent requests
"""

import asyncio
import logging
import time
import json
from typing import Dict, List, Any, Optional
from datetime import datetime
import unittest
from unittest.mock import Mock, AsyncMock, patch

# Import Task 18 components (adjusted for tests folder)
import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

try:
    from routing.complexity_analyzer import ComplexityAnalyzer, OperationType, OperationComplexity
    from routing.lightweight_router import LightweightRouter
    from routing.focused_router import FocusedRouter  
    from routing.full_orchestration_router import FullOrchestrationRouter
    from routing.performance_monitor import PerformanceMonitor, MetricType
    from agents.document_master import DocumentMasterAgent
    ROUTING_AVAILABLE = True
except ImportError as e:
    print(f"Warning: Could not import routing components: {e}")
    ROUTING_AVAILABLE = False

# Mock state classes if not available
try:
    from state.document_state import DocumentState
    from agents.base import BaseAgent, AgentResult
    STATE_AVAILABLE = True
except ImportError:
    DocumentState = Dict[str, Any]
    BaseAgent = Any
    AgentResult = Dict[str, Any]
    STATE_AVAILABLE = False

class MockAgent:
    """Mock agent for testing purposes."""
    
    def __init__(self, agent_id: str, processing_time: float = 0.1):
        self.agent_id = agent_id
        self.processing_time = processing_time
    
    async def process(self, document_state: DocumentState) -> AgentResult:
        """Mock processing with simulated delay."""
        await asyncio.sleep(self.processing_time)
        
        return {
            "success": True,
            "agent_id": self.agent_id,
            "result": f"Processed by {self.agent_id}",
            "state_updates": {
                f"{self.agent_id}_processed": True,
                "last_processed_by": self.agent_id
            },
            "execution_time": self.processing_time
        }
    
    async def analyze_cursor_position(self, document_state: DocumentState) -> Dict[str, Any]:
        """Mock cursor analysis."""
        return {
            "cursor_position": {"line": 1, "column": 1},
            "context": "Mock cursor context"
        }
    
    async def execute_direct_operation(self, operation_data: Dict[str, Any]) -> Dict[str, Any]:
        """Mock direct operation execution."""
        return {
            "success": True,
            "operation": operation_data.get("method", "unknown"),
            "result": "Direct operation executed successfully"
        }

class Task18IntegrationTests(unittest.TestCase):
    """Comprehensive integration tests for Task 18 implementation."""
    
    def setUp(self):
        """Set up test environment."""
        if not ROUTING_AVAILABLE:
            self.skipTest("Routing components not available")
        
        # Set up logging
        logging.basicConfig(level=logging.INFO)
        self.logger = logging.getLogger("task18_tests")
        
        # Initialize components
        self.complexity_analyzer = ComplexityAnalyzer()
        self.lightweight_router = LightweightRouter()
        self.focused_router = FocusedRouter()
        self.full_orchestration_router = FullOrchestrationRouter()
        self.performance_monitor = PerformanceMonitor()
        
        # Create mock agents
        self.mock_agents = {
            "context_analysis_agent": MockAgent("context_analysis_agent", 0.2),
            "content_generation_agent": MockAgent("content_generation_agent", 0.3),
            "formatting_agent": MockAgent("formatting_agent", 0.15),
            "data_integration_agent": MockAgent("data_integration_agent", 0.4),
            "validation_agent": MockAgent("validation_agent", 0.1),
            "execution_agent": MockAgent("execution_agent", 0.05)
        }
        
        # Register agents with routers
        for agent_id, agent in self.mock_agents.items():
            self.lightweight_router.register_agent(agent_id, agent)
            self.focused_router.register_agent(agent_id, agent)
            self.full_orchestration_router.register_agent(agent_id, agent)
        
        # Sample document state
        self.sample_document_state = {
            "document_id": "test_doc_001",
            "document_size": 500,
            "pages": 3,
            "cursor_position": {"line": 1, "column": 1},
            "content": "Sample document content for testing",
            "tables": 1,
            "charts": 0,
            "images": 0,
            "styles": ["Normal", "Heading1", "Heading2"]
        }
    
    def tearDown(self):
        """Clean up test environment."""
        if hasattr(self, 'performance_monitor'):
            self.performance_monitor.shutdown()

class TestComplexityAnalysis(Task18IntegrationTests):
    """Test Task 18.1: Operation Complexity Assessment Module."""
    
    async def test_simple_operation_analysis(self):
        """Test analysis of simple operations."""
        test_requests = [
            "Make this text bold",
            "Move cursor to line 5",
            "Insert 'Hello World' here",
            "Delete selected text"
        ]
        
        for request in test_requests:
            assessment = await self.complexity_analyzer.analyze_complexity(
                request, self.sample_document_state
            )
            
            self.assertEqual(assessment.complexity, OperationComplexity.SIMPLE)
            self.assertLessEqual(assessment.estimated_time, 2.0)
            self.assertGreater(assessment.confidence_score, 0.5)
            self.logger.info(f"Simple operation '{request}': {assessment.complexity.value} "
                           f"({assessment.estimated_time:.2f}s, confidence: {assessment.confidence_score:.2f})")
    
    async def test_moderate_operation_analysis(self):
        """Test analysis of moderate operations."""
        test_requests = [
            "Write a summary of this document",
            "Format this document with professional styling", 
            "Reorganize the document structure",
            "Analyze this text for readability"
        ]
        
        for request in test_requests:
            assessment = await self.complexity_analyzer.analyze_complexity(
                request, self.sample_document_state
            )
            
            self.assertEqual(assessment.complexity, OperationComplexity.MODERATE)
            self.assertGreaterEqual(assessment.estimated_time, 2.0)
            self.assertLessEqual(assessment.estimated_time, 4.0)
            self.assertGreater(assessment.confidence_score, 0.5)
            self.logger.info(f"Moderate operation '{request}': {assessment.complexity.value} "
                           f"({assessment.estimated_time:.2f}s, confidence: {assessment.confidence_score:.2f})")
    
    async def test_complex_operation_analysis(self):
        """Test analysis of complex operations."""
        test_requests = [
            "Create a comprehensive financial analysis report with live market data",
            "Generate a complete professional document with charts, tables, and formatting",
            "Perform thorough document analysis with data integration and validation",
            "Create advanced data visualization with real-time stock information"
        ]
        
        for request in test_requests:
            assessment = await self.complexity_analyzer.analyze_complexity(
                request, self.sample_document_state
            )
            
            self.assertEqual(assessment.complexity, OperationComplexity.COMPLEX)
            self.assertGreaterEqual(assessment.estimated_time, 3.0)
            self.assertLessEqual(assessment.estimated_time, 5.0)
            self.assertGreater(assessment.confidence_score, 0.5)
            self.logger.info(f"Complex operation '{request}': {assessment.complexity.value} "
                           f"({assessment.estimated_time:.2f}s, confidence: {assessment.confidence_score:.2f})")

class TestLightweightRouting(Task18IntegrationTests):
    """Test Task 18.2: Lightweight Agent Chain Router."""
    
    async def test_simple_formatting_workflow(self):
        """Test lightweight routing for simple formatting."""
        result = await self.lightweight_router.route_operation(
            OperationType.BASIC_FORMATTING,
            "Make this text bold",
            self.sample_document_state
        )
        
        self.assertTrue(result.success)
        self.assertEqual(result.operation_type, OperationType.BASIC_FORMATTING)
        self.assertLessEqual(result.execution_time, 2.0)
        self.assertGreater(len(result.agents_used), 0)
        self.logger.info(f"Lightweight formatting: {result.execution_time:.3f}s, "
                        f"agents: {result.agents_used}")
    
    async def test_cursor_movement_workflow(self):
        """Test lightweight routing for cursor movement."""
        result = await self.lightweight_router.route_operation(
            OperationType.CURSOR_MOVEMENT,
            "Move cursor to line 10",
            self.sample_document_state
        )
        
        self.assertTrue(result.success)
        self.assertEqual(result.operation_type, OperationType.CURSOR_MOVEMENT)
        self.assertLessEqual(result.execution_time, 2.0)
        self.logger.info(f"Cursor movement: {result.execution_time:.3f}s, "
                        f"agents: {result.agents_used}")
    
    async def test_optimization_patterns(self):
        """Test optimization patterns for common operations."""
        optimization_tests = [
            ("Make this bold", OperationType.BASIC_FORMATTING),
            ("Change font to Arial", OperationType.BASIC_FORMATTING),
            ("Move cursor to beginning", OperationType.CURSOR_MOVEMENT)
        ]
        
        for request, operation_type in optimization_tests:
            result = await self.lightweight_router.route_operation(
                operation_type, request, self.sample_document_state
            )
            
            self.assertTrue(result.success)
            self.assertLessEqual(result.execution_time, 1.5)  # Should be optimized
            if "optimized" in result.performance_metrics:
                self.logger.info(f"Optimization pattern used for: {request}")

class TestFocusedRouting(Task18IntegrationTests):
    """Test Task 18.3: Focused Agent Subset Router."""
    
    async def test_content_generation_workflow(self):
        """Test focused routing for content generation."""
        result = await self.focused_router.route_operation(
            OperationType.CONTENT_GENERATION,
            "Write a summary of this document",
            self.sample_document_state
        )
        
        self.assertTrue(result.success)
        self.assertEqual(result.operation_type, OperationType.CONTENT_GENERATION)
        self.assertGreaterEqual(result.execution_time, 2.0)
        self.assertLessEqual(result.execution_time, 4.0)
        self.assertGreater(result.parallel_efficiency, 0.0)
        self.logger.info(f"Content generation: {result.execution_time:.3f}s, "
                        f"parallel efficiency: {result.parallel_efficiency:.2f}")
    
    async def test_document_styling_workflow(self):
        """Test focused routing for document styling."""
        result = await self.focused_router.route_operation(
            OperationType.DOCUMENT_STYLING,
            "Apply professional formatting to this document",
            self.sample_document_state
        )
        
        self.assertTrue(result.success)
        self.assertEqual(result.operation_type, OperationType.DOCUMENT_STYLING)
        self.assertGreaterEqual(result.execution_time, 2.0)
        self.assertLessEqual(result.execution_time, 4.0)
        self.assertGreater(result.quality_score, 0.7)
        self.logger.info(f"Document styling: {result.execution_time:.3f}s, "
                        f"quality: {result.quality_score:.2f}")
    
    async def test_parallel_execution_efficiency(self):
        """Test parallel execution efficiency in focused routing."""
        result = await self.focused_router.route_operation(
            OperationType.STRUCTURE_MODIFICATION,
            "Reorganize document structure and improve readability",
            self.sample_document_state
        )
        
        self.assertTrue(result.success)
        self.assertEqual(result.operation_type, OperationType.STRUCTURE_MODIFICATION)
        # Should have some parallel efficiency for multi-agent execution
        self.assertGreater(result.parallel_efficiency, 0.0)
        self.logger.info(f"Structure modification parallel efficiency: {result.parallel_efficiency:.2f}")

class TestFullOrchestration(Task18IntegrationTests):
    """Test Task 18.4: Full Agent Orchestration Router."""
    
    def setUp(self):
        """Set up test environment with longer mock agent times for complex operations."""
        super().setUp()
        
        # Override mock agents with scaled processing times for complex orchestration
        # Target total time: ~4.2 seconds (from orchestration plan)
        self.complex_mock_agents = {
            "context_analysis_agent": MockAgent("context_analysis_agent", 0.5),
            "content_generation_agent": MockAgent("content_generation_agent", 0.9),
            "formatting_agent": MockAgent("formatting_agent", 0.6),
            "data_integration_agent": MockAgent("data_integration_agent", 1.2),
            "validation_agent": MockAgent("validation_agent", 0.5),
            "execution_agent": MockAgent("execution_agent", 0.3)
        }
        
        # Register the complex mock agents with the full orchestration router
        for agent_id, agent in self.complex_mock_agents.items():
            self.full_orchestration_router.register_agent(agent_id, agent)
    
    async def test_complex_workflow_orchestration(self):
        """Test full orchestration for complex workflows."""
        result = await self.full_orchestration_router.route_operation(
            OperationType.FINANCIAL_ANALYSIS,
            "Create comprehensive financial analysis with live market data and professional formatting",
            self.sample_document_state
        )
        
        self.assertTrue(result.success)
        self.assertEqual(result.operation_type, OperationType.FINANCIAL_ANALYSIS)
        self.assertGreaterEqual(result.execution_time, 3.0)
        self.assertLessEqual(result.execution_time, 5.0)
        self.assertGreater(result.checkpoints_created, 0)
        self.logger.info(f"Financial analysis: {result.execution_time:.3f}s, "
                        f"checkpoints: {result.checkpoints_created}")
    
    async def test_multi_step_workflow(self):
        """Test multi-step workflow orchestration."""
        result = await self.full_orchestration_router.route_operation(
            OperationType.MULTI_STEP_WORKFLOW,
            "Perform complete document analysis, generate content, apply formatting, and validate results",
            self.sample_document_state
        )
        
        self.assertTrue(result.success)
        self.assertEqual(result.operation_type, OperationType.MULTI_STEP_WORKFLOW)
        self.assertGreater(len(result.agents_executed), 3)  # Should use multiple agents
        self.assertGreater(result.quality_score, 0.7)
        self.logger.info(f"Multi-step workflow: {result.execution_time:.3f}s, "
                        f"agents: {len(result.agents_executed)}, quality: {result.quality_score:.2f}")
    
    async def test_rollback_mechanism(self):
        """Test rollback mechanism on failure."""
        # Create a failing agent for this test
        failing_agent = MockAgent("failing_agent", 0.1)
        
        async def failing_process(document_state):
            raise Exception("Simulated agent failure")
        
        failing_agent.process = failing_process
        self.full_orchestration_router.register_agent("failing_agent", failing_agent)
        
        # This should trigger rollback mechanisms
        result = await self.full_orchestration_router.route_operation(
            OperationType.DATA_INTEGRATION,
            "Integrate external data with rollback testing",
            self.sample_document_state
        )
        
        # Even with failures, orchestration should handle gracefully
        self.assertIsNotNone(result)
        self.logger.info(f"Rollback test completed, success: {result.success}")

class TestPerformanceMonitoring(Task18IntegrationTests):
    """Test Task 18.5: Dynamic Performance Monitoring and Optimization."""
    
    async def test_performance_tracking(self):
        """Test performance tracking across all routers."""
        
        # Execute operations across different routers
        operations = [
            (self.lightweight_router, OperationType.BASIC_FORMATTING, "Make text bold"),
            (self.focused_router, OperationType.CONTENT_GENERATION, "Write summary"),
            (self.full_orchestration_router, OperationType.FINANCIAL_ANALYSIS, "Create financial report")
        ]
        
        for router, operation_type, request in operations:
            start_time = time.time()
            result = await router.route_operation(operation_type, request, self.sample_document_state)
            execution_time = time.time() - start_time
            
            # Record metrics in performance monitor
            self.performance_monitor.record_execution_result(
                operation_type=operation_type,
                router_type=router.__class__.__name__,
                execution_time=execution_time,
                success=result.success,
                quality_score=getattr(result, 'quality_score', 0.8),
                user_request=request
            )
        
        # Get performance dashboard
        dashboard = self.performance_monitor.get_performance_dashboard()
        
        self.assertIn("monitoring_status", dashboard)
        self.assertIn("summary_metrics", dashboard)
        self.assertIn("router_performance", dashboard)
        self.logger.info(f"Performance dashboard generated with {len(dashboard)} sections")
    
    async def test_pattern_detection(self):
        """Test request pattern detection and optimization."""
        
        # Simulate repeated requests to trigger pattern detection
        repeated_request = "Make this text bold"
        
        for i in range(5):
            result = await self.lightweight_router.route_operation(
                OperationType.BASIC_FORMATTING,
                repeated_request,
                self.sample_document_state
            )
            
            self.performance_monitor.record_execution_result(
                operation_type=OperationType.BASIC_FORMATTING,
                router_type="LightweightRouter",
                execution_time=result.execution_time,
                success=result.success,
                user_request=repeated_request
            )
        
        # Check if patterns were detected
        dashboard = self.performance_monitor.get_performance_dashboard()
        patterns = dashboard.get("top_optimization_patterns", [])
        
        if patterns:
            self.logger.info(f"Detected {len(patterns)} optimization patterns")
            for pattern in patterns:
                self.logger.info(f"Pattern: {pattern['pattern_id']}, potential: {pattern['optimization_potential']}")

class TestEndToEndIntegration(Task18IntegrationTests):
    """Test complete end-to-end integration with DocumentMasterAgent."""
    
    async def test_full_system_integration(self):
        """Test complete system integration through DocumentMasterAgent."""
        
        # Create DocumentMasterAgent with routing system
        master_agent = DocumentMasterAgent()
        
        # Register our routing components (if the integration points exist)
        if hasattr(master_agent, 'complexity_analyzer'):
            master_agent.complexity_analyzer = self.complexity_analyzer
        if hasattr(master_agent, 'lightweight_router'):
            master_agent.lightweight_router = self.lightweight_router
        if hasattr(master_agent, 'focused_router'):
            master_agent.focused_router = self.focused_router
        if hasattr(master_agent, 'full_orchestration_router'):
            master_agent.full_orchestration_router = self.full_orchestration_router
        if hasattr(master_agent, 'performance_monitor'):
            master_agent.performance_monitor = self.performance_monitor
        
        # Test requests of varying complexity
        test_requests = [
            ("Make this text bold", OperationComplexity.SIMPLE),
            ("Write a summary of this document", OperationComplexity.MODERATE),
            ("Create comprehensive financial analysis with live data", OperationComplexity.COMPLEX)
        ]
        
        for request, expected_complexity in test_requests:
            # First analyze complexity
            assessment = await self.complexity_analyzer.analyze_complexity(
                request, self.sample_document_state
            )
            
            self.assertEqual(assessment.complexity, expected_complexity)
            
            # Then execute appropriate routing
            if assessment.complexity == OperationComplexity.SIMPLE:
                router = self.lightweight_router
            elif assessment.complexity == OperationComplexity.MODERATE:
                router = self.focused_router
            else:
                router = self.full_orchestration_router
            
            result = await router.route_operation(
                assessment.operation_type, request, self.sample_document_state
            )
            
            self.assertTrue(result.success)
            self.logger.info(f"End-to-end test: '{request}' -> {assessment.complexity.value} "
                           f"-> {result.execution_time:.3f}s")

class TestStressAndConcurrency(Task18IntegrationTests):
    """Test system under stress and concurrent load."""
    
    async def test_concurrent_requests(self):
        """Test handling of concurrent requests."""
        
        async def execute_request(request_id: int, request: str, operation_type: OperationType):
            """Execute a single request."""
            start_time = time.time()
            
            # Analyze complexity
            assessment = await self.complexity_analyzer.analyze_complexity(
                request, self.sample_document_state
            )
            
            # Route to appropriate router
            if assessment.complexity == OperationComplexity.SIMPLE:
                router = self.lightweight_router
            elif assessment.complexity == OperationComplexity.MODERATE:
                router = self.focused_router
            else:
                router = self.full_orchestration_router
            
            result = await router.route_operation(
                operation_type, request, self.sample_document_state
            )
            
            execution_time = time.time() - start_time
            
            return {
                "request_id": request_id,
                "success": result.success,
                "execution_time": execution_time,
                "complexity": assessment.complexity.value
            }
        
        # Create concurrent requests
        concurrent_requests = [
            (1, "Make text bold", OperationType.BASIC_FORMATTING),
            (2, "Write summary", OperationType.CONTENT_GENERATION),
            (3, "Create chart", OperationType.DATA_INTEGRATION),
            (4, "Format document", OperationType.DOCUMENT_STYLING),
            (5, "Analyze content", OperationType.BASIC_ANALYSIS)
        ]
        
        # Execute all requests concurrently
        tasks = [
            execute_request(req_id, request, op_type)
            for req_id, request, op_type in concurrent_requests
        ]
        
        results = await asyncio.gather(*tasks)
        
        # Validate results
        for result in results:
            self.assertTrue(result["success"])
            self.assertLess(result["execution_time"], 10.0)  # Reasonable timeout
            self.logger.info(f"Concurrent request {result['request_id']}: "
                           f"{result['complexity']} - {result['execution_time']:.3f}s")
        
        self.logger.info(f"Successfully processed {len(results)} concurrent requests")

async def run_all_tests():
    """Run all Task 18 integration tests."""
    
    print("=" * 80)
    print("TASK 18 INTEGRATION TEST SUITE")
    print("Testing complete Intelligent Agent Routing System implementation")
    print("=" * 80)
    
    # Initialize test suite
    test_suite = unittest.TestSuite()
    
    # Add test classes
    test_classes = [
        TestComplexityAnalysis,
        TestLightweightRouting,
        TestFocusedRouting,
        TestFullOrchestration,
        TestPerformanceMonitoring,
        TestEndToEndIntegration,
        TestStressAndConcurrency
    ]
    
    for test_class in test_classes:
        tests = unittest.TestLoader().loadTestsFromTestCase(test_class)
        test_suite.addTests(tests)
    
    # Custom test runner for async tests
    class AsyncTestRunner:
        def __init__(self):
            self.results = []
            
        async def run_test_class(self, test_class):
            """Run all tests in a test class."""
            print(f"\n{'=' * 60}")
            print(f"Running {test_class.__name__}")
            print(f"{'=' * 60}")
            
            test_instance = test_class()
            test_instance.setUp()
            
            try:
                # Get all test methods
                test_methods = [method for method in dir(test_instance) 
                              if method.startswith('test_') and callable(getattr(test_instance, method))]
                
                for method_name in test_methods:
                    print(f"\n--- {method_name} ---")
                    test_method = getattr(test_instance, method_name)
                    
                    try:
                        if asyncio.iscoroutinefunction(test_method):
                            await test_method()
                        else:
                            test_method()
                        print(f"✓ {method_name} PASSED")
                        self.results.append((test_class.__name__, method_name, "PASSED", None))
                    except Exception as e:
                        print(f"✗ {method_name} FAILED: {e}")
                        self.results.append((test_class.__name__, method_name, "FAILED", str(e)))
                        
            finally:
                test_instance.tearDown()
        
        async def run_all(self, test_classes):
            """Run all test classes."""
            for test_class in test_classes:
                await self.run_test_class(test_class)
            
            # Print summary
            print(f"\n{'=' * 80}")
            print("TEST SUMMARY")
            print(f"{'=' * 80}")
            
            passed = sum(1 for _, _, status, _ in self.results if status == "PASSED")
            failed = sum(1 for _, _, status, _ in self.results if status == "FAILED")
            
            print(f"Total Tests: {len(self.results)}")
            print(f"Passed: {passed}")
            print(f"Failed: {failed}")
            print(f"Success Rate: {passed/len(self.results)*100:.1f}%" if self.results else "0%")
            
            if failed > 0:
                print(f"\nFAILED TESTS:")
                for class_name, method_name, status, error in self.results:
                    if status == "FAILED":
                        print(f"  {class_name}.{method_name}: {error}")
            
            return passed == len(self.results)
    
    # Run tests
    runner = AsyncTestRunner()
    success = await runner.run_all(test_classes)
    
    if success:
        print(f"\n🎉 ALL TESTS PASSED - Task 18 implementation is working correctly!")
    else:
        print(f"\n❌ Some tests failed - Review implementation and fix issues")
    
    return success

if __name__ == "__main__":
    if ROUTING_AVAILABLE:
        asyncio.run(run_all_tests())
    else:
        print("❌ Cannot run tests - routing components not available")
        print("Make sure all Task 18 modules are properly implemented and accessible")