#!/usr/bin/env python3
"""Basic LLM integration test."""

import asyncio
from llm_client import get_llm_client

async def test_basic_llm():
    """Test basic LLM functionality."""
    print("🧪 Testing basic LLM integration...")
    
    try:
        # Test client initialization
        llm_client = get_llm_client()
        print(f"  ✅ LLM client initialized: {llm_client.primary_provider.value}")
        
        # Test simple request
        response = await llm_client._make_llm_request(
            prompt="Return the JSON: {\"test\": \"success\", \"number\": 42}",
            max_tokens=50,
            temperature=0.0
        )
        
        print(f"  ✅ LLM request: success={response.success}")
        if response.success:
            print(f"  ✅ Response: {response.content[:100]}...")
            print(f"  ✅ Provider: {response.provider.value}")
            print(f"  ✅ Model: {response.model}")
            if response.tokens_used:
                print(f"  ✅ Tokens: {response.tokens_used}")
        else:
            print(f"  ❌ Error: {response.error}")
        
        return response.success
        
    except Exception as e:
        print(f"  ❌ Test failed: {e}")
        return False

if __name__ == "__main__":
    success = asyncio.run(test_basic_llm())
    if success:
        print("\n🎉 LLM integration working!")
    else:
        print("\n❌ LLM integration failed")
    exit(0 if success else 1)