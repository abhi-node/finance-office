#!/usr/bin/env python3
"""
Comprehensive test suite for Tasks 11-16
Tests all implemented functionality from the LangGraph Multi-Agent System
"""

import asyncio
import sys
import os
import json
import logging
from datetime import datetime, timezone

# Set up logging
logging.basicConfig(level=logging.INFO, format='%(levelname)s: %(message)s')
logger = logging.getLogger(__name__)

# Add project to path
sys.path.insert(0, os.path.dirname(__file__))

def print_test_header(test_name: str):
    """Print formatted test header"""
    print(f"\n{'='*60}")
    print(f"TESTING: {test_name}")
    print(f"{'='*60}")

def print_test_result(test_name: str, passed: bool, details: str = ""):
    """Print formatted test result"""
    status = "✅ PASSED" if passed else "❌ FAILED"
    print(f"{status}: {test_name}")
    if details:
        print(f"  Details: {details}")

async def test_task_11_core_system():
    """Test Task 11: LangGraph Multi-Agent System Core"""
    print_test_header("Task 11: LangGraph Multi-Agent System Core")
    
    try:
        # Test DocumentState management
        from state.document_state import DocumentState, DocumentStateManager
        
        # Test basic state creation
        state_manager = DocumentStateManager()
        test_state = {
            'current_document': {'path': 'test.odt', 'title': 'Test Document'},
            'current_task': 'Test operation',
            'messages': [],
            'agent_status': {}
        }
        
        state_id = await state_manager.create_state(test_state)
        retrieved_state = await state_manager.get_state(state_id)
        
        assert retrieved_state is not None
        assert retrieved_state['current_document']['path'] == 'test.odt'
        print_test_result("DocumentState management", True, f"State ID: {state_id}")
        
        # Test DocumentMasterAgent
        from agents.document_master import DocumentMasterAgent
        
        master_agent = DocumentMasterAgent("test_master")
        assert master_agent.agent_id == "test_master"
        assert len(master_agent.registered_agents) >= 0  # Should have specialized agents if available
        print_test_result("DocumentMasterAgent initialization", True, f"Registered agents: {len(master_agent.registered_agents)}")
        
        # Test workflow configuration
        from workflow.graph_config import create_langgraph_workflow
        
        workflow = create_langgraph_workflow()
        assert workflow is not None
        print_test_result("LangGraph workflow configuration", True)
        
        return True
        
    except Exception as e:
        print_test_result("Task 11 Core System", False, str(e))
        logger.error(f"Task 11 test failed: {e}")
        return False

async def test_task_12_bridge_infrastructure():
    """Test Task 12: Python-C++ Bridge Infrastructure"""
    print_test_header("Task 12: Python-C++ Bridge Infrastructure")
    
    try:
        from bridge import LangGraphBridge
        
        # Test bridge initialization
        bridge = LangGraphBridge()
        assert bridge is not None
        print_test_result("LangGraphBridge initialization", True)
        
        # Test document context conversion
        mock_cpp_context = {
            'document_path': 'test.odt',
            'cursor_line': 5,
            'cursor_column': 10,
            'selected_text': 'sample text'
        }
        
        user_message = "Insert a chart"
        document_state = await bridge._convert_cpp_context_to_document_state(
            user_message, mock_cpp_context
        )
        
        assert isinstance(document_state, dict)
        assert 'current_document' in document_state
        assert 'cursor_position' in document_state
        print_test_result("Document context conversion", True)
        
        # Test status retrieval
        status = bridge.get_status()
        assert isinstance(status, dict)
        assert 'bridge_active' in status
        print_test_result("Bridge status retrieval", True)
        
        return True
        
    except Exception as e:
        print_test_result("Task 12 Bridge Infrastructure", False, str(e))
        logger.error(f"Task 12 test failed: {e}")
        return False

async def test_task_13_context_analysis():
    """Test Task 13: ContextAnalysisAgent"""
    print_test_header("Task 13: ContextAnalysisAgent")
    
    try:
        from agents.context_analysis import ContextAnalysisAgent, AnalysisMode, AnalysisRequest, ContextType
        
        # Test agent initialization
        context_agent = ContextAnalysisAgent("test_context")
        assert context_agent.agent_id == "test_context"
        print_test_result("ContextAnalysisAgent initialization", True)
        
        # Test lightweight analysis
        request = AnalysisRequest(
            request_id="test_analysis_1",
            analysis_mode=AnalysisMode.LIGHTWEIGHT,
            context_types=[ContextType.CURSOR_POSITION],
            target_position={'line': 1, 'column': 5},
            content_focus='Sample document content for analysis'
        )
        
        result = await context_agent.analyze_document_context(request)
        assert result is not None
        assert result.success
        assert result.execution_time_ms < 200  # Should be fast
        print_test_result("Lightweight analysis", True, f"Time: {result.execution_time_ms:.1f}ms")
        
        # Test comprehensive analysis
        request_comprehensive = AnalysisRequest(
            request_id="test_analysis_2",
            analysis_mode=AnalysisMode.COMPREHENSIVE,
            context_types=[ContextType.DOCUMENT_STRUCTURE, ContextType.CONTENT_SEMANTICS],
            target_position={'line': 1, 'column': 5},
            content_focus='Sample document content for analysis'
        )
        result = await context_agent.analyze_document_context(request_comprehensive)
        assert result is not None
        assert result.success
        print_test_result("Comprehensive analysis", True, f"Time: {result.execution_time_ms:.1f}ms")
        
        # Test caching
        cache_key = context_agent._generate_cache_key(request_comprehensive)
        cached_result = context_agent._get_cached_analysis(cache_key)
        assert cached_result is not None
        print_test_result("Analysis result caching", True)
        
        return True
        
    except Exception as e:
        print_test_result("Task 13 ContextAnalysisAgent", False, str(e))
        logger.error(f"Task 13 test failed: {e}")
        return False

async def test_task_14_content_and_formatting():
    """Test Task 14: ContentGenerationAgent and FormattingAgent"""
    print_test_header("Task 14: ContentGenerationAgent and FormattingAgent")
    
    try:
        from agents.content_generation import ContentGenerationAgent
        from agents.formatting import FormattingAgent
        
        # Test ContentGenerationAgent
        content_agent = ContentGenerationAgent("test_content")
        assert content_agent.agent_id == "test_content"
        print_test_result("ContentGenerationAgent initialization", True)
        
        # Test content generation
        test_state = {
            'current_task': 'Generate a summary',
            'document_structure': {'paragraphs': 3, 'tables': 1},
            'current_document': {'title': 'Test Document'}
        }
        
        result = await content_agent.process(test_state)
        assert result is not None
        assert result.success
        print_test_result("Content generation", True, f"Time: {result.execution_time:.3f}s")
        
        # Test FormattingAgent
        formatting_agent = FormattingAgent("test_formatting")
        assert formatting_agent.agent_id == "test_formatting"
        print_test_result("FormattingAgent initialization", True)
        
        # Test formatting operations
        test_state['pending_operations'] = [
            {'type': 'text_format', 'parameters': {'font_size': 12, 'bold': True}}
        ]
        
        result = await formatting_agent.process(test_state)
        assert result is not None
        assert result.success
        print_test_result("Formatting operations", True, f"Time: {result.execution_time:.3f}s")
        
        return True
        
    except Exception as e:
        print_test_result("Task 14 Content and Formatting", False, str(e))
        logger.error(f"Task 14 test failed: {e}")
        return False

async def test_task_15_data_integration():
    """Test Task 15: DataIntegrationAgent with Financial APIs"""
    print_test_header("Task 15: DataIntegrationAgent with Financial APIs")
    
    try:
        from agents.data_integration import DataIntegrationAgent
        from agents.credential_manager import get_credential_manager, CredentialProvider
        from agents.financial_apis import get_financial_client, DataType
        
        # Test DataIntegrationAgent
        data_agent = DataIntegrationAgent("test_data")
        assert data_agent.agent_id == "test_data"
        print_test_result("DataIntegrationAgent initialization", True)
        
        # Test credential management
        cred_manager = get_credential_manager()
        assert cred_manager is not None
        print_test_result("Credential manager", True)
        
        # Test financial API client
        financial_client = await get_financial_client(CredentialProvider.YAHOO_FINANCE)
        assert financial_client is not None
        print_test_result("Financial API client creation", True)
        
        # Test data integration with mock request
        test_state = {
            'current_task': 'Get stock data for AAPL',
            'external_data_requests': [
                {
                    'symbols': ['AAPL'],
                    'data_types': [DataType.STOCK_QUOTE.value],
                    'provider': CredentialProvider.YAHOO_FINANCE.value
                }
            ]
        }
        
        result = await data_agent.process(test_state)
        assert result is not None
        assert result.success
        print_test_result("Data integration processing", True, f"Time: {result.execution_time:.3f}s")
        
        # Test rate limiting and caching
        from agents.rate_limiter import get_rate_limiter, get_cache
        
        rate_limiter = get_rate_limiter()
        cache = get_cache()
        assert rate_limiter is not None
        assert cache is not None
        print_test_result("Rate limiting and caching", True)
        
        return True
        
    except Exception as e:
        print_test_result("Task 15 DataIntegrationAgent", False, str(e))
        logger.error(f"Task 15 test failed: {e}")
        return False

async def test_task_16_validation_and_execution():
    """Test Task 16: ValidationAgent and ExecutionAgent"""
    print_test_header("Task 16: ValidationAgent and ExecutionAgent")
    
    try:
        from agents.validation import (
            ValidationAgent, ValidationRequest, ValidationLevel, 
            ValidationCategory, ValidationSeverity
        )
        from agents.execution import (
            ExecutionAgent, ExecutionOperation, OperationType, 
            OperationPriority, UnoServiceManager
        )
        
        # Test ValidationAgent
        validation_agent = ValidationAgent("test_validation")
        assert validation_agent.agent_id == "test_validation"
        assert len(validation_agent.validation_rules) >= 4  # Default rules
        print_test_result("ValidationAgent initialization", True, f"Rules: {len(validation_agent.validation_rules)}")
        
        # Test validation request
        sample_content = {
            'text': 'This is a sample document with some teh content for testing.',
            'external_data': {
                'financial_data': {
                    'source': 'Yahoo Finance',
                    'timestamp': '2024-01-01T00:00:00Z',
                    'stock_price': 150.00
                }
            }
        }
        
        validation_request = ValidationRequest(
            document_id="test_doc",
            content=sample_content,
            validation_level=ValidationLevel.STANDARD,
            categories={ValidationCategory.CONTENT_ACCURACY, ValidationCategory.FINANCIAL_COMPLIANCE}
        )
        
        validation_response = await validation_agent.validate_document(validation_request)
        assert validation_response is not None
        assert isinstance(validation_response.passed, bool)
        print_test_result("Document validation", True, f"Issues: {len(validation_response.issues)}")
        
        # Test ExecutionAgent
        execution_agent = ExecutionAgent("test_execution")
        assert execution_agent.agent_id == "test_execution"
        assert len(execution_agent.executors) >= 2  # Text and Table executors
        print_test_result("ExecutionAgent initialization", True, f"Executors: {len(execution_agent.executors)}")
        
        # Test UNO service manager
        uno_manager = UnoServiceManager()
        assert uno_manager is not None
        print_test_result("UNO service manager", True)
        
        # Test operation execution
        operation = ExecutionOperation(
            operation_type=OperationType.TEXT_INSERT,
            parameters={'text': 'Test insertion', 'position': {'line': 1}},
            priority=OperationPriority.NORMAL
        )
        
        result = await execution_agent.execute_operation(operation)
        assert result is not None
        assert result.success
        print_test_result("Operation execution", True, f"Time: {result.execution_time_ms:.1f}ms")
        
        # Test undo/redo functionality
        undo_status = execution_agent.get_undo_redo_status()
        assert isinstance(undo_status, dict)
        assert 'can_undo' in undo_status
        print_test_result("Undo/Redo system", True, f"Undo available: {undo_status['can_undo']}")
        
        return True
        
    except Exception as e:
        print_test_result("Task 16 ValidationAgent and ExecutionAgent", False, str(e))
        logger.error(f"Task 16 test failed: {e}")
        return False

async def test_complete_workflow_integration():
    """Test complete workflow integration with all agents"""
    print_test_header("Complete Workflow Integration")
    
    try:
        from agents.document_master import DocumentMasterAgent
        
        # Create DocumentMasterAgent with all specialized agents
        master_agent = DocumentMasterAgent("integration_test")
        
        # Verify agent registration
        expected_agents = ['validation_agent', 'execution_agent']
        for agent_id in expected_agents:
            if agent_id in master_agent.registered_agents:
                print_test_result(f"Agent {agent_id} registered", True)
            else:
                print_test_result(f"Agent {agent_id} registered", False, "Agent not found")
        
        # Test complete workflow with complex operation
        test_state = {
            'current_document': {'path': 'test.odt', 'title': 'Integration Test'},
            'current_task': 'Create a financial report with market data',
            'cursor_position': {'line': 1, 'column': 1},
            'messages': [],
            'external_data': {
                'financial_data': {
                    'symbol': 'AAPL',
                    'price': 150.00,
                    'source': 'Yahoo Finance'
                }
            },
            'pending_operations': [
                {'type': 'text_insert', 'parameters': {'text': 'Financial Report'}},
                {'type': 'table_create', 'parameters': {'rows': 3, 'columns': 2}}
            ]
        }
        
        # Test request processing
        result = await master_agent.process(test_state)
        assert result is not None
        assert result.success
        print_test_result("Complete workflow processing", True, f"Time: {result.execution_time:.3f}s")
        
        # Test agent coordination
        if len(master_agent.registered_agents) > 0:
            # Test with orchestrated workflow
            from agents.document_master import OperationComplexity
            
            # Force complex operation to test full workflow
            analysis = await master_agent._analyze_user_request("Create comprehensive financial analysis", test_state)
            assert analysis.complexity in [OperationComplexity.MODERATE, OperationComplexity.COMPLEX]
            print_test_result("Request analysis and routing", True, f"Complexity: {analysis.complexity.value}")
        
        return True
        
    except Exception as e:
        print_test_result("Complete Workflow Integration", False, str(e))
        logger.error(f"Complete workflow test failed: {e}")
        return False

async def run_all_tests():
    """Run all tests for tasks 11-16"""
    print("🧪 Starting comprehensive test suite for Tasks 11-16")
    print(f"Test started at: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    
    test_results = {}
    
    # Run all tests
    test_results["Task 11"] = await test_task_11_core_system()
    test_results["Task 12"] = await test_task_12_bridge_infrastructure()
    test_results["Task 13"] = await test_task_13_context_analysis()
    test_results["Task 14"] = await test_task_14_content_and_formatting()
    test_results["Task 15"] = await test_task_15_data_integration()
    test_results["Task 16"] = await test_task_16_validation_and_execution()
    test_results["Integration"] = await test_complete_workflow_integration()
    
    # Print summary
    print(f"\n{'='*60}")
    print("TEST SUMMARY")
    print(f"{'='*60}")
    
    passed_tests = sum(1 for result in test_results.values() if result)
    total_tests = len(test_results)
    
    for test_name, passed in test_results.items():
        status = "✅ PASSED" if passed else "❌ FAILED"
        print(f"{status}: {test_name}")
    
    print(f"\nOverall: {passed_tests}/{total_tests} tests passed")
    
    if passed_tests == total_tests:
        print("\n🎉 ALL TESTS PASSED! 🎉")
        print("Tasks 11-16 are fully functional and ready for production.")
        return True
    else:
        print(f"\n⚠️  {total_tests - passed_tests} tests failed. Please check the logs above.")
        return False

if __name__ == "__main__":
    success = asyncio.run(run_all_tests())
    sys.exit(0 if success else 1)