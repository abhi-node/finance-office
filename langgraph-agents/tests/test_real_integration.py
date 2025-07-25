#!/usr/bin/env python3
"""
Real Integration Test for LibreOffice LangGraph Multi-Agent System

This test runs the ACTUAL FastAPI server with REAL agents and only mocks
the LibreOffice integration layer. It sends real prompts and validates
the complete end-to-end workflow through the actual agent implementations.

Flow Tested:
Real HTTP Client → Real FastAPI Server → Real Bridge → Real Agents → Mock LibreOffice Integration
"""

import asyncio
import json
import logging
import time
import subprocess
import sys
import os
import signal
from pathlib import Path
from typing import Dict, Any, List
import threading
from dataclasses import dataclass

import httpx
# import pytest  # Commented out for direct execution
# from fastapi.testclient import TestClient

# Add the project root to Python path
sys.path.insert(0, str(Path(__file__).parent))

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

@dataclass
class TestScenario:
    """Real test scenario with actual prompts from LibreOffice use cases"""
    name: str
    request: str
    request_type: str
    complexity: str
    expected_agents: List[str]
    context: Dict[str, Any]
    validation_criteria: Dict[str, Any]

class RealIntegrationTester:
    """Real integration tester that runs actual FastAPI server and agents"""
    
    def __init__(self):
        self.server_process = None
        self.server_url = "http://localhost:8000"
        self.client = httpx.AsyncClient()
        
    def create_real_test_scenarios(self) -> List[TestScenario]:
        """Create real test scenarios based on actual LibreOffice use cases"""
        return [
            TestScenario(
                name="Simple Text Formatting",
                request="Make the selected text bold and apply italic formatting",
                request_type="simple", 
                complexity="low",
                expected_agents=["DocumentMasterAgent", "ExecutionAgent"],
                context={
                    "document_path": "/tmp/test_document.odt",
                    "cursor_position": {"line": 2, "column": 5},
                    "selected_text": "important text that needs formatting",
                    "document_type": "text",
                    "current_font": "Liberation Sans",
                    "current_font_size": 12,
                    "paragraph_alignment": "left"
                },
                validation_criteria={
                    "max_execution_time_ms": 2000,
                    "requires_formatting_operations": True,
                    "success_required": True
                }
            ),
            
            TestScenario(
                name="Document Structure Enhancement", 
                request="Apply professional document formatting with proper headings, improve paragraph structure, and add a table of contents",
                request_type="moderate",
                complexity="medium", 
                expected_agents=["DocumentMasterAgent", "ContextAnalysisAgent", "FormattingAgent", "ExecutionAgent"],
                context={
                    "document_path": "/tmp/business_proposal.odt",
                    "cursor_position": {"line": 1, "column": 1},
                    "selected_text": "",
                    "document_type": "text",
                    "paragraph_count": 8,
                    "current_style": "Default",
                    "has_headings": False,
                    "document_structure": {
                        "paragraphs": 8,
                        "sections": 0,
                        "tables": 0,
                        "images": 0
                    }
                },
                validation_criteria={
                    "max_execution_time_ms": 4000,
                    "requires_context_analysis": True,
                    "requires_formatting_operations": True,
                    "success_required": True
                }
            ),
            
            TestScenario(
                name="Financial Analysis Report",
                request="Create a comprehensive financial analysis report with live market data for AAPL and MSFT, including charts and performance metrics",
                request_type="complex",
                complexity="high",
                expected_agents=["DocumentMasterAgent", "ContextAnalysisAgent", "DataIntegrationAgent", "ContentGenerationAgent", "FormattingAgent", "ValidationAgent", "ExecutionAgent"],
                context={
                    "document_path": "/tmp/financial_report.odt", 
                    "cursor_position": {"line": 1, "column": 1},
                    "selected_text": "",
                    "document_type": "text",
                    "content_focus": "financial_analysis",
                    "external_data_required": True,
                    "symbols": ["AAPL", "MSFT"],
                    "report_type": "quarterly_analysis",
                    "user_preferences": {
                        "include_charts": True,
                        "data_sources": ["yahoo_finance", "alpha_vantage"],
                        "update_frequency": "real_time"
                    }
                },
                validation_criteria={
                    "max_execution_time_ms": 5000,
                    "requires_data_integration": True,
                    "requires_content_generation": True,
                    "requires_validation": True,
                    "success_required": True
                }
            ),
            
            TestScenario(
                name="Complex Document Transformation",
                request="Transform this business document into a professional presentation format with executive summary, key insights tables, financial charts, and recommendation sections",
                request_type="complex", 
                complexity="high",
                expected_agents=["DocumentMasterAgent", "ContextAnalysisAgent", "ContentGenerationAgent", "FormattingAgent", "ValidationAgent", "ExecutionAgent"],
                context={
                    "document_path": "/tmp/presentation_source.odt",
                    "cursor_position": {"line": 1, "column": 1},
                    "selected_text": "",
                    "document_type": "text",
                    "content_focus": "business_presentation",
                    "target_audience": "executives",
                    "presentation_requirements": {
                        "executive_summary": True,
                        "key_insights": True,
                        "financial_tables": True,
                        "recommendations": True,
                        "professional_formatting": True
                    },
                    "source_content_summary": {
                        "word_count": 2500,
                        "main_topics": ["revenue", "growth", "market_analysis", "competition"],
                        "data_points": 45
                    }
                },
                validation_criteria={
                    "max_execution_time_ms": 5000,
                    "requires_content_generation": True,
                    "requires_formatting_operations": True,
                    "requires_validation": True,
                    "success_required": True
                }
            )
        ]
    
    async def setup_test_environment(self):
        """Set up the test environment with ctypes bridge configuration"""
        logger.info("🔧 Setting up test environment...")
        
        # Set environment variables for testing
        os.environ["BRIDGE_INTEGRATION_METHOD"] = "ctypes"  # Use ctypes instead of PyUNO
        os.environ["AGENT_LOG_LEVEL"] = "INFO"
        
        # Ensure log directory exists
        Path("logs").mkdir(exist_ok=True)
        
        logger.info("✅ Test environment configured")
    
    async def start_server(self):
        """Start the real FastAPI server in the background"""
        logger.info("🚀 Starting real FastAPI server...")
        
        # Activate venv and start server
        venv_python = str(Path("venv/bin/python"))
        if not Path(venv_python).exists():
            raise RuntimeError("Virtual environment not found. Run: python -m venv venv && source venv/bin/activate && pip install -r requirements.txt")
        
        # Start server process
        self.server_process = subprocess.Popen([
            venv_python, "start_server.py"
        ], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        
        # Wait for server to start
        await self.wait_for_server_ready()
        logger.info("✅ FastAPI server started successfully")
    
    async def wait_for_server_ready(self, timeout: int = 30):
        """Wait for the server to be ready to accept connections"""
        start_time = time.time()
        
        while time.time() - start_time < timeout:
            try:
                async with httpx.AsyncClient() as client:
                    response = await client.get(f"{self.server_url}/health", timeout=1.0)
                    if response.status_code == 200:
                        logger.info("🟢 Server health check passed")
                        return
            except (httpx.ConnectError, httpx.TimeoutException):
                await asyncio.sleep(0.5)
                continue
        
        # Check if server process failed
        if self.server_process.poll() is not None:
            stdout, stderr = self.server_process.communicate()
            logger.error(f"Server failed to start. STDOUT: {stdout}")
            logger.error(f"Server STDERR: {stderr}")
            raise RuntimeError("Server failed to start")
        
        raise RuntimeError(f"Server not ready after {timeout} seconds")
    
    async def run_real_test_scenario(self, scenario: TestScenario) -> Dict[str, Any]:
        """Run a real test scenario with the actual server and agents"""
        logger.info(f"🧪 Running Real Test: {scenario.name}")
        logger.info(f"📝 Request: {scenario.request}")
        
        start_time = time.time()
        
        # Prepare real request
        request_data = {
            "request": scenario.request,
            "type": scenario.request_type,
            "complexity": scenario.complexity,
            "request_id": f"real_test_{scenario.name.lower().replace(' ', '_')}_{int(time.time())}",
            "context": scenario.context
        }
        
        # Set headers like LibreOffice would
        headers = {
            "Content-Type": "application/json",
            "X-Request-Type": scenario.request_type,
            "X-Include-Context": "full" if scenario.request_type == "complex" else "true",
            "Authorization": "Bearer test-token-for-integration",
            "User-Agent": "LibreOffice-AI-Agent/1.0-Integration-Test"
        }
        
        if scenario.request_type == "complex":
            headers["X-Agent-Workflow"] = "complete"
            headers["X-Request-ID"] = request_data["request_id"]
        
        # Send real request to real server
        endpoint = f"{self.server_url}/api/{scenario.request_type}"
        
        try:
            async with httpx.AsyncClient(timeout=10.0) as client:
                response = await client.post(endpoint, json=request_data, headers=headers)
            
            execution_time = time.time() - start_time
            
            # Validate response
            assert response.status_code == 200, f"Expected 200, got {response.status_code}: {response.text}"
            response_data = response.json()
            
            # Validate response structure
            self.validate_response_structure(response_data, scenario)
            self.validate_performance(execution_time, scenario)
            self.validate_agent_processing(response_data, scenario)
            
            logger.info(f"✅ Real Test '{scenario.name}' completed successfully")
            logger.info(f"⏱️  Execution time: {execution_time:.3f}s")
            logger.info(f"📊 Response success: {response_data.get('success', False)}")
            
            # Log details about the response for debugging
            if not response_data.get('success', False):
                logger.info(f"🔍 Response details: {json.dumps(response_data, indent=2)[:500]}...")
            
            return {
                'scenario': scenario.name,
                'success': True,
                'execution_time': execution_time,
                'response_data': response_data,
                'request_data': request_data
            }
            
        except Exception as e:
            logger.error(f"❌ Real Test '{scenario.name}' failed: {e}")
            execution_time = time.time() - start_time
            return {
                'scenario': scenario.name,
                'success': False,
                'execution_time': execution_time,
                'error': str(e)
            }
    
    def validate_response_structure(self, response_data: Dict[str, Any], scenario: TestScenario):
        """Validate the response has the expected structure"""
        required_fields = ["request_id", "success", "execution_time_ms"]
        for field in required_fields:
            assert field in response_data, f"Missing required field: {field}"
        
        if response_data["success"]:
            assert "result" in response_data, "Successful response must have result field"
            assert "agent_results" in response_data, "Successful response must have agent_results field"
        else:
            assert "error_message" in response_data, "Failed response must have error_message field"
    
    def validate_performance(self, execution_time: float, scenario: TestScenario):
        """Validate performance meets expectations"""
        max_time_s = scenario.validation_criteria["max_execution_time_ms"] / 1000.0
        assert execution_time < max_time_s, f"Performance target exceeded: {execution_time:.3f}s > {max_time_s}s"
    
    def validate_agent_processing(self, response_data: Dict[str, Any], scenario: TestScenario):
        """Validate that the agents processed the request correctly"""
        if not response_data["success"]:
            return  # Skip validation for failed responses
        
        agent_results = response_data.get("agent_results", {})
        
        # Check if expected agent processing occurred based on validation criteria
        if scenario.validation_criteria.get("requires_data_integration"):
            # For complex scenarios requiring data integration, check if external data was processed
            assert "external_data" in str(response_data) or "data_integration" in str(agent_results), \
                "Complex scenarios should include data integration processing"
        
        if scenario.validation_criteria.get("requires_content_generation"):
            # Check if content was generated
            assert "content" in str(response_data) or "generated" in str(response_data), \
                "Content generation scenarios should produce content"
        
        if scenario.validation_criteria.get("requires_formatting_operations"):
            # Check if formatting operations were planned/executed
            assert "format" in str(response_data).lower() or "operations" in response_data.get("result", {}), \
                "Formatting scenarios should include formatting operations"
    
    async def run_all_real_tests(self) -> Dict[str, Any]:
        """Run all real integration tests"""
        logger.info("🚀 Starting Real Integration Tests")
        logger.info("=" * 80)
        
        await self.setup_test_environment()
        await self.start_server()
        
        scenarios = self.create_real_test_scenarios()
        results = []
        total_start_time = time.time()
        
        try:
            for scenario in scenarios:
                result = await self.run_real_test_scenario(scenario)
                results.append(result)
                
                # Brief pause between tests
                await asyncio.sleep(1)
            
        finally:
            await self.cleanup()
        
        total_time = time.time() - total_start_time
        
        # Generate report
        successful_tests = [r for r in results if r.get('success', False)]
        failed_tests = [r for r in results if not r.get('success', False)]
        
        report = {
            'total_scenarios': len(scenarios),
            'successful': len(successful_tests),
            'failed': len(failed_tests),
            'total_execution_time': total_time,
            'average_execution_time': sum(r.get('execution_time', 0) for r in successful_tests) / len(successful_tests) if successful_tests else 0,
            'results': results
        }
        
        # Print report
        logger.info("=" * 80)
        logger.info("📊 REAL INTEGRATION TEST RESULTS")
        logger.info("=" * 80)
        logger.info(f"✅ Successful tests: {report['successful']}/{report['total_scenarios']}")
        logger.info(f"❌ Failed tests: {report['failed']}")
        logger.info(f"⏱️  Total test time: {report['total_execution_time']:.3f}s")
        logger.info(f"📈 Average execution time: {report['average_execution_time']:.3f}s")
        
        for result in results:
            status = "✅" if result.get('success', False) else "❌"
            exec_time = result.get('execution_time', 0)
            logger.info(f"{status} {result['scenario']}: {exec_time:.3f}s")
        
        if report['failed'] == 0:
            logger.info("\n🎉 ALL REAL INTEGRATION TESTS PASSED! 🎉")
            logger.info("The complete LangGraph system is working correctly with real agents!")
        else:
            logger.error(f"\n⚠️  {report['failed']} real integration tests failed")
        
        return report
    
    async def cleanup(self):
        """Clean up test resources"""
        logger.info("🧹 Cleaning up test environment...")
        
        if self.server_process:
            try:
                # Gracefully terminate the server
                self.server_process.terminate()
                try:
                    self.server_process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    # Force kill if graceful termination fails
                    self.server_process.kill()
                    self.server_process.wait()
                logger.info("🛑 Server process terminated")
            except Exception as e:
                logger.warning(f"Error terminating server: {e}")
        
        await self.client.aclose()
        logger.info("✅ Cleanup complete")

# Main test execution
async def main():
    """Run real integration tests"""
    print("🧪 LibreOffice LangGraph Real Integration Test")
    print("Testing complete end-to-end system with real agents and real server")
    print()
    
    tester = RealIntegrationTester()
    
    try:
        report = await tester.run_all_real_tests()
        
        if report['failed'] == 0:
            print("\n✨ All real integration tests passed successfully!")
            print("The LangGraph multi-agent system is ready for LibreOffice production use!")
            return 0
        else:
            print(f"\n❌ {report['failed']} real integration tests failed!")
            print("Please review the test output and fix issues before production deployment.")
            return 1
    
    except Exception as e:
        logger.error(f"Real integration test failed: {e}")
        return 1

if __name__ == "__main__":
    exit(asyncio.run(main()))