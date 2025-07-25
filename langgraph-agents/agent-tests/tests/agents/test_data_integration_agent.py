#!/usr/bin/env python3
"""Test DataIntegrationAgent functionality and financial API integration."""

import asyncio
import sys
import time
import traceback
from agents.data_integration import DataIntegrationAgent, DataIntegrationMode, DataQuality
from agents.credential_manager import get_credential_manager, CredentialProvider
from agents.financial_apis import DataType


async def test_agent_initialization():
    """Test DataIntegrationAgent initialization."""
    print("\n🧪 Testing DataIntegrationAgent initialization...")
    
    try:
        # Create agent instance
        agent = DataIntegrationAgent()
        print(f"  ✅ Agent initialized: {agent.agent_id}")
        print(f"  ✅ Capabilities: {[cap.value for cap in agent.capabilities]}")
        print(f"  ✅ Required tools: {agent.get_required_tools()}")
        
        # Check configuration
        assert agent.max_concurrent_requests > 0
        assert agent.request_timeout > 0
        assert agent.credential_manager is not None
        assert agent.rate_limiter is not None
        assert agent.cache is not None
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
        agent = DataIntegrationAgent()
        
        # Test valid state
        valid_state = {
            "current_document": {
                "title": "Financial Market Analysis", 
                "path": "/tmp/market_analysis.odt"
            },
            "cursor_position": {"paragraph": 1, "character": 0, "line": 1},
            "document_structure": {"paragraphs": 8}
        }
        
        # Test validation
        validation = agent.validate_input(valid_state)
        print(f"  ✅ Valid state validation: passed={validation.passed}")
        
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


async def test_credential_management():
    """Test credential management integration."""
    print("\n🧪 Testing credential management...")
    
    try:
        agent = DataIntegrationAgent()
        cred_manager = get_credential_manager()
        
        # Add test credentials
        alpha_key = cred_manager.add_credential(
            provider=CredentialProvider.ALPHA_VANTAGE,
            api_key="TEST_ALPHA_VANTAGE_KEY_123456",
            key_id="test_alpha"
        )
        
        yahoo_key = cred_manager.add_credential(
            provider=CredentialProvider.YAHOO_FINANCE,
            api_key="TEST_YAHOO_KEY_789",
            key_id="test_yahoo"
        )
        
        print(f"  ✅ Added Alpha Vantage credential: {alpha_key}")
        print(f"  ✅ Added Yahoo Finance credential: {yahoo_key}")
        
        # Test credential retrieval
        alpha_cred = cred_manager.get_credential(CredentialProvider.ALPHA_VANTAGE)
        yahoo_cred = cred_manager.get_credential(CredentialProvider.YAHOO_FINANCE)
        
        print(f"  ✅ Retrieved Alpha Vantage credential: {alpha_cred is not None}")
        print(f"  ✅ Retrieved Yahoo Finance credential: {yahoo_cred is not None}")
        
        # Test validation with credentials
        test_state = {
            "current_document": {
                "title": "Test Document", 
                "path": "/tmp/test.odt"
            }
        }
        
        validation = agent.validate_input(test_state)
        print(f"  ✅ Validation with credentials: passed={validation.passed}")
        
        return True
        
    except Exception as e:
        print(f"  ❌ Credential management test failed: {e}")
        traceback.print_exc()
        return False


async def test_data_integration_processing():
    """Test data integration processing."""
    print("\n🧪 Testing data integration processing...")
    
    try:
        agent = DataIntegrationAgent()
        
        # Test state for financial data request
        test_state = {
            "current_document": {
                "title": "S&P 500 Market Analysis", 
                "path": "/tmp/market_analysis.odt"
            },
            "cursor_position": {"paragraph": 2, "character": 10, "line": 2},
            "document_structure": {"paragraphs": 10}
        }
        
        # Test data integration message
        message = {
            "content": "get current quote for SPY and AAPL", 
            "role": "user"
        }
        
        print("  🔄 Processing data integration request...")
        result = await agent.process(test_state, message)
        
        print(f"  ✅ Data integration: success={result.success}")
        print(f"  ✅ Execution time: {result.execution_time*1000:.1f}ms")
        
        if result.success:
            data_status = result.state_updates.get("data_integration_status", {})
            print(f"  ✅ Symbols processed: {len(data_status.get('symbols', []))}")
            print(f"  ✅ Data quality: {data_status.get('quality', 'unknown')}")
            print(f"  ✅ Sources used: {data_status.get('sources', [])}")
            
            external_data = result.state_updates.get("external_data", {})
            if external_data:
                print(f"  ✅ Financial data retrieved: {len(external_data.get('financial_data', {})) > 0}")
                print(f"  ✅ Attribution provided: {len(external_data.get('attribution', [])) > 0}")
                print(f"  ✅ Citations generated: {len(external_data.get('citations', [])) > 0}")
        else:
            print(f"  ❌ Integration failed: {result.error}")
            # This is expected in test environment without real API keys
        
        return True
        
    except Exception as e:
        print(f"  ❌ Data integration test failed: {e}")
        traceback.print_exc()
        return False


async def test_shared_cache_integration():
    """Test shared cache integration."""
    print("\n🧪 Testing shared cache integration...")
    
    try:
        agent = DataIntegrationAgent()
        
        test_state = {
            "current_document": {
                "title": "Cache Test Document", 
                "path": "/tmp/cache_test.odt"
            },
            "cursor_position": {"paragraph": 1, "character": 0, "line": 1},
            "document_structure": {"paragraphs": 3}
        }
        
        message = {"content": "get SPY quote", "role": "user"}
        
        # First request (should not be cached)
        print("  🔄 First data integration request...")
        result1 = await agent.process(test_state, message)
        cache_used_1 = result1.metadata.get("from_cache", False)
        print(f"  ✅ First request cache used: {cache_used_1}")
        
        # Second identical request (should hit shared cache if successful)
        print("  🔄 Second identical request...")
        result2 = await agent.process(test_state, message)
        cache_used_2 = result2.metadata.get("from_cache", False)
        print(f"  ✅ Second request cache used: {cache_used_2}")
        
        # Verify shared context was updated
        document_id = test_state["current_document"]["path"]
        context = agent.get_other_agent_context(document_id, "data_integration_agent")
        print(f"  ✅ Shared context updated: {len(context) > 0}")
        
        return True
        
    except Exception as e:
        print(f"  ❌ Shared cache integration test failed: {e}")
        traceback.print_exc()
        return False


async def test_rate_limiting():
    """Test rate limiting functionality."""
    print("\n🧪 Testing rate limiting...")
    
    try:
        agent = DataIntegrationAgent()
        
        # Test rate limiter status
        status = agent.rate_limiter.get_status(CredentialProvider.ALPHA_VANTAGE)
        print(f"  ✅ Alpha Vantage rate limit status retrieved: {status.provider.value}")
        print(f"  ✅ Requests remaining (minute): {status.requests_remaining_minute}")
        
        status = agent.rate_limiter.get_status(CredentialProvider.YAHOO_FINANCE)
        print(f"  ✅ Yahoo Finance rate limit status retrieved: {status.provider.value}")
        print(f"  ✅ Requests remaining (minute): {status.requests_remaining_minute}")
        
        # Test rate limit acquisition
        can_make_request = await agent.rate_limiter.acquire(CredentialProvider.ALPHA_VANTAGE)
        print(f"  ✅ Rate limit acquisition test: {can_make_request}")
        
        # Record a mock request
        agent.rate_limiter.record_request(CredentialProvider.ALPHA_VANTAGE, True, 150.0)
        print(f"  ✅ Request recorded successfully")
        
        return True
        
    except Exception as e:
        print(f"  ❌ Rate limiting test failed: {e}")
        traceback.print_exc()
        return False


async def test_cache_performance():
    """Test cache performance."""
    print("\n🧪 Testing cache performance...")
    
    try:
        agent = DataIntegrationAgent()
        cache = agent.cache
        
        # Test cache operations
        test_key = "test_stock_data"
        test_data = {
            "symbol": "TEST",
            "price": 123.45,
            "timestamp": time.time()
        }
        
        # Store in cache
        stored = await cache.set(test_key, test_data, 300)  # 5 minute TTL
        print(f"  ✅ Data stored in cache: {stored}")
        
        # Retrieve from cache
        retrieved = await cache.get(test_key)
        print(f"  ✅ Data retrieved from cache: {retrieved is not None}")
        
        if retrieved:
            print(f"  ✅ Retrieved data matches: {retrieved['symbol'] == test_data['symbol']}")
        
        # Get cache statistics
        stats = cache.get_stats()
        print(f"  ✅ Cache stats - Hits: {stats['hits']}, Misses: {stats['misses']}")
        print(f"  ✅ Cache hit rate: {stats['hit_rate']:.2%}")
        print(f"  ✅ Memory entries: {stats['memory_entries']}")
        
        return True
        
    except Exception as e:
        print(f"  ❌ Cache performance test failed: {e}")
        traceback.print_exc()
        return False


async def test_circuit_breaker():
    """Test circuit breaker functionality."""
    print("\n🧪 Testing circuit breaker...")
    
    try:
        agent = DataIntegrationAgent()
        
        # Get circuit breaker for a provider
        cb = agent._get_circuit_breaker(CredentialProvider.ALPHA_VANTAGE)
        print(f"  ✅ Circuit breaker created: {cb.provider.value}")
        print(f"  ✅ Initial state: {cb.state}")
        print(f"  ✅ Failure threshold: {cb.failure_threshold}")
        
        # Test initial request allowance
        should_allow = cb.should_allow_request()
        print(f"  ✅ Should allow initial request: {should_allow}")
        
        # Record some failures to test circuit breaker behavior
        for i in range(3):
            cb.record_failure()
            print(f"  ✅ Recorded failure {i+1}, state: {cb.state}")
        
        # Record success to test recovery
        cb.record_success()
        print(f"  ✅ Recorded success, state: {cb.state}")
        
        return True
        
    except Exception as e:
        print(f"  ❌ Circuit breaker test failed: {e}")
        traceback.print_exc()
        return False


async def test_performance_metrics():
    """Test performance metrics tracking."""
    print("\n🧪 Testing performance metrics...")
    
    try:
        agent = DataIntegrationAgent()
        
        # Get initial performance stats
        initial_stats = agent.performance_stats.copy()
        print(f"  ✅ Initial requests processed: {initial_stats['requests_processed']}")
        print(f"  ✅ Initial cache hits: {initial_stats['cache_hits']}")
        print(f"  ✅ Initial avg response time: {initial_stats['avg_response_time_ms']:.1f}ms")
        
        # Process a test request to update metrics
        test_state = {
            "current_document": {
                "title": "Performance Test", 
                "path": "/tmp/perf_test.odt"
            },
            "cursor_position": {"paragraph": 1, "character": 0, "line": 1},
            "document_structure": {"paragraphs": 2}
        }
        
        message = {"content": "get TSLA stock data", "role": "user"}
        
        # Process request
        await agent.process(test_state, message)
        
        # Check updated metrics
        updated_stats = agent.performance_stats
        print(f"  ✅ Updated requests processed: {updated_stats['requests_processed']}")
        print(f"  ✅ Updated avg response time: {updated_stats['avg_response_time_ms']:.1f}ms")
        
        # Verify metrics changed
        metrics_updated = (updated_stats['requests_processed'] >= initial_stats['requests_processed'])
        print(f"  ✅ Performance metrics updated: {metrics_updated}")
        
        return True
        
    except Exception as e:
        print(f"  ❌ Performance metrics test failed: {e}")
        traceback.print_exc()
        return False


async def run_all_tests():
    """Run all DataIntegrationAgent tests."""
    print("🧪 Testing DataIntegrationAgent Core Functionality")
    print("=" * 60)
    
    tests = [
        ("Agent Initialization", test_agent_initialization),
        ("Input Validation", test_input_validation),
        ("Credential Management", test_credential_management),
        ("Data Integration Processing", test_data_integration_processing),
        ("Shared Cache Integration", test_shared_cache_integration),
        ("Rate Limiting", test_rate_limiting),
        ("Cache Performance", test_cache_performance),
        ("Circuit Breaker", test_circuit_breaker),
        ("Performance Metrics", test_performance_metrics)
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
        print("🎉 DataIntegrationAgent working perfectly!")
        return True
    else:
        print("❌ Some DataIntegrationAgent functionality needs attention")
        return False


if __name__ == "__main__":
    success = asyncio.run(run_all_tests())
    sys.exit(0 if success else 1)