#!/usr/bin/env python3
"""
End-to-End Document Processing Flow Test

Creates realistic document states and tests the complete agent flow from 
document input through routing to final output generation. This test simulates
real-world usage scenarios and captures complete response flows.
"""

import asyncio
import json
import time
import requests
import subprocess
import sys
import os
from typing import Dict, Any, List
from datetime import datetime

# Add parent directory to path for imports
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# Test document states representing different document types
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
        
        [Additional detailed analysis sections would follow...]
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
        
        Implementation Timeline:
        Phase 1: Research & Development (6 months)
        Phase 2: MVP Development (8 months)
        Phase 3: Market Launch (4 months)
        
        [Detailed sections would continue...]
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
    },
    
    "technical_memo": {
        "document_type": "technical_memo",
        "content": """
        Technical Memorandum: System Architecture Review
        
        TO: Engineering Team
        FROM: Senior Architect
        DATE: October 10, 2024
        RE: Quarterly Architecture Review - Q3 2024
        
        Purpose:
        This memo outlines the findings from our Q3 system architecture review 
        and provides recommendations for system improvements.
        
        Current System Overview:
        Our microservices architecture consists of 15 core services running on 
        Kubernetes clusters across 3 availability zones.
        
        Performance Metrics:
        - Average response time: 250ms
        - System uptime: 99.7%
        - Peak throughput: 10,000 requests/minute
        - Data processing: 500GB/day
        
        Issues Identified:
        1. Database connection pooling inefficiencies
        2. Memory leaks in the notification service
        3. Inadequate monitoring for edge cases
        
        Recommendations:
        1. Implement connection pool optimization
        2. Upgrade to latest framework version
        3. Enhanced monitoring and alerting
        
        Next Steps:
        Implementation of these recommendations should begin in Q4 2024.
        """,
        "metadata": {
            "created_date": "2024-10-10",
            "author": "System Architecture Team",
            "version": "1.0",
            "document_id": "TM_ARCH_Q3_2024",
            "page_count": 3,
            "word_count": 850,
            "contains_tables": False,
            "contains_charts": False,
            "classification": "internal"
        },
        "structure": {
            "sections": [
                {"name": "Header", "start_page": 1, "length": 100},
                {"name": "Purpose", "start_page": 1, "length": 150},
                {"name": "System Overview", "start_page": 1, "length": 200},
                {"name": "Performance Metrics", "start_page": 2, "length": 180},
                {"name": "Issues & Recommendations", "start_page": 2, "length": 220}
            ],
            "tables": [],
            "charts": []
        },
        "formatting": {
            "font_family": "Times New Roman",
            "font_size": 11,
            "line_spacing": 1.0,
            "margins": {"top": 1.0, "bottom": 1.0, "left": 1.5, "right": 1.0},
            "headers_formatted": True,
            "page_numbers": False,
            "table_of_contents": False
        },
        "current_state": {
            "cursor_position": {"page": 2, "line": 12, "column": 0},
            "selection": None,
            "last_modified": "2024-10-10T11:20:00Z",
            "unsaved_changes": False,
            "editing_mode": "normal"
        }
    }
}

# Test prompts for different complexity levels
TEST_PROMPTS = {
    "simple": [
        "Make the title bold",
        "Change the font size to 14pt",
        "Add bullet points to the recommendations list",
        "Move cursor to the end of the document",
        "Insert a page break after the executive summary"
    ],
    "moderate": [
        "Create a professional summary of the financial performance section",
        "Format this document with a professional business style",
        "Generate a table of contents for this document",
        "Reorganize the sections for better flow",
        "Add appropriate headers and subheaders throughout"
    ],
    "complex": [
        "Create a comprehensive financial analysis dashboard with live market data",
        "Generate an executive presentation with charts and visualizations from this data",
        "Perform risk analysis and create detailed recommendations with supporting data",
        "Build an interactive financial model with scenario planning",
        "Create a multi-format report package for stakeholders with automated updates"
    ]
}

class DocumentProcessingFlowTest:
    """Test class for end-to-end document processing flow."""
    
    def __init__(self):
        self.base_url = "http://localhost:8000"
        self.results_dir = "test_results"
        self.server_process = None
        
        # Create results directory
        os.makedirs(self.results_dir, exist_ok=True)
        
    def start_server(self):
        """Start the API server for testing."""
        print("🚀 Starting API server...")
        try:
            # Start the server in background
            self.server_process = subprocess.Popen(
                [sys.executable, "api_server.py"],
                cwd=os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE
            )
            
            # Wait for server to start
            time.sleep(5)
            
            # Test server connectivity
            response = requests.get(f"{self.base_url}/health")
            if response.status_code == 200:
                print("✅ API server started successfully")
                return True
            else:
                print(f"❌ Server health check failed: {response.status_code}")
                return False
                
        except Exception as e:
            print(f"❌ Failed to start server: {e}")
            return False
    
    def stop_server(self):
        """Stop the API server."""
        if self.server_process:
            print("🛑 Stopping API server...")
            self.server_process.terminate()
            self.server_process.wait()
            print("✅ API server stopped")
    
    def test_document_processing(self, document_name: str, prompt: str, complexity: str) -> Dict[str, Any]:
        """Test document processing with given prompt."""
        
        document_state = SAMPLE_DOCUMENTS[document_name]
        
        # Prepare request payload
        payload = {
            "user_request": prompt,
            "document_state": document_state,
            "options": {
                "include_performance_metrics": True,
                "include_routing_details": True,
                "include_agent_details": True
            }
        }
        
        print(f"\n📄 Testing {document_name} with {complexity} prompt:")
        print(f"   Prompt: '{prompt}'")
        
        try:
            start_time = time.time()
            
            # Send request to API
            response = requests.post(
                f"{self.base_url}/process",
                json=payload,
                headers={"Content-Type": "application/json"},
                timeout=30
            )
            
            execution_time = time.time() - start_time
            
            if response.status_code == 200:
                result = response.json()
                result["test_metadata"] = {
                    "document_type": document_name,
                    "prompt": prompt,
                    "complexity": complexity,
                    "execution_time": execution_time,
                    "timestamp": datetime.now().isoformat()
                }
                
                print(f"   ✅ Success in {execution_time:.2f}s")
                return result
            else:
                print(f"   ❌ Request failed: {response.status_code}")
                print(f"   Error: {response.text}")
                return {
                    "success": False,
                    "error": f"HTTP {response.status_code}: {response.text}",
                    "test_metadata": {
                        "document_type": document_name,
                        "prompt": prompt,
                        "complexity": complexity,
                        "execution_time": execution_time,
                        "timestamp": datetime.now().isoformat()
                    }
                }
                
        except Exception as e:
            print(f"   ❌ Exception: {e}")
            return {
                "success": False,
                "error": str(e),
                "test_metadata": {
                    "document_type": document_name,
                    "prompt": prompt,
                    "complexity": complexity,
                    "timestamp": datetime.now().isoformat()
                }
            }
    
    def save_result(self, result: Dict[str, Any], filename: str):
        """Save test result to file."""
        filepath = os.path.join(self.results_dir, filename)
        
        # Create formatted output for readability
        formatted_result = self.format_result_for_reading(result)
        
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(formatted_result)
        
        print(f"   💾 Result saved to: {filepath}")
    
    def format_result_for_reading(self, result: Dict[str, Any]) -> str:
        """Format result for human-readable output."""
        
        output = []
        output.append("=" * 80)
        output.append("LANGGRAPH AGENT DOCUMENT PROCESSING RESULT")
        output.append("=" * 80)
        
        # Test metadata
        if "test_metadata" in result:
            meta = result["test_metadata"]
            output.append(f"\n📋 TEST INFORMATION:")
            output.append(f"   Document Type: {meta.get('document_type', 'Unknown')}")
            output.append(f"   Prompt: {meta.get('prompt', 'Unknown')}")
            output.append(f"   Expected Complexity: {meta.get('complexity', 'Unknown')}")
            output.append(f"   Execution Time: {meta.get('execution_time', 0):.3f} seconds")
            output.append(f"   Timestamp: {meta.get('timestamp', 'Unknown')}")
        
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
            output.append(f"   Total Agents: {len(execution.get('agent_results', []))}")
            output.append(f"   Parallel Efficiency: {execution.get('parallel_efficiency', 0):.2f}")
            
            # Individual agent results
            if "agent_results" in execution:
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
            
            if "generated_content" in final:
                content = final["generated_content"]
                output.append(f"\n   Generated Content:")
                output.append(f"      {content}")
        
        # Debug information (if available)
        if "debug_info" in result:
            debug = result["debug_info"]
            output.append(f"\n🔍 DEBUG INFORMATION:")
            for key, value in debug.items():
                output.append(f"   {key}: {value}")
        
        output.append("\n" + "=" * 80)
        output.append(f"End of Result - Generated at {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        output.append("=" * 80)
        
        return "\n".join(output)
    
    def run_comprehensive_test(self):
        """Run comprehensive document processing tests."""
        
        print("🧪 Starting Comprehensive Document Processing Flow Test")
        print("=" * 60)
        
        # Start server
        if not self.start_server():
            print("❌ Cannot start server. Exiting test.")
            return
        
        try:
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
                        
                        # Run the test
                        result = self.test_document_processing(doc_name, prompt, complexity)
                        
                        # Save result
                        filename = f"{doc_name}_{complexity}_{i:02d}_{datetime.now().strftime('%H%M%S')}.txt"
                        self.save_result(result, filename)
                        
                        if result.get("success", False):
                            successful_tests += 1
                        
                        # Small delay between tests
                        time.sleep(1)
            
            # Summary
            print(f"\n🏁 TEST SUMMARY:")
            print(f"   Total Tests: {test_count}")
            print(f"   Successful: {successful_tests}")
            print(f"   Failed: {test_count - successful_tests}")
            print(f"   Success Rate: {(successful_tests/test_count)*100:.1f}%")
            print(f"   Results saved in: {self.results_dir}/")
            
        finally:
            self.stop_server()

def main():
    """Main test execution."""
    tester = DocumentProcessingFlowTest()
    tester.run_comprehensive_test()

if __name__ == "__main__":
    main()