#!/usr/bin/env python3
"""
Test server for Phase 8 development - bypasses PyUNO integration
"""

import asyncio
import json
import logging
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from typing import Dict, Any, Optional

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Simple request/response models
class TestRequest(BaseModel):
    request: str
    type: str = "simple"
    complexity: str = "low"
    request_id: Optional[str] = None
    context: Dict[str, Any] = {}

class TestResponse(BaseModel):
    request_id: str
    success: bool
    result: Dict[str, Any] = {}
    error_message: Optional[str] = None
    execution_time_ms: float = 0.0

# Create FastAPI app
app = FastAPI(title="LibreOffice Test Server")

# Add CORS middleware
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

@app.get("/health")
async def health_check():
    return {"status": "healthy", "message": "Test server running"}

@app.get("/status")
async def status_check():
    return {
        "status": "active",
        "integration_method": "test_mode",
        "pyuno_available": False,
        "agent_graph_initialized": True
    }

@app.post("/api/simple")
async def handle_simple_request(request: TestRequest):
    logger.info(f"Simple request: {request.request}")
    
    # Simulate successful processing
    return TestResponse(
        request_id=request.request_id or "test_simple_001",
        success=True,
        result={
            "operations": [
                {"type": "text_formatting", "status": "completed"}
            ],
            "content": f"Processed simple request: {request.request}",
            "formatting": {}
        },
        execution_time_ms=150.0
    )

@app.post("/api/moderate")
async def handle_moderate_request(request: TestRequest):
    logger.info(f"Moderate request: {request.request}")
    
    return TestResponse(
        request_id=request.request_id or "test_moderate_001",
        success=True,
        result={
            "operations": [
                {"type": "formatting", "status": "completed"},
                {"type": "content_generation", "status": "completed"}
            ],
            "content": f"Processed moderate request: {request.request}",
            "formatting": {}
        },
        execution_time_ms=300.0
    )

@app.post("/api/complex")
async def handle_complex_request(request: TestRequest):
    logger.info(f"Complex request: {request.request}")
    
    return TestResponse(
        request_id=request.request_id or "test_complex_001",
        success=True,
        result={
            "operations": [
                {"type": "data_integration", "status": "completed"},
                {"type": "table_creation", "status": "completed"},
                {"type": "chart_insertion", "status": "completed"}
            ],
            "content": f"Processed complex request: {request.request}",
            "formatting": {}
        },
        execution_time_ms=800.0
    )

@app.post("/api/error_test")
async def test_error_handling(request: TestRequest):
    """Endpoint for testing error handling scenarios"""
    logger.info(f"Error test request: {request.request}")
    
    error_type = request.context.get("error_type", "validation")
    
    if error_type == "validation":
        return TestResponse(
            request_id=request.request_id or "test_error_001",
            success=False,
            error_message="Validation error: Invalid input format",
            execution_time_ms=50.0
        )
    elif error_type == "network":
        return TestResponse(
            request_id=request.request_id or "test_error_002",
            success=False,
            error_message="Network error: Connection timeout",
            execution_time_ms=5000.0
        )
    elif error_type == "system":
        return TestResponse(
            request_id=request.request_id or "test_error_003",
            success=False,
            error_message="System error: Out of memory",
            execution_time_ms=100.0
        )
    else:
        return TestResponse(
            request_id=request.request_id or "test_error_004",
            success=False,
            error_message="Unknown error occurred",
            execution_time_ms=200.0
        )

if __name__ == "__main__":
    import uvicorn
    print("🧪 Starting LibreOffice Test Server for Phase 8 Development")
    print("Available endpoints:")
    print("  • POST http://localhost:8001/api/simple")
    print("  • POST http://localhost:8001/api/moderate") 
    print("  • POST http://localhost:8001/api/complex")
    print("  • POST http://localhost:8001/api/error_test")
    print("  • GET  http://localhost:8001/health")
    print("  • GET  http://localhost:8001/status")
    
    uvicorn.run(app, host="0.0.0.0", port=8001, log_level="info")