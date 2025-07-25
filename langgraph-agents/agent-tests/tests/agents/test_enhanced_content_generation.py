#!/usr/bin/env python3
"""Test enhanced ContentGenerationAgent capabilities."""

import asyncio
import sys
import traceback
from agents.content_generation import ContentGenerationAgent, ContentType, GenerationMode

async def test_basic_generation():
    """Test basic content generation with enhanced prompts."""
    print("\n🧪 Testing basic content generation with enhanced prompts...")
    
    try:
        agent = ContentGenerationAgent()
        
        # Test state with financial context
        test_state = {
            "current_document": {
                "title": "Q4 2024 Financial Report - TechCorp Inc.", 
                "path": "/tmp/financial_report.odt"
            },
            "cursor_position": {"paragraph": 3, "character": 15, "line": 3},
            "selected_text": "Revenue increased by 25% this quarter",
            "document_structure": {
                "paragraphs": 15,
                "sections": [
                    {"title": "Executive Summary", "start_paragraph": 0, "end_paragraph": 2},
                    {"title": "Financial Results", "start_paragraph": 3, "end_paragraph": 8}
                ]
            }
        }
        
        # Test enhanced executive summary generation
        message = {"content": "generate a comprehensive executive summary for this financial report", "role": "user"}
        result = await agent.process(test_state, message)
        
        print(f"  ✅ Enhanced generation: success={result.success}")
        print(f"  ✅ Execution time: {result.execution_time*1000:.1f}ms")
        
        if result.success:
            content_gen = result.state_updates.get("content_generation", {})
            print(f"  ✅ Content type: {content_gen.get('content_type', 'unknown')}")
            print(f"  ✅ Word count: {content_gen.get('word_count', 0)}")
            print(f"  ✅ Quality score: {content_gen.get('quality_score', 0.0):.2f}")
            
            generated_content = result.state_updates.get("generated_content", [])
            if generated_content:
                preview = generated_content[0][:150] + "..." if len(generated_content[0]) > 150 else generated_content[0]
                print(f"  ✅ Generated text preview: {preview}")
        
        return result.success
        
    except Exception as e:
        print(f"  ❌ Basic generation test failed: {e}")
        traceback.print_exc()
        return False

async def test_content_rewriting():
    """Test content rewriting capabilities."""
    print("\n🧪 Testing content rewriting capabilities...")
    
    try:
        agent = ContentGenerationAgent()
        
        original_content = "The company made money this quarter. Sales went up. Profits were good."
        rewrite_instructions = "Make this more professional and detailed for a financial report"
        
        result = await agent.rewrite_content(
            content=original_content,
            rewrite_instructions=rewrite_instructions,
            context={"document_type": "financial_report"}
        )
        
        print(f"  ✅ Rewrite generation: success={result.success}")
        print(f"  ✅ Execution time: {result.execution_time_ms:.1f}ms")
        
        if result.success:
            print(f"  ✅ Original length: {result.metadata.get('original_length', 0)} words")
            print(f"  ✅ Rewritten length: {result.metadata.get('rewritten_length', 0)} words")
            print(f"  ✅ Quality score: {result.quality_score:.2f}")
            
            preview = result.generated_text[:150] + "..." if len(result.generated_text) > 150 else result.generated_text
            print(f"  ✅ Rewritten text preview: {preview}")
        else:
            print(f"  ❌ Rewrite failed: {result.error_message}")
        
        return result.success
        
    except Exception as e:
        print(f"  ❌ Content rewriting test failed: {e}")
        traceback.print_exc()
        return False

async def test_content_enhancement():
    """Test content enhancement capabilities."""
    print("\n🧪 Testing content enhancement capabilities...")
    
    try:
        agent = ContentGenerationAgent()
        
        original_content = "The quarterly results show good performance. Revenue and profit increased."
        enhancement_type = "financial"
        
        result = await agent.enhance_content(
            content=original_content,
            enhancement_type=enhancement_type,
            context={"document_type": "financial_analysis"}
        )
        
        print(f"  ✅ Enhancement generation: success={result.success}")
        print(f"  ✅ Execution time: {result.execution_time_ms:.1f}ms")
        
        if result.success:
            print(f"  ✅ Enhancement type: {result.metadata.get('enhancement_type', 'unknown')}")
            print(f"  ✅ Original length: {result.metadata.get('original_length', 0)} words")
            print(f"  ✅ Enhanced length: {result.metadata.get('enhanced_length', 0)} words")
            print(f"  ✅ Quality score: {result.quality_score:.2f}")
            
            preview = result.generated_text[:150] + "..." if len(result.generated_text) > 150 else result.generated_text
            print(f"  ✅ Enhanced text preview: {preview}")
        else:
            print(f"  ❌ Enhancement failed: {result.error_message}")
        
        return result.success
        
    except Exception as e:
        print(f"  ❌ Content enhancement test failed: {e}")
        traceback.print_exc()
        return False

async def test_structured_financial_content():
    """Test structured financial content generation."""
    print("\n🧪 Testing structured financial content generation...")
    
    try:
        agent = ContentGenerationAgent()
        
        financial_data = {
            "revenue": "$12.5M",
            "revenue_growth": "25%",
            "gross_margin": "68%",
            "operating_margin": "22%",
            "net_margin": "18%",
            "kpis": {
                "Customer Acquisition Cost": "$125",
                "Lifetime Value": "$1,250",
                "Monthly Recurring Revenue": "$2.1M"
            }
        }
        
        result = await agent.generate_structured_financial_content(
            structure_type="executive_summary",
            financial_data=financial_data,
            context={"document_type": "quarterly_report"}
        )
        
        print(f"  ✅ Structured generation: success={result.success}")
        print(f"  ✅ Execution time: {result.execution_time_ms:.1f}ms")
        
        if result.success:
            metadata = result.metadata
            print(f"  ✅ Structure type: {metadata.get('structure_type', 'unknown')}")
            print(f"  ✅ Target words: {metadata.get('target_words', 0)}")
            print(f"  ✅ Actual words: {result.word_count}")
            print(f"  ✅ Quality score: {result.quality_score:.2f}")
            print(f"  ✅ Financial data used: {metadata.get('financial_data_used', False)}")
            
            preview = result.generated_text[:200] + "..." if len(result.generated_text) > 200 else result.generated_text
            print(f"  ✅ Structured content preview: {preview}")
        else:
            print(f"  ❌ Structured generation failed: {result.error_message}")
        
        return result.success
        
    except Exception as e:
        print(f"  ❌ Structured content test failed: {e}")
        traceback.print_exc()
        return False

async def test_quality_scoring():
    """Test enhanced quality scoring system."""
    print("\n🧪 Testing enhanced quality scoring system...")
    
    try:
        agent = ContentGenerationAgent()
        
        # Test different quality levels
        test_cases = [
            {
                "text": "Revenue increased significantly this quarter, demonstrating strong market performance and operational efficiency. Our comprehensive analysis shows sustained growth across key metrics.",
                "content_type": ContentType.FINANCIAL_ANALYSIS,
                "generation_mode": GenerationMode.STANDARD,
                "context": {"document_title": "Financial Analysis Report", "selected_text": "revenue growth"},
                "expected_score_range": (0.7, 1.0)
            },
            {
                "text": "Good results. Up.",
                "content_type": ContentType.EXECUTIVE_SUMMARY,
                "generation_mode": GenerationMode.QUICK,
                "context": {},
                "expected_score_range": (0.0, 0.4)
            },
            {
                "text": "This comprehensive executive summary provides detailed insights into our quarterly performance, highlighting revenue growth of 25%, improved operational margins, and strategic initiatives that position the company for continued success in the competitive marketplace.",
                "content_type": ContentType.EXECUTIVE_SUMMARY,
                "generation_mode": GenerationMode.COMPREHENSIVE,
                "context": {"document_title": "Executive Summary", "selected_text": "quarterly performance"},
                "expected_score_range": (0.8, 1.0)
            }
        ]
        
        all_passed = True
        for i, test_case in enumerate(test_cases):
            score = agent._calculate_comprehensive_quality(
                test_case["text"],
                test_case["content_type"],
                test_case["generation_mode"],
                test_case["context"]
            )
            
            min_score, max_score = test_case["expected_score_range"]
            passed = min_score <= score <= max_score
            
            print(f"  {'✅' if passed else '❌'} Test case {i+1}: score={score:.2f}, expected={min_score:.1f}-{max_score:.1f}")
            
            if not passed:
                all_passed = False
        
        return all_passed
        
    except Exception as e:
        print(f"  ❌ Quality scoring test failed: {e}")
        traceback.print_exc()
        return False

async def run_all_tests():
    """Run all enhanced content generation tests."""
    print("🧪 Testing Enhanced ContentGenerationAgent Capabilities")
    print("=" * 60)
    
    tests = [
        ("Basic Generation", test_basic_generation),
        ("Content Rewriting", test_content_rewriting),
        ("Content Enhancement", test_content_enhancement),
        ("Structured Financial Content", test_structured_financial_content),
        ("Quality Scoring", test_quality_scoring)
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
        print("🎉 All enhanced content generation capabilities working!")
        return True
    else:
        print("❌ Some enhanced capabilities failed")
        return False

if __name__ == "__main__":
    success = asyncio.run(run_all_tests())
    sys.exit(0 if success else 1)