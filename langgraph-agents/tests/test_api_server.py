#!/usr/bin/env python3
"""
Comprehensive test suite for LibreOffice LangGraph FastAPI server

This test suite simulates the complete end-to-end flow:
1. LibreOffice C++ AgentCoordinator making HTTP/WebSocket requests
2. FastAPI server processing requests through bridge.py  
3. LangGraph agents coordinating to fulfill requests
4. Responses being sent back to LibreOffice in compatible format

Tests cover all endpoints, request/response formats, error handling,
and WebSocket streaming functionality.
"""

import asyncio
import json
import logging
import pytest
import time
import uuid
from typing import Dict, Any, List

import httpx
import websockets
from fastapi.testclient import TestClient

# Import server components
from api_server import app, LibreOfficeRequest, LibreOfficeResponse
from bridge import initialize_bridge, shutdown_bridge

# Configure test logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class LibreOfficeSimulator:
    """
    Simulates LibreOffice C++ AgentCoordinator behavior for testing
    
    This class replicates the exact request patterns, headers, and data formats
    that the real LibreOffice C++ implementation sends to the FastAPI server.
    """
    
    def __init__(self, base_url: str = "http://localhost:8000"):
        self.base_url = base_url
        self.session = httpx.Client(base_url=base_url, timeout=30.0)
        
    def generate_request_id(self) -> str:
        """Generate request ID matching LibreOffice format"""
        return f"req_{int(time.time())}_{uuid.uuid4().hex[:8]}"
    
    def create_cpp_headers(self, request_type: str, include_context: str = "true") -> Dict[str, str]:
        """Create headers matching C++ AgentCoordinator implementation"""
        headers = {
            "Content-Type": "application/json",
            "X-Request-Type": request_type,
            "X-Include-Context": include_context,
            "User-Agent": "LibreOffice-AI-Agent/1.0"
        }
        
        if request_type == "complex":
            headers["X-Agent-Workflow"] = "complete"
            
        # Simulate auth header (TODO: real implementation will have proper auth)
        headers["Authorization"] = "Bearer your-api-key-here"
        
        return headers
    
    def create_document_context(self, scenario: str = "basic") -> Dict[str, Any]:
        """Create document context matching UNO PropertyValue structure"""
        contexts = {
            "basic": {
                "document_path": "test_document.odt",
                "cursor_position": {"line": 1, "column": 5},
                "selected_text": "",
                "document_type": "text"
            },
            "with_selection": {
                "document_path": "financial_report.odt", 
                "cursor_position": {"line": 3, "column": 10},
                "selected_text": "Q4 Revenue Summary",
                "document_type": "text",
                "paragraph_count": 5,
                "table_count": 2
            },
            "financial": {
                "document_path": "market_analysis.odt",
                "cursor_position": {"line": 10, "column": 1},
                "selected_text": "",
                "document_type": "text",
                "content_focus": "financial_data",
                "external_data_required": True
            }
        }
        return contexts.get(scenario, contexts["basic"])
    
    def send_simple_request(self, user_message: str, context_scenario: str = "basic") -> httpx.Response:
        """Simulate simple request from LibreOffice (< 2 seconds target)"""
        request_data = LibreOfficeRequest(
            request=user_message,
            type="simple", 
            complexity="low",
            request_id=self.generate_request_id(),
            context=self.create_document_context(context_scenario)
        )
        
        headers = self.create_cpp_headers("simple")
        
        logger.info(f"Sending simple request: {user_message}")
        return self.session.post("/api/simple", json=request_data.model_dump(), headers=headers)
    
    def send_moderate_request(self, user_message: str, context_scenario: str = "with_selection") -> httpx.Response:
        """Simulate moderate request from LibreOffice (< 4 seconds target)"""
        request_data = LibreOfficeRequest(
            request=user_message,
            type="moderate",
            complexity="medium", 
            request_id=self.generate_request_id(),
            context=self.create_document_context(context_scenario)
        )
        
        headers = self.create_cpp_headers("moderate", "true")
        
        logger.info(f"Sending moderate request: {user_message}")
        return self.session.post("/api/moderate", json=request_data.model_dump(), headers=headers)
    
    def send_complex_request(self, user_message: str, context_scenario: str = "financial") -> httpx.Response:
        """Simulate complex request from LibreOffice (< 5 seconds target)"""
        request_data = LibreOfficeRequest(
            request=user_message,
            type="complex",
            complexity="high",
            request_id=self.generate_request_id(), 
            context=self.create_document_context(context_scenario)
        )
        
        headers = self.create_cpp_headers("complex", "full")
        headers["X-Request-ID"] = request_data.request_id
        
        logger.info(f"Sending complex request: {user_message}")
        return self.session.post("/api/complex", json=request_data.model_dump(), headers=headers)
    
    def send_general_request(self, user_message: str) -> httpx.Response:
        """Simulate general processing request (legacy endpoint)"""
        request_data = LibreOfficeRequest(
            request=user_message,
            type="general",
            complexity="auto",
            request_id=self.generate_request_id(),
            context=self.create_document_context()
        )
        
        headers = self.create_cpp_headers("general")
        
        logger.info(f"Sending general request: {user_message}")
        return self.session.post("/api/process", json=request_data.model_dump(), headers=headers)

class WebSocketTester:
    """Test WebSocket functionality matching LibreOffice expectations"""
    
    def __init__(self, ws_url: str = "ws://localhost:8000/ws/libreoffice"):
        self.ws_url = ws_url
        
    async def test_websocket_connection(self) -> Dict[str, Any]:
        """Test WebSocket connection with proper subprotocol"""
        try:
            async with websockets.connect(
                self.ws_url, 
                subprotocols=["langgraph-ai"],
                ping_interval=20,
                ping_timeout=10
            ) as websocket:
                
                logger.info("WebSocket connected successfully")
                
                # Wait for welcome message
                welcome_data = await websocket.recv()
                welcome_msg = json.loads(welcome_data)
                
                assert welcome_msg["type"] == "connection"
                assert welcome_msg["content"]["status"] == "connected"
                assert welcome_msg["content"]["protocol"] == "langgraph-ai"
                
                return welcome_msg
                
        except Exception as e:
            logger.error(f"WebSocket connection failed: {e}")
            raise
    
    async def test_websocket_request_processing(self) -> Dict[str, Any]:
        """Test complete request processing via WebSocket with streaming"""
        async with websockets.connect(
            self.ws_url,
            subprotocols=["langgraph-ai"],
            ping_interval=20
        ) as websocket:
            
            # Skip welcome message
            await websocket.recv()
            
            # Send processing request
            request_data = {
                "type": "request",
                "request_id": f"ws_test_{int(time.time())}",
                "message": "Create a comprehensive financial analysis table with market data",
                "context": {
                    "document_path": "test.odt",
                    "cursor_position": {"line": 1, "column": 1},
                    "document_type": "text"
                }
            }
            
            await websocket.send(json.dumps(request_data))
            logger.info("Sent WebSocket processing request")
            
            # Collect all response messages
            messages = []
            timeout_count = 0
            max_timeouts = 10
            
            while timeout_count < max_timeouts:
                try:
                    message_data = await asyncio.wait_for(websocket.recv(), timeout=2.0)
                    message = json.loads(message_data)
                    messages.append(message)
                    
                    logger.info(f"Received WebSocket message: {message['type']}")
                    
                    # Stop when we get final response or error
                    if message["type"] in ["response", "error"]:
                        break
                        
                except asyncio.TimeoutError:
                    timeout_count += 1
                    logger.debug(f"WebSocket timeout {timeout_count}/{max_timeouts}")
                    continue
            
            return {
                "request": request_data,
                "messages": messages,
                "final_message": messages[-1] if messages else None
            }
    
    async def test_websocket_cancellation(self) -> Dict[str, Any]:
        """Test request cancellation via WebSocket"""
        async with websockets.connect(
            self.ws_url,
            subprotocols=["langgraph-ai"]
        ) as websocket:
            
            # Skip welcome message
            await websocket.recv()
            
            # Send cancellation request
            cancel_data = {
                "type": "cancel",
                "request_id": "test_cancel_request"
            }
            
            await websocket.send(json.dumps(cancel_data))
            logger.info("Sent WebSocket cancellation request")
            
            # Wait for cancellation response
            response_data = await websocket.recv()
            response = json.loads(response_data)
            
            assert response["type"] == "cancelled"
            assert response["content"]["status"] == "cancelled"
            
            return response

# Test Suite
class TestLibreOfficeAPIServer:
    """Main test class for comprehensive API server testing"""
    
    @classmethod
    def setup_class(cls):
        """Set up test environment"""
        cls.client = TestClient(app)
        cls.simulator = LibreOfficeSimulator()
        cls.ws_tester = WebSocketTester()
        logger.info("Test environment initialized")
    
    def test_health_check(self):
        """Test server health endpoint"""
        response = self.client.get("/health")
        assert response.status_code == 200
        
        data = response.json()
        assert "status" in data
        assert data["status"] == "healthy"
        logger.info("✅ Health check passed")
    
    def test_status_endpoint(self):
        """Test server status endpoint"""
        response = self.client.get("/status")
        assert response.status_code == 200
        
        data = response.json()
        assert "server" in data
        assert "bridge_status" in data
        assert data["server"] == "LibreOffice LangGraph API Server"
        logger.info("✅ Status endpoint passed")
    
    def test_simple_request_format(self):
        """Test simple request format compatibility"""
        response = self.simulator.send_simple_request(
            "Make selected text bold",
            "basic"
        )
        
        assert response.status_code == 200
        data = response.json()
        
        # Validate LibreOfficeResponse format
        assert "request_id" in data
        assert "success" in data
        assert "execution_time_ms" in data
        assert isinstance(data["execution_time_ms"], (int, float))
        
        logger.info(f"✅ Simple request format test passed - {data['execution_time_ms']:.1f}ms")
    
    def test_moderate_request_format(self):
        """Test moderate request format compatibility"""
        response = self.simulator.send_moderate_request(
            "Create a professional report header with proper formatting",
            "with_selection"
        )
        
        assert response.status_code == 200
        data = response.json()
        
        # Validate response structure
        assert "request_id" in data
        assert "success" in data
        assert "result" in data
        assert "metadata" in data
        assert data["metadata"]["request_type"] == "moderate"
        
        logger.info(f"✅ Moderate request format test passed - {data['execution_time_ms']:.1f}ms")
    
    def test_complex_request_format(self):
        """Test complex request format compatibility"""
        response = self.simulator.send_complex_request(
            "Analyze market trends and create comprehensive financial report with charts and data integration",
            "financial"
        )
        
        assert response.status_code == 200
        data = response.json()
        
        # Validate comprehensive response  
        assert "request_id" in data
        assert "success" in data
        assert "agent_results" in data
        assert "final_state" in data
        assert data["metadata"]["request_type"] == "complex"
        
        logger.info(f"✅ Complex request format test passed - {data['execution_time_ms']:.1f}ms")
    
    def test_general_request_format(self):
        """Test general processing endpoint (legacy compatibility)"""
        response = self.simulator.send_general_request(
            "Help me improve this document"
        )
        
        assert response.status_code == 200
        data = response.json()
        
        assert "request_id" in data
        assert "success" in data
        assert data["metadata"]["request_type"] == "general"
        
        logger.info(f"✅ General request format test passed - {data['execution_time_ms']:.1f}ms")
    
    def test_header_validation(self):
        """Test that expected headers are properly processed"""
        # Test with all expected headers
        request_data = LibreOfficeRequest(
            request="Test header validation",
            type="simple",
            complexity="low"
        )
        
        headers = {
            "Content-Type": "application/json",
            "X-Request-Type": "simple",
            "X-Include-Context": "true", 
            "Authorization": "Bearer test-token",
            "User-Agent": "LibreOffice-AI-Agent/1.0"
        }
        
        response = self.client.post("/api/simple", json=request_data.model_dump(), headers=headers)
        assert response.status_code == 200
        
        data = response.json()
        assert "metadata" in data
        assert "headers" in data["metadata"]
        
        logger.info("✅ Header validation test passed")
    
    def test_error_handling(self):
        """Test error handling with invalid requests"""
        # Test invalid JSON
        response = self.client.post("/api/simple", content="invalid json", headers={"Content-Type": "application/json"})
        assert response.status_code == 422  # Validation error
        
        # Test missing required fields
        response = self.client.post("/api/simple", json={})
        assert response.status_code == 422
        
        logger.info("✅ Error handling test passed")
    
    @pytest.mark.asyncio
    async def test_websocket_connection(self):
        """Test WebSocket connection establishment"""
        welcome_msg = await self.ws_tester.test_websocket_connection()
        
        assert welcome_msg["type"] == "connection"
        assert "capabilities" in welcome_msg["content"]
        
        logger.info("✅ WebSocket connection test passed")
    
    @pytest.mark.asyncio 
    async def test_websocket_request_processing(self):
        """Test complete WebSocket request processing flow"""
        result = await self.ws_tester.test_websocket_request_processing()
        
        assert len(result["messages"]) > 0
        
        # Check for expected message types
        message_types = [msg["type"] for msg in result["messages"]]
        assert "progress" in message_types or "response" in message_types
        
        # Validate final response
        final_msg = result["final_message"]
        if final_msg:
            assert final_msg["type"] in ["response", "error"]
            assert "request_id" in final_msg
        
        logger.info("✅ WebSocket request processing test passed")
    
    @pytest.mark.asyncio
    async def test_websocket_cancellation(self):
        """Test WebSocket cancellation functionality"""
        response = await self.ws_tester.test_websocket_cancellation()
        
        assert response["type"] == "cancelled"
        assert response["content"]["status"] == "cancelled"
        
        logger.info("✅ WebSocket cancellation test passed")
    
    def test_performance_targets(self):
        """Test that performance targets are met"""
        # Test simple request performance (< 2 seconds)
        start_time = time.time()
        response = self.simulator.send_simple_request("Make text bold")
        simple_time = time.time() - start_time
        
        assert response.status_code == 200
        assert simple_time < 2.0, f"Simple request took {simple_time:.2f}s (target: < 2.0s)"
        
        # Test moderate request performance (< 4 seconds)
        start_time = time.time()
        response = self.simulator.send_moderate_request("Format document professionally")
        moderate_time = time.time() - start_time
        
        assert response.status_code == 200
        assert moderate_time < 4.0, f"Moderate request took {moderate_time:.2f}s (target: < 4.0s)"
        
        logger.info(f"✅ Performance targets met - Simple: {simple_time:.2f}s, Moderate: {moderate_time:.2f}s")
    
    def test_concurrent_requests(self):
        """Test handling multiple concurrent requests"""
        import concurrent.futures
        
        def send_request(i):
            return self.simulator.send_simple_request(f"Test concurrent request {i}")
        
        # Send 5 concurrent requests
        with concurrent.futures.ThreadPoolExecutor(max_workers=5) as executor:
            futures = [executor.submit(send_request, i) for i in range(5)]
            responses = [future.result() for future in concurrent.futures.as_completed(futures)]
        
        # All should succeed
        for response in responses:
            assert response.status_code == 200
            data = response.json()
            assert data["success"] is not None
        
        logger.info("✅ Concurrent requests test passed")

class TestEndToEndWorkflow:
    """Test complete end-to-end workflows simulating real LibreOffice usage"""
    
    def setup_method(self):
        """Set up for each test method"""
        self.simulator = LibreOfficeSimulator()
    
    def test_document_formatting_workflow(self):
        """Test complete document formatting workflow"""
        logger.info("🔄 Testing document formatting workflow...")
        
        # Step 1: Simple formatting
        response1 = self.simulator.send_simple_request(
            "Make the selected text bold",
            "with_selection"
        )
        assert response1.status_code == 200
        
        # Step 2: Moderate formatting 
        response2 = self.simulator.send_moderate_request(
            "Apply professional heading styles and add table of contents",
            "with_selection"
        )
        assert response2.status_code == 200
        
        # Step 3: Complex document enhancement
        response3 = self.simulator.send_complex_request(
            "Create comprehensive document with financial analysis, charts, and executive summary",
            "financial"
        )
        assert response3.status_code == 200
        
        logger.info("✅ Document formatting workflow completed successfully")
    
    def test_financial_document_workflow(self):
        """Test financial document creation with data integration"""
        logger.info("🔄 Testing financial document workflow...")
        
        # Complex request with financial data
        response = self.simulator.send_complex_request(
            "Create quarterly financial report with latest market data, trends analysis, and performance charts for Apple Inc (AAPL)",
            "financial"
        )
        
        assert response.status_code == 200
        data = response.json()
        
        # Should have agent coordination results
        assert "agent_results" in data
        assert "final_state" in data
        
        logger.info("✅ Financial document workflow completed successfully")
    
    @pytest.mark.asyncio
    async def test_realtime_complex_workflow(self):
        """Test complex workflow with real-time WebSocket updates"""
        logger.info("🔄 Testing real-time complex workflow...")
        
        ws_tester = WebSocketTester()
        result = await ws_tester.test_websocket_request_processing()
        
        # Should have progress updates
        progress_messages = [msg for msg in result["messages"] if msg["type"] == "progress"]
        assert len(progress_messages) > 0, "Expected progress updates via WebSocket"
        
        logger.info("✅ Real-time complex workflow completed successfully")

# Test Runner
async def run_all_tests():
    """Run comprehensive test suite"""
    logger.info("🧪 Starting LibreOffice LangGraph API Server Test Suite")
    logger.info("="*70)
    
    # Initialize test environment
    test_client = TestClient(app)
    
    # Run HTTP API tests
    logger.info("📡 Testing HTTP API Endpoints...")
    http_tests = TestLibreOfficeAPIServer()
    http_tests.setup_class()
    
    try:
        http_tests.test_health_check()
        http_tests.test_status_endpoint()
        http_tests.test_simple_request_format()
        http_tests.test_moderate_request_format()
        http_tests.test_complex_request_format()
        http_tests.test_general_request_format()
        http_tests.test_header_validation()
        http_tests.test_error_handling()
        http_tests.test_performance_targets()
        http_tests.test_concurrent_requests()
        
        logger.info("✅ All HTTP API tests passed!")
        
    except Exception as e:
        logger.error(f"❌ HTTP API test failed: {e}")
        raise
    
    # Run WebSocket tests
    logger.info("\n🌐 Testing WebSocket Functionality...")
    try:
        await http_tests.test_websocket_connection()
        await http_tests.test_websocket_request_processing()
        await http_tests.test_websocket_cancellation()
        
        logger.info("✅ All WebSocket tests passed!")
        
    except Exception as e:
        logger.error(f"❌ WebSocket test failed: {e}")
        raise
    
    # Run end-to-end workflow tests
    logger.info("\n🔄 Testing End-to-End Workflows...")
    workflow_tests = TestEndToEndWorkflow()
    
    try:
        workflow_tests.test_document_formatting_workflow()
        workflow_tests.test_financial_document_workflow()
        await workflow_tests.test_realtime_complex_workflow()
        
        logger.info("✅ All workflow tests passed!")
        
    except Exception as e:
        logger.error(f"❌ Workflow test failed: {e}")
        raise
    
    logger.info("\n🎉 ALL TESTS PASSED! 🎉")
    logger.info("LibreOffice LangGraph API Server is ready for production")
    logger.info("="*70)

if __name__ == "__main__":
    # Run the test suite
    try:
        asyncio.run(run_all_tests())
    except KeyboardInterrupt:
        logger.info("Tests interrupted by user")
    except Exception as e:
        logger.error(f"Test suite failed: {e}")
        exit(1)