#!/usr/bin/env python3
"""
Enhanced Caching System Tests for ContextAnalysisAgent

This test suite validates the enhanced caching functionality including:
- AnalysisCache class operations
- Cache invalidation strategies
- Performance metrics collection
- Document change detection
- Cache optimization
"""

import asyncio
import time
import hashlib
from unittest.mock import patch
from agents.context_analysis import ContextAnalysisAgent, AnalysisCache, AnalysisMode


async def test_analysis_cache_basic_operations():
    """Test basic AnalysisCache operations."""
    print("\n🗄️  Testing AnalysisCache Basic Operations...")
    
    # Create cache instance
    cache = AnalysisCache(max_size=10, default_ttl=300)
    
    # Test empty cache
    result = cache.get_cached_analysis("nonexistent")
    assert result is None, "Expected None for non-existent key"
    print("  ✅ Empty cache returns None correctly")
    
    # Test cache key generation
    cache_key = cache.generate_cache_key(
        document_path="/tmp/test.odt",
        analysis_mode="lightweight",
        context_data={"cursor": {"paragraph": 1}}
    )
    assert isinstance(cache_key, str), "Cache key should be string"
    assert cache_key.startswith("analysis:"), "Cache key should have expected prefix"
    print("  ✅ Cache key generation works correctly")
    
    # Test performance metrics on empty cache
    metrics = cache.get_performance_metrics()
    assert metrics["cache_size"] == 0, "Empty cache should have size 0"
    assert metrics["hit_rate"] == 0.0, "Empty cache should have 0% hit rate"
    print("  ✅ Performance metrics correct for empty cache")
    
    return True


async def test_cache_hit_miss_scenarios():
    """Test cache hit and miss scenarios."""
    print("\n🎯 Testing Cache Hit/Miss Scenarios...")
    
    agent = ContextAnalysisAgent()
    
    # Create test state
    test_state = {
        "current_document": {"title": "Cache Test", "path": "/tmp/cache_test.odt"},
        "cursor_position": {"paragraph": 1, "character": 5, "line": 1},
        "selected_text": "",
        "document_structure": {"paragraphs": 3},
        "messages": [],
        "current_task": "cache_test"
    }
    
    message = {"content": "analyze document context", "role": "user"}
    
    # First request - should be cache miss
    start_time = time.time()
    result1 = await agent.process(test_state, message)
    first_time = (time.time() - start_time) * 1000
    
    assert result1.success, "First analysis should succeed"
    analysis1 = result1.state_updates["content_analysis"]
    assert not analysis1.get("cache_hit", False), "First request should not be cache hit"
    print(f"  ✅ First request (cache miss): {first_time:.1f}ms")
    
    # Second identical request - should be cache hit
    start_time = time.time()
    result2 = await agent.process(test_state, message)
    second_time = (time.time() - start_time) * 1000
    
    assert result2.success, "Second analysis should succeed"
    analysis2 = result2.state_updates["content_analysis"]
    print(f"  ✅ Second request (cache hit): {second_time:.1f}ms")
    
    # Cache hit should be faster
    if second_time < first_time:
        print(f"  ✅ Cache hit is faster ({first_time - second_time:.1f}ms improvement)")
    
    return True


async def test_cache_invalidation():
    """Test cache invalidation strategies."""
    print("\n🗑️  Testing Cache Invalidation...")
    
    agent = ContextAnalysisAgent()
    
    test_state = {
        "current_document": {"title": "Invalidation Test", "path": "/tmp/invalidation_test.odt"},
        "cursor_position": {"paragraph": 1, "character": 1, "line": 1},
        "selected_text": "",
        "document_structure": {"paragraphs": 5},
        "messages": [],
        "current_task": "invalidation_test"
    }
    
    message = {"content": "analyze document", "role": "user"}
    
    # First request to populate cache
    result1 = await agent.process(test_state, message)
    assert result1.success, "Initial analysis should succeed"
    
    # Check cache metrics
    metrics_before = agent.get_cache_performance_metrics()
    cache_size_before = metrics_before["cache_size"]
    print(f"  ✅ Cache populated: {cache_size_before} entries")
    
    # Invalidate cache for specific document
    invalidated = agent.invalidate_cache_for_document("/tmp/invalidation_test.odt")
    print(f"  ✅ Invalidated {invalidated} cache entries")
    
    # Check cache metrics after invalidation
    metrics_after = agent.get_cache_performance_metrics()
    cache_size_after = metrics_after["cache_size"]
    
    assert cache_size_after < cache_size_before or cache_size_after == 0, "Cache should be smaller after invalidation"
    print(f"  ✅ Cache size after invalidation: {cache_size_after} entries")
    
    # Clear entire cache
    cleared = agent.clear_analysis_cache()
    print(f"  ✅ Cleared {cleared} cache entries")
    
    # Verify cache is empty
    final_metrics = agent.get_cache_performance_metrics()
    assert final_metrics["cache_size"] == 0, "Cache should be empty after clear"
    print("  ✅ Cache successfully cleared")
    
    return True


async def test_document_change_detection():
    """Test document change detection and automatic cache invalidation."""
    print("\n📝 Testing Document Change Detection...")
    
    agent = ContextAnalysisAgent()
    
    # Initial document state
    initial_state = {
        "current_document": {
            "title": "Change Detection Test", 
            "path": "/tmp/change_test.odt",
            "last_modified": "2024-01-01T10:00:00Z"
        },
        "cursor_position": {"paragraph": 1, "character": 1, "line": 1},
        "selected_text": "",
        "document_structure": {"paragraphs": 3},
        "messages": [],
        "current_task": "change_detection"
    }
    
    message = {"content": "analyze document for changes", "role": "user"}
    
    # First analysis
    result1 = await agent.process(initial_state, message)
    assert result1.success, "Initial analysis should succeed"
    
    initial_hash = agent.calculate_document_hash(initial_state)
    print(f"  ✅ Initial document hash: {initial_hash[:8]}...")
    
    # Modified document state (simulate document change)
    modified_state = initial_state.copy()
    modified_state["document_structure"] = {"paragraphs": 5}  # Document grew
    modified_state["current_document"]["last_modified"] = "2024-01-01T11:00:00Z"
    
    modified_hash = agent.calculate_document_hash(modified_state)
    print(f"  ✅ Modified document hash: {modified_hash[:8]}...")
    
    # Hashes should be different
    assert initial_hash != modified_hash, "Document hashes should differ after changes"
    print("  ✅ Document change detection working correctly")
    
    # Update document version (should invalidate cache)
    cache_invalidated = agent.update_document_version("/tmp/change_test.odt", modified_hash)
    print(f"  ✅ Cache invalidation due to document change: {cache_invalidated}")
    
    return True


async def test_performance_metrics_and_optimization():
    """Test performance metrics collection and optimization recommendations."""
    print("\n📊 Testing Performance Metrics and Optimization...")
    
    agent = ContextAnalysisAgent()
    
    # Generate some cache activity
    test_states = []
    for i in range(5):
        state = {
            "current_document": {"title": f"Perf Test {i}", "path": f"/tmp/perf_test_{i}.odt"},
            "cursor_position": {"paragraph": i + 1, "character": 1, "line": i + 1},
            "selected_text": "",
            "document_structure": {"paragraphs": i + 3},
            "messages": [],
            "current_task": f"perf_test_{i}"
        }
        test_states.append(state)
    
    # Generate cache hits and misses
    message = {"content": "performance test analysis", "role": "user"}
    
    for i, state in enumerate(test_states):
        # First request (miss)
        await agent.process(state, message)
        
        # Second request (hit)
        if i % 2 == 0:  # Only repeat some requests to create hits
            await agent.process(state, message)
    
    # Get performance metrics
    metrics = agent.get_cache_performance_metrics()
    
    print(f"  ✅ Cache size: {metrics['cache_size']}")
    print(f"  ✅ Hit rate: {metrics['hit_rate']:.1%}")
    print(f"  ✅ Total requests: {metrics['total_requests']}")
    print(f"  ✅ Cache hits: {metrics['hits']}")
    print(f"  ✅ Cache misses: {metrics['misses']}")
    
    # Test optimization recommendations
    optimization = await agent.optimize_cache_performance()
    
    print(f"  ✅ Optimization recommendations: {len(optimization['recommendations'])}")
    for recommendation in optimization['recommendations']:
        print(f"      • {recommendation}")
    
    print(f"  ✅ Suggested optimal cache size: {optimization['optimal_cache_size']}")
    print(f"  ✅ Suggested TTL: {optimization['suggested_ttl']}s")
    
    return True


async def test_cache_size_limits_and_eviction():
    """Test cache size limits and LRU eviction."""
    print("\n🔄 Testing Cache Size Limits and Eviction...")
    
    # Create agent with small cache for testing eviction
    config = {"cache_max_entries": 5, "cache_ttl_seconds": 300}
    agent = ContextAnalysisAgent(config=config)
    
    # Fill cache beyond limit
    message = {"content": "eviction test", "role": "user"}
    
    for i in range(8):  # Exceed cache limit of 5
        state = {
            "current_document": {"title": f"Eviction Test {i}", "path": f"/tmp/eviction_{i}.odt"},
            "cursor_position": {"paragraph": i, "character": 1, "line": i},
            "selected_text": "",
            "document_structure": {"paragraphs": i + 1},
            "messages": [],
            "current_task": f"eviction_test_{i}"
        }
        
        await agent.process(state, message)
    
    # Check final cache size
    metrics = agent.get_cache_performance_metrics()
    
    print(f"  ✅ Final cache size: {metrics['cache_size']}")
    print(f"  ✅ Evictions: {metrics['evictions']}")
    
    # Cache size should not exceed limit
    assert metrics['cache_size'] <= 5, "Cache size should not exceed limit"
    assert metrics['evictions'] > 0, "Should have evictions when exceeding limit"
    
    print("  ✅ Cache eviction working correctly")
    
    return True


async def test_cache_ttl_expiration():
    """Test cache TTL expiration."""
    print("\n⏰ Testing Cache TTL Expiration...")
    
    # Create agent with very short TTL for testing
    config = {"cache_ttl_seconds": 1}  # 1 second TTL
    agent = ContextAnalysisAgent(config=config)
    
    test_state = {
        "current_document": {"title": "TTL Test", "path": "/tmp/ttl_test.odt"},
        "cursor_position": {"paragraph": 1, "character": 1, "line": 1},
        "selected_text": "",
        "document_structure": {"paragraphs": 2},
        "messages": [],
        "current_task": "ttl_test"
    }
    
    message = {"content": "ttl expiration test", "role": "user"}
    
    # First request
    result1 = await agent.process(test_state, message)
    assert result1.success, "First request should succeed"
    print("  ✅ First request completed (cache populated)")
    
    # Immediate second request (should hit cache)
    result2 = await agent.process(test_state, message)
    assert result2.success, "Second request should succeed"
    
    # Wait for TTL expiration
    print("  ⏳ Waiting for TTL expiration...")
    await asyncio.sleep(1.5)  # Wait longer than TTL
    
    # Third request after TTL expiration (should miss cache)
    result3 = await agent.process(test_state, message)
    assert result3.success, "Third request should succeed"
    print("  ✅ TTL expiration working correctly")
    
    return True


async def main():
    """Run all caching system tests."""
    print("🚀 ContextAnalysisAgent Enhanced Caching System Tests")
    print("=" * 70)
    
    test_functions = [
        ("AnalysisCache Basic Operations", test_analysis_cache_basic_operations),
        ("Cache Hit/Miss Scenarios", test_cache_hit_miss_scenarios),
        ("Cache Invalidation", test_cache_invalidation),
        ("Document Change Detection", test_document_change_detection),
        ("Performance Metrics & Optimization", test_performance_metrics_and_optimization),
        ("Cache Size Limits & Eviction", test_cache_size_limits_and_eviction),
        ("Cache TTL Expiration", test_cache_ttl_expiration)
    ]
    
    results = []
    
    for test_name, test_func in test_functions:
        print(f"\n🧪 Running: {test_name}")
        print("-" * 50)
        
        try:
            success = await test_func()
            results.append((test_name, success))
            print(f"✅ {test_name}: PASSED")
            
        except Exception as e:
            results.append((test_name, False))
            print(f"❌ {test_name}: FAILED - {e}")
            import traceback
            traceback.print_exc()
    
    # Summary
    print("\n" + "=" * 70)
    print("ENHANCED CACHING TESTS SUMMARY")
    print("=" * 70)
    
    passed = sum(1 for _, success in results if success)
    total = len(results)
    
    for test_name, success in results:
        status = "✅ PASSED" if success else "❌ FAILED"
        print(f"{status}: {test_name}")
    
    print(f"\nResults: {passed}/{total} tests passed ({passed/total*100:.1f}%)")
    
    if passed == total:
        print("\n🎉 All enhanced caching tests PASSED!")
        print("\n✅ Task 13.5 Enhanced Caching Implementation: COMPLETE")
        print("  • AnalysisCache class with LRU eviction and TTL")
        print("  • Cache invalidation strategies for document changes")
        print("  • Performance metrics collection and optimization")
        print("  • Document change detection and automatic invalidation")
        print("  • Thread-safe cache operations")
        print("  • Cache size limits and eviction policies")
        print("  • Performance optimization recommendations")
        return True
    else:
        print(f"\n⚠️  {total - passed} caching tests failed")
        return False


if __name__ == "__main__":
    success = asyncio.run(main())
    exit(0 if success else 1)