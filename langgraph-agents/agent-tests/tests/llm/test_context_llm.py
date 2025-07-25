#!/usr/bin/env python3
"""Test ContextAnalysisAgent with LLM integration."""

import asyncio
import time
from agents.context_analysis import ContextAnalysisAgent

async def test_context_analysis_llm():
    """Test ContextAnalysisAgent with LLM."""
    print("🧪 Testing ContextAnalysisAgent with LLM...")
    
    try:
        agent = ContextAnalysisAgent()
        
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
            },
            "messages": [
                {"role": "user", "content": "analyze the financial context", "timestamp": time.time()}
            ],
            "current_task": "llm_test"
        }
        
        message = {"content": "perform lightweight analysis of this financial document", "role": "user"}
        
        print("  🔄 Running LLM-powered analysis...")
        start_time = time.time()
        result = await agent.process(test_state, message)
        execution_time = (time.time() - start_time) * 1000
        
        print(f"  ✅ Analysis completed: success={result.success}")
        print(f"  ✅ Execution time: {execution_time:.1f}ms")
        
        if result.success:
            analysis = result.state_updates.get("content_analysis", {})
            print(f"  ✅ Analysis mode: {analysis.get('analysis_mode', 'unknown')}")
            
            # Check for LLM metadata
            metadata = analysis.get("metadata", {})
            if "llm_provider" in metadata:
                print(f"  ✅ LLM provider: {metadata['llm_provider']}")
                print(f"  ✅ LLM model: {metadata['llm_model']}")
                if metadata.get("tokens_used"):
                    print(f"  ✅ Tokens used: {metadata['tokens_used']}")
            
            print(f"  ✅ Analysis data keys: {list(analysis.keys())}")
        else:
            print(f"  ❌ Analysis failed: {result.error}")
        
        return result.success
        
    except Exception as e:
        print(f"  ❌ Test failed: {e}")
        import traceback
        traceback.print_exc()
        return False

if __name__ == "__main__":
    success = asyncio.run(test_context_analysis_llm())
    if success:
        print("\n🎉 ContextAnalysisAgent LLM integration working!")
    else:
        print("\n❌ ContextAnalysisAgent LLM integration failed")
    exit(0 if success else 1)