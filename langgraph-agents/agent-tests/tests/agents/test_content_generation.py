#!/usr/bin/env python3
"""Test ContentGenerationAgent core functionality."""

import asyncio
from agents.content_generation import ContentGenerationAgent, ContentType, GenerationMode

async def test_content_generation_basic():
    """Test basic ContentGenerationAgent functionality."""
    print("🧪 Testing ContentGenerationAgent basic functionality...")
    
    try:
        # Create agent instance
        agent = ContentGenerationAgent()
        print(f"  ✅ Agent initialized: {agent.agent_id}")
        print(f"  ✅ Capabilities: {[cap.value for cap in agent.capabilities]}")
        
        # Test input validation
        test_state = {
            "current_document": {
                "title": "Financial Report Q4 2024", 
                "path": "/tmp/financial_report.odt"
            },
            "cursor_position": {"paragraph": 5, "character": 20, "line": 5},
            "selected_text": "quarterly revenue increased by 15%",
            "document_structure": {
                "paragraphs": 12,
                "sections": [
                    {"title": "Executive Summary", "start_paragraph": 0, "end_paragraph": 3},
                    {"title": "Financial Results", "start_paragraph": 4, "end_paragraph": 8}
                ]
            }
        }
        
        # Test validation
        validation = agent.validate_input(test_state)
        print(f"  ✅ Input validation: success={validation.passed}")
        
        # Test content generation
        message = {"content": "generate an executive summary for this financial report", "role": "user"}
        
        print("  🔄 Generating content...")
        result = await agent.process(test_state, message)
        
        print(f"  ✅ Content generation: success={result.success}")
        print(f"  ✅ Execution time: {result.execution_time*1000:.1f}ms")
        
        if result.success:
            content_gen = result.state_updates.get("content_generation", {})
            print(f"  ✅ Content type: {content_gen.get('content_type', 'unknown')}")
            print(f"  ✅ Word count: {content_gen.get('word_count', 0)}")
            print(f"  ✅ Quality score: {content_gen.get('quality_score', 0.0):.2f}")
            
            generated_content = result.state_updates.get("generated_content", [])
            if generated_content:
                print(f"  ✅ Generated text preview: {generated_content[0][:100]}...")
        else:
            print(f"  ❌ Generation failed: {result.error}")
        
        return result.success
        
    except Exception as e:
        print(f"  ❌ Test failed: {e}")
        import traceback
        traceback.print_exc()
        return False

if __name__ == "__main__":
    success = asyncio.run(test_content_generation_basic())
    if success:
        print("\n🎉 ContentGenerationAgent core structure working!")
    else:
        print("\n❌ ContentGenerationAgent core structure failed")
    exit(0 if success else 1)