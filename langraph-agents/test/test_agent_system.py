#!/usr/bin/env python3
"""
Comprehensive test script for LangGraph Agent System

Tests all 4 operation types plus the Data Analyst Agent:
1. Format Operation (classify_formatting)
2. Insert Operation (generate_content)
3. Table Operation (create_table)  
4. Chart Operation (create_chart)
5. Financial Data Operation (generate_content with financial data)

The script:
- Starts the FastAPI server
- Runs all test cases
- Saves results to files
- Stops the server
"""

import asyncio
import json
import time
import subprocess
import signal
import os
import sys
import httpx
from datetime import datetime
from pathlib import Path
from dotenv import load_dotenv

class AgentSystemTester:
    """Test runner for the LangGraph agent system"""
    
    def __init__(self):
        self.server_process = None
        self.server_url = "http://127.0.0.1:8000"
        self.test_results = []
        self.start_time = None
        
        # Test cases covering all operations (using LibreOffice document context format)
        self.test_cases = [
            {
                "name": "Format Operation - Bold Text",
                "description": "Test formatting classification for making text bold",
                "request": {
                    "request": "Make this text bold and italic",
                    "request_id": "test_format_001",
                    "context": {
                        "document_available": True,
                        "cursor_position": {"node_index": 12, "content_index": 15},
                        "selected_text": "This is important text",
                        "document_structure": {"paragraph_count": 3, "page_count": 1, "word_count": 42, "character_count": 215},
                        "document_content": "This is a sample business document.\n\nThis is important text that needs formatting.\n\nEnd of document.",
                        "formatting_state": {"has_selection": True}
                    }
                },
                "expected_type": "format",
                "expected_fields": ["formatting"],
                "operation_category": "Formatting"
            },
            {
                "name": "Insert Operation - Simple Content",
                "description": "Test content generation without financial data",
                "request": {
                    "request": "Insert a professional introduction paragraph for a business letter",
                    "request_id": "test_insert_001", 
                    "context": {
                        "document_available": True,
                        "cursor_position": {"node_index": 1, "content_index": 0},
                        "selected_text": "",
                        "document_structure": {"paragraph_count": 1, "page_count": 1, "word_count": 0, "character_count": 0},
                        "document_content": "",
                        "formatting_state": {"has_selection": False}
                    }
                },
                "expected_type": "insert",
                "expected_fields": ["content"],
                "operation_category": "Content Generation"
            },
            {
                "name": "Table Operation - Data Table",
                "description": "Test table creation with specific dimensions",
                "request": {
                    "request": "Create a 5x3 table for quarterly sales data",
                    "request_id": "test_table_001",
                    "context": {
                        "document_available": True,
                        "cursor_position": {"node_index": 15, "content_index": 0},
                        "selected_text": "",
                        "document_structure": {"paragraph_count": 5, "page_count": 1, "word_count": 87, "character_count": 456},
                        "document_content": "Quarterly Sales Report\n\nQ1 Sales Analysis\n\nInsert table here for data analysis\n\nConclusion section",
                        "formatting_state": {"has_selection": False}
                    }
                },
                "expected_type": "table",
                "expected_fields": ["rows", "columns"],
                "operation_category": "Table Creation"
            },
            {
                "name": "Chart Operation - Sales Chart",
                "description": "Test chart creation with type specification",
                "request": {
                    "request": "Insert a line chart to show quarterly trends",
                    "request_id": "test_chart_001",
                    "context": {
                        "document_available": True,
                        "cursor_position": {"node_index": 18, "content_index": 0},
                        "selected_text": "",
                        "document_structure": {"paragraph_count": 7, "page_count": 1, "word_count": 134, "character_count": 672},
                        "document_content": "Financial Report\n\nData Analysis Section\n\nChart will be inserted here\n\nTrends and Insights\n\nConclusions",
                        "formatting_state": {"has_selection": False}
                    }
                },
                "expected_type": "chart",
                "expected_fields": ["chart_type"],
                "operation_category": "Chart Creation"
            },
            {
                "name": "Financial Data Operation - Stock Analysis",
                "description": "Test content generation with financial data integration",
                "request": {
                    "request": "Insert an analysis of AAPL stock performance and current market price",
                    "request_id": "test_financial_001",
                    "context": {
                        "document_available": True,
                        "cursor_position": {"node_index": 8, "content_index": 0},
                        "selected_text": "",
                        "document_structure": {"paragraph_count": 4, "page_count": 1, "word_count": 65, "character_count": 324},
                        "document_content": "Investment Analysis Report\n\nStock Performance Analysis\n\nInsert AAPL analysis here\n\nRecommendations",
                        "formatting_state": {"has_selection": False}
                    }
                },
                "expected_type": "insert",
                "expected_fields": ["content"],
                "operation_category": "Financial Data Analysis",
                "requires_financial_data": True
            }
        ]
    
    async def start_server(self):
        """Start the FastAPI server"""
        print("🚀 Starting FastAPI server...")
        
        # Change to the langraph-agents directory
        os.chdir(Path(__file__).parent.parent)
        
        # Start server process
        self.server_process = subprocess.Popen([
            sys.executable, "server.py"
        ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        
        # Wait for server to start
        for attempt in range(30):  # 30 second timeout
            try:
                async with httpx.AsyncClient() as client:
                    response = await client.get(f"{self.server_url}/health")
                    if response.status_code == 200:
                        print("✅ Server started successfully")
                        return True
            except:
                pass
            
            await asyncio.sleep(1)
            
        print("❌ Failed to start server")
        return False
    
    def stop_server(self):
        """Stop the FastAPI server"""
        if self.server_process:
            print("🛑 Stopping server...")
            self.server_process.terminate()
            try:
                self.server_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.server_process.kill()
                self.server_process.wait()
            print("✅ Server stopped")
    
    async def run_test_case(self, test_case):
        """Run a single test case"""
        print(f"\n--- Running: {test_case['name']} ---")
        print(f"Description: {test_case['description']}")
        
        test_result = {
            "test_case": test_case,
            "start_time": datetime.now().isoformat(),
            "success": False,
            "response": None,
            "error": None,
            "validation_results": [],
            "execution_time_ms": 0
        }
        
        start_time = time.time()
        
        try:
            async with httpx.AsyncClient(timeout=120.0) as client:  # 2 minute timeout
                print(f"📤 Sending request: {test_case['request']['request'][:50]}...")
                
                response = await client.post(
                    f"{self.server_url}/api/agent",
                    json=test_case["request"]
                )
                
                execution_time = int((time.time() - start_time) * 1000)
                test_result["execution_time_ms"] = execution_time
                
                if response.status_code == 200:
                    result_data = response.json()
                    test_result["response"] = result_data
                    test_result["success"] = True
                    
                    print(f"✅ Request successful ({execution_time}ms)")
                    print(f"   Type: {result_data.get('type')}")
                    print(f"   Response: {result_data.get('response', '')[:100]}...")
                    
                    # Display full response object
                    print(f"\n📋 FULL RESPONSE OBJECT:")
                    print(f"   {json.dumps(result_data, indent=4)}")
                    
                    # Validate response
                    validation_results = self.validate_response(test_case, result_data)
                    test_result["validation_results"] = validation_results
                    
                    # Print validation results
                    print(f"\n🔍 VALIDATION RESULTS:")
                    for validation in validation_results:
                        status = "✅" if validation["passed"] else "❌"
                        print(f"   {status} {validation['check']}")
                    
                else:
                    test_result["error"] = f"HTTP {response.status_code}: {response.text}"
                    print(f"❌ HTTP Error: {response.status_code}")
                    print(f"   Error: {response.text}")
        
        except Exception as e:
            execution_time = int((time.time() - start_time) * 1000)
            test_result["execution_time_ms"] = execution_time
            test_result["error"] = str(e)
            print(f"❌ Exception occurred: {str(e)}")
        
        test_result["end_time"] = datetime.now().isoformat()
        return test_result
    
    def validate_response(self, test_case, response):
        """Validate the agent response"""
        validations = []
        
        # Check response type
        expected_type = test_case["expected_type"]
        actual_type = response.get("type")
        validations.append({
            "check": f"Response type is '{expected_type}'",
            "passed": actual_type == expected_type,
            "expected": expected_type,
            "actual": actual_type
        })
        
        # Check required fields
        for field in test_case["expected_fields"]:
            has_field = field in response and response[field] is not None
            validations.append({
                "check": f"Has required field '{field}'",
                "passed": has_field,
                "expected": "field present",
                "actual": "present" if has_field else "missing"
            })
        
        # Check response message
        has_response_msg = "response" in response and len(response["response"]) > 0
        validations.append({
            "check": "Has response message",
            "passed": has_response_msg,
            "expected": "non-empty response",
            "actual": "present" if has_response_msg else "missing"
        })
        
        # Check request ID
        has_request_id = response.get("request_id") == test_case["request"]["request_id"]
        validations.append({
            "check": "Request ID matches",
            "passed": has_request_id,
            "expected": test_case["request"]["request_id"],
            "actual": response.get("request_id")
        })
        
        # Financial data specific validation
        if test_case.get("requires_financial_data"):
            has_financial_metadata = (
                response.get("metadata", {}).get("has_financial_data") == True
            )
            validations.append({
                "check": "Financial data was used",
                "passed": has_financial_metadata,
                "expected": "financial data used",
                "actual": "used" if has_financial_metadata else "not used"
            })
        
        return validations
    
    async def run_all_tests(self):
        """Run all test cases"""
        print("🧪 Starting comprehensive agent system tests")
        print(f"Test cases: {len(self.test_cases)}")
        print(f"Server URL: {self.server_url}")
        
        self.start_time = datetime.now()
        
        # Start server
        if not await self.start_server():
            print("❌ Cannot start server, aborting tests")
            return False
        
        try:
            # Run all test cases
            for i, test_case in enumerate(self.test_cases, 1):
                print(f"\n{'='*60}")
                print(f"TEST {i}/{len(self.test_cases)}: {test_case['operation_category']}")
                print(f"{'='*60}")
                
                result = await self.run_test_case(test_case)
                self.test_results.append(result)
                
                # Small delay between tests
                await asyncio.sleep(1)
            
            # Save results
            await self.save_results()
            
            # Print summary
            self.print_summary()
            
            return True
            
        finally:
            # Always stop server
            self.stop_server()
    
    async def save_results(self):
        """Save test results to files"""
        print("\n💾 Saving test results...")
        
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        
        # Create detailed results file
        detailed_results = {
            "test_run_info": {
                "timestamp": self.start_time.isoformat(),
                "total_tests": len(self.test_cases),
                "server_url": self.server_url,
                "duration_seconds": (datetime.now() - self.start_time).total_seconds()
            },
            "test_results": self.test_results
        }
        
        detailed_file = f"test/test_results_detailed_{timestamp}.json"
        with open(detailed_file, 'w') as f:
            json.dump(detailed_results, f, indent=2, default=str)
        print(f"✅ Detailed results saved: {detailed_file}")
        
        # Create summary file
        summary = {
            "test_run_summary": {
                "timestamp": self.start_time.isoformat(),
                "total_tests": len(self.test_results),
                "passed_tests": sum(1 for r in self.test_results if r["success"]),
                "failed_tests": sum(1 for r in self.test_results if not r["success"]),
                "average_execution_time_ms": sum(r["execution_time_ms"] for r in self.test_results) / len(self.test_results)
            },
            "test_cases_summary": []
        }
        
        for result in self.test_results:
            validations_passed = sum(1 for v in result["validation_results"] if v["passed"])
            total_validations = len(result["validation_results"])
            
            summary["test_cases_summary"].append({
                "name": result["test_case"]["name"],
                "operation_category": result["test_case"]["operation_category"],
                "success": result["success"],
                "execution_time_ms": result["execution_time_ms"],
                "validations_passed": f"{validations_passed}/{total_validations}",
                "response_type": result["response"].get("type") if result["response"] else None,
                "error": result["error"]
            })
        
        summary_file = f"test/test_results_summary_{timestamp}.json"
        with open(summary_file, 'w') as f:
            json.dump(summary, f, indent=2, default=str)
        print(f"✅ Summary results saved: {summary_file}")
        
        # Create human-readable report
        report_file = f"test/test_report_{timestamp}.txt"
        with open(report_file, 'w') as f:
            f.write("LANGRAPH AGENT SYSTEM TEST REPORT\n")
            f.write("=" * 50 + "\n\n")
            f.write(f"Test Run: {self.start_time.strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"Duration: {(datetime.now() - self.start_time).total_seconds():.2f} seconds\n")
            f.write(f"Total Tests: {len(self.test_results)}\n")
            f.write(f"Passed: {sum(1 for r in self.test_results if r['success'])}\n")
            f.write(f"Failed: {sum(1 for r in self.test_results if not r['success'])}\n\n")
            
            for i, result in enumerate(self.test_results, 1):
                f.write(f"TEST {i}: {result['test_case']['name']}\n")
                f.write("-" * 40 + "\n")
                f.write(f"Category: {result['test_case']['operation_category']}\n")
                f.write(f"Status: {'PASSED' if result['success'] else 'FAILED'}\n")
                f.write(f"Execution Time: {result['execution_time_ms']}ms\n")
                
                if result["response"]:
                    f.write(f"Response Type: {result['response'].get('type')}\n")
                    f.write(f"Response Message: {result['response'].get('response', '')[:100]}...\n")
                
                if result["error"]:
                    f.write(f"Error: {result['error']}\n")
                
                f.write("Validations:\n")
                for validation in result["validation_results"]:
                    status = "✅ PASS" if validation["passed"] else "❌ FAIL"
                    f.write(f"  {status}: {validation['check']}\n")
                
                f.write("\n")
        
        print(f"✅ Human-readable report saved: {report_file}")
    
    def print_summary(self):
        """Print test run summary"""
        print(f"\n{'='*60}")
        print("TEST RUN SUMMARY")
        print(f"{'='*60}")
        
        total_tests = len(self.test_results)
        passed_tests = sum(1 for r in self.test_results if r["success"])
        failed_tests = total_tests - passed_tests
        
        print(f"Total Tests: {total_tests}")
        print(f"Passed: {passed_tests} ✅")
        print(f"Failed: {failed_tests} ❌")
        print(f"Success Rate: {(passed_tests/total_tests)*100:.1f}%")
        print(f"Duration: {(datetime.now() - self.start_time).total_seconds():.2f} seconds")
        
        if failed_tests > 0:
            print(f"\nFailed Tests:")
            for result in self.test_results:
                if not result["success"]:
                    print(f"  ❌ {result['test_case']['name']}: {result['error']}")
        
        print(f"\n🎉 Test run complete!")

async def main():
    """Main test runner"""
    # Load environment variables from .env file
    load_dotenv()
    
    # Check if we're in the right directory
    if not os.path.exists("server.py"):
        print("❌ Error: Must run from langraph-agents directory")
        print("Current directory:", os.getcwd())
        return
    
    # Check for OpenAI API key
    api_key = os.getenv("OPENAI_API_KEY")
    if not api_key:
        print("❌ Error: OPENAI_API_KEY not found in environment or .env file")
        print("Please set OPENAI_API_KEY in your .env file or environment")
        return
    
    print(f"✅ OpenAI API key found (ends with: ...{api_key[-4:]})")
    print("🚀 Starting comprehensive agent system test...")
    
    # Run tests
    tester = AgentSystemTester()
    success = await tester.run_all_tests()
    
    if success:
        print("\n✅ All tests completed successfully!")
    else:
        print("\n❌ Some tests failed. Check the results files for details.")

if __name__ == "__main__":
    asyncio.run(main())