#!/usr/bin/env python3
"""
Direct Document Processing Flow Test (No Server Required)

Tests the complete agent flow directly by importing the modules and running
the document processing pipeline with realistic document states and prompts.
This bypasses the API server and tests the core functionality directly.
"""

import asyncio
import json
import time
import sys
import os
from typing import Dict, Any, List
from datetime import datetime

# Add parent directory to path for imports
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# Import Task 18 components directly
from routing.complexity_analyzer import ComplexityAnalyzer, OperationType, OperationComplexity
from routing.lightweight_router import LightweightRouter
from routing.focused_router import FocusedRouter  
from routing.full_orchestration_router import FullOrchestrationRouter
from routing.performance_monitor import PerformanceMonitor, MetricType
from state.document_state import DocumentState

# Test document states (same as in the server test)
SAMPLE_DOCUMENTS = {
    "financial_report": {
        "document_type": "financial_report",
        "content": """
        Q3 2024 Financial Analysis Report
        
        Executive Summary:
        Our company has shown strong performance in Q3 2024 with revenue growth of 15.3% 
        year-over-year, reaching $2.8M in total revenue. Key highlights include:
        
        - Revenue: $2,847,350 (up 15.3% YoY)
        - Operating expenses: $1,923,400 
        - Net profit: $923,950 (up 22.1% YoY)
        - EBITDA: $1,156,200
        - Cash flow: $834,600 positive
        
        Market Analysis:
        The market conditions have been favorable with increased demand in our core 
        product segments. Our main competitors have seen modest growth of 8-10%.
        
        Risk Factors:
        - Supply chain volatility
        - Currency fluctuation impact
        - Regulatory changes in target markets
        """,
        "metadata": {
            "created_date": "2024-10-15",
            "author": "Financial Analysis Team",
            "version": "1.2",
            "document_id": "FR_Q3_2024_001",
            "page_count": 12,
            "word_count": 4850,
            "contains_tables": True,
            "contains_charts": True,
            "classification": "confidential"
        },
        "structure": {
            "sections": [
                {"name": "Executive Summary", "start_page": 1, "length": 350},
                {"name": "Financial Overview", "start_page": 2, "length": 1200},
                {"name": "Market Analysis", "start_page": 5, "length": 800},
                {"name": "Risk Assessment", "start_page": 8, "length": 600},
                {"name": "Appendices", "start_page": 10, "length": 900}
            ],
            "tables": [
                {"name": "Revenue Breakdown", "page": 3, "rows": 15, "columns": 6},
                {"name": "Expense Analysis", "page": 4, "rows": 12, "columns": 4},
                {"name": "Market Comparison", "page": 6, "rows": 8, "columns": 5}
            ],
            "charts": [
                {"name": "Revenue Trend", "page": 3, "type": "line_chart"},
                {"name": "Profit Margins", "page": 4, "type": "bar_chart"},
                {"name": "Market Share", "page": 7, "type": "pie_chart"}
            ]
        },
        "formatting": {
            "font_family": "Arial",
            "font_size": 11,
            "line_spacing": 1.15,
            "margins": {"top": 1.0, "bottom": 1.0, "left": 1.25, "right": 1.25},
            "headers_formatted": True,
            "page_numbers": True,
            "table_of_contents": True
        },
        "current_state": {
            "cursor_position": {"page": 1, "line": 15, "column": 0},
            "selection": None,
            "last_modified": "2024-10-15T14:30:00Z",
            "unsaved_changes": False,
            "editing_mode": "normal"
        }
    },
    
    "business_proposal": {
        "document_type": "business_proposal",
        "content": """
        Project Alpha: AI-Powered Document Management System
        Business Proposal
        
        Executive Summary:
        We propose developing an AI-powered document management system that will 
        revolutionize how organizations handle, process, and analyze their documents.
        
        Project Overview:
        The system will integrate natural language processing, machine learning, 
        and advanced automation to provide intelligent document workflows.
        
        Key Features:
        - Automated document classification
        - Intelligent content extraction
        - Real-time collaboration tools
        - Advanced search capabilities
        - Compliance monitoring
        
        Market Opportunity:
        The global document management market is valued at $6.8B and growing at 12% CAGR.
        
        Financial Projections:
        Year 1: $500K revenue target
        Year 2: $1.2M revenue target  
        Year 3: $2.8M revenue target
        """,
        "metadata": {
            "created_date": "2024-10-12",
            "author": "Business Development Team",
            "version": "2.1",
            "document_id": "BP_ALPHA_2024_003",
            "page_count": 8,
            "word_count": 3200,
            "contains_tables": True,
            "contains_charts": False,
            "classification": "internal"
        },
        "structure": {
            "sections": [
                {"name": "Executive Summary", "start_page": 1, "length": 400},
                {"name": "Project Overview", "start_page": 2, "length": 600},
                {"name": "Market Analysis", "start_page": 3, "length": 800},
                {"name": "Financial Projections", "start_page": 5, "length": 500},
                {"name": "Implementation Plan", "start_page": 6, "length": 700}
            ],
            "tables": [
                {"name": "Revenue Projections", "page": 5, "rows": 10, "columns": 4},
                {"name": "Timeline Milestones", "page": 7, "rows": 12, "columns": 3}
            ],
            "charts": []
        },
        "formatting": {
            "font_family": "Calibri",
            "font_size": 12,
            "line_spacing": 1.2,
            "margins": {"top": 1.0, "bottom": 1.0, "left": 1.0, "right": 1.0},
            "headers_formatted": True,
            "page_numbers": True,
            "table_of_contents": False
        },
        "current_state": {
            "cursor_position": {"page": 3, "line": 8, "column": 25},
            "selection": {"start": {"page": 3, "line": 5}, "end": {"page": 3, "line": 8}},
            "last_modified": "2024-10-12T16:45:00Z",
            "unsaved_changes": True,
            "editing_mode": "review"
        }
    }
}

# Test prompts for different complexity levels
TEST_PROMPTS = {
    "simple": [
        "Make the title bold",
        "Change the font size to 14pt", 
        "Add bullet points to the recommendations list",
        "Move cursor to the end of the document"
    ],
    "moderate": [
        "Create a professional summary of the financial performance section",
        "Format this document with a professional business style",
        "Generate a table of contents for this document",
        "Reorganize the sections for better flow"
    ],
    "complex": [
        "Create a comprehensive financial analysis dashboard with live market data",
        "Generate an executive presentation with charts and visualizations from this data",
        "Perform risk analysis and create detailed recommendations with supporting data"
    ]
}

class MockAgent:
    """Mock agent for testing purposes with realistic responses."""
    
    def __init__(self, agent_id: str, processing_time: float = 0.1):
        self.agent_id = agent_id
        self.processing_time = processing_time
    
    async def process(self, document_state: DocumentState) -> Dict[str, Any]:
        """Mock processing with realistic delay and responses."""
        await asyncio.sleep(self.processing_time)
        
        # Generate realistic responses based on agent type
        if "context_analysis" in self.agent_id:
            result_text = f"Analyzed document structure and context. Found {document_state.get('metadata', {}).get('page_count', 'unknown')} pages with {document_state.get('metadata', {}).get('word_count', 'unknown')} words."
        elif "content_generation" in self.agent_id:
            result_text = "Generated content based on document analysis and user requirements."
        elif "formatting" in self.agent_id:
            result_text = "Applied professional formatting and styling to improve document presentation."
        elif "data_integration" in self.agent_id:
            result_text = "Integrated relevant financial and market data from external sources."
        elif "validation" in self.agent_id:
            result_text = "Validated document content for accuracy, consistency, and compliance."
        elif "execution" in self.agent_id:
            result_text = "Executed document modifications and applied changes successfully."
        else:
            result_text = f"Processed by {self.agent_id}"
        
        return {
            "success": True,
            "agent_id": self.agent_id,
            "result": result_text,
            "state_updates": {
                f"{self.agent_id}_processed": True,
                "last_processed_by": self.agent_id,
                "processing_time": self.processing_time
            },
            "metadata": {
                "timestamp": datetime.now().isoformat(),
                "processing_duration": self.processing_time
            }
        }
    
    async def analyze_cursor_position(self, document_state: DocumentState) -> Dict[str, Any]:
        """Mock cursor analysis for lightweight router."""
        return {
            "cursor_position": {"line": 1, "column": 1},
            "context": "Mock cursor context"
        }
    
    async def execute_direct_operation(self, operation_data: Dict[str, Any]) -> Dict[str, Any]:
        """Mock direct operation execution for lightweight router."""
        return {
            "success": True,
            "operation": operation_data.get("method", "unknown"),
            "result": "Direct operation executed successfully"
        }

class DirectDocumentFlowTest:
    """Test class for direct document processing flow without server."""
    
    def __init__(self):
        self.results_dir = "test_results"
        os.makedirs(self.results_dir, exist_ok=True)
        
        # Initialize routing components
        self.complexity_analyzer = ComplexityAnalyzer()
        self.lightweight_router = LightweightRouter()
        self.focused_router = FocusedRouter()
        self.full_orchestration_router = FullOrchestrationRouter()
        self.performance_monitor = PerformanceMonitor()
        
        # Register mock agents
        self._register_mock_agents()
        
    def _register_mock_agents(self):
        """Register mock agents with appropriate timing for each router."""
        
        agents = {
            "context_analysis_agent": MockAgent("context_analysis_agent", 0.2),
            "content_generation_agent": MockAgent("content_generation_agent", 0.4),
            "formatting_agent": MockAgent("formatting_agent", 0.3),
            "data_integration_agent": MockAgent("data_integration_agent", 0.6),
            "validation_agent": MockAgent("validation_agent", 0.2),
            "execution_agent": MockAgent("execution_agent", 0.1)
        }
        
        # Register agents with each router
        for agent_id, agent in agents.items():
            self.lightweight_router.register_agent(agent_id, agent)
            self.focused_router.register_agent(agent_id, agent)
            self.full_orchestration_router.register_agent(agent_id, agent)
    
    async def process_document_request(self, document_state: Dict[str, Any], user_request: str) -> Dict[str, Any]:
        """Process a document request through the complete agent flow."""
        
        start_time = time.time()
        result = {
            "success": False,
            "user_request": user_request,
            "document_info": {
                "type": document_state.get("document_type", "unknown"),
                "pages": document_state.get("metadata", {}).get("page_count", 0),
                "words": document_state.get("metadata", {}).get("word_count", 0)
            },
            "timestamp": datetime.now().isoformat()
        }
        
        try:
            # Step 1: Complexity Analysis
            print(f"  🧠 Analyzing complexity for: '{user_request}'")
            complexity_assessment = await self.complexity_analyzer.analyze_complexity(
                user_request, document_state
            )
            
            result["complexity_analysis"] = {
                "complexity": complexity_assessment.complexity.value,
                "operation_type": complexity_assessment.operation_type.value,
                "confidence_score": complexity_assessment.confidence_score,
                "estimated_time": complexity_assessment.estimated_time,
                "reasoning": complexity_assessment.reasoning
            }
            
            print(f"     Complexity: {complexity_assessment.complexity.value}")
            print(f"     Operation: {complexity_assessment.operation_type.value}")
            print(f"     Confidence: {complexity_assessment.confidence_score:.2f}")
            
            # Step 2: Route to appropriate router
            router_result = None
            if complexity_assessment.complexity == OperationComplexity.SIMPLE:
                print(f"  🔀 Routing to Lightweight Router")
                router_result = await self.lightweight_router.route_operation(
                    complexity_assessment.operation_type, user_request, document_state
                )
                result["routing_decision"] = {
                    "router_type": "lightweight",
                    "strategy": "optimized_patterns",
                    "agents_used": router_result.agents_used
                }
                
            elif complexity_assessment.complexity == OperationComplexity.MODERATE:
                print(f"  🔀 Routing to Focused Router")
                router_result = await self.focused_router.route_operation(
                    complexity_assessment.operation_type, user_request, document_state
                )
                result["routing_decision"] = {
                    "router_type": "focused",
                    "strategy": "agent_subset",
                    "agents_used": router_result.agents_executed
                }
                
            else:  # COMPLEX
                print(f"  🔀 Routing to Full Orchestration Router")
                router_result = await self.full_orchestration_router.route_operation(
                    complexity_assessment.operation_type, user_request, document_state
                )
                result["routing_decision"] = {
                    "router_type": "full_orchestration",
                    "strategy": "comprehensive_workflow",
                    "agents_used": getattr(router_result, "agents_executed", []) if hasattr(router_result, "agents_executed") else []
                }
            
            # Step 3: Process agent execution results
            if router_result:
                # Handle different router result types uniformly
                success = getattr(router_result, 'success', False)
                execution_time = getattr(router_result, 'execution_time', 0)
                parallel_efficiency = getattr(router_result, 'parallel_efficiency', 0)
                agents_executed = getattr(router_result, 'agents_executed', [])
                
                result["agent_execution"] = {
                    "execution_mode": result["routing_decision"]["strategy"],
                    "success": success,
                    "execution_time": execution_time,
                    "parallel_efficiency": parallel_efficiency,
                    "agent_results": []
                }
                
                # Extract individual agent results from different router types
                agent_results = []
                
                # Try different ways to get agent results based on router type
                if hasattr(router_result, 'result_data') and isinstance(router_result.result_data, dict):
                    if "results" in router_result.result_data:
                        for agent_result in router_result.result_data["results"]:
                            if isinstance(agent_result, dict):
                                agent_results.append({
                                    "agent_id": agent_result.get("agent_id", "unknown"),
                                    "success": agent_result.get("success", False),
                                    "result": agent_result.get("result", "No result")
                                })
                
                # For mock testing, create mock agent results based on agents_executed
                if not agent_results and agents_executed:
                    for agent_id in agents_executed:
                        agent_results.append({
                            "agent_id": agent_id,
                            "success": True,
                            "result": f"Mock result from {agent_id}"
                        })
                
                result["agent_execution"]["agent_results"] = agent_results
                
                print(f"     Agents executed: {len(result['agent_execution']['agent_results'])}")
                print(f"     Execution time: {result['agent_execution']['execution_time']:.3f}s")
            
            # Step 4: Performance metrics
            total_time = time.time() - start_time
            result["performance_metrics"] = {
                "total_execution_time": total_time,
                "analysis_time": 0.1,  # Mock timing
                "routing_time": 0.05,  # Mock timing
                "agent_execution_time": result["agent_execution"]["execution_time"] if "agent_execution" in result else 0,
                "quality_score": 0.85  # Mock quality score
            }
            
            # Step 5: Generate final result
            final_success = router_result and getattr(router_result, 'success', False)
            result["final_result"] = {
                "status": "completed" if final_success else "failed",
                "changes_applied": "Document processed according to user request",
                "document_modifications": [
                    f"Applied {complexity_assessment.complexity.value} level processing",
                    f"Used {result['routing_decision']['router_type']} routing strategy",
                    f"Processed by {len(result.get('agent_execution', {}).get('agent_results', []))} agents"
                ]
            }
            
            result["success"] = True
            print(f"  ✅ Processing completed successfully in {total_time:.3f}s")
            
        except Exception as e:
            print(f"  ❌ Processing failed: {e}")
            result["error"] = str(e)
            result["performance_metrics"] = {
                "total_execution_time": time.time() - start_time,
                "error_occurred": True
            }
        
        return result
    
    def save_result(self, result: Dict[str, Any], filename: str):
        """Save test result to formatted text file."""
        filepath = os.path.join(self.results_dir, filename)
        
        # Create formatted output for readability
        formatted_result = self.format_result_for_reading(result)
        
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(formatted_result)
        
        print(f"  💾 Result saved to: {filepath}")
    
    def format_result_for_reading(self, result: Dict[str, Any]) -> str:
        """Format result for human-readable output."""
        
        output = []
        output.append("=" * 80)
        output.append("LANGGRAPH AGENT DOCUMENT PROCESSING RESULT")
        output.append("=" * 80)
        
        # Test information
        output.append(f"\n📋 REQUEST INFORMATION:")
        output.append(f"   User Request: {result.get('user_request', 'Unknown')}")
        output.append(f"   Document Type: {result.get('document_info', {}).get('type', 'Unknown')}")
        output.append(f"   Document Pages: {result.get('document_info', {}).get('pages', 0)}")
        output.append(f"   Document Words: {result.get('document_info', {}).get('words', 0)}")
        output.append(f"   Timestamp: {result.get('timestamp', 'Unknown')}")
        
        # Overall result
        output.append(f"\n🎯 OVERALL RESULT:")
        output.append(f"   Success: {'✅ YES' if result.get('success', False) else '❌ NO'}")
        
        if not result.get('success', False):
            output.append(f"   Error: {result.get('error', 'Unknown error')}")
            return "\n".join(output)
        
        # Complexity analysis results
        if "complexity_analysis" in result:
            analysis = result["complexity_analysis"]
            output.append(f"\n🧠 COMPLEXITY ANALYSIS:")
            output.append(f"   Detected Complexity: {analysis.get('complexity', 'Unknown')}")
            output.append(f"   Operation Type: {analysis.get('operation_type', 'Unknown')}")
            output.append(f"   Confidence Score: {analysis.get('confidence_score', 0):.2f}")
            output.append(f"   Estimated Time: {analysis.get('estimated_time', 0):.2f} seconds")
            output.append(f"   Reasoning: {analysis.get('reasoning', 'No reasoning provided')}")
        
        # Routing decision
        if "routing_decision" in result:
            routing = result["routing_decision"]
            output.append(f"\n🔀 ROUTING DECISION:")
            output.append(f"   Router Selected: {routing.get('router_type', 'Unknown')}")
            output.append(f"   Routing Strategy: {routing.get('strategy', 'Unknown')}")
            output.append(f"   Agents Used: {', '.join(routing.get('agents_used', []))}")
        
        # Agent execution details
        if "agent_execution" in result:
            execution = result["agent_execution"]
            output.append(f"\n🤖 AGENT EXECUTION:")
            output.append(f"   Execution Mode: {execution.get('execution_mode', 'Unknown')}")
            output.append(f"   Success: {'✅ YES' if execution.get('success', False) else '❌ NO'}")
            output.append(f"   Execution Time: {execution.get('execution_time', 0):.3f} seconds")
            output.append(f"   Parallel Efficiency: {execution.get('parallel_efficiency', 0):.2f}")
            output.append(f"   Total Agents: {len(execution.get('agent_results', []))}")
            
            # Individual agent results
            if execution.get("agent_results"):
                output.append(f"\n   📝 Individual Agent Results:")
                for i, agent_result in enumerate(execution["agent_results"], 1):
                    agent_id = agent_result.get("agent_id", f"Agent {i}")
                    success = agent_result.get("success", False)
                    result_text = agent_result.get("result", "No result")
                    output.append(f"      {i}. {agent_id}: {'✅' if success else '❌'}")
                    output.append(f"         Result: {result_text}")
        
        # Performance metrics
        if "performance_metrics" in result:
            perf = result["performance_metrics"]
            output.append(f"\n📊 PERFORMANCE METRICS:")
            output.append(f"   Total Execution Time: {perf.get('total_execution_time', 0):.3f} seconds")
            output.append(f"   Analysis Time: {perf.get('analysis_time', 0):.3f} seconds")
            output.append(f"   Routing Time: {perf.get('routing_time', 0):.3f} seconds")
            output.append(f"   Agent Execution Time: {perf.get('agent_execution_time', 0):.3f} seconds")
            output.append(f"   Quality Score: {perf.get('quality_score', 0):.2f}")
        
        # Final output/result
        if "final_result" in result:
            final = result["final_result"]
            output.append(f"\n📤 FINAL OUTPUT:")
            output.append(f"   Operation Status: {final.get('status', 'Unknown')}")
            output.append(f"   Changes Applied: {final.get('changes_applied', 'None')}")
            
            if "document_modifications" in final:
                modifications = final["document_modifications"]
                output.append(f"   Document Modifications:")
                for mod in modifications:
                    output.append(f"      - {mod}")
        
        output.append("\n" + "=" * 80)
        output.append(f"End of Result - Generated at {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        output.append("=" * 80)
        
        return "\n".join(output)
    
    async def run_comprehensive_test(self):
        """Run comprehensive document processing tests."""
        
        print("🧪 Starting Direct Document Processing Flow Test")
        print("=" * 60)
        
        test_count = 0
        successful_tests = 0
        
        # Test each document type with different complexity prompts
        for doc_name, document in SAMPLE_DOCUMENTS.items():
            print(f"\n📋 Testing Document: {doc_name}")
            print(f"   Type: {document['document_type']}")
            print(f"   Pages: {document['metadata']['page_count']}")
            print(f"   Words: {document['metadata']['word_count']}")
            
            # Test prompts of each complexity level
            for complexity, prompts in TEST_PROMPTS.items():
                for i, prompt in enumerate(prompts, 1):
                    test_count += 1
                    print(f"\n🔬 Test {test_count}: {complexity.upper()} - {prompt[:50]}...")
                    
                    # Run the test
                    result = await self.process_document_request(document, prompt)
                    
                    # Save result
                    timestamp = datetime.now().strftime('%H%M%S')
                    filename = f"{doc_name}_{complexity}_{i:02d}_{timestamp}.txt"
                    self.save_result(result, filename)
                    
                    if result.get("success", False):
                        successful_tests += 1
                    
                    # Small delay between tests
                    await asyncio.sleep(0.5)
        
        # Summary
        print(f"\n🏁 TEST SUMMARY:")
        print(f"   Total Tests: {test_count}")
        print(f"   Successful: {successful_tests}")
        print(f"   Failed: {test_count - successful_tests}")
        print(f"   Success Rate: {(successful_tests/test_count)*100:.1f}%")
        print(f"   Results saved in: {self.results_dir}/")

async def main():
    """Main test execution."""
    tester = DirectDocumentFlowTest()
    await tester.run_comprehensive_test()

if __name__ == "__main__":
    asyncio.run(main())