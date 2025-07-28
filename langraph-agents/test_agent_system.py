#!/usr/bin/env python3
"""
Quick test script to validate the agent system
"""

import asyncio
import os
from agents.agent_orchestrator import AgentOrchestrator

async def test_agent_system():
    """Test the agent orchestrator with sample requests"""
    
    # Check if OpenAI API key is available
    if not os.getenv("OPENAI_API_KEY"):
        print("Warning: OPENAI_API_KEY environment variable not set")
        print("Agent system validation: Structure OK, but API calls will fail")
        return
    
    # Initialize orchestrator
    orchestrator = AgentOrchestrator()
    print("✅ Agent orchestrator initialized successfully")
    
    # Test cases
    test_cases = [
        {
            "request": "Make this text bold",
            "request_id": "test_001",
            "context": {"selected_text": "Hello World"},
            "expected_type": "format"
        },
        {
            "request": "Insert a quarterly sales report",
            "request_id": "test_002", 
            "context": {"cursor_position": {"line": 1, "column": 1}},
            "expected_type": "insert"
        },
        {
            "request": "Create a bar chart for sales data",
            "request_id": "test_003",
            "context": {},
            "expected_type": "chart"
        },
        {
            "request": "Add a 3x4 table",
            "request_id": "test_004",
            "context": {},
            "expected_type": "table"
        }
    ]
    
    print("\nTesting agent system with sample requests...")
    
    for i, test_case in enumerate(test_cases, 1):
        try:
            print(f"\n--- Test {i}: {test_case['request']} ---")
            
            result = await orchestrator.process_request(
                user_request=test_case["request"],
                request_id=test_case["request_id"],
                context=test_case["context"]
            )
            
            print(f"✅ Request processed successfully")
            print(f"   Type: {result.get('type')}")
            print(f"   Response: {result.get('response', '')[:100]}...")
            
            # Validate response type
            if result.get("type") == test_case["expected_type"]:
                print(f"✅ Response type matches expected: {test_case['expected_type']}")
            else:
                print(f"⚠️  Response type mismatch. Expected: {test_case['expected_type']}, Got: {result.get('type')}")
                
        except Exception as e:
            print(f"❌ Test {i} failed: {str(e)}")
    
    print("\n🎉 Agent system validation complete!")

if __name__ == "__main__":
    asyncio.run(test_agent_system())