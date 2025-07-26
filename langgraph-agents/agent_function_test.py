#!/usr/bin/env python3
"""
Comprehensive Agent Function Test Suite

This script tests the LangGraph agent system with various request types,
captures all responses, and logs server output for debugging.
"""

import asyncio
import json
import time
import requests
import threading
import subprocess
import sys
from datetime import datetime
from typing import Dict, List, Any
import signal
import os

class AgentTester:
    def __init__(self, server_url: str = "http://127.0.0.1:8000"):
        self.server_url = server_url
        self.server_process = None
        self.test_results = []
        self.server_logs = []
        
    def start_server(self):
        """Start the agent server and capture logs."""
        print("🚀 Starting agent server...")
        
        # Start server process with log capture
        self.server_process = subprocess.Popen(
            ["python", "ai_agent_server.py"],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            universal_newlines=True
        )
        
        # Wait for server to initialize
        print("⏳ Waiting for server initialization...")
        time.sleep(8)
        
        # Check if server is running
        try:
            response = requests.get(f"{self.server_url}/health", timeout=5)
            if response.status_code == 200:
                print("✅ Server started successfully")
                return True
        except Exception as e:
            print(f"❌ Server failed to start: {e}")
            return False
        
        return False
    
    def stop_server(self):
        """Stop the agent server."""
        if self.server_process:
            print("🔄 Stopping server...")
            self.server_process.terminate()
            self.server_process.wait(timeout=10)
            print("✅ Server stopped")
    
    def capture_server_logs(self):
        """Capture server logs in background thread."""
        def log_reader():
            if self.server_process:
                for line in iter(self.server_process.stdout.readline, ''):
                    self.server_logs.append(f"[{datetime.now().strftime('%H:%M:%S.%f')[:-3]}] {line.strip()}")
                    print(f"SERVER: {line.strip()}")
        
        log_thread = threading.Thread(target=log_reader, daemon=True)
        log_thread.start()
        return log_thread
    
    def create_test_request(self, test_name: str, request_text: str, 
                          context: Dict[str, Any] = None, 
                          user_preferences: Dict[str, Any] = None) -> Dict[str, Any]:
        """Create a standardized test request."""
        request_id = f"test_{test_name}_{int(time.time())}"
        
        default_context = {
            "cursor_position": {"line": 1, "column": 1},
            "selected_text": "",
            "document_structure": {
                "paragraph_count": 5,
                "page_count": 1,
                "word_count": 45
            },
            "formatting_state": {"has_selection": False}
        }
        
        default_preferences = {
            "language": "en-US",
            "currency": "USD",
            "date_format": "MM/DD/YYYY"
        }
        
        return {
            "request": request_text,
            "request_id": request_id,
            "context": context or default_context,
            "user_preferences": user_preferences or default_preferences,
            "session_id": f"test_session_{test_name}"
        }
    
    def send_request(self, test_request: Dict[str, Any], test_name: str) -> Dict[str, Any]:
        """Send request to agent and capture response."""
        print(f"\n📨 Testing: {test_name}")
        print(f"Request: {test_request['request'][:80]}...")
        
        start_time = time.time()
        
        try:
            response = requests.post(
                f"{self.server_url}/api/agent",
                json=test_request,
                headers={
                    "Content-Type": "application/json",
                    "X-Request-ID": test_request["request_id"],
                    "X-Agent-System": "langgraph",
                    "X-LibreOffice-Version": "24.8.0",
                    "X-Integration-Layer": "test_suite"
                },
                timeout=30
            )
            
            execution_time = (time.time() - start_time) * 1000
            
            if response.status_code == 200:
                result = response.json()
                print(f"✅ Response received ({execution_time:.1f}ms)")
                print(f"   Success: {result.get('success', False)}")
                print(f"   Operations: {len(result.get('operations', []))}")
                print(f"   Agents Used: {result.get('agents_used', [])}")
                
                return {
                    "test_name": test_name,
                    "request": test_request,
                    "response": result,
                    "status_code": response.status_code,
                    "execution_time_ms": execution_time,
                    "timestamp": datetime.now().isoformat()
                }
            else:
                print(f"❌ HTTP Error: {response.status_code}")
                return {
                    "test_name": test_name,
                    "request": test_request,
                    "error": f"HTTP {response.status_code}: {response.text}",
                    "status_code": response.status_code,
                    "execution_time_ms": execution_time,
                    "timestamp": datetime.now().isoformat()
                }
                
        except Exception as e:
            execution_time = (time.time() - start_time) * 1000
            print(f"❌ Request failed: {e}")
            return {
                "test_name": test_name,
                "request": test_request,
                "error": str(e),
                "status_code": 0,
                "execution_time_ms": execution_time,
                "timestamp": datetime.now().isoformat()
            }
    
    def run_test_suite(self):
        """Run comprehensive test suite."""
        print("🧪 Starting Comprehensive Agent Function Test Suite")
        print("=" * 60)
        
        # Define test cases
        test_cases = [
            {
                "name": "simple_text_insertion",
                "request": "Insert the text 'Hello World' at the current cursor position",
                "context": {
                    "cursor_position": {"line": 5, "column": 10},
                    "selected_text": "",
                    "document_structure": {"paragraph_count": 3, "page_count": 1, "word_count": 25},
                    "formatting_state": {"has_selection": False, "current_style": "Normal"}
                }
            },
            {
                "name": "financial_table_creation",
                "request": "Create a quarterly financial summary table with Q1-Q4 2024 revenue data, including columns for Quarter, Revenue, Growth %, and Notes",
                "context": {
                    "cursor_position": {"line": 10, "column": 1},
                    "selected_text": "Financial Summary",
                    "document_structure": {"paragraph_count": 15, "page_count": 2, "word_count": 450},
                    "formatting_state": {"has_selection": True, "current_style": "Heading 2"}
                }
            },
            {
                "name": "document_formatting",
                "request": "Apply professional business formatting to the selected text: bold, 14pt font, center alignment",
                "context": {
                    "cursor_position": {"line": 3, "column": 5},
                    "selected_text": "QUARTERLY REPORT 2024",
                    "document_structure": {"paragraph_count": 8, "page_count": 1, "word_count": 120},
                    "formatting_state": {"has_selection": True, "current_style": "Normal"}
                }
            },
            {
                "name": "complex_document_analysis",
                "request": "Analyze the current document structure and suggest improvements for readability, including adding headers, bullet points, and proper spacing",
                "context": {
                    "cursor_position": {"line": 1, "column": 1},
                    "selected_text": "",
                    "document_structure": {"paragraph_count": 25, "page_count": 3, "word_count": 850},
                    "formatting_state": {"has_selection": False, "current_style": "Normal"}
                }
            },
            {
                "name": "data_integration_request",
                "request": "Insert current stock market data for AAPL, GOOGL, and MSFT in a formatted table with price, change, and volume",
                "context": {
                    "cursor_position": {"line": 12, "column": 1},
                    "selected_text": "",
                    "document_structure": {"paragraph_count": 10, "page_count": 1, "word_count": 200},
                    "formatting_state": {"has_selection": False, "current_style": "Normal"}
                }
            },
            {
                "name": "template_application",
                "request": "Apply a professional business letter template with company header, date, and signature blocks",
                "context": {
                    "cursor_position": {"line": 1, "column": 1},
                    "selected_text": "",
                    "document_structure": {"paragraph_count": 1, "page_count": 1, "word_count": 5},
                    "formatting_state": {"has_selection": False, "current_style": "Normal"}
                }
            },
            {
                "name": "chart_creation",
                "request": "Create a bar chart showing monthly sales data from January to June 2024 with values: Jan $10k, Feb $12k, Mar $15k, Apr $13k, May $18k, Jun $20k",
                "context": {
                    "cursor_position": {"line": 20, "column": 1},
                    "selected_text": "",
                    "document_structure": {"paragraph_count": 18, "page_count": 2, "word_count": 600},
                    "formatting_state": {"has_selection": False, "current_style": "Normal"}
                }
            },
            {
                "name": "multilingual_content",
                "request": "Insert a bilingual greeting in English and Spanish: 'Welcome to our annual report / Bienvenidos a nuestro informe anual'",
                "context": {
                    "cursor_position": {"line": 2, "column": 1},
                    "selected_text": "",
                    "document_structure": {"paragraph_count": 5, "page_count": 1, "word_count": 75},
                    "formatting_state": {"has_selection": False, "current_style": "Normal"}
                },
                "user_preferences": {
                    "language": "en-US",
                    "secondary_language": "es-ES",
                    "currency": "USD"
                }
            },
            {
                "name": "error_handling_test",
                "request": "This is an intentionally invalid request with malformed data requirements that should trigger error handling",
                "context": {
                    "cursor_position": {"line": -1, "column": -1},  # Invalid position
                    "selected_text": "",
                    "document_structure": {"paragraph_count": 0, "page_count": 0, "word_count": 0},
                    "formatting_state": {"has_selection": None}  # Invalid state
                }
            },
            {
                "name": "performance_stress_test",
                "request": "Create a comprehensive 50-page business report with executive summary, financial analysis, market trends, competitive analysis, risk assessment, recommendations, appendices, charts, tables, and formatted sections with professional styling throughout",
                "context": {
                    "cursor_position": {"line": 1, "column": 1},
                    "selected_text": "",
                    "document_structure": {"paragraph_count": 1, "page_count": 1, "word_count": 0},
                    "formatting_state": {"has_selection": False, "current_style": "Normal"}
                }
            }
        ]
        
        # Run tests
        for test_case in test_cases:
            test_request = self.create_test_request(
                test_case["name"],
                test_case["request"],
                test_case.get("context"),
                test_case.get("user_preferences")
            )
            
            result = self.send_request(test_request, test_case["name"])
            self.test_results.append(result)
            
            # Wait between tests to avoid overwhelming the server
            time.sleep(2)
        
        print(f"\n✅ Test suite completed. {len(self.test_results)} tests executed.")
    
    def save_results(self, filename: str = "agent_test_results.txt"):
        """Save all test results and server logs to file."""
        print(f"💾 Saving results to {filename}...")
        
        with open(filename, 'w', encoding='utf-8') as f:
            f.write("COMPREHENSIVE AGENT FUNCTION TEST RESULTS\n")
            f.write("=" * 80 + "\n")
            f.write(f"Test executed at: {datetime.now().isoformat()}\n")
            f.write(f"Total tests: {len(self.test_results)}\n")
            f.write(f"Server URL: {self.server_url}\n")
            f.write("\n")
            
            # Summary statistics
            successful_tests = [r for r in self.test_results if r.get('response', {}).get('success', False)]
            failed_tests = [r for r in self.test_results if not r.get('response', {}).get('success', False)]
            
            f.write("SUMMARY STATISTICS:\n")
            f.write("-" * 40 + "\n")
            f.write(f"Successful requests: {len(successful_tests)}\n")
            f.write(f"Failed requests: {len(failed_tests)}\n")
            f.write(f"Success rate: {len(successful_tests)/len(self.test_results)*100:.1f}%\n")
            
            avg_time = sum(r.get('execution_time_ms', 0) for r in self.test_results) / len(self.test_results)
            f.write(f"Average response time: {avg_time:.1f}ms\n")
            f.write("\n")
            
            # Detailed test results
            f.write("DETAILED TEST RESULTS:\n")
            f.write("=" * 80 + "\n")
            
            for i, result in enumerate(self.test_results, 1):
                f.write(f"\nTEST {i}: {result['test_name']}\n")
                f.write("-" * 60 + "\n")
                f.write(f"Timestamp: {result['timestamp']}\n")
                f.write(f"Status Code: {result['status_code']}\n")
                f.write(f"Execution Time: {result.get('execution_time_ms', 0):.1f}ms\n")
                
                f.write(f"\nREQUEST:\n")
                f.write(json.dumps(result['request'], indent=2))
                f.write(f"\n\nRESPONSE:\n")
                
                if 'response' in result:
                    f.write(json.dumps(result['response'], indent=2))
                elif 'error' in result:
                    f.write(f"ERROR: {result['error']}\n")
                
                f.write("\n" + "=" * 60 + "\n")
            
            # Server logs
            f.write("\n\nSERVER LOGS:\n")
            f.write("=" * 80 + "\n")
            for log_line in self.server_logs:
                f.write(log_line + "\n")
        
        print(f"✅ Results saved to {filename}")

def main():
    """Main test execution function."""
    tester = AgentTester()
    
    try:
        # Start server
        if not tester.start_server():
            print("❌ Failed to start server. Exiting.")
            return
        
        # Start log capture
        log_thread = tester.capture_server_logs()
        
        # Run tests
        tester.run_test_suite()
        
        # Wait a bit for final logs
        time.sleep(2)
        
        # Save results
        tester.save_results()
        
    except KeyboardInterrupt:
        print("\n🛑 Test interrupted by user")
    except Exception as e:
        print(f"❌ Test suite failed: {e}")
    finally:
        # Cleanup
        tester.stop_server()

if __name__ == "__main__":
    main()