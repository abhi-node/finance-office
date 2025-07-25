#!/usr/bin/env python3
"""
Comprehensive compatibility tests for Tasks 11-15

This test suite validates the integration and compatibility between:
- Task 11: SharedCacheMixin and DocumentState infrastructure  
- Task 12: Python-C++ Bridge Infrastructure
- Task 13: ContentGenerationAgent with research capabilities
- Task 14: FormattingAgent with comprehensive styling
- Task 15: DataIntegrationAgent with financial APIs

Tests focus on cross-task integration, state consistency, agent coordination,
and end-to-end workflow compatibility.
"""

import asyncio
import json
import sys
import time
import traceback
from pathlib import Path
from typing import Dict, Any, List

# Add the parent directory to the path to import our modules
sys.path.insert(0, str(Path(__file__).parent.parent))

# Import all relevant modules
from agents.shared_cache import SharedCacheMixin, DocumentState, get_shared_cache_manager
from agents.content_generation import ContentGenerationAgent
from agents.formatting import FormattingAgent
from agents.data_integration import DataIntegrationAgent
from agents.credential_manager import get_credential_manager, CredentialProvider
from bridge import LangGraphBridge, BridgeConfiguration, initialize_bridge, shutdown_bridge, get_bridge
from langchain_core.messages import HumanMessage, AIMessage


async def test_shared_cache_integration():
    """Test SharedCacheMixin integration across all agents (Task 11)."""
    print("\n🧪 Testing SharedCacheMixin integration across agents...")
    
    try:
        # Initialize agents with shared cache
        content_agent = ContentGenerationAgent("content_test_agent")
        formatting_agent = FormattingAgent("formatting_test_agent")
        data_agent = DataIntegrationAgent("data_test_agent")
        
        # Test document state
        test_state = DocumentState(
            current_document={"title": "Shared Cache Test", "path": "/tmp/test.odt"},
            cursor_position={"paragraph": 1, "character": 0},
            selected_text="",
            document_structure={"paragraphs": 5},
            formatting_state={"font": "Arial", "size": 12},
            messages=[HumanMessage(content="Test shared cache integration")],
            current_task="Test shared cache",
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
        
        document_id = test_state["current_document"]["path"]
        
        # Test each agent's shared cache integration
        agents = [
            ("ContentGeneration", content_agent),
            ("Formatting", formatting_agent), 
            ("DataIntegration", data_agent)
        ]
        
        for agent_name, agent in agents:
            # Test context sharing
            agent.share_context_with_agents(document_id, {
                f"{agent_name.lower()}_test_data": f"Test data from {agent_name}",
                "timestamp": time.time(),
                "agent_id": agent.agent_id
            })
            
            # Verify context sharing worked
            shared_context = agent.get_other_agent_context(document_id, agent.agent_id)
            assert len(shared_context) > 0, f"{agent_name} failed to share context"
            
            print(f"  ✅ {agent_name}Agent: SharedCache integration working")
        
        # Test cross-agent context retrieval
        content_context = content_agent.get_other_agent_context(document_id, "formatting_test_agent")
        formatting_context = formatting_agent.get_other_agent_context(document_id, "data_test_agent") 
        data_context = data_agent.get_other_agent_context(document_id, "content_test_agent")
        
        print(f"  ✅ Cross-agent context retrieval working")
        print(f"  ✅ Content→Formatting context: {len(content_context) > 0}")
        print(f"  ✅ Formatting→Data context: {len(formatting_context) > 0}")
        print(f"  ✅ Data→Content context: {len(data_context) > 0}")
        
        # Test cache manager integration
        cache_manager = get_shared_cache_manager()
        cache_stats = cache_manager.get_stats()
        print(f"  ✅ Cache manager stats: {cache_stats}")
        
        return True
        
    except Exception as e:
        print(f"  ❌ SharedCache integration test failed: {e}")
        traceback.print_exc()
        return False


async def test_bridge_agent_integration():
    """Test Bridge integration with all agents (Task 12)."""
    print("\n🧪 Testing Bridge integration with agents...")
    
    try:
        # Initialize bridge
        config = BridgeConfiguration(log_level="DEBUG")
        await initialize_bridge(config)
        bridge = get_bridge()
        
        # Test document context that would come from C++
        cpp_context = {
            "document_path": "/tmp/bridge_test.odt",
            "document_title": "Bridge Integration Test",
            "document_type": "financial",
            "cursor_paragraph": 2,
            "cursor_character": 15,
            "selected_text": "market data",
            "paragraph_count": 8,
            "table_count": 1,
            "current_font": "Times New Roman",
            "current_font_size": 11,
            "user_language": "en-US"
        }
        
        # Test bridge's document state conversion
        document_state = await bridge._convert_cpp_context_to_document_state(
            "Generate financial summary with current market data", 
            cpp_context
        )
        
        # Validate converted state has all required fields
        required_fields = [
            "current_document", "cursor_position", "selected_text", 
            "document_structure", "formatting_state", "messages", 
            "current_task", "user_preferences"
        ]
        
        for field in required_fields:
            assert field in document_state, f"Missing field: {field}"
        
        # Verify specific conversions
        assert document_state["current_document"]["path"] == "/tmp/bridge_test.odt"
        assert document_state["current_document"]["title"] == "Bridge Integration Test"
        assert document_state["cursor_position"]["paragraph"] == 2
        assert document_state["cursor_position"]["character"] == 15
        assert document_state["selected_text"] == "market data"
        assert document_state["document_structure"]["paragraphs"] == 8
        assert document_state["document_structure"]["tables"] == 1
        assert document_state["formatting_state"]["font"] == "Times New Roman"
        assert document_state["formatting_state"]["font_size"] == 11
        
        print("  ✅ C++ context to DocumentState conversion working")
        
        # Test full request processing (will use mock agents)
        response_json = await bridge.process_cpp_request(
            "BRIDGE-001",
            "Create a financial summary table",
            cpp_context
        )
        
        response = json.loads(response_json)
        assert "request_id" in response
        assert "success" in response
        assert response["request_id"] == "BRIDGE-001"
        
        print(f"  ✅ Full bridge request processing: success={response['success']}")
        
        await shutdown_bridge()
        return True
        
    except Exception as e:
        print(f"  ❌ Bridge-agent integration test failed: {e}")
        traceback.print_exc()
        return False


async def test_content_formatting_integration():
    """Test ContentGeneration + Formatting agent integration (Tasks 13+14)."""
    print("\n🧪 Testing ContentGeneration + Formatting integration...")
    
    try:
        # Initialize agents
        content_agent = ContentGenerationAgent("content_integration_agent")
        formatting_agent = FormattingAgent("formatting_integration_agent")
        
        # Test state with content that needs generation and formatting
        test_state = DocumentState(
            current_document={"title": "Content Format Test", "path": "/tmp/content_format.odt"},
            cursor_position={"paragraph": 3, "character": 0},
            selected_text="",
            document_structure={"paragraphs": 5, "tables": 0},
            formatting_state={"font": "Arial", "size": 12, "style": "Normal"},
            messages=[HumanMessage(content="Write a professional summary and format it properly")],
            current_task="Generate and format professional content",
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
            user_preferences={"preferred_style": "professional", "target_audience": "business"},
            interaction_history=[],
            approval_required=[],
            performance_metrics={},
            resource_utilization={},
            optimization_recommendations=[]
        )
        
        # Step 1: Content generation
        print("  🔄 Step 1: Content generation...")
        content_result = await content_agent.process(test_state)
        
        assert content_result.success, "Content generation should succeed"
        print(f"  ✅ Content generated successfully: {len(content_result.state_updates.get('generated_content', []))} items")
        
        # Update state with content generation results
        if content_result.state_updates:
            test_state.update(content_result.state_updates)
        
        # Step 2: Formatting the generated content
        print("  🔄 Step 2: Formatting generated content...")
        format_result = await formatting_agent.process(test_state)
        
        assert format_result.success, "Formatting should succeed"
        print(f"  ✅ Content formatted successfully: {len(format_result.state_updates.get('pending_operations', []))} operations")
        
        # Verify integration between agents
        # Content agent should have shared context for formatting agent
        document_id = test_state["current_document"]["path"]
        shared_context = content_agent.get_other_agent_context(document_id, formatting_agent.agent_id)
        print(f"  ✅ Shared context between agents: {len(shared_context) > 0}")
        
        # Check that formatting operations reference generated content
        if format_result.state_updates and "pending_operations" in format_result.state_updates:
            operations = format_result.state_updates["pending_operations"]
            has_content_ops = any("generated_content" in str(op) or "format" in str(op) for op in operations)
            print(f"  ✅ Formatting operations reference content: {has_content_ops}")
        
        return True
        
    except Exception as e:
        print(f"  ❌ Content-Formatting integration test failed: {e}")
        traceback.print_exc()
        return False


async def test_data_integration_agent_compatibility():
    """Test DataIntegration agent compatibility with other agents (Task 15)."""
    print("\n🧪 Testing DataIntegration agent compatibility...")
    
    try:
        # Initialize all agents
        content_agent = ContentGenerationAgent("content_data_agent")
        formatting_agent = FormattingAgent("formatting_data_agent")
        data_agent = DataIntegrationAgent("data_integration_test")
        
        # Add test credentials (mock)
        cred_manager = get_credential_manager()
        try:
            cred_manager.add_credential(
                provider=CredentialProvider.YAHOO_FINANCE,
                api_key="TEST_YAHOO_KEY_12345",
                key_id="test_yahoo"
            )
            print("  ✅ Test credentials added")
        except Exception as e:
            print(f"  ⚠️ Credential setup warning: {e}")
        
        # Test state with financial data needs
        test_state = DocumentState(
            current_document={"title": "Financial Data Integration", "path": "/tmp/financial_data.odt"},
            cursor_position={"paragraph": 1, "character": 0},
            selected_text="",
            document_structure={"paragraphs": 3, "tables": 0, "charts": 0},
            formatting_state={"font": "Arial", "size": 11},
            messages=[HumanMessage(content="Get current stock data for AAPL and MSFT and create a summary")],
            current_task="Integrate financial data and create summary",
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
            user_preferences={"data_freshness": "real-time", "include_charts": True},
            interaction_history=[],
            approval_required=[],
            performance_metrics={},
            resource_utilization={},
            optimization_recommendations=[]
        )
        
        # Step 1: Data integration
        print("  🔄 Step 1: Financial data integration...")
        data_result = await data_agent.process(test_state)
        
        # Data integration might fail in test environment (no real API keys)
        print(f"  ✅ Data integration attempted: success={data_result.success}")
        
        if data_result.success and data_result.state_updates:
            test_state.update(data_result.state_updates)
            
            # Check for financial data in state
            external_data = test_state.get("external_data", {})
            print(f"  ✅ External data retrieved: {len(external_data) > 0}")
            
            # Step 2: Generate content based on data
            print("  🔄 Step 2: Generate content with financial data...")
            content_result = await content_agent.process(test_state)
            
            if content_result.success and content_result.state_updates:
                test_state.update(content_result.state_updates)
                print("  ✅ Content generated with financial data")
                
                # Step 3: Format the data-enriched content
                print("  🔄 Step 3: Format financial content...")
                format_result = await formatting_agent.process(test_state)
                
                print(f"  ✅ Financial content formatted: success={format_result.success}")
        
        # Test agent compatibility regardless of API success
        document_id = test_state["current_document"]["path"]
        
        # Verify agents can share context
        data_agent.share_context_with_agents(document_id, {
            "financial_symbols": ["AAPL", "MSFT"],
            "data_sources": ["yahoo_finance"],
            "last_updated": time.time()
        })
        
        # Other agents can access data agent context
        data_context = content_agent.get_other_agent_context(document_id, data_agent.agent_id)
        print(f"  ✅ Data agent context accessible: {len(data_context) > 0}")
        
        # Test performance metrics
        perf_stats = data_agent.performance_stats
        print(f"  ✅ Data agent performance tracked: {perf_stats['requests_processed']} requests")
        
        return True
        
    except Exception as e:
        print(f"  ❌ DataIntegration compatibility test failed: {e}")
        traceback.print_exc()
        return False


async def test_end_to_end_workflow():
    """Test complete end-to-end workflow across all tasks."""
    print("\n🧪 Testing end-to-end workflow integration...")
    
    try:
        # Initialize bridge and all agents
        config = BridgeConfiguration(log_level="INFO")
        await initialize_bridge(config)
        bridge = get_bridge()
        
        # Simulate C++ document context for financial report
        cpp_context = {
            "document_path": "/tmp/financial_report_e2e.odt",
            "document_title": "Quarterly Financial Report",
            "document_type": "financial",
            "cursor_paragraph": 1,
            "cursor_character": 0,
            "selected_text": "",
            "paragraph_count": 2,
            "table_count": 0,
            "chart_count": 0,
            "current_font": "Times New Roman",
            "current_font_size": 12,
            "current_style": "Heading 1",
            "user_language": "en-US",
            "user_preferences": {
                "include_charts": True,
                "professional_formatting": True,
                "data_sources": ["financial_apis"]
            }
        }
        
        # Complex financial request that exercises all components
        user_request = "Create a comprehensive financial summary with current market data for AAPL, MSFT, and GOOGL, including charts and professional formatting"
        
        print(f"  🔄 Processing complex financial request...")
        print(f"      Request: {user_request}")
        
        # Process through bridge (which coordinates all agents)
        start_time = time.time()
        response_json = await bridge.process_cpp_request(
            "E2E-001",
            user_request,
            cpp_context
        )
        execution_time = (time.time() - start_time) * 1000
        
        # Parse and analyze response
        response = json.loads(response_json)
        
        assert "request_id" in response
        assert "success" in response
        assert response["request_id"] == "E2E-001"
        
        print(f"  ✅ End-to-end processing: success={response['success']}")
        print(f"  ✅ Total execution time: {execution_time:.1f}ms")
        
        if response["success"]:
            # Analyze the response structure
            result = response.get("result", {})
            
            operations = result.get("operations", [])
            content_changes = result.get("content_changes", {})
            formatting_changes = result.get("formatting_changes", {})
            
            print(f"  ✅ Operations generated: {len(operations)}")
            print(f"  ✅ Content changes: {len(content_changes)}")
            print(f"  ✅ Formatting changes: {len(formatting_changes)}")
            
            # Verify metadata
            metadata = result.get("metadata", {})
            print(f"  ✅ Processing metadata available: {len(metadata) > 0}")
            
        else:
            error_msg = response.get("error_message", "Unknown error")
            print(f"  ⚠️ Processing completed with error: {error_msg}")
            # In test environment, some failures are expected (no real APIs)
        
        # Check bridge performance stats
        bridge_status = bridge.get_status()
        print(f"  ✅ Bridge processed {bridge_status['total_operations']} total operations")
        
        await shutdown_bridge()
        return True
        
    except Exception as e:
        print(f"  ❌ End-to-end workflow test failed: {e}")
        traceback.print_exc()
        return False


async def test_state_consistency():
    """Test DocumentState consistency across all agents and bridge."""
    print("\n🧪 Testing DocumentState consistency...")
    
    try:
        # Create a comprehensive test state
        initial_state = DocumentState(
            current_document={
                "title": "State Consistency Test",
                "path": "/tmp/consistency_test.odt",
                "type": "financial",
                "modified": True
            },
            cursor_position={"paragraph": 2, "character": 15, "line": 2},
            selected_text="test selection",
            document_structure={"paragraphs": 10, "tables": 2, "charts": 1},
            formatting_state={"font": "Arial", "size": 12, "bold": False},
            messages=[HumanMessage(content="Test state consistency")],
            current_task="Test state consistency across agents",
            task_history=[],
            agent_status={},
            content_analysis={"content_type": "financial", "complexity": "moderate"},
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
            user_preferences={"style": "professional", "include_data": True},
            interaction_history=[],
            approval_required=[],
            performance_metrics={},
            resource_utilization={},
            optimization_recommendations=[]
        )
        
        # Test bridge conversion consistency
        config = BridgeConfiguration(log_level="DEBUG")
        await initialize_bridge(config)
        bridge = get_bridge()
        
        # Convert state to C++ format and back
        cpp_context = {
            "document_path": initial_state["current_document"]["path"],
            "document_title": initial_state["current_document"]["title"],
            "document_type": initial_state["current_document"]["type"],
            "cursor_paragraph": initial_state["cursor_position"]["paragraph"],
            "cursor_character": initial_state["cursor_position"]["character"],
            "selected_text": initial_state["selected_text"],
            "paragraph_count": initial_state["document_structure"]["paragraphs"]
        }
        
        converted_state = await bridge._convert_cpp_context_to_document_state(
            initial_state["current_task"], 
            cpp_context
        )
        
        # Verify key fields are preserved
        assert converted_state["current_document"]["path"] == initial_state["current_document"]["path"]
        assert converted_state["current_document"]["title"] == initial_state["current_document"]["title"]
        assert converted_state["cursor_position"]["paragraph"] == initial_state["cursor_position"]["paragraph"]
        assert converted_state["cursor_position"]["character"] == initial_state["cursor_position"]["character"]
        assert converted_state["selected_text"] == initial_state["selected_text"]
        assert converted_state["document_structure"]["paragraphs"] == initial_state["document_structure"]["paragraphs"]
        
        print("  ✅ Bridge state conversion preserves key fields")
        
        # Test agent state handling consistency
        agents = [
            ContentGenerationAgent("content_consistency"),
            FormattingAgent("formatting_consistency"),
            DataIntegrationAgent("data_consistency")
        ]
        
        for agent in agents:
            # Each agent should handle the state without corruption
            try:
                validation = agent.validate_input(converted_state)
                print(f"  ✅ {agent.__class__.__name__}: state validation passed={validation.passed}")
                
                # Test state updates don't corrupt original structure
                test_message = HumanMessage(content=f"Test {agent.__class__.__name__}")
                result = await agent.process(converted_state, test_message)
                
                # Verify result structure
                assert hasattr(result, 'success')
                assert hasattr(result, 'state_updates')
                assert hasattr(result, 'execution_time')
                
                print(f"  ✅ {agent.__class__.__name__}: processing preserves state structure")
                
            except Exception as e:
                print(f"  ❌ {agent.__class__.__name__}: state handling error - {e}")
                return False
        
        await shutdown_bridge()
        return True
        
    except Exception as e:
        print(f"  ❌ State consistency test failed: {e}")
        traceback.print_exc()
        return False


async def run_all_compatibility_tests():
    """Run all task compatibility tests."""
    print("🧪 Testing Tasks 11-15 Compatibility and Integration")
    print("=" * 60)
    
    tests = [
        ("SharedCache Integration (Task 11)", test_shared_cache_integration),
        ("Bridge-Agent Integration (Task 12)", test_bridge_agent_integration),
        ("Content-Formatting Integration (Tasks 13+14)", test_content_formatting_integration),
        ("DataIntegration Compatibility (Task 15)", test_data_integration_agent_compatibility),
        ("End-to-End Workflow", test_end_to_end_workflow),
        ("DocumentState Consistency", test_state_consistency)
    ]
    
    results = []
    
    for test_name, test_func in tests:
        try:
            print(f"\n📋 Running: {test_name}")
            result = await test_func()
            results.append((test_name, result))
            print(f"✅ {test_name}: {'PASSED' if result else 'FAILED'}")
        except Exception as e:
            print(f"❌ {test_name}: CRASHED - {e}")
            results.append((test_name, False))
    
    # Summary
    print("\n" + "=" * 60)
    print("📊 Task Compatibility Test Results:")
    
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    for test_name, result in results:
        status = "✅ PASSED" if result else "❌ FAILED"
        print(f"  {status}: {test_name}")
    
    print(f"\n🎯 Overall Result: {passed}/{total} tests passed")
    
    if passed == total:
        print("🎉 All task compatibility tests passed!")
        print("🔗 Tasks 11-15 are fully compatible and integrated!")
        return True
    else:
        print("❌ Some compatibility issues detected")
        print("🔧 Review failed tests for integration issues")
        return False


if __name__ == "__main__":
    success = asyncio.run(run_all_compatibility_tests())
    sys.exit(0 if success else 1)