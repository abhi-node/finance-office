#!/usr/bin/env python3
"""
LibreOffice AI Agent Server - LangGraph Integration
Unified endpoint using DocumentMasterAgent orchestration and specialized agent coordination.

This server replaces the previous LLM-only implementation with full LangGraph integration,
providing intelligent routing, multi-agent coordination, and comprehensive document operations.

Architecture:
1. Receives unified HTTP requests from LibreOffice C++ AgentCoordinator
2. Uses LangGraphBridge to convert C++ context to DocumentState
3. Routes through DocumentMasterAgent for intelligent complexity analysis and workflow planning
4. Coordinates specialized agents (ContextAnalysis, ContentGeneration, Formatting, etc.)
5. Returns structured operations + AI chat response for LibreOffice execution

Key Features:
- DocumentMasterAgent-driven complexity analysis (Simple/Moderate/Complex)
- Intelligent agent routing and workflow orchestration
- Comprehensive DocumentState management
- Seamless C++ ↔ Python integration via bridge
- Error handling and recovery across all layers
"""

import asyncio
import json
import logging
import os
import time
import uuid
from datetime import datetime
from typing import Dict, Any, List, Optional, Union

from fastapi import FastAPI, HTTPException, Request, status
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse
from pydantic import BaseModel, Field
import uvicorn

# LangGraph Bridge and Agent Integration
try:
    from bridge import (
        LangGraphBridge, 
        BridgeConfiguration, 
        IntegrationMethod,
        get_bridge, 
        initialize_bridge, 
        shutdown_bridge
    )
    from agents.document_master import DocumentMasterAgent, OperationComplexity
    from state.document_state import DocumentState, DocumentStateManager
    from agents.error_handler import UnifiedErrorCoordinator, ErrorContext, ErrorSeverity
    LANGGRAPH_AVAILABLE = True
except ImportError as e:
    logging.error(f"LangGraph components not available: {e}")
    LANGGRAPH_AVAILABLE = False

# Configure comprehensive logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.StreamHandler(),
        logging.FileHandler('logs/ai_agent_server.log')
    ]
)
logger = logging.getLogger(__name__)

# Ensure logs directory exists
os.makedirs("logs", exist_ok=True)

# Request/Response Models for LibreOffice Integration
class LibreOfficeRequest(BaseModel):
    """Unified request model for all LibreOffice operations."""
    request: str = Field(..., description="User's natural language request")
    request_id: Optional[str] = Field(default=None, description="Unique request identifier")
    context: Optional[Dict[str, Any]] = Field(default=None, description="LibreOffice document context")
    user_preferences: Optional[Dict[str, Any]] = Field(default=None, description="User preferences and settings")
    session_id: Optional[str] = Field(default=None, description="User session identifier")
    
    class Config:
        schema_extra = {
            "example": {
                "request": "Create a financial analysis chart for AAPL stock data",
                "request_id": "req_1234567890_abcd",
                "context": {
                    "cursor_position": {"line": 5, "column": 12},
                    "selected_text": "AAPL Q3 Results",
                    "document_structure": {
                        "paragraph_count": 25,
                        "page_count": 3,
                        "word_count": 750
                    },
                    "formatting_state": {"has_selection": True}
                },
                "user_preferences": {"language": "en-US"},
                "session_id": "session_abc123"
            }
        }

class LibreOfficeResponse(BaseModel):
    """Comprehensive response model for LibreOffice operations."""
    request_id: str
    success: bool
    response_content: str = Field(default="", description="AI chat response for user display")
    operations: List[Dict[str, Any]] = Field(default_factory=list, description="Document operations to execute")
    operation_summaries: List[str] = Field(default_factory=list, description="Human-readable operation descriptions")
    
    # Enhanced response fields
    complexity_detected: Optional[str] = Field(default=None, description="Detected complexity level")
    workflow_path: Optional[str] = Field(default=None, description="Workflow execution path used")
    agents_used: List[str] = Field(default_factory=list, description="Agents involved in processing")
    execution_time_ms: float = Field(default=0.0, description="Total processing time")
    
    # Content and formatting changes
    content_changes: Dict[str, Any] = Field(default_factory=dict, description="Content modifications")
    formatting_changes: Dict[str, Any] = Field(default_factory=dict, description="Formatting modifications")
    
    # User interaction and validation
    warnings: List[str] = Field(default_factory=list, description="Warnings for user attention")
    approval_required: List[Dict[str, Any]] = Field(default_factory=list, description="Operations requiring user approval")
    validation_results: Dict[str, Any] = Field(default_factory=dict, description="Quality validation results")
    
    # Performance and optimization
    performance_metrics: Dict[str, Any] = Field(default_factory=dict, description="Performance measurements")
    optimization_recommendations: List[str] = Field(default_factory=list, description="Performance optimization suggestions")
    
    # Error handling
    error_context: Optional[Dict[str, Any]] = Field(default=None, description="Error information if applicable")
    recovery_options: List[str] = Field(default_factory=list, description="Available recovery options")
    
    # Metadata
    metadata: Dict[str, Any] = Field(default_factory=dict, description="Additional response metadata")
    
    class Config:
        schema_extra = {
            "example": {
                "request_id": "req_1234567890_abcd",
                "success": True,
                "response_content": "I've created a comprehensive financial analysis chart for AAPL with Q3 data visualization and trend analysis.",
                "operations": [
                    {
                        "operation_id": "op_001",
                        "operation_type": "insertChart",
                        "parameters": {
                            "chart_type": "line",
                            "data_range": "A1:E10",
                            "title": "AAPL Stock Performance Q3"
                        }
                    }
                ],
                "operation_summaries": ["Insert line chart: AAPL Stock Performance Q3"],
                "complexity_detected": "complex",
                "workflow_path": "orchestrated",
                "agents_used": ["DocumentMasterAgent", "ContextAnalysisAgent", "DataIntegrationAgent", "ContentGenerationAgent"],
                "execution_time_ms": 3456.78
            }
        }


class LangGraphAgentProcessor:
    """
    Core agent processing engine using LangGraph bridge and DocumentMasterAgent orchestration.
    
    This class provides the unified interface between LibreOffice C++ components and the
    Python LangGraph multi-agent system, handling request routing, state management,
    and response coordination.
    """
    
    def __init__(self):
        """Initialize the LangGraph agent processor."""
        self.bridge: Optional[LangGraphBridge] = None
        self.error_coordinator: Optional[UnifiedErrorCoordinator] = None
        self.state_manager: Optional[DocumentStateManager] = None
        self.initialized = False
        self.startup_time = time.time()
        
        # Performance tracking
        self.request_count = 0
        self.total_processing_time = 0.0
        self.error_count = 0
        
        logger.info("LangGraphAgentProcessor created - awaiting initialization")
    
    async def initialize(self) -> bool:
        """
        Initialize the LangGraph bridge and agent system.
        
        Returns:
            bool: True if initialization successful, False otherwise
        """
        try:
            logger.info("🚀 Initializing LangGraph Agent System...")
            
            if not LANGGRAPH_AVAILABLE:
                logger.error("❌ LangGraph components not available - cannot initialize")
                return False
            
            # Create bridge configuration optimized for LibreOffice integration
            bridge_config = BridgeConfiguration(
                integration_method=IntegrationMethod.HTTP_API,  # Pure API integration method
                max_concurrent_operations=5,
                operation_timeout_seconds=30,
                enable_progress_streaming=True,
                enable_websocket=False,  # HTTP-only for Phase 1
                log_level="INFO",
                cache_enabled=True,
                cache_ttl_seconds=300,
                max_retry_attempts=3,
                retry_delay_seconds=1.0,
                enable_all_agents=True,
                agent_priorities={
                    "document_master": 1,
                    "context_analysis": 2,
                    "content_generation": 3,
                    "formatting": 4,
                    "data_integration": 5,
                    "validation": 6,
                    "execution": 7
                }
            )
            
            # Initialize the LangGraph bridge
            success = await initialize_bridge(bridge_config)
            if not success:
                logger.error("❌ Failed to initialize LangGraph bridge")
                return False
            
            # Get the initialized bridge instance
            self.bridge = get_bridge()
            if not self.bridge:
                logger.error("❌ Failed to get bridge instance after initialization")
                return False
            
            # Initialize error coordination system
            self.error_coordinator = UnifiedErrorCoordinator(
                config={
                    "escalation_thresholds": {
                        ErrorSeverity.CRITICAL: 1,
                        ErrorSeverity.HIGH: 3,
                        ErrorSeverity.MEDIUM: 10,
                        ErrorSeverity.LOW: 50
                    },
                    "adaptive_recovery": True,
                    "learning_enabled": True
                }
            )
            
            # Initialize document state manager
            self.state_manager = DocumentStateManager()
            
            # Verify bridge status
            bridge_status = self.bridge.get_status()
            logger.info(f"✅ Bridge Status: {bridge_status['status']}")
            logger.info(f"✅ Integration Method: {bridge_status['integration_method']}")
            logger.info(f"✅ Active Operations: {bridge_status['active_operations']}")
            
            self.initialized = True
            logger.info("🎉 LangGraph Agent System initialization completed successfully!")
            
            return True
            
        except Exception as e:
            logger.error(f"❌ LangGraph Agent System initialization failed: {e}")
            logger.error(f"Exception details: {traceback.format_exc()}")
            return False
    
    async def shutdown(self) -> bool:
        """Shutdown the LangGraph bridge and cleanup resources."""
        try:
            logger.info("🔄 Shutting down LangGraph Agent System...")
            
            if self.bridge:
                await shutdown_bridge()
                self.bridge = None
            
            self.initialized = False
            logger.info("✅ LangGraph Agent System shutdown completed")
            return True
            
        except Exception as e:
            logger.error(f"❌ Shutdown failed: {e}")
            return False
    
    async def process_unified_request(self, request_data: LibreOfficeRequest) -> LibreOfficeResponse:
        """
        Process unified request through LangGraph agent system.
        
        Args:
            request_data: LibreOffice request with user message and document context
            
        Returns:
            LibreOfficeResponse: Comprehensive response with operations and chat content
        """
        start_time = time.time()
        request_id = request_data.request_id or self._generate_request_id()
        
        try:
            # Validate initialization
            if not self.initialized or not self.bridge:
                raise RuntimeError("LangGraph Agent System not properly initialized")
            
            # Update performance tracking
            self.request_count += 1
            
            logger.info(f"🔵 Processing unified request {request_id}: {request_data.request[:100]}...")
            
            # Prepare context for bridge processing
            bridge_context = self._prepare_bridge_context(request_data)
            
            # Process through LangGraph bridge with DocumentMasterAgent orchestration
            # This will:
            # 1. Convert C++ context to DocumentState
            # 2. Route through DocumentMasterAgent for complexity analysis
            # 3. Coordinate specialized agents based on detected complexity
            # 4. Aggregate results and generate operations + chat response
            logger.info(f"🤖 Routing request {request_id} through DocumentMasterAgent...")
            logger.info(f"DEBUG: About to call bridge.process_cpp_request with request_id={request_id}")
            logger.info(f"DEBUG: Bridge instance: {self.bridge}")
            logger.info(f"DEBUG: Bridge type: {type(self.bridge)}")
            
            bridge_response_json = await self.bridge.process_cpp_request(
                request_id,
                request_data.request,
                bridge_context
            )
            
            logger.info(f"DEBUG: bridge.process_cpp_request returned: {type(bridge_response_json)}")
            
            # Parse bridge response with error handling
            if bridge_response_json is None:
                raise RuntimeError("Bridge returned None response")
            
            try:
                bridge_response = json.loads(bridge_response_json)
                logger.info(f"✅ Bridge processing completed for {request_id}")
            except json.JSONDecodeError as e:
                logger.error(f"Failed to parse bridge response: {e}")
                logger.error(f"Raw response: {bridge_response_json}")
                raise RuntimeError(f"Invalid JSON response from bridge: {e}")
            
            # Convert bridge response to LibreOffice format
            libreoffice_response = self._convert_bridge_response_to_libreoffice(
                bridge_response, request_id, start_time
            )
            
            # Update performance metrics
            execution_time = (time.time() - start_time) * 1000
            self.total_processing_time += execution_time
            
            logger.info(f"🎉 Request {request_id} completed successfully in {execution_time:.2f}ms")
            
            return libreoffice_response
            
        except Exception as e:
            # Handle errors through unified error coordinator
            execution_time = (time.time() - start_time) * 1000
            self.error_count += 1
            
            logger.error(f"❌ Request {request_id} failed after {execution_time:.2f}ms: {e}")
            
            # Create error context for proper error handling
            error_context = ErrorContext(
                error_message=str(e),
                agent_id="ai_agent_server",
                operation_id=request_id,
                user_request=request_data.request,
                document_context=request_data.context or {},
                user_session_id=request_data.session_id,
                metadata={
                    "execution_time_ms": execution_time,
                    "request_count": self.request_count
                }
            )
            
            # Get error response from coordinator
            if self.error_coordinator:
                error_response = await self.error_coordinator.handle_error(error_context)
                
                return LibreOfficeResponse(
                    request_id=request_id,
                    success=False,
                    response_content=error_response.user_message,
                    execution_time_ms=execution_time,
                    error_context=error_response.to_dict(),
                    recovery_options=error_response.recovery_options,
                    metadata={
                        "error_type": type(e).__name__,
                        "technical_message": error_response.technical_message
                    }
                )
            else:
                # Fallback error response
                return LibreOfficeResponse(
                    request_id=request_id,
                    success=False,
                    response_content=f"Processing error: {str(e)}",
                    execution_time_ms=execution_time,
                    metadata={"error_type": type(e).__name__}
                )
    
    def _prepare_bridge_context(self, request_data: LibreOfficeRequest) -> Dict[str, Any]:
        """Prepare context dictionary for bridge processing."""
        context = request_data.context or {}
        
        # Enhance context with additional metadata
        enhanced_context = {
            **context,
            "request_metadata": {
                "session_id": request_data.session_id,
                "user_preferences": request_data.user_preferences or {},
                "server_timestamp": datetime.now().isoformat(),
                "server_version": "2.0.0-langgraph"
            }
        }
        
        return enhanced_context
    
    def _convert_bridge_response_to_libreoffice(self, 
                                               bridge_response: Dict[str, Any], 
                                               request_id: str, 
                                               start_time: float) -> LibreOfficeResponse:
        """Convert bridge response to LibreOffice format."""
        execution_time = (time.time() - start_time) * 1000
        
        # Extract response data from bridge (OperationResponse structure)
        success = bridge_response.get("success", False)
        result = bridge_response.get("result", {}) or {}  # Handle None result
        metadata = bridge_response.get("metadata", {}) or {}  # Handle None metadata
        agent_results = bridge_response.get("agent_results", {}) or {}
        
        # Extract orchestration information from metadata or agent_results
        orchestration = metadata.get("orchestration", {}) or agent_results.get("orchestration", {})
        
        # Build comprehensive LibreOffice response
        return LibreOfficeResponse(
            request_id=request_id,
            success=success,
            response_content=result.get("response_content", ""),
            operations=result.get("operations", []),
            operation_summaries=result.get("operation_summaries", []),
            
            # Enhanced response fields from agent orchestration
            complexity_detected=orchestration.get("complexity", "unknown"),
            workflow_path=orchestration.get("workflow_path", "unknown"),
            agents_used=orchestration.get("agents_used", []),
            execution_time_ms=execution_time,
            
            # Content and formatting changes
            content_changes=result.get("content_changes", {}),
            formatting_changes=result.get("formatting_changes", {}),
            
            # User interaction
            warnings=result.get("warnings", []),
            approval_required=bridge_response.get("approval_required", []),
            validation_results=bridge_response.get("validation_results", {}),
            
            # Performance metrics from agents
            performance_metrics={
                "execution_time_ms": execution_time,
                "complexity_detected": orchestration.get("complexity"),
                "performance_target_met": orchestration.get("performance_target_met", False),
                "analysis_confidence": orchestration.get("analysis_confidence", 0.0),
                "agents_count": len(orchestration.get("agents_used", []))
            },
            optimization_recommendations=bridge_response.get("optimization_recommendations", []),
            
            # Error information
            error_context={"error": bridge_response.get("error")} if bridge_response.get("error") else None,
            
            # Comprehensive metadata
            metadata={
                **metadata,
                "server_processing_time_ms": execution_time,
                "bridge_integration": "langgraph",
                "agent_orchestration_enabled": True,
                "request_count": self.request_count,
                "server_uptime_seconds": time.time() - self.startup_time
            }
        )
    
    def _generate_request_id(self) -> str:
        """Generate unique request ID."""
        timestamp = int(time.time() * 1000)
        random_suffix = str(uuid.uuid4())[:8]
        return f"agent_{timestamp}_{random_suffix}"
    
    def get_system_status(self) -> Dict[str, Any]:
        """Get comprehensive system status."""
        uptime = time.time() - self.startup_time
        avg_processing_time = (self.total_processing_time / self.request_count) if self.request_count > 0 else 0.0
        
        status = {
            "initialized": self.initialized,
            "langgraph_available": LANGGRAPH_AVAILABLE,
            "uptime_seconds": uptime,
            "request_count": self.request_count,
            "error_count": self.error_count,
            "success_rate": ((self.request_count - self.error_count) / self.request_count * 100) if self.request_count > 0 else 100.0,
            "average_processing_time_ms": avg_processing_time,
            "total_processing_time_ms": self.total_processing_time
        }
        
        # Add bridge status if available
        if self.bridge:
            bridge_status = self.bridge.get_status()
            status["bridge_status"] = bridge_status
        
        return status


# Initialize the global agent processor
agent_processor = LangGraphAgentProcessor()

# Create FastAPI application
app = FastAPI(
    title="LibreOffice LangGraph Agent Server",
    description="Unified AI agent server using LangGraph bridge and DocumentMasterAgent orchestration",
    version="2.0.0-langgraph",
    docs_url="/docs",
    redoc_url="/redoc"
)

# Add CORS middleware for LibreOffice integration
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # LibreOffice local requests
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Exception handler for proper error responses
@app.exception_handler(Exception)
async def global_exception_handler(request: Request, exc: Exception):
    logger.error(f"Unhandled exception: {exc}")
    return JSONResponse(
        status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
        content={
            "success": False,
            "error": "Internal server error",
            "detail": str(exc),
            "timestamp": datetime.now().isoformat()
        }
    )

# Startup and shutdown events
@app.on_event("startup")
async def startup_event():
    """Initialize LangGraph agent system on startup."""
    logger.info("🚀 Starting LibreOffice LangGraph Agent Server...")
    
    success = await agent_processor.initialize()
    if not success:
        logger.error("❌ Failed to initialize agent system - server will run in degraded mode")
    else:
        logger.info("✅ Agent system initialized successfully")

@app.on_event("shutdown")
async def shutdown_event():
    """Cleanup on shutdown."""
    logger.info("🔄 Shutting down LibreOffice LangGraph Agent Server...")
    await agent_processor.shutdown()
    logger.info("✅ Server shutdown completed")

# Health and status endpoints
@app.get("/health")
async def health_check():
    """Health check endpoint for LibreOffice monitoring."""
    return {
        "status": "healthy" if agent_processor.initialized else "degraded",
        "timestamp": datetime.now().isoformat(),
        "service": "LibreOffice LangGraph Agent Server",
        "version": "2.0.0-langgraph",
        "langgraph_available": LANGGRAPH_AVAILABLE,
        "initialized": agent_processor.initialized
    }

@app.get("/status")
async def status_check():
    """Detailed status information."""
    return {
        "service": "LibreOffice LangGraph Agent Server",
        "version": "2.0.0-langgraph",
        "system_status": agent_processor.get_system_status(),
        "capabilities": [
            "DocumentMasterAgent orchestration",
            "Intelligent complexity analysis",
            "Multi-agent coordination",
            "Comprehensive document operations",
            "Error handling and recovery",
            "Performance optimization"
        ],
        "endpoints": ["/api/agent"],
        "timestamp": datetime.now().isoformat()
    }

# UNIFIED AGENT ENDPOINT
@app.post("/api/agent", response_model=LibreOfficeResponse)
async def process_agent_request(request_data: LibreOfficeRequest):
    """
    Unified endpoint for all LibreOffice AI operations.
    
    This endpoint replaces the previous separate simple/moderate/complex endpoints
    with a single unified interface that uses DocumentMasterAgent for intelligent
    routing and complexity analysis.
    
    Flow:
    1. Receives request + context from LibreOffice C++ AgentCoordinator
    2. Routes through LangGraphBridge for DocumentState conversion
    3. Uses DocumentMasterAgent for intelligent complexity analysis and routing
    4. Coordinates specialized agents based on detected complexity
    5. Returns structured operations + AI chat response for LibreOffice execution
    """
    logger.info(f"DEBUG: /api/agent endpoint called with request_id: {request_data.request_id}")
    logger.info(f"DEBUG: agent_processor available: {agent_processor is not None}")
    
    logger.info(f"📨 Received agent request: {request_data.request[:100]}...")
    
    try:
        # Process through LangGraph agent system
        response = await agent_processor.process_unified_request(request_data)
        
        logger.info(f"✅ Agent request processed successfully: {response.request_id}")
        return response
        
    except Exception as e:
        logger.error(f"❌ Agent request processing failed: {e}")
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail=f"Agent processing failed: {str(e)}"
        )

# Legacy endpoint compatibility (optional - can be removed once C++ is updated)
@app.post("/api/simple", response_model=LibreOfficeResponse)
@app.post("/api/moderate", response_model=LibreOfficeResponse) 
@app.post("/api/complex", response_model=LibreOfficeResponse)
async def legacy_endpoint_compatibility(request_data: LibreOfficeRequest):
    """
    Legacy endpoint compatibility for gradual migration.
    All requests are now routed through the unified agent system.
    """
    logger.info("📨 Legacy endpoint accessed - routing to unified agent system")
    return await process_agent_request(request_data)

def start_server(host: str = "0.0.0.0", port: int = 8000, reload: bool = False):
    """Start the LangGraph Agent Server."""
    print("🤖 LibreOffice LangGraph Agent Server")
    print("=" * 60)
    print("🎯 Unified AI Agent System with LangGraph Integration")
    print(f"🌐 Server starting on http://{host}:{port}")
    print()
    print("📊 Available Endpoints:")
    print(f"   • POST http://{host}:{port}/api/agent    (Unified agent processing)")
    print(f"   • GET  http://{host}:{port}/health       (Health check)")
    print(f"   • GET  http://{host}:{port}/status       (Detailed status)")
    print(f"   • GET  http://{host}:{port}/docs         (API documentation)")
    print()
    print("🧠 Agent System Features:")
    print("   ✅ DocumentMasterAgent orchestration")
    print("   ✅ Intelligent complexity analysis and routing")
    print("   ✅ Multi-agent coordination (Context, Content, Formatting, Data, etc.)")
    print("   ✅ Comprehensive DocumentState management")
    print("   ✅ LangGraph bridge integration")
    print("   ✅ Unified error handling and recovery")
    print("   ✅ Performance optimization and monitoring")
    print("=" * 60)
    
    # Start the server
    uvicorn.run(
        "ai_agent_server:app",
        host=host,
        port=port,
        reload=reload,
        log_level="info",
        access_log=True
    )

if __name__ == "__main__":
    import sys
    
    # Handle command line arguments
    reload_mode = "--reload" in sys.argv or "--dev" in sys.argv
    
    start_server(reload=reload_mode)