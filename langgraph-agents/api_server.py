#!/usr/bin/env python3
"""
FastAPI server for LibreOffice AI Writing Assistant LangGraph Multi-Agent System

This server provides HTTP and WebSocket endpoints for LibreOffice C++ integration,
wrapping the existing bridge.py functionality with a proper API layer.

Expected Endpoints (from LibreOffice C++ AgentCoordinator):
- POST /api/simple - Fast processing for simple operations  
- POST /api/moderate - Balanced workflow for moderate operations
- POST /api/complex - Full agent workflow for complex operations
- POST /api/process - General processing endpoint
- WS /ws/libreoffice - WebSocket for real-time streaming updates
"""

import asyncio
import json
import logging
import time
import uuid
from contextlib import asynccontextmanager
from typing import Dict, Any, Optional, List

import uvicorn
from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse
from pydantic import BaseModel, Field

# Import LangGraph bridge components
from bridge import LangGraphBridge, get_bridge, initialize_bridge, shutdown_bridge
from config import get_config

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# Global bridge instance
bridge_instance: Optional[LangGraphBridge] = None
active_websockets: Dict[str, WebSocket] = {}

# Request/Response Models
class LibreOfficeRequest(BaseModel):
    """Standard request format from LibreOffice C++ AgentCoordinator"""
    request: str = Field(..., description="User's natural language request")
    type: str = Field(..., description="Request type: simple|moderate|complex")
    complexity: str = Field(..., description="Complexity level: low|medium|high")
    request_id: Optional[str] = Field(default=None, description="Unique request identifier")
    context: Optional[Dict[str, Any]] = Field(default=None, description="Document context from C++")

class LibreOfficeResponse(BaseModel):
    """Enhanced response format for LibreOffice C++ AgentCoordinator with separate operations and content"""
    request_id: str
    success: bool
    
    # Enhanced: Separate operations and content fields
    operations: List[Dict[str, Any]] = Field(default_factory=list, description="Document operations to execute")
    response_content: str = Field(default="", description="Chat response content for user display")
    operation_summaries: List[str] = Field(default_factory=list, description="Human-readable operation descriptions")
    
    # Legacy compatibility fields
    result: Optional[Dict[str, Any]] = None
    error_message: Optional[str] = None
    progress_updates: List[Dict[str, Any]] = Field(default_factory=list)
    execution_time_ms: float = 0.0
    agent_results: Dict[str, Any] = Field(default_factory=dict)
    final_state: Optional[Dict[str, Any]] = None
    
    # Enhanced metadata and formatting
    content_changes: Dict[str, Any] = Field(default_factory=dict, description="Document content modifications")
    formatting_changes: Dict[str, Any] = Field(default_factory=dict, description="Formatting state changes")
    warnings: List[str] = Field(default_factory=list, description="Validation warnings or notices")
    metadata: Dict[str, Any] = Field(default_factory=dict)

class WebSocketMessage(BaseModel):
    """WebSocket message format for real-time updates"""
    type: str = Field(..., description="Message type: request|response|progress|error")
    request_id: str = Field(..., description="Request identifier")
    content: Dict[str, Any] = Field(..., description="Message content")
    timestamp: float = Field(default_factory=time.time)

# FastAPI lifecycle management
@asynccontextmanager
async def lifespan(app: FastAPI):
    """Manage FastAPI application lifecycle with bridge initialization/cleanup"""
    global bridge_instance
    
    logger.info("Starting LibreOffice LangGraph API Server...")
    
    try:
        # Initialize bridge
        bridge_success = await initialize_bridge()
        if not bridge_success:
            raise RuntimeError("Failed to initialize LangGraph bridge")
        
        # Get the bridge instance
        bridge_instance = get_bridge()
        
        logger.info("LangGraph bridge initialized successfully")
        yield
        
    except Exception as e:
        logger.error(f"Failed to initialize bridge: {e}")
        raise
    finally:
        # Cleanup
        logger.info("Shutting down LibreOffice LangGraph API Server...")
        if bridge_instance:
            await shutdown_bridge()
        
        # Close all WebSocket connections
        for ws_id, websocket in active_websockets.items():
            try:
                await websocket.close()
            except Exception as e:
                logger.warning(f"Error closing WebSocket {ws_id}: {e}")
        active_websockets.clear()

# Create FastAPI application
app = FastAPI(
    title="LibreOffice AI Writing Assistant API",
    description="LangGraph Multi-Agent System for LibreOffice Writer AI Assistant",
    version="1.0.0",
    lifespan=lifespan
)

# Add CORS middleware for development
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # In production, restrict this
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Utility functions
def generate_request_id() -> str:
    """Generate unique request ID"""
    return str(uuid.uuid4())

def validate_headers(request: Request) -> Dict[str, str]:
    """Validate and extract expected headers from LibreOffice"""
    headers = {}
    
    # Expected headers from C++ AgentCoordinator
    expected_headers = {
        "x-request-type": "Request type identifier",
        "x-include-context": "Context inclusion flag", 
        "x-agent-workflow": "Agent workflow specification",
        "authorization": "Bearer token for authentication",
        "content-type": "Should be application/json"
    }
    
    for header_name, description in expected_headers.items():
        header_value = request.headers.get(header_name)
        if header_value:
            headers[header_name] = header_value
            logger.debug(f"Header {header_name}: {header_value}")
    
    return headers

async def process_request_with_bridge(
    request_data: LibreOfficeRequest, 
    request_type: str,
    headers: Dict[str, str]
) -> LibreOfficeResponse:
    """Process request using LangGraph bridge with proper error handling"""
    global bridge_instance
    
    start_time = time.time()
    request_id = request_data.request_id or generate_request_id()
    
    logger.info(f"Processing {request_type} request {request_id}: {request_data.request[:100]}...")
    
    try:
        if not bridge_instance:
            raise HTTPException(status_code=503, detail="LangGraph bridge not available")
        
        # Convert to bridge format
        document_context_json = json.dumps(request_data.context or {})
        
        # Process with bridge
        response_json = await bridge_instance.process_cpp_request(
            request_id=request_id,
            user_message=request_data.request,
            document_context=request_data.context or {}
        )
        
        # Parse response
        response_data = json.loads(response_json)
        execution_time_ms = (time.time() - start_time) * 1000
        
        # Extract enhanced fields from bridge response
        result = response_data.get("result", {})
        
        # Enhanced: Extract operations and response content separately
        operations = []
        response_content = ""
        operation_summaries = []
        content_changes = {}
        formatting_changes = {}
        warnings = []
        
        if isinstance(result, dict):
            # Extract operations for execution
            operations = result.get("operations", [])
            
            # Extract chat response content
            response_content = result.get("response_content", "")
            
            # Extract operation summaries for user feedback
            operation_summaries = result.get("operation_summaries", [])
            
            # Extract content and formatting changes
            content_changes = result.get("content_changes", {})
            formatting_changes = result.get("formatting_changes", {})
            
            # Extract warnings
            warnings = result.get("warnings", [])
        
        # Fallback: Generate response content if none provided
        if not response_content:
            if operations:
                op_count = len(operations)
                if op_count == 1:
                    response_content = "Operation prepared for execution."
                else:
                    response_content = f"{op_count} operations prepared for execution."
            else:
                response_content = "Request processed successfully."
        
        # Create enhanced LibreOffice-compatible response
        return LibreOfficeResponse(
            request_id=request_id,
            success=response_data.get("success", False),
            
            # Enhanced: Separate operations and content
            operations=operations,
            response_content=response_content,
            operation_summaries=operation_summaries,
            
            # Enhanced: Content and formatting changes
            content_changes=content_changes,
            formatting_changes=formatting_changes,
            warnings=warnings,
            
            # Legacy compatibility
            result=result,
            error_message=response_data.get("error_message"),
            execution_time_ms=execution_time_ms,
            agent_results=response_data.get("agent_results", {}),
            final_state=response_data.get("final_state"),
            
            # Enhanced metadata
            metadata={
                "request_type": request_type,
                "processing_time_ms": execution_time_ms,
                "headers": headers,
                "operations_count": len(operations),
                "response_length": len(response_content),
                "has_warnings": len(warnings) > 0,
                "bridge_status": "success"
            }
        )
        
    except Exception as e:
        execution_time_ms = (time.time() - start_time) * 1000
        error_msg = f"Request processing failed: {str(e)}"
        logger.error(f"Error processing request {request_id}: {error_msg}")
        
        return LibreOfficeResponse(
            request_id=request_id,
            success=False,
            operations=[],
            response_content=f"Error: {error_msg}",
            operation_summaries=[],
            content_changes={},
            formatting_changes={},
            warnings=[],
            error_message=error_msg,
            execution_time_ms=execution_time_ms,
            metadata={
                "request_type": request_type,
                "error": str(e),
                "operations_count": 0,
                "response_length": len(error_msg),
                "has_warnings": False,
                "bridge_status": "error"
            }
        )

# API Endpoints
@app.get("/health")
async def health_check():
    """Health check endpoint"""
    return {
        "status": "healthy",
        "bridge_active": bridge_instance is not None,
        "active_websockets": len(active_websockets),
        "timestamp": time.time()
    }

@app.get("/status")
async def get_status():
    """Get detailed server status"""
    global bridge_instance
    
    bridge_status = {}
    if bridge_instance:
        bridge_status = bridge_instance.get_status()
    
    return {
        "server": "LibreOffice LangGraph API Server",
        "version": "1.0.0",
        "bridge_status": bridge_status,
        "active_websockets": len(active_websockets),
        "websocket_connections": list(active_websockets.keys()),
        "timestamp": time.time()
    }

@app.post("/api/simple", response_model=LibreOfficeResponse)
async def process_simple_request(request_data: LibreOfficeRequest, request: Request):
    """
    Process simple requests - fast response for basic operations
    Target: <2 seconds, local processing preferred
    """
    headers = validate_headers(request)
    logger.info(f"Simple request: {request_data.request[:50]}...")
    return await process_request_with_bridge(request_data, "simple", headers)

@app.post("/api/moderate", response_model=LibreOfficeResponse)
async def process_moderate_request(request_data: LibreOfficeRequest, request: Request):
    """
    Process moderate requests - balanced workflow for medium complexity
    Target: <4 seconds, agent coordination
    """
    headers = validate_headers(request)
    logger.info(f"Moderate request: {request_data.request[:50]}...")
    return await process_request_with_bridge(request_data, "moderate", headers)

@app.post("/api/complex", response_model=LibreOfficeResponse)
async def process_complex_request(request_data: LibreOfficeRequest, request: Request):
    """
    Process complex requests - full agent workflow with all specialists
    Target: <5 seconds, complete agent orchestration
    """
    headers = validate_headers(request)
    logger.info(f"Complex request: {request_data.request[:50]}...")
    return await process_request_with_bridge(request_data, "complex", headers)

@app.post("/api/process", response_model=LibreOfficeResponse)
async def process_general_request(request_data: LibreOfficeRequest, request: Request):
    """
    General processing endpoint - automatically determines complexity
    Used for legacy compatibility
    """
    headers = validate_headers(request)
    logger.info(f"General request: {request_data.request[:50]}...")
    return await process_request_with_bridge(request_data, "general", headers)

# WebSocket endpoint for real-time streaming
@app.websocket("/ws/libreoffice")
async def websocket_endpoint(websocket: WebSocket):
    """
    WebSocket endpoint for real-time communication with LibreOffice
    Supports streaming progress updates during complex agent processing
    """
    connection_id = generate_request_id()
    logger.info(f"WebSocket connection attempt: {connection_id}")
    
    try:
        await websocket.accept(subprotocol="langgraph-ai")
        active_websockets[connection_id] = websocket
        logger.info(f"WebSocket connected: {connection_id}")
        
        # Send welcome message
        welcome_msg = WebSocketMessage(
            type="connection",
            request_id=connection_id,
            content={
                "status": "connected",
                "server": "LibreOffice LangGraph API Server",
                "protocol": "langgraph-ai",
                "capabilities": ["streaming", "progress_updates", "cancellation"]
            }
        )
        await websocket.send_text(welcome_msg.model_dump_json())
        
        # Message handling loop
        while True:
            try:
                # Wait for message from LibreOffice
                data = await websocket.receive_text()
                logger.debug(f"WebSocket received: {data[:100]}...")
                
                # Parse message
                try:
                    message_data = json.loads(data)
                except json.JSONDecodeError as e:
                    error_response = WebSocketMessage(
                        type="error",
                        request_id=connection_id,
                        content={"error": f"Invalid JSON: {str(e)}"}
                    )
                    await websocket.send_text(error_response.model_dump_json())
                    continue
                
                # Handle different message types
                message_type = message_data.get("type", "request")
                
                if message_type == "request":
                    # Process agent request with streaming
                    await handle_websocket_request(websocket, connection_id, message_data)
                    
                elif message_type == "cancel":
                    # Handle cancellation request
                    await handle_websocket_cancellation(websocket, connection_id, message_data)
                    
                elif message_type == "ping":
                    # Handle ping/pong for connection keepalive
                    pong_response = WebSocketMessage(
                        type="pong",
                        request_id=connection_id,
                        content={"timestamp": time.time()}
                    )
                    await websocket.send_text(pong_response.model_dump_json())
                    
                else:
                    logger.warning(f"Unknown WebSocket message type: {message_type}")
                    
            except WebSocketDisconnect:
                logger.info(f"WebSocket disconnected normally: {connection_id}")
                break
            except Exception as e:
                logger.error(f"WebSocket error: {e}")
                error_response = WebSocketMessage(
                    type="error",
                    request_id=connection_id,
                    content={"error": str(e)}
                )
                try:
                    await websocket.send_text(error_response.model_dump_json())
                except:
                    break  # Connection is broken
                    
    except Exception as e:
        logger.error(f"WebSocket connection error: {e}")
    finally:
        # Cleanup
        if connection_id in active_websockets:
            del active_websockets[connection_id]
        logger.info(f"WebSocket connection closed: {connection_id}")

async def handle_websocket_request(websocket: WebSocket, connection_id: str, message_data: Dict[str, Any]):
    """Handle agent processing request via WebSocket with streaming updates"""
    global bridge_instance
    
    request_id = message_data.get("request_id", generate_request_id())
    user_message = message_data.get("message", "")
    context = message_data.get("context", {})
    
    logger.info(f"WebSocket processing request {request_id}: {user_message[:50]}...")
    
    try:
        # Send processing started message
        start_msg = WebSocketMessage(
            type="progress",
            request_id=request_id,
            content={
                "status": "processing_started",
                "message": "Agent processing initiated",
                "progress": 0
            }
        )
        await websocket.send_text(start_msg.model_dump_json())
        
        if not bridge_instance:
            raise Exception("LangGraph bridge not available")
        
        # Process with bridge (this could be enhanced for streaming)
        response_json = await bridge_instance.process_cpp_request(
            request_id=request_id,
            user_message=user_message,
            document_context=context
        )
        
        # Send progress update (simulated - real implementation would have agent callbacks)
        progress_msg = WebSocketMessage(
            type="progress",
            request_id=request_id,
            content={
                "status": "agents_coordinating",
                "message": "DocumentMasterAgent analyzing request",
                "progress": 50
            }
        )
        await websocket.send_text(progress_msg.model_dump_json())
        
        # Parse and send final response
        response_data = json.loads(response_json)
        
        final_msg = WebSocketMessage(
            type="response",
            request_id=request_id,
            content={
                "success": response_data.get("success", False),
                "result": response_data.get("result"),
                "agent_results": response_data.get("agent_results", {}),
                "final_state": response_data.get("final_state"),
                "error_message": response_data.get("error_message"),
                "progress": 100
            }
        )
        await websocket.send_text(final_msg.model_dump_json())
        
    except Exception as e:
        error_msg = WebSocketMessage(
            type="error",
            request_id=request_id,
            content={
                "error": str(e),
                "status": "processing_failed"
            }
        )
        await websocket.send_text(error_msg.model_dump_json())

async def handle_websocket_cancellation(websocket: WebSocket, connection_id: str, message_data: Dict[str, Any]):
    """Handle request cancellation via WebSocket"""
    request_id = message_data.get("request_id", "unknown")
    
    logger.info(f"WebSocket cancellation request for: {request_id}")
    
    # Send cancellation acknowledgment
    cancel_msg = WebSocketMessage(
        type="cancelled",
        request_id=request_id,
        content={
            "status": "cancelled",
            "message": "Request cancellation processed"
        }
    )
    await websocket.send_text(cancel_msg.model_dump_json())

# Error handlers
@app.exception_handler(HTTPException)
async def http_exception_handler(request: Request, exc: HTTPException):
    """Handle HTTP exceptions with proper LibreOffice-compatible format"""
    return JSONResponse(
        status_code=exc.status_code,
        content={
            "request_id": getattr(request.state, "request_id", "unknown"),
            "success": False,
            "error_message": exc.detail,
            "execution_time_ms": 0,
            "metadata": {
                "status_code": exc.status_code,
                "endpoint": str(request.url)
            }
        }
    )

@app.exception_handler(Exception)
async def general_exception_handler(request: Request, exc: Exception):
    """Handle general exceptions"""
    logger.error(f"Unhandled exception: {exc}")
    return JSONResponse(
        status_code=500,
        content={
            "request_id": getattr(request.state, "request_id", "unknown"),
            "success": False,
            "error_message": "Internal server error",
            "execution_time_ms": 0,
            "metadata": {
                "error_type": type(exc).__name__,
                "endpoint": str(request.url)
            }
        }
    )

# Development server startup
if __name__ == "__main__":
    # Load configuration
    config = get_config()
    
    # Configure uvicorn logging
    log_config = uvicorn.config.LOGGING_CONFIG
    log_config["formatters"]["default"]["fmt"] = "%(asctime)s - %(name)s - %(levelname)s - %(message)s"
    log_config["formatters"]["access"]["fmt"] = "%(asctime)s - %(name)s - %(levelname)s - %(message)s"
    
    logger.info("Starting LibreOffice LangGraph API Server...")
    logger.info(f"Expected endpoints:")
    logger.info(f"  HTTP API: http://localhost:8000/api/{{simple,moderate,complex,process}}")
    logger.info(f"  WebSocket: ws://localhost:8000/ws/libreoffice") 
    logger.info(f"  Health: http://localhost:8000/health")
    logger.info(f"  Status: http://localhost:8000/status")
    
    uvicorn.run(
        "api_server:app",
        host="0.0.0.0",
        port=8000,
        log_config=log_config,
        access_log=True,
        reload=False  # Set to True for development
    )