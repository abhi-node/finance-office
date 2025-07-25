#!/usr/bin/env python3
"""
Quick integration test for LibreOffice LangGraph API Server

This test validates the core integration without requiring a full server startup.
Tests the key components: bridge integration, request/response format compatibility,
and basic agent coordination.
"""

import asyncio
import json
import logging
import time
from typing import Dict, Any

from fastapi.testclient import TestClient

# Import components
from api_server import app, LibreOfficeRequest, LibreOfficeResponse

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class QuickIntegrationTest:
    """Quick test suite for core functionality"""
    
    def __init__(self):
        self.client = TestClient(app)
        logger.info("Initialized quick integration test")
    
    def test_server_startup(self) -> bool:
        """Test that server starts up correctly"""
        try:
            response = self.client.get("/health")
            if response.status_code == 200:
                data = response.json()
                logger.info(f"✅ Server health check: {data['status']}")
                return True
            else:
                logger.error(f"❌ Health check failed: {response.status_code}")
                return False
        except Exception as e:
            logger.error(f"❌ Server startup test failed: {e}")
            return False
    
    def test_request_format_compatibility(self) -> bool:
        """Test LibreOffice request/response format compatibility"""
        try:
            # Create request matching LibreOffice C++ format
            request_data = LibreOfficeRequest(
                request="Make the selected text bold",
                type="simple",
                complexity="low",
                request_id="test_integration_001",
                context={
                    "document_path": "test.odt",
                    "cursor_position": {"line": 1, "column": 5},
                    "selected_text": "Sample text",
                    "document_type": "text"
                }
            )
            
            # Send request with LibreOffice-style headers
            headers = {
                "Content-Type": "application/json",
                "X-Request-Type": "simple",
                "X-Include-Context": "true",
                "Authorization": "Bearer test-token"
            }
            
            response = self.client.post(
                "/api/simple", 
                json=request_data.model_dump(), 
                headers=headers
            )
            
            if response.status_code == 200:
                data = response.json()
                
                # Validate response format
                required_fields = ["request_id", "success", "execution_time_ms", "metadata"]
                for field in required_fields:
                    if field not in data:
                        logger.error(f"❌ Missing required field: {field}")
                        return False
                
                logger.info(f"✅ Request format test passed - {data['execution_time_ms']:.1f}ms")
                logger.info(f"   Response success: {data['success']}")
                return True
            else:
                logger.error(f"❌ Request failed: {response.status_code}")
                return False
                
        except Exception as e:
            logger.error(f"❌ Request format test failed: {e}")
            return False
    
    def test_all_endpoints(self) -> bool:
        """Test all API endpoints with LibreOffice format"""
        endpoints = [
            ("/api/simple", "simple", "low"),
            ("/api/moderate", "moderate", "medium"), 
            ("/api/complex", "complex", "high"),
            ("/api/process", "general", "auto")
        ]
        
        all_passed = True
        
        for endpoint, req_type, complexity in endpoints:
            try:
                request_data = LibreOfficeRequest(
                    request=f"Test {req_type} request functionality",
                    type=req_type,
                    complexity=complexity,
                    request_id=f"test_{req_type}_{int(time.time())}",
                    context={"document_type": "text", "cursor_position": {"line": 1, "column": 1}}
                )
                
                headers = {
                    "Content-Type": "application/json",
                    "X-Request-Type": req_type,
                    "X-Include-Context": "true"
                }
                
                response = self.client.post(endpoint, json=request_data.model_dump(), headers=headers)
                
                if response.status_code == 200:
                    data = response.json()
                    logger.info(f"✅ {endpoint} test passed - {data['execution_time_ms']:.1f}ms")
                else:
                    logger.error(f"❌ {endpoint} test failed: {response.status_code}")
                    all_passed = False
                    
            except Exception as e:
                logger.error(f"❌ {endpoint} test failed: {e}")
                all_passed = False
        
        return all_passed
    
    def test_error_handling(self) -> bool:
        """Test error handling with invalid requests"""
        try:
            # Test invalid JSON
            response = self.client.post("/api/simple", content="invalid json")
            if response.status_code not in [400, 422]:
                logger.error(f"❌ Expected error status for invalid JSON, got {response.status_code}")
                return False
            
            # Test missing required fields
            response = self.client.post("/api/simple", json={})
            if response.status_code not in [400, 422]:
                logger.error(f"❌ Expected error status for missing fields, got {response.status_code}")
                return False
            
            logger.info("✅ Error handling test passed")
            return True
            
        except Exception as e:
            logger.error(f"❌ Error handling test failed: {e}")
            return False
    
    def run_all_tests(self) -> bool:
        """Run all quick integration tests"""
        logger.info("🧪 Starting Quick Integration Test Suite")
        logger.info("=" * 50)
        
        tests = [
            ("Server Startup", self.test_server_startup),
            ("Request Format Compatibility", self.test_request_format_compatibility),
            ("All Endpoints", self.test_all_endpoints),
            ("Error Handling", self.test_error_handling)
        ]
        
        passed_tests = 0
        total_tests = len(tests)
        
        for test_name, test_func in tests:
            logger.info(f"🔄 Running: {test_name}")
            try:
                if test_func():
                    passed_tests += 1
                else:
                    logger.error(f"❌ {test_name} failed")
            except Exception as e:
                logger.error(f"❌ {test_name} failed with exception: {e}")
        
        logger.info("=" * 50)
        logger.info(f"📊 Test Results: {passed_tests}/{total_tests} passed")
        
        if passed_tests == total_tests:
            logger.info("🎉 ALL TESTS PASSED!")
            logger.info("LibreOffice integration is ready!")
            return True
        else:
            logger.error(f"❌ {total_tests - passed_tests} tests failed")
            return False

def main():
    """Run quick integration tests"""
    print("🚀 LibreOffice LangGraph Integration Test")
    print("Testing core functionality without full server startup...")
    print()
    
    tester = QuickIntegrationTest()
    success = tester.run_all_tests()
    
    if success:
        print("\n✨ Integration test completed successfully!")
        print("The API server is ready for LibreOffice integration.")
        print("\nNext steps:")
        print("1. Run: python start_server.py")
        print("2. Test with LibreOffice build")
        return 0
    else:
        print("\n❌ Integration test failed!")
        print("Please check the logs and fix issues before proceeding.")
        return 1

if __name__ == "__main__":
    exit(main())