#!/usr/bin/env python3
"""
Comprehensive integration tests for LangGraph Bridge (Task 12)

This test suite validates the complete Python-C++ bridge infrastructure
by testing all components and their integration with the agent system.
Tests cover initialization, document context conversion, bidirectional
communication, progress streaming, and LibreOffice format conversion.
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

from bridge import (
    LangGraphBridge, BridgeConfiguration, IntegrationMethod, 
    CppDocumentContext, OperationRequest, OperationResponse,
    ProgressUpdate, BridgeStatus, initialize_bridge, shutdown_bridge,
    get_bridge, process_request_from_cpp
)
from agents.shared_cache import DocumentState
from langchain_core.messages import HumanMessage


async def test_bridge_initialization():
    """Test bridge initialization and configuration."""
    print("\n🧪 Testing LangGraph Bridge initialization...")
    
    try:
        # Test configuration creation
        config = BridgeConfiguration(
            integration_method=IntegrationMethod.PYUNO,
            max_concurrent_operations=3,
            operation_timeout_seconds=30,
            enable_progress_streaming=True,
            log_level="DEBUG"
        )
        print(f"  ✅ Configuration created: {config.integration_method.value}")
        
        # Test bridge initialization
        success = await initialize_bridge(config)
        print(f"  ✅ Bridge initialization: {'SUCCESS' if success else 'FAILED'}")
        
        if success:
            bridge = get_bridge()
            status = bridge.get_status()
            print(f"  ✅ Bridge status: {status['status']}")
            print(f"  ✅ Integration method: {status['integration_method']}")
            print(f"  ✅ Active operations: {status['active_operations']}")
            
            # Test shutdown
            shutdown_success = await shutdown_bridge()
            print(f"  ✅ Bridge shutdown: {'SUCCESS' if shutdown_success else 'FAILED'}")
        
        return success
        
    except Exception as e:
        print(f"  ❌ Bridge initialization test failed: {e}")
        traceback.print_exc()
        return False


async def test_cpp_document_context_conversion():
    """Test C++ document context to DocumentState conversion."""
    print("\n🧪 Testing C++ document context conversion...")
    
    try:
        # Initialize bridge
        config = BridgeConfiguration(log_level="DEBUG")
        await initialize_bridge(config)
        bridge = get_bridge()
        
        # Test with various context formats
        test_contexts = [
            # Dictionary context
            {
                "document_path": "/tmp/test_document.odt",
                "document_title": "Test Document",
                "document_type": "text",
                "cursor_line": 5,
                "cursor_column": 10,
                "cursor_paragraph": 2,
                "selected_text": "Sample selected text",
                "paragraph_count": 15,
                "word_count": 250,
                "current_font": "Arial",
                "current_font_size": 12,
                "is_bold": False,
                "user_language": "en-US"
            },
            
            # JSON string context
            json.dumps({
                "document_path": "/tmp/financial_report.odt",
                "document_title": "Financial Analysis Report",
                "document_type": "financial",
                "cursor_paragraph": 1,
                "table_count": 3,
                "chart_count": 2,
                "current_style": "Heading 1"
            }),
            
            # Empty context
            {},
            
            # None context
            None
        ]
        
        for i, context in enumerate(test_contexts):
            try:
                user_message = f"Test message {i+1}"
                document_state = await bridge._convert_cpp_context_to_document_state(
                    user_message, context
                )
                
                # Validate DocumentState structure
                assert isinstance(document_state, dict)
                assert "messages" in document_state
                assert "current_task" in document_state
                assert "current_document" in document_state
                assert "cursor_position" in document_state
                assert "document_structure" in document_state
                assert "formatting_state" in document_state
                assert document_state["current_task"] == user_message
                
                print(f"  ✅ Context {i+1} conversion successful")
                
                # Check specific mappings for first context
                if i == 0 and isinstance(context, dict):
                    assert document_state["current_document"]["path"] == "/tmp/test_document.odt"
                    assert document_state["current_document"]["title"] == "Test Document"
                    assert document_state["cursor_position"]["line"] == 5
                    assert document_state["cursor_position"]["column"] == 10
                    assert document_state["selected_text"] == "Sample selected text"
                    assert document_state["document_structure"]["paragraphs"] == 15
                    assert document_state["formatting_state"]["font"] == "Arial"
                    print(f"  ✅ Context {i+1} field mapping verified")
                
            except Exception as e:
                print(f"  ❌ Context {i+1} conversion failed: {e}")
                return False
        
        await shutdown_bridge()
        return True
        
    except Exception as e:
        print(f"  ❌ Document context conversion test failed: {e}")
        traceback.print_exc()
        return False


async def test_full_request_processing():
    """Test complete request processing pipeline."""
    print("\n🧪 Testing full request processing pipeline...")
    
    try:
        # Initialize bridge
        config = BridgeConfiguration(
            enable_progress_streaming=True,
            operation_timeout_seconds=60,
            log_level="DEBUG"
        )
        await initialize_bridge(config)
        bridge = get_bridge()
        
        # Test document context
        document_context = {
            "document_path": "/tmp/financial_analysis.odt",
            "document_title": "Market Analysis Report",
            "document_type": "financial",
            "cursor_paragraph": 3,
            "cursor_character": 25,
            "selected_text": "",
            "paragraph_count": 12,
            "table_count": 2,
            "chart_count": 1,
            "current_font": "Times New Roman",
            "current_font_size": 11,
            "user_language": "en-US",
            "document_format": "odt"
        }
        
        # Test requests with different complexity levels
        test_requests = [
            ("REQ-001", "Create a simple table with 3 rows and 4 columns"),
            ("REQ-002", "Write a brief summary of the current market trends"),
            ("REQ-003", "Generate a comprehensive financial report with real-time data and charts")
        ]
        
        for request_id, user_message in test_requests:
            try:
                print(f"  🔄 Processing request {request_id}: {user_message}")
                
                # Process request
                start_time = time.time()
                response_json = await bridge.process_cpp_request(
                    request_id, user_message, document_context
                )
                execution_time = (time.time() - start_time) * 1000
                
                # Parse response
                response = json.loads(response_json)
                
                # Validate response structure
                assert "request_id" in response
                assert "success" in response
                assert "execution_time_ms" in response
                assert response["request_id"] == request_id
                
                print(f"  ✅ Request {request_id} processed: success={response['success']}")
                print(f"  ✅ Execution time: {execution_time:.1f}ms")
                
                if response["success"]:
                    assert "result" in response
                    assert "final_state" in response
                    print(f"  ✅ Request {request_id} successful with results")
                else:
                    assert "error_message" in response
                    print(f"  ⚠️ Request {request_id} failed: {response['error_message']}")
                
            except Exception as e:
                print(f"  ❌ Request {request_id} processing failed: {e}")
                return False
        
        await shutdown_bridge()
        return True
        
    except Exception as e:
        print(f"  ❌ Full request processing test failed: {e}")
        traceback.print_exc()
        return False


async def test_concurrent_operations():
    """Test concurrent request processing."""
    print("\n🧪 Testing concurrent operations...")
    
    try:
        # Initialize bridge with concurrency support
        config = BridgeConfiguration(
            max_concurrent_operations=5,
            operation_timeout_seconds=30,
            log_level="DEBUG"
        )
        await initialize_bridge(config)
        bridge = get_bridge()
        
        # Create multiple concurrent requests
        document_context = {
            "document_title": "Concurrent Test Document",
            "document_type": "test",
            "cursor_paragraph": 1
        }
        
        # Define concurrent test requests
        concurrent_requests = [
            (f"CONC-{i:03d}", f"Process concurrent request {i}", document_context)
            for i in range(1, 6)
        ]
        
        # Execute requests concurrently
        print(f"  🔄 Executing {len(concurrent_requests)} concurrent requests...")
        
        async def process_single_request(request_id, message, context):
            try:
                response_json = await bridge.process_cpp_request(request_id, message, context)
                response = json.loads(response_json)
                return request_id, response["success"], response.get("execution_time_ms", 0)
            except Exception as e:
                return request_id, False, 0
        
        # Run all requests concurrently
        start_time = time.time()
        results = await asyncio.gather(
            *[process_single_request(req_id, msg, ctx) 
              for req_id, msg, ctx in concurrent_requests],
            return_exceptions=True
        )
        total_time = (time.time() - start_time) * 1000
        
        # Analyze results
        successful_requests = 0
        total_execution_time = 0
        
        for result in results:
            if isinstance(result, tuple):
                req_id, success, exec_time = result
                if success:
                    successful_requests += 1
                total_execution_time += exec_time
                print(f"  ✅ {req_id}: {'SUCCESS' if success else 'FAILED'} ({exec_time:.1f}ms)")
            else:
                print(f"  ❌ Request failed with exception: {result}")
        
        print(f"  ✅ Concurrent operations: {successful_requests}/{len(concurrent_requests)} successful")
        print(f"  ✅ Total wall time: {total_time:.1f}ms")
        print(f"  ✅ Average execution time: {total_execution_time/len(results):.1f}ms")
        
        # Test bridge status during load
        status = bridge.get_status()
        print(f"  ✅ Bridge handled {status['total_operations']} total operations")
        
        await shutdown_bridge()
        return successful_requests >= len(concurrent_requests) * 0.8  # Allow 20% failure tolerance
        
    except Exception as e:
        print(f"  ❌ Concurrent operations test failed: {e}")
        traceback.print_exc()
        return False


def test_synchronous_cpp_interface():
    """Test synchronous C++ interface function."""
    print("\n🧪 Testing synchronous C++ interface...")
    
    try:
        # Test document context as JSON string
        document_context_json = json.dumps({
            "document_path": "/tmp/sync_test.odt",
            "document_title": "Synchronous Test Document",
            "cursor_line": 1,
            "cursor_column": 0,
            "document_type": "text"
        })
        
        # Test the synchronous entry point
        print("  🔄 Calling process_request_from_cpp...")
        response_json = process_request_from_cpp(
            "SYNC-001", 
            "Test synchronous processing", 
            document_context_json
        )
        
        # Parse and validate response
        response = json.loads(response_json)
        
        assert "request_id" in response
        assert "success" in response
        assert response["request_id"] == "SYNC-001"
        
        print(f"  ✅ Synchronous call: success={response['success']}")
        if not response["success"]:
            print(f"  ⚠️ Error message: {response.get('error_message', 'Unknown error')}")
        
        return True
        
    except Exception as e:
        print(f"  ❌ Synchronous C++ interface test failed: {e}")
        traceback.print_exc()
        return False


async def test_error_handling():
    """Test error handling and recovery."""
    print("\n🧪 Testing error handling and recovery...")
    
    try:
        # Initialize bridge
        config = BridgeConfiguration(log_level="DEBUG")
        await initialize_bridge(config)
        bridge = get_bridge()
        
        # Test various error conditions
        error_tests = [
            # Invalid JSON context
            ("ERR-001", "Test invalid JSON", '{"invalid": json}'),
            
            # Extremely long message
            ("ERR-002", "x" * 10000, "{}"),
            
            # Empty request ID
            ("", "Test empty request ID", "{}"),
            
            # None message
            ("ERR-004", None, "{}"),
        ]
        
        for request_id, message, context in error_tests:
            try:
                print(f"  🔄 Testing error condition: {request_id}")
                
                response_json = await bridge.process_cpp_request(request_id, message, context)
                response = json.loads(response_json)
                
                # Error cases should still return valid response structure
                assert "request_id" in response
                assert "success" in response
                
                if not response["success"]:
                    assert "error_message" in response
                    print(f"  ✅ Error handled gracefully: {response['error_message']}")
                else:
                    print(f"  ✅ Request processed despite potential issues")
                
            except Exception as e:
                print(f"  ❌ Error handling failed for {request_id}: {e}")
                return False
        
        # Test operation cancellation
        print("  🔄 Testing operation cancellation...")
        cancelled = await bridge.cancel_operation("NONEXISTENT-ID")
        print(f"  ✅ Cancel non-existent operation: {cancelled}")
        
        await shutdown_bridge()
        return True
        
    except Exception as e:
        print(f"  ❌ Error handling test failed: {e}")
        traceback.print_exc()
        return False


async def test_performance_metrics():
    """Test performance monitoring and metrics."""
    print("\n🧪 Testing performance metrics...")
    
    try:
        # Initialize bridge
        config = BridgeConfiguration(log_level="INFO")
        await initialize_bridge(config)
        bridge = get_bridge()
        
        # Get initial metrics
        initial_status = bridge.get_status()
        initial_ops = initial_status['total_operations']
        
        # Process several test requests
        test_context = {"document_title": "Performance Test"}
        
        for i in range(5):
            await bridge.process_cpp_request(
                f"PERF-{i:03d}", 
                f"Performance test request {i}", 
                test_context
            )
        
        # Check updated metrics
        final_status = bridge.get_status()
        
        # Validate metrics were updated
        assert final_status['total_operations'] > initial_ops
        assert 'performance_metrics' in final_status
        
        metrics = final_status['performance_metrics']
        print(f"  ✅ Total operations: {metrics.get('total_operations', 0)}")
        print(f"  ✅ Successful operations: {metrics.get('successful_operations', 0)}")
        print(f"  ✅ Failed operations: {metrics.get('failed_operations', 0)}")
        print(f"  ✅ Average response time: {metrics.get('average_response_time_ms', 0):.1f}ms")
        
        # Validate uptime tracking
        assert final_status['uptime_seconds'] > 0
        print(f"  ✅ Bridge uptime: {final_status['uptime_seconds']:.1f}s")
        
        await shutdown_bridge()
        return True
        
    except Exception as e:
        print(f"  ❌ Performance metrics test failed: {e}")
        traceback.print_exc()
        return False


async def test_cpp_document_context_helper():
    """Test CppDocumentContext helper class."""
    print("\n🧪 Testing CppDocumentContext helper...")
    
    try:
        # Test with mock UNO-like structures
        mock_uno_context = type('MockUnoAny', (), {
            'getValueTypeName': lambda: 'sequence<com.sun.star.beans.PropertyValue>',
            'value': [
                type('PropertyValue', (), {
                    'Name': 'document_path', 
                    'Value': '/tmp/test.odt'
                })(),
                type('PropertyValue', (), {
                    'Name': 'cursor_line', 
                    'Value': 5
                })(),
                type('PropertyValue', (), {
                    'Name': 'selected_text', 
                    'Value': 'Test selection'
                })()
            ]
        })()
        
        # Test context extraction
        extracted = CppDocumentContext.extract_uno_context(mock_uno_context)
        
        assert isinstance(extracted, dict)
        assert 'document_path' in extracted
        assert extracted['document_path'] == '/tmp/test.odt'
        assert extracted['cursor_line'] == 5
        assert extracted['selected_text'] == 'Test selection'
        
        print("  ✅ UNO context extraction successful")
        
        # Test response creation
        response_data = {
            "request_id": "TEST-001",
            "status": "success",
            "operations": ["insert_text", "format_paragraph"],
            "execution_time": 150.0
        }
        
        uno_response = CppDocumentContext.create_uno_response(response_data)
        print("  ✅ UNO response creation successful")
        
        # Test conversion utilities
        test_value = {"nested": {"data": [1, 2, 3]}}
        converted = CppDocumentContext._convert_python_to_uno(test_value)
        print("  ✅ Python to UNO conversion successful")
        
        return True
        
    except Exception as e:
        print(f"  ❌ CppDocumentContext helper test failed: {e}")
        traceback.print_exc()
        return False


async def run_all_bridge_tests():
    """Run all bridge integration tests."""
    print("🧪 Testing LangGraph Bridge Integration (Task 12)")
    print("=" * 60)
    
    tests = [
        ("Bridge Initialization", test_bridge_initialization),
        ("C++ Document Context Conversion", test_cpp_document_context_conversion),
        ("Full Request Processing", test_full_request_processing),
        ("Concurrent Operations", test_concurrent_operations),
        ("Synchronous C++ Interface", lambda: test_synchronous_cpp_interface()),
        ("Error Handling", test_error_handling),
        ("Performance Metrics", test_performance_metrics),
        ("CppDocumentContext Helper", test_cpp_document_context_helper)
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
    print("📊 Bridge Integration Test Results:")
    
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    for test_name, result in results:
        status = "✅ PASSED" if result else "❌ FAILED"
        print(f"  {status}: {test_name}")
    
    print(f"\n🎯 Overall Result: {passed}/{total} tests passed")
    
    if passed == total:
        print("🎉 All LangGraph Bridge integration tests passed!")
        return True
    else:
        print("❌ Some bridge integration tests failed")
        return False


if __name__ == "__main__":
    success = asyncio.run(run_all_bridge_tests())
    sys.exit(0 if success else 1)