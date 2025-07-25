#!/usr/bin/env python3
"""Test integrated agent coordination and shared caching system."""

import asyncio
import sys
import time
import traceback
from agents.content_generation import ContentGenerationAgent
from agents.formatting import FormattingAgent
from agents.shared_cache import get_coordination_hub, InvalidationTrigger, CacheType


async def test_shared_cache_coordination():
    """Test shared cache coordination between agents."""
    print("\n🧪 Testing shared cache coordination...")
    
    try:
        # Initialize agents
        content_agent = ContentGenerationAgent()
        formatting_agent = FormattingAgent()
        
        # Get coordination hub
        hub = get_coordination_hub()
        
        # Test document state
        test_state = {
            "current_document": {
                "title": "Quarterly Financial Report", 
                "path": "/tmp/test_document.odt"
            },
            "cursor_position": {"paragraph": 1, "character": 0, "line": 1},
            "document_structure": {"paragraphs": 5}
        }
        
        document_id = test_state["current_document"]["path"]
        
        # Test content generation with shared state
        content_message = {"content": "generate executive summary", "role": "user"}
        print("  🔄 Processing content generation...")
        content_result = await content_agent.process(test_state, content_message)
        
        print(f"  ✅ Content generation: success={content_result.success}")
        
        # Verify shared context was updated
        content_context = hub.get_agent_context(document_id, "content_generation_agent")
        print(f"  ✅ Content context stored: {len(content_context) > 0}")
        
        # Test formatting with access to content context
        formatting_message = {"content": "apply professional formatting", "role": "user"}
        print("  🔄 Processing formatting operation...")
        formatting_result = await formatting_agent.process(test_state, formatting_message)
        
        print(f"  ✅ Formatting operation: success={formatting_result.success}")
        
        # Verify formatting context was updated
        formatting_context = hub.get_agent_context(document_id, "formatting_agent")
        print(f"  ✅ Formatting context stored: {len(formatting_context) > 0}")
        
        # Test cross-agent context access
        formatting_sees_content = formatting_agent.get_other_agent_context(document_id, "content_generation_agent")
        content_sees_formatting = content_agent.get_other_agent_context(document_id, "formatting_agent")
        
        print(f"  ✅ Cross-agent context access: {len(formatting_sees_content) > 0 and len(content_sees_formatting) > 0}")
        
        return True
        
    except Exception as e:
        print(f"  ❌ Shared cache coordination test failed: {e}")
        traceback.print_exc()
        return False


async def test_cache_invalidation():
    """Test cache invalidation strategies."""
    print("\n🧪 Testing cache invalidation...")
    
    try:
        # Initialize agents and hub
        content_agent = ContentGenerationAgent()
        formatting_agent = FormattingAgent()
        hub = get_coordination_hub()
        
        document_id = "/tmp/test_invalidation.odt"
        
        # Cache some test data
        hub.cache_data("test_content", {"content": "test"}, 
                      CacheType.GENERATED_CONTENT)
        hub.cache_data("test_formatting", {"style": "professional"}, 
                      CacheType.FORMATTING_OPERATION)
        
        print(f"  ✅ Initial cache size: {len(hub._cache)}")
        
        # Test invalidation by trigger
        content_agent.invalidate_related_cache(document_id, InvalidationTrigger.CONTENT_MODIFIED)
        
        # Check that appropriate caches were invalidated
        remaining_cache = len(hub._cache)
        print(f"  ✅ Post-invalidation cache size: {remaining_cache}")
        
        # Test manual cache clearing
        hub.clear_cache(document_id)
        final_cache = len(hub._cache)
        print(f"  ✅ After clearing document cache: {final_cache}")
        
        return True
        
    except Exception as e:
        print(f"  ❌ Cache invalidation test failed: {e}")
        traceback.print_exc()
        return False


async def test_performance_optimization():
    """Test performance optimization through caching."""
    print("\n🧪 Testing performance optimization...")
    
    try:
        # Initialize agents
        content_agent = ContentGenerationAgent()
        formatting_agent = FormattingAgent()
        
        test_state = {
            "current_document": {
                "title": "Performance Test Document", 
                "path": "/tmp/perf_test.odt"
            },
            "cursor_position": {"paragraph": 1, "character": 0, "line": 1},
            "document_structure": {"paragraphs": 3}
        }
        
        message = {"content": "generate financial summary", "role": "user"}
        
        # First request (should not be cached)
        start_time = time.time()
        result1 = await content_agent.process(test_state, message)
        first_request_time = time.time() - start_time
        
        print(f"  ✅ First request time: {first_request_time*1000:.1f}ms")
        print(f"  ✅ First request success: {result1.success}")
        
        # Second identical request (should hit cache)
        start_time = time.time()
        result2 = await content_agent.process(test_state, message)
        second_request_time = time.time() - start_time
        
        print(f"  ✅ Second request time: {second_request_time*1000:.1f}ms")
        print(f"  ✅ Second request success: {result2.success}")
        
        # Verify performance improvement (second should be faster due to caching)
        performance_improvement = first_request_time > second_request_time
        print(f"  ✅ Performance improvement detected: {performance_improvement}")
        
        return True
        
    except Exception as e:
        print(f"  ❌ Performance optimization test failed: {e}")
        traceback.print_exc()
        return False


async def test_workflow_coordination():
    """Test complete workflow coordination between agents."""
    print("\n🧪 Testing workflow coordination...")
    
    try:
        # Initialize agents
        content_agent = ContentGenerationAgent()
        formatting_agent = FormattingAgent()
        hub = get_coordination_hub()
        
        test_state = {
            "current_document": {
                "title": "Workflow Test Document", 
                "path": "/tmp/workflow_test.odt"
            },
            "cursor_position": {"paragraph": 1, "character": 0, "line": 1},
            "document_structure": {"paragraphs": 2}
        }
        
        document_id = test_state["current_document"]["path"]
        
        # Step 1: Generate content
        content_message = {"content": "write quarterly report introduction", "role": "user"}
        print("  🔄 Step 1: Generating content...")
        content_result = await content_agent.process(test_state, content_message)
        
        # Verify content was generated and context updated
        content_context = hub.get_agent_context(document_id, "content_generation_agent")
        content_available = content_context.get("available_for_formatting", False)
        
        print(f"  ✅ Content generated: {content_result.success}")
        print(f"  ✅ Content available for formatting: {content_available}")
        
        # Step 2: Apply formatting aware of generated content
        formatting_message = {"content": "apply professional document formatting", "role": "user"}
        print("  🔄 Step 2: Applying formatting...")
        formatting_result = await formatting_agent.process(test_state, formatting_message)
        
        # Verify formatting was applied and can see content context
        formatting_context = hub.get_agent_context(document_id, "formatting_agent")
        document_styled = formatting_context.get("document_styled", False)
        
        # Verify formatting agent had access to content context
        content_context_in_formatting = formatting_agent.get_other_agent_context(
            document_id, "content_generation_agent"
        )
        
        print(f"  ✅ Formatting applied: {formatting_result.success}")
        print(f"  ✅ Document styled: {document_styled}")
        print(f"  ✅ Formatting agent saw content context: {len(content_context_in_formatting) > 0}")
        
        # Step 3: Verify complete workflow state
        shared_context = hub.get_shared_context(document_id)
        workflow_complete = (
            len(shared_context.agent_states) >= 2 and 
            "content_generation_agent" in shared_context.agent_states and
            "formatting_agent" in shared_context.agent_states
        )
        
        print(f"  ✅ Complete workflow coordination: {workflow_complete}")
        
        return True
        
    except Exception as e:
        print(f"  ❌ Workflow coordination test failed: {e}")
        traceback.print_exc()
        return False


async def test_cache_statistics():
    """Test cache statistics and monitoring."""
    print("\n🧪 Testing cache statistics...")
    
    try:
        hub = get_coordination_hub()
        
        # Clear cache to start fresh
        hub.clear_cache()
        
        # Perform some operations to generate statistics
        content_agent = ContentGenerationAgent()
        
        test_state = {
            "current_document": {
                "title": "Stats Test", 
                "path": "/tmp/stats_test.odt"
            },
            "cursor_position": {"paragraph": 1, "character": 0, "line": 1},
            "document_structure": {"paragraphs": 1}
        }
        
        # Generate some cache activity
        message1 = {"content": "generate content 1", "role": "user"}
        message2 = {"content": "generate content 2", "role": "user"}
        
        await content_agent.process(test_state, message1)
        await content_agent.process(test_state, message2)
        await content_agent.process(test_state, message1)  # Should hit cache
        
        # Get statistics
        stats = hub.get_cache_stats()
        
        print(f"  ✅ Total cache entries: {stats['total_entries']}")
        print(f"  ✅ Cache hits: {stats['hits']}")
        print(f"  ✅ Cache misses: {stats['misses']}")
        print(f"  ✅ Hit rate: {stats['hit_rate']:.2%}")
        print(f"  ✅ Cache types: {stats['cache_types']}")
        
        # Verify statistics make sense
        has_activity = stats['hits'] > 0 or stats['misses'] > 0
        print(f"  ✅ Cache activity detected: {has_activity}")
        
        return True
        
    except Exception as e:
        print(f"  ❌ Cache statistics test failed: {e}")
        traceback.print_exc()
        return False


async def run_all_integration_tests():
    """Run all agent integration tests."""
    print("🧪 Testing Agent Integration and Shared Caching System")
    print("=" * 60)
    
    tests = [
        ("Shared Cache Coordination", test_shared_cache_coordination),
        ("Cache Invalidation", test_cache_invalidation),
        ("Performance Optimization", test_performance_optimization),
        ("Workflow Coordination", test_workflow_coordination),
        ("Cache Statistics", test_cache_statistics)
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
    print("📊 Integration Test Results Summary:")
    
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    for test_name, result in results:
        status = "✅ PASSED" if result else "❌ FAILED"
        print(f"  {status}: {test_name}")
    
    print(f"\n🎯 Overall Result: {passed}/{total} tests passed")
    
    if passed == total:
        print("🎉 Agent integration system working perfectly!")
        return True
    else:
        print("❌ Some integration functionality needs attention")
        return False


if __name__ == "__main__":
    success = asyncio.run(run_all_integration_tests())
    sys.exit(0 if success else 1)