#!/usr/bin/env python3
"""Test FormattingAgent core functionality and formatting capabilities."""

import asyncio
import sys
import traceback
from agents.formatting import (
    FormattingAgent, 
    FormattingType, 
    FormattingComplexity
)

async def test_basic_initialization():
    """Test basic FormattingAgent initialization."""
    print("\n🧪 Testing FormattingAgent initialization...")
    
    try:
        # Create agent instance
        agent = FormattingAgent()
        print(f"  ✅ Agent initialized: {agent.agent_id}")
        print(f"  ✅ Capabilities: {[cap.value for cap in agent.capabilities]}")
        print(f"  ✅ Required tools: {agent.get_required_tools()}")
        
        # Check configuration
        assert agent.supported_formats is not None
        assert agent.performance_targets is not None
        assert agent.financial_templates is not None
        print(f"  ✅ Configuration loaded successfully")
        
        return True
        
    except Exception as e:
        print(f"  ❌ Initialization test failed: {e}")
        traceback.print_exc()
        return False

async def test_input_validation():
    """Test input validation functionality."""
    print("\n🧪 Testing input validation...")
    
    try:
        agent = FormattingAgent()
        
        # Test valid state
        valid_state = {
            "current_document": {
                "title": "Financial Report Q4 2024", 
                "path": "/tmp/financial_report.odt"
            },
            "cursor_position": {"paragraph": 2, "character": 10, "line": 2},
            "selected_text": "quarterly revenue increased by 25%",
            "document_structure": {
                "paragraphs": 10,
                "sections": [
                    {"title": "Executive Summary", "start_paragraph": 0, "end_paragraph": 2},
                    {"title": "Financial Results", "start_paragraph": 3, "end_paragraph": 6}
                ]
            }
        }
        
        # Test validation
        validation = agent.validate_input(valid_state)
        print(f"  ✅ Valid state validation: passed={validation.passed}")
        assert validation.passed == True
        
        # Test invalid state
        invalid_state = {}
        validation = agent.validate_input(invalid_state)
        print(f"  ✅ Invalid state validation: passed={validation.passed}")
        assert validation.passed == False
        
        return True
        
    except Exception as e:
        print(f"  ❌ Input validation test failed: {e}")
        traceback.print_exc()
        return False

async def test_text_formatting():
    """Test text formatting capabilities."""
    print("\n🧪 Testing text formatting...")
    
    try:
        agent = FormattingAgent()
        
        # Test state with formatting request
        test_state = {
            "current_document": {
                "title": "Business Document", 
                "path": "/tmp/business_doc.odt"
            },
            "cursor_position": {"paragraph": 1, "character": 5, "line": 1},
            "selected_text": "important text",
            "document_structure": {"paragraphs": 5}
        }
        
        # Test text formatting message
        message = {
            "content": "make the selected text bold and underlined", 
            "role": "user"
        }
        
        print("  🔄 Processing text formatting request...")
        result = await agent.process(test_state, message)
        
        print(f"  ✅ Text formatting: success={result.success}")
        print(f"  ✅ Execution time: {result.execution_time*1000:.1f}ms")
        
        if result.success:
            formatting_status = result.state_updates.get("formatting_status", {})
            print(f"  ✅ Formatting type: {formatting_status.get('formatting_type', 'unknown')}")
            print(f"  ✅ Validation status: {formatting_status.get('validation_status', 'unknown')}")
            
            operations = result.state_updates.get("formatting_operations", [])
            if operations:
                operation = operations[0]
                print(f"  ✅ Operation type: {operation.get('operation_type', 'unknown')}")
                attrs = operation.get('formatting_attributes', {})
                print(f"  ✅ Bold applied: {attrs.get('CharWeight', 400) > 400}")
                print(f"  ✅ Underline applied: {attrs.get('CharUnderline', 0) > 0}")
        else:
            print(f"  ❌ Formatting failed: {result.error}")
        
        return result.success
        
    except Exception as e:
        print(f"  ❌ Text formatting test failed: {e}")
        traceback.print_exc()
        return False

async def test_table_creation():
    """Test table creation and formatting."""
    print("\n🧪 Testing table creation...")
    
    try:
        agent = FormattingAgent()
        
        test_state = {
            "current_document": {
                "title": "Financial Analysis Report", 
                "path": "/tmp/financial_analysis.odt"
            },
            "cursor_position": {"paragraph": 3, "character": 0, "line": 3},
            "document_structure": {"paragraphs": 8}
        }
        
        # Test table creation message
        message = {
            "content": "create a table with 4 rows and 3 columns for financial data", 
            "role": "user"
        }
        
        print("  🔄 Processing table creation request...")
        result = await agent.process(test_state, message)
        
        print(f"  ✅ Table creation: success={result.success}")
        print(f"  ✅ Execution time: {result.execution_time*1000:.1f}ms")
        
        if result.success:
            formatting_status = result.state_updates.get("formatting_status", {})
            print(f"  ✅ Formatting type: {formatting_status.get('formatting_type', 'unknown')}")
            
            operations = result.state_updates.get("formatting_operations", [])
            if operations:
                operation = operations[0]
                print(f"  ✅ Operation type: {operation.get('operation_type', 'unknown')}")
                dims = operation.get('table_dimensions', {})
                print(f"  ✅ Table dimensions: {dims.get('rows', 0)}x{dims.get('cols', 0)}")
                
                attrs = operation.get('formatting_attributes', {})
                print(f"  ✅ Professional styling: {attrs.get('TableStyle', 'none')}")
        else:
            print(f"  ❌ Table creation failed: {result.error}")
        
        return result.success
        
    except Exception as e:
        print(f"  ❌ Table creation test failed: {e}")
        traceback.print_exc()
        return False

async def test_chart_formatting():
    """Test chart creation and formatting."""
    print("\n🧪 Testing chart formatting...")
    
    try:
        agent = FormattingAgent()
        
        test_state = {
            "current_document": {
                "title": "Quarterly Performance Report", 
                "path": "/tmp/performance_report.odt"
            },
            "cursor_position": {"paragraph": 5, "character": 0, "line": 5},
            "document_structure": {"paragraphs": 12}
        }
        
        # Test chart creation message
        message = {
            "content": "create a bar chart showing quarterly revenue growth", 
            "role": "user"
        }
        
        print("  🔄 Processing chart creation request...")
        result = await agent.process(test_state, message)
        
        print(f"  ✅ Chart creation: success={result.success}")
        print(f"  ✅ Execution time: {result.execution_time*1000:.1f}ms")
        
        if result.success:
            formatting_status = result.state_updates.get("formatting_status", {})
            print(f"  ✅ Formatting type: {formatting_status.get('formatting_type', 'unknown')}")
            
            operations = result.state_updates.get("formatting_operations", [])
            if operations:
                operation = operations[0]
                print(f"  ✅ Operation type: {operation.get('operation_type', 'unknown')}")
                specs = operation.get('chart_specifications', {})
                print(f"  ✅ Chart type: {specs.get('ChartType', 'unknown')}")
                print(f"  ✅ Professional styling: {operation.get('formatting_attributes', {}).get('ProfessionalStyling', False)}")
        else:
            print(f"  ❌ Chart creation failed: {result.error}")
        
        return result.success
        
    except Exception as e:
        print(f"  ❌ Chart formatting test failed: {e}")
        traceback.print_exc()
        return False

async def test_financial_template():
    """Test financial document template application."""
    print("\n🧪 Testing financial template application...")
    
    try:
        agent = FormattingAgent()
        
        test_state = {
            "current_document": {
                "title": "Q4 2024 Financial Report", 
                "path": "/tmp/q4_report.odt"
            },
            "cursor_position": {"paragraph": 0, "character": 0, "line": 0},
            "document_structure": {"paragraphs": 1}
        }
        
        # Test financial template message
        message = {
            "content": "apply quarterly financial report template", 
            "role": "user"
        }
        
        print("  🔄 Processing financial template request...")
        result = await agent.process(test_state, message)
        
        print(f"  ✅ Template application: success={result.success}")
        print(f"  ✅ Execution time: {result.execution_time*1000:.1f}ms")
        
        if result.success:
            formatting_status = result.state_updates.get("formatting_status", {})
            print(f"  ✅ Formatting type: {formatting_status.get('formatting_type', 'unknown')}")
            
            operations = result.state_updates.get("formatting_operations", [])
            if operations:
                operation = operations[0]
                print(f"  ✅ Operation type: {operation.get('operation_type', 'unknown')}")
                specs = operation.get('template_specifications', {})
                print(f"  ✅ Template type: {specs.get('TemplateType', 'unknown')}")
                print(f"  ✅ Compliance level: {specs.get('ComplianceLevel', 'unknown')}")
        else:
            print(f"  ❌ Template application failed: {result.error}")
        
        return result.success
        
    except Exception as e:
        print(f"  ❌ Financial template test failed: {e}")
        traceback.print_exc()
        return False

async def test_caching_system():
    """Test formatting operation caching."""
    print("\n🧪 Testing caching system...")
    
    try:
        agent = FormattingAgent()
        
        test_state = {
            "current_document": {
                "title": "Test Document", 
                "path": "/tmp/test_doc.odt"
            },
            "cursor_position": {"paragraph": 1, "character": 0, "line": 1},
            "selected_text": "test text",
            "document_structure": {"paragraphs": 3}
        }
        
        message = {"content": "make text bold", "role": "user"}
        
        # First request (should not be cached)
        print("  🔄 First formatting request...")
        result1 = await agent.process(test_state, message)
        cache_used_1 = result1.metadata.get("cache_used", False)
        print(f"  ✅ First request cache used: {cache_used_1}")
        
        # Second identical request (should be cached)
        print("  🔄 Second identical request...")
        result2 = await agent.process(test_state, message)
        cache_used_2 = result2.metadata.get("cache_used", False)
        print(f"  ✅ Second request cache used: {cache_used_2}")
        
        # Verify caching worked
        success = result1.success and result2.success and not cache_used_1 and cache_used_2
        print(f"  ✅ Caching system working: {success}")
        
        return success
        
    except Exception as e:
        print(f"  ❌ Caching test failed: {e}")
        traceback.print_exc()
        return False

async def test_performance_targets():
    """Test that formatting operations meet performance targets."""
    print("\n🧪 Testing performance targets...")
    
    try:
        agent = FormattingAgent()
        
        test_cases = [
            {
                "name": "Simple formatting",
                "message": {"content": "make text bold", "role": "user"},
                "expected_complexity": FormattingComplexity.SIMPLE,
                "target_ms": 500
            },
            {
                "name": "Standard table",
                "message": {"content": "create a table", "role": "user"},
                "expected_complexity": FormattingComplexity.STANDARD,
                "target_ms": 1500
            },
            {
                "name": "Complex layout",
                "message": {"content": "optimize document layout comprehensively", "role": "user"},
                "expected_complexity": FormattingComplexity.COMPLEX,
                "target_ms": 3000
            }
        ]
        
        test_state = {
            "current_document": {"title": "Performance Test", "path": "/tmp/perf_test.odt"},
            "cursor_position": {"paragraph": 1, "character": 0, "line": 1},
            "document_structure": {"paragraphs": 5}
        }
        
        all_passed = True
        for test_case in test_cases:
            print(f"  🔄 Testing {test_case['name']}...")
            result = await agent.process(test_state, test_case["message"])
            
            if result.success:
                actual_ms = result.execution_time * 1000
                target_ms = test_case["target_ms"]
                performance_met = actual_ms <= target_ms
                
                print(f"    ✅ Execution time: {actual_ms:.1f}ms (target: {target_ms}ms)")
                print(f"    {'✅' if performance_met else '❌'} Performance target met: {performance_met}")
                
                if not performance_met:
                    all_passed = False
            else:
                print(f"    ❌ Test failed: {result.error}")
                all_passed = False
        
        print(f"  ✅ Overall performance: {'All targets met' if all_passed else 'Some targets missed'}")
        return all_passed
        
    except Exception as e:
        print(f"  ❌ Performance test failed: {e}")
        traceback.print_exc()
        return False

async def run_all_tests():
    """Run all FormattingAgent tests."""
    print("🧪 Testing FormattingAgent Core Functionality")
    print("=" * 60)
    
    tests = [
        ("Basic Initialization", test_basic_initialization),
        ("Input Validation", test_input_validation),
        ("Text Formatting", test_text_formatting),
        ("Table Creation", test_table_creation),
        ("Chart Formatting", test_chart_formatting),
        ("Financial Templates", test_financial_template),
        ("Caching System", test_caching_system),
        ("Performance Targets", test_performance_targets)
    ]
    
    results = []
    for test_name, test_func in tests:
        try:
            result = await test_func()
            results.append((test_name, result))
        except Exception as e:
            print(f"❌ {test_name} test crashed: {e}")
            results.append((test_name, False))
    
    # Summary
    print("\n" + "=" * 60)
    print("📊 Test Results Summary:")
    
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    for test_name, result in results:
        status = "✅ PASSED" if result else "❌ FAILED"
        print(f"  {status}: {test_name}")
    
    print(f"\n🎯 Overall Result: {passed}/{total} tests passed")
    
    if passed == total:
        print("🎉 FormattingAgent core structure working perfectly!")
        return True
    else:
        print("❌ Some FormattingAgent functionality needs attention")
        return False

if __name__ == "__main__":
    success = asyncio.run(run_all_tests())
    sys.exit(0 if success else 1)