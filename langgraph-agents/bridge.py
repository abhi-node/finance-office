#!/usr/bin/env python3
"""
LangGraph Bridge for LibreOffice Integration

This module provides the critical translation layer between LibreOffice C++
components and Python LangGraph agents. It implements the LangGraphBridge class
that handles bidirectional communication, document state conversion, and
progress streaming between the LibreOffice AgentCoordinator and the multi-agent
system.

Key Features:
- C++ document context to LangGraph DocumentState conversion
- Bidirectional communication with proper serialization/deserialization
- Progress updates streaming back to UI components
- Agent response to LibreOffice-compatible format conversion
- PyUNO or ctypes integration for C++ communication
- Comprehensive error handling and resource management

Architecture:
The bridge acts as the primary interface between:
1. LibreOffice C++ AgentCoordinator (sw/source/core/ai/AgentCoordinator.cxx)
2. Python LangGraph Multi-Agent System (agents/*.py)
3. UI Progress Display (sw/source/ui/sidebar/ai/AIPanel.cxx)
"""

import asyncio
import json
import logging
import os
import time
import threading
import uuid
from abc import ABC, abstractmethod
from dataclasses import dataclass, field, asdict
from datetime import datetime, timezone
from enum import Enum
from pathlib import Path
from typing import Any, Dict, List, Optional, Union, Callable, AsyncGenerator
from concurrent.futures import ThreadPoolExecutor
import traceback

# LangGraph and agent imports
from langgraph.graph import StateGraph
from langchain_core.messages import BaseMessage, HumanMessage, AIMessage, SystemMessage

# Import our agent system
from agents.shared_cache import SharedCacheMixin
from state.document_state import DocumentState
from agents.document_master import DocumentMasterAgent
from agents.context_analysis import ContextAnalysisAgent
from agents.content_generation import ContentGenerationAgent
from agents.formatting import FormattingAgent
from agents.data_integration import DataIntegrationAgent


# No PyUNO dependencies - pure API integration
PYUNO_AVAILABLE = False


class IntegrationMethod(Enum):
    """Available integration methods for C++ communication."""
    PYUNO = "pyuno"
    CTYPES = "ctypes"
    HTTP_API = "http_api"
    WEBSOCKET = "websocket"


class BridgeStatus(Enum):
    """Bridge operational status."""
    DISCONNECTED = "disconnected"
    INITIALIZING = "initializing"
    CONNECTED = "connected"
    PROCESSING = "processing"
    ERROR = "error"
    SHUTDOWN = "shutdown"


@dataclass
class BridgeConfiguration:
    """Configuration for LangGraphBridge."""
    integration_method: IntegrationMethod = IntegrationMethod.HTTP_API
    max_concurrent_operations: int = 5
    operation_timeout_seconds: int = 30
    enable_progress_streaming: bool = True
    enable_websocket: bool = False
    websocket_port: int = 8765
    log_level: str = "INFO"
    cache_enabled: bool = True
    cache_ttl_seconds: int = 300
    max_retry_attempts: int = 3
    retry_delay_seconds: float = 1.0
    resource_cleanup_interval: int = 60
    
    # C++ Integration Settings
    libreoffice_socket_path: Optional[str] = None
    uno_component_context: Optional[str] = None
    
    # Agent Configuration
    enable_all_agents: bool = True
    agent_priorities: Dict[str, int] = field(default_factory=lambda: {
        "document_master": 1,
        "context_analysis": 2,
        "content_generation": 3,
        "formatting": 4,
        "data_integration": 5
    })


@dataclass
class OperationRequest:
    """Request structure for bridge operations."""
    request_id: str
    user_message: str
    document_context: Dict[str, Any]
    operation_type: str = "process_request"
    priority: int = 1
    timeout_seconds: Optional[int] = None
    metadata: Dict[str, Any] = field(default_factory=dict)
    timestamp: float = field(default_factory=time.time)


@dataclass
class OperationResponse:
    """Response structure for bridge operations."""
    request_id: str
    success: bool
    result: Optional[Dict[str, Any]] = None
    error_message: Optional[str] = None
    progress_updates: List[Dict[str, Any]] = field(default_factory=list)
    execution_time_ms: float = 0.0
    agent_results: Dict[str, Any] = field(default_factory=dict)
    final_state: Optional[Dict[str, Any]] = None
    metadata: Dict[str, Any] = field(default_factory=dict)


@dataclass
class ProgressUpdate:
    """Progress update structure for streaming."""
    request_id: str
    operation_stage: str
    progress_percentage: float
    current_agent: Optional[str] = None
    status_message: str = ""
    estimated_remaining_seconds: Optional[float] = None
    partial_results: Optional[Dict[str, Any]] = None
    timestamp: float = field(default_factory=time.time)


class CppDocumentContext:
    """
    Helper class to handle C++ document context structures.
    
    This class provides methods to convert between C++ UNO structures
    and Python dictionaries for seamless integration.
    """
    
    @staticmethod
    def extract_json_context(json_context: Union[str, Dict[str, Any]]) -> Dict[str, Any]:
        """
        Extract document context from JSON data (pure API mode).
        
        Args:
            json_context: JSON string or dictionary with context from C++
            
        Returns:
            Python dictionary with extracted context
        """
        try:
            if isinstance(json_context, str):
                return json.loads(json_context)
            elif isinstance(json_context, dict):
                return json_context
            else:
                return {"raw_context": str(json_context)}
        except Exception as e:
            logging.warning(f"Failed to extract JSON context: {e}")
            return {"error": str(e), "raw_context": str(json_context)}
    
    @staticmethod
    def create_json_response(response_data: Dict[str, Any]) -> str:
        """
        Create JSON response for C++ HTTP communication.
        
        Args:
            response_data: Python dictionary with response data
            
        Returns:
            JSON string for HTTP response
        """
        try:
            return json.dumps(response_data, indent=2)
        except Exception as e:
            logging.warning(f"Failed to create JSON response: {e}")
            return json.dumps({"error": f"Response serialization failed: {e}"})


class BaseIntegrationLayer(ABC):
    """Abstract base class for C++ integration methods."""
    
    def __init__(self, config: BridgeConfiguration):
        self.config = config
        self.logger = logging.getLogger(f"{__name__}.{self.__class__.__name__}")
    
    @abstractmethod
    async def initialize(self) -> bool:
        """Initialize the integration layer."""
        pass
    
    @abstractmethod
    async def shutdown(self) -> bool:
        """Shutdown the integration layer."""
        pass
    
    @abstractmethod
    async def send_progress_update(self, update: ProgressUpdate) -> bool:
        """Send progress update to C++ side."""
        pass
    
    @abstractmethod
    async def send_response(self, response: OperationResponse) -> bool:
        """Send final response to C++ side."""
        pass


class HttpApiIntegrationLayer(BaseIntegrationLayer):
    """HTTP API-based integration layer for pure API communication."""
    
    def __init__(self, config: BridgeConfiguration):
        super().__init__(config)
        self.api_available = True
    
    async def initialize(self) -> bool:
        """Initialize HTTP API integration."""
        try:
            # HTTP API integration is always available - no special initialization needed
            self.logger.info("HTTP API integration initialized successfully")
            return True
        except Exception as e:
            self.logger.error(f"HTTP API initialization failed: {e}")
            return False
    
    async def shutdown(self) -> bool:
        """Shutdown HTTP API integration."""
        try:
            self.api_available = False
            self.logger.info("HTTP API integration shutdown complete")
            return True
        except Exception as e:
            self.logger.error(f"HTTP API shutdown failed: {e}")
            return False
    
    async def send_progress_update(self, update: ProgressUpdate) -> bool:
        """Send progress update through HTTP response."""
        try:
            # Progress updates are sent back through HTTP response
            self.logger.debug(f"Progress update: {update.operation_stage} - {update.progress_percentage:.1f}%")
            return True
        except Exception as e:
            self.logger.error(f"Failed to send progress update: {e}")
            return False
    
    async def send_response(self, response: OperationResponse) -> bool:
        """Send final response via HTTP JSON."""
        try:
            # Convert response to JSON format
            json_response = CppDocumentContext.create_json_response(asdict(response))
            self.logger.info(f"Sent response for request {response.request_id}")
            return True
        except Exception as e:
            self.logger.error(f"Failed to send response: {e}")
            return False


# CtypesIntegrationLayer removed - using pure HTTP API communication


class LangGraphBridge:
    """
    Main bridge class for LibreOffice-LangGraph integration.
    
    This class serves as the primary interface between LibreOffice C++
    components and the Python LangGraph multi-agent system. It handles
    request routing, state conversion, progress streaming, and response
    formatting.
    """
    
    def __init__(self, config: Optional[BridgeConfiguration] = None):
        """
        Initialize the LangGraph bridge.
        
        Args:
            config: Optional bridge configuration
        """
        self.config = config or BridgeConfiguration()
        self.logger = self._setup_logging()
        
        # Bridge state
        self.status = BridgeStatus.DISCONNECTED
        self.operation_count = 0
        self.start_time = time.time()
        
        # Component management
        self.integration_layer: Optional[BaseIntegrationLayer] = None
        self.agent_graph: Optional[StateGraph] = None
        self.document_master_agent = None  # NEW: DocumentMasterAgent instance
        self.executor = ThreadPoolExecutor(max_workers=self.config.max_concurrent_operations)
        
        # Operation tracking
        self.active_operations: Dict[str, OperationRequest] = {}
        self.operation_results: Dict[str, OperationResponse] = {}
        self.progress_callbacks: Dict[str, List[Callable]] = {}
        
        # Performance monitoring
        self.performance_metrics = {
            "total_operations": 0,
            "successful_operations": 0,
            "failed_operations": 0,
            "average_response_time_ms": 0.0,
            "total_processing_time_ms": 0.0
        }
        
        # Resource management
        self._cleanup_thread = None
        self._shutdown_event = threading.Event()
        
        self.logger.info("DEBUG: LangGraphBridge.__init__() called - new instance created")
        self.logger.info("LangGraphBridge initialized")
    
    def _setup_logging(self) -> logging.Logger:
        """Setup logging configuration."""
        logger = logging.getLogger(__name__)
        logger.setLevel(getattr(logging, self.config.log_level))
        
        if not logger.handlers:
            handler = logging.StreamHandler()
            formatter = logging.Formatter(
                '%(asctime)s - %(name)s - %(levelname)s - %(message)s'
            )
            handler.setFormatter(formatter)
            logger.addHandler(handler)
        
        return logger
    
    async def initialize(self) -> bool:
        """
        Initialize the bridge and all its components.
        
        Returns:
            True if initialization successful, False otherwise
        """
        try:
            self.status = BridgeStatus.INITIALIZING
            self.logger.info("Initializing LangGraphBridge...")
            
            # Initialize integration layer
            if not await self._initialize_integration_layer():
                self.logger.error("Failed to initialize integration layer")
                return False
            
            # Initialize agent graph
            if not await self._initialize_agent_graph():
                self.logger.error("Failed to initialize agent graph")
                return False
            
            # Start resource cleanup thread
            self._start_cleanup_thread()
            
            self.status = BridgeStatus.CONNECTED
            self.logger.info("LangGraphBridge initialization complete")
            return True
            
        except Exception as e:
            self.status = BridgeStatus.ERROR
            self.logger.error(f"Bridge initialization failed: {e}")
            self.logger.debug(traceback.format_exc())
            return False
    
    async def _initialize_integration_layer(self) -> bool:
        """Initialize the C++ integration layer."""
        try:
            # Always use HTTP API integration (pure API mode)
            self.integration_layer = HttpApiIntegrationLayer(self.config)
            
            return await self.integration_layer.initialize()
            
        except Exception as e:
            self.logger.error(f"Integration layer initialization failed: {e}")
            return False
    
    async def _initialize_agent_graph(self) -> bool:
        """Initialize DocumentMasterAgent - simplified approach with single orchestrator."""
        try:
            # Create only DocumentMasterAgent - it will handle all internal orchestration
            if self.config.enable_all_agents:
                self.document_master_agent = DocumentMasterAgent("document_master_agent")
                self.logger.info("DEBUG: DocumentMasterAgent created successfully")
                
                # Log registered agents for debugging
                registered_agents = self.document_master_agent.get_registered_agents()
                self.logger.info(f"DEBUG: DocumentMaster has {len(registered_agents)} registered agents: {list(registered_agents.keys())}")
                
                # No LangGraph workflow needed - DocumentMaster handles everything internally
                self.agent_graph = None  # Not using LangGraph orchestration anymore
                
                self.logger.info("DocumentMasterAgent initialized - handling internal orchestration")
                return True
            else:
                self.logger.error("All agents disabled in configuration")
                return False
            
        except Exception as e:
            self.logger.error(f"DocumentMasterAgent initialization failed: {e}")
            self.logger.error(f"Exception details: {traceback.format_exc()}")
            return False
    
    def _create_agent_node(self, agent):
        """Create a node function for an agent."""
        async def agent_node(state: DocumentState) -> DocumentState:
            try:
                # Process request through agent
                result = await agent.process(state)
                
                # Update state with agent results
                if result.success and result.state_updates:
                    state.update(result.state_updates)
                
                return state
                
            except Exception as e:
                self.logger.error(f"Agent {agent.agent_id} processing failed: {e}")
                return state
        
        return agent_node
    
    def _start_cleanup_thread(self):
        """Start background cleanup thread."""
        def cleanup_worker():
            while not self._shutdown_event.wait(self.config.resource_cleanup_interval):
                try:
                    self._cleanup_expired_operations()
                except Exception as e:
                    self.logger.error(f"Cleanup thread error: {e}")
        
        self._cleanup_thread = threading.Thread(target=cleanup_worker, daemon=True)
        self._cleanup_thread.start()
        self.logger.debug("Cleanup thread started")
    
    def _cleanup_expired_operations(self):
        """Clean up expired operations and results."""
        current_time = time.time()
        expired_operations = []
        
        # Find expired operations
        for request_id, request in self.active_operations.items():
            if current_time - request.timestamp > (request.timeout_seconds or self.config.operation_timeout_seconds):
                expired_operations.append(request_id)
        
        # Remove expired operations
        for request_id in expired_operations:
            self.logger.warning(f"Operation {request_id} expired, cleaning up")
            self.active_operations.pop(request_id, None)
            self.operation_results.pop(request_id, None)
            self.progress_callbacks.pop(request_id, None)
    
    async def shutdown(self) -> bool:
        """
        Shutdown the bridge and cleanup resources.
        
        Returns:
            True if shutdown successful
        """
        try:
            self.status = BridgeStatus.SHUTDOWN
            self.logger.info("Shutting down LangGraphBridge...")
            
            # Signal cleanup thread to stop
            self._shutdown_event.set()
            if self._cleanup_thread and self._cleanup_thread.is_alive():
                self._cleanup_thread.join(timeout=5)
            
            # Cancel active operations
            for request_id in list(self.active_operations.keys()):
                await self.cancel_operation(request_id)
            
            # Shutdown integration layer
            if self.integration_layer:
                await self.integration_layer.shutdown()
            
            # Shutdown executor
            self.executor.shutdown(wait=True)
            
            self.logger.info("LangGraphBridge shutdown complete")
            return True
            
        except Exception as e:
            self.logger.error(f"Bridge shutdown failed: {e}")
            return False
    
    def get_status(self) -> Dict[str, Any]:
        """
        Get current bridge status and metrics.
        
        Returns:
            Dictionary with status information
        """
        uptime_seconds = time.time() - self.start_time
        
        return {
            "status": self.status.value,
            "uptime_seconds": uptime_seconds,
            "integration_method": self.config.integration_method.value,
            "active_operations": len(self.active_operations),
            "total_operations": self.operation_count,
            "performance_metrics": self.performance_metrics.copy(),
            "configuration": {
                "max_concurrent_operations": self.config.max_concurrent_operations,
                "operation_timeout_seconds": self.config.operation_timeout_seconds,
                "enable_progress_streaming": self.config.enable_progress_streaming,
                "cache_enabled": self.config.cache_enabled
            }
        }
    
    async def process_cpp_request(self, request_id: str, user_message: str, 
                                document_context: Any) -> str:
        """
        Process request from C++ AgentCoordinator.
        
        Args:
            request_id: Unique request identifier
            user_message: User's natural language request
            document_context: C++ document context (UNO Any or JSON string)
            
        Returns:
            JSON-serialized response
        """
        try:
            start_time = time.time()
            self.operation_count += 1
            
            self.logger.info(f"DEBUG: Bridge.process_cpp_request() called with request_id={request_id}")
            self.logger.info(f"DEBUG: User message: {user_message}")
            self.logger.info(f"DEBUG: Has document_master_agent: {hasattr(self, 'document_master_agent') and self.document_master_agent is not None}")
            
            self.logger.info(f"Processing C++ request {request_id}: {user_message}")
            
            # Convert C++ document context to LangGraph DocumentState
            document_state = await self._convert_cpp_context_to_document_state(
                user_message, document_context
            )
            
            # Create operation request
            operation_request = OperationRequest(
                request_id=request_id,
                user_message=user_message,
                document_context=document_context,
                timeout_seconds=self.config.operation_timeout_seconds
            )
            
            # Track operation
            self.active_operations[request_id] = operation_request
            
            # Process through DocumentMasterAgent directly  
            if hasattr(self, 'document_master_agent') and self.document_master_agent:
                self.logger.info("DEBUG: Bridge has DocumentMasterAgent - starting processing")
                
                # Send progress update - starting processing
                await self._send_progress_update(request_id, "initializing", 0.0, 
                                               "Starting DocumentMasterAgent processing")
                
                final_state = None
                progress_count = 0
                total_agents = 5  # Approximate number of processing stages
                
                # Stream through DocumentMasterAgent with progress updates
                self.logger.info("DEBUG: About to call _stream_agent_processing")
                async for state_update in self._stream_agent_processing(document_state):
                    progress_count += 1
                    progress_percentage = min((progress_count / total_agents) * 100, 95.0)
                    
                    self.logger.info(f"DEBUG: Received state_update from DocumentMaster - progress: {progress_count}")
                    
                    await self._send_progress_update(
                        request_id, 
                        "processing", 
                        progress_percentage,
                        f"Processing through DocumentMaster {progress_count}/{total_agents}"
                    )
                    
                    final_state = state_update
                
                self.logger.info("DEBUG: Finished processing through DocumentMaster")
                
                # Convert final state back to LibreOffice format
                await self._send_progress_update(request_id, "finalizing", 95.0, 
                                               "Converting results to LibreOffice format")
                
                libreoffice_response = await self._convert_agent_state_to_libreoffice_format(
                    final_state, request_id
                )
                
                execution_time_ms = (time.time() - start_time) * 1000
                
                # Create successful response
                response = OperationResponse(
                    request_id=request_id,
                    success=True,
                    result=libreoffice_response,
                    execution_time_ms=execution_time_ms,
                    final_state=final_state
                )
                
                # Update metrics
                self._update_performance_metrics(execution_time_ms, True)
                
                # Send final progress update
                await self._send_progress_update(request_id, "completed", 100.0, 
                                               "Processing completed successfully")
                
            else:
                # Fallback response if DocumentMasterAgent not available
                self.logger.error("DEBUG: DocumentMasterAgent not available for processing!")
                response = OperationResponse(
                    request_id=request_id,
                    success=False,
                    error_message="DocumentMasterAgent not initialized"
                )
                self._update_performance_metrics(0, False)
            
            # Store result and cleanup
            self.operation_results[request_id] = response
            self.active_operations.pop(request_id, None)
            
            # Send response via integration layer
            if self.integration_layer:
                await self.integration_layer.send_response(response)
            
            return self._serialize_response(response)
            
        except Exception as e:
            execution_time_ms = (time.time() - start_time) * 1000
            self.logger.error(f"Request processing failed for {request_id}: {e}")
            self.logger.debug(traceback.format_exc())
            
            # Create error response
            error_response = OperationResponse(
                request_id=request_id,
                success=False,
                error_message=f"Processing failed: {str(e)}",
                execution_time_ms=execution_time_ms
            )
            
            self._update_performance_metrics(execution_time_ms, False)
            
            # Cleanup
            self.active_operations.pop(request_id, None)
            self.operation_results[request_id] = error_response
            self.logger.debug(f"LangGraphBridge.process_cpp_request() - Cleaned up failed operation {request_id}")
            
            return self._serialize_response(error_response)
    
    def _serialize_response(self, response: OperationResponse) -> str:
        """
        Serialize OperationResponse to JSON, handling LangChain message objects.
        
        Args:
            response: OperationResponse to serialize
            
        Returns:
            JSON string
        """
        try:
            # Convert to dictionary
            response_dict = asdict(response)
            
            # Convert any LangChain message objects to serializable format
            response_dict = self._convert_messages_to_dict(response_dict)
            
            return json.dumps(response_dict)
            
        except Exception as e:
            self.logger.warning(f"Failed to serialize response: {e}")
            # Fallback to basic response
            return json.dumps({
                "request_id": response.request_id,
                "success": response.success,
                "error_message": f"Serialization failed: {str(e)}",
                "execution_time_ms": response.execution_time_ms
            })
    
    def _convert_messages_to_dict(self, obj: Any) -> Any:
        """
        Recursively convert LangChain message objects to dictionaries.
        
        Args:
            obj: Object to convert
            
        Returns:
            Converted object with messages as dictionaries
        """
        if isinstance(obj, (HumanMessage, AIMessage, SystemMessage)):
            return {
                "type": obj.__class__.__name__,
                "content": obj.content,
                "additional_kwargs": getattr(obj, 'additional_kwargs', {})
            }
        elif isinstance(obj, dict):
            return {key: self._convert_messages_to_dict(value) for key, value in obj.items()}
        elif isinstance(obj, list):
            return [self._convert_messages_to_dict(item) for item in obj]
        else:
            return obj
    
    async def _convert_cpp_context_to_document_state(self, user_message: str, 
                                                   cpp_context: Any) -> DocumentState:
        """
        Convert C++ document context to LangGraph DocumentState format.
        
        Args:
            user_message: User's request message
            cpp_context: C++ document context (UNO Any, dict, or JSON string)
            
        Returns:
            LangGraph DocumentState
        """
        try:
            # Initialize base DocumentState structure
            document_state = DocumentState(
                # Document Context
                current_document={},
                cursor_position={},
                selected_text="",
                document_structure={},
                formatting_state={},
                
                # Agent Communication
                messages=[HumanMessage(content=user_message)],
                current_task=user_message,
                task_history=[],
                agent_status={},
                
                # Content and Analysis
                content_analysis={},
                generated_content=[],
                content_suggestions=[],
                
                # External Integration
                external_data={},
                research_citations=[],
                api_usage={},
                
                # Operations Management
                pending_operations=[],
                completed_operations=[],
                validation_results={},
                
                # Error Handling and Recovery
                last_error=None,
                retry_count=0,
                error_recovery={},
                rollback_points=[],
                
                # User Interaction
                user_preferences={},
                interaction_history=[],
                approval_required=[],
                
                # Performance and Optimization
                performance_metrics={},
                resource_utilization={},
                optimization_recommendations=[]
            )
            
            # Convert C++ context based on its type
            if cpp_context is None:
                self.logger.warning("No document context provided")
                context_dict = {}
            elif isinstance(cpp_context, str):
                # JSON string from C++
                try:
                    context_dict = json.loads(cpp_context)
                except json.JSONDecodeError as e:
                    self.logger.warning(f"Failed to parse JSON context: {e}")
                    context_dict = {"raw_context": cpp_context}
            elif isinstance(cpp_context, dict):
                # Already a dictionary
                context_dict = cpp_context
            else:
                # Try to extract as JSON context (pure API mode)
                context_dict = CppDocumentContext.extract_json_context(cpp_context)
            
            # Map C++ context fields to DocumentState
            self.logger.debug("LangGraphBridge._convert_cpp_context_to_document_state() - Mapping context fields to DocumentState")
            await self._map_document_context(context_dict, document_state)
            await self._map_cursor_context(context_dict, document_state)
            await self._map_document_structure(context_dict, document_state)
            await self._map_formatting_state(context_dict, document_state)
            await self._map_user_preferences(context_dict, document_state)
            
            self.logger.info(f"LangGraphBridge._convert_cpp_context_to_document_state() - Successfully converted C++ context to DocumentState")
            return document_state
            
        except Exception as e:
            self.logger.error(f"LangGraphBridge._convert_cpp_context_to_document_state() - Failed to convert C++ context: {e}")
            self.logger.error(f"LangGraphBridge._convert_cpp_context_to_document_state() - Exception details: {traceback.format_exc()}")
            
            # Return minimal valid DocumentState
            self.logger.warning("LangGraphBridge._convert_cpp_context_to_document_state() - Returning minimal DocumentState due to error")
            return DocumentState(
                messages=[HumanMessage(content=user_message)],
                current_task=user_message,
                current_document={"error": f"Context conversion failed: {str(e)}"},
                cursor_position={},
                selected_text="",
                document_structure={},
                formatting_state={},
                task_history=[],
                agent_status={},
                content_analysis={},
                generated_content=[],
                content_suggestions=[],
                external_data={},
                research_citations=[],
                api_usage={},
                pending_operations=[],
                completed_operations=[],
                validation_results={},
                last_error=str(e),
                retry_count=0,
                error_recovery={},
                rollback_points=[],
                user_preferences={},
                interaction_history=[],
                approval_required=[],
                performance_metrics={},
                resource_utilization={},
                optimization_recommendations=[]
            )
    
    async def _map_document_context(self, context_dict: Dict[str, Any], 
                                  document_state: DocumentState):
        """Map document-related context to DocumentState."""
        try:
            current_document = {}
            
            # Extract document identification
            if "document_path" in context_dict:
                current_document["path"] = context_dict["document_path"]
            elif "document_url" in context_dict:
                current_document["path"] = context_dict["document_url"]
            
            if "document_title" in context_dict:
                current_document["title"] = context_dict["document_title"]
            elif "title" in context_dict:
                current_document["title"] = context_dict["title"]
            
            # Extract document type and format
            if "document_type" in context_dict:
                current_document["type"] = context_dict["document_type"]
            if "document_format" in context_dict:
                current_document["format"] = context_dict["document_format"]
            
            # Extract modification state
            if "is_modified" in context_dict:
                current_document["modified"] = context_dict["is_modified"]
            if "last_saved" in context_dict:
                current_document["last_saved"] = context_dict["last_saved"]
            
            # Extract document content summary
            if "content_summary" in context_dict:
                current_document["content_summary"] = context_dict["content_summary"]
            if "word_count" in context_dict:
                current_document["word_count"] = context_dict["word_count"]
            if "page_count" in context_dict:
                current_document["page_count"] = context_dict["page_count"]
            
            document_state["current_document"] = current_document
            
        except Exception as e:
            self.logger.warning(f"Failed to map document context: {e}")
    
    async def _map_cursor_context(self, context_dict: Dict[str, Any], 
                                document_state: DocumentState):
        """Map cursor and selection context to DocumentState."""
        try:
            cursor_position = {}
            
            # Extract cursor position
            if "cursor_line" in context_dict:
                cursor_position["line"] = context_dict["cursor_line"]
            if "cursor_column" in context_dict:
                cursor_position["column"] = context_dict["cursor_column"]
            if "cursor_paragraph" in context_dict:
                cursor_position["paragraph"] = context_dict["cursor_paragraph"]
            if "cursor_character" in context_dict:
                cursor_position["character"] = context_dict["cursor_character"]
            
            # Extract page and position information
            if "current_page" in context_dict:
                cursor_position["page"] = context_dict["current_page"]
            if "cursor_offset" in context_dict:
                cursor_position["offset"] = context_dict["cursor_offset"]
            
            document_state["cursor_position"] = cursor_position
            
            # Extract selected text
            selected_text = ""
            if "selected_text" in context_dict:
                selected_text = context_dict["selected_text"]
            elif "selection" in context_dict:
                if isinstance(context_dict["selection"], str):
                    selected_text = context_dict["selection"]
                elif isinstance(context_dict["selection"], dict):
                    selected_text = context_dict["selection"].get("text", "")
            
            document_state["selected_text"] = selected_text
            
        except Exception as e:
            self.logger.warning(f"Failed to map cursor context: {e}")
    
    async def _map_document_structure(self, context_dict: Dict[str, Any], 
                                    document_state: DocumentState):
        """Map document structure information to DocumentState with enhanced LibreOffice data extraction."""
        try:
            document_structure = {}
            
            # Extract basic document structure counts
            if "paragraph_count" in context_dict:
                document_structure["paragraphs"] = context_dict["paragraph_count"]
            if "section_count" in context_dict:
                document_structure["sections"] = context_dict["section_count"]
            if "table_count" in context_dict:
                document_structure["tables"] = context_dict["table_count"]
            if "image_count" in context_dict:
                document_structure["images"] = context_dict["image_count"]
            if "chart_count" in context_dict:
                document_structure["charts"] = context_dict["chart_count"]
            
            # Extract detailed structural elements with full LibreOffice context
            if "document_outline" in context_dict:
                document_structure["outline"] = context_dict["document_outline"]
            if "table_of_contents" in context_dict:
                document_structure["toc"] = context_dict["table_of_contents"]
            if "headings" in context_dict:
                document_structure["headings"] = context_dict["headings"]
            
            # Enhanced: Extract detailed table information
            if "table_details" in context_dict:
                document_structure["table_details"] = context_dict["table_details"]
            if "tables" in context_dict and isinstance(context_dict["tables"], list):
                document_structure["table_list"] = []
                for table_info in context_dict["tables"]:
                    if isinstance(table_info, dict):
                        document_structure["table_list"].append({
                            "name": table_info.get("name", ""),
                            "rows": table_info.get("rows", 0),
                            "columns": table_info.get("columns", 0),
                            "position": table_info.get("position", {}),
                            "style": table_info.get("style", "")
                        })
            
            # Enhanced: Extract detailed image/graphics information  
            if "images" in context_dict and isinstance(context_dict["images"], list):
                document_structure["image_list"] = []
                for image_info in context_dict["images"]:
                    if isinstance(image_info, dict):
                        document_structure["image_list"].append({
                            "name": image_info.get("name", ""),
                            "type": image_info.get("type", ""),
                            "size": image_info.get("size", {}),
                            "position": image_info.get("position", {}),
                            "anchor": image_info.get("anchor", "")
                        })
            
            # Enhanced: Extract section details with formatting
            if "sections" in context_dict and isinstance(context_dict["sections"], list):
                document_structure["section_list"] = []
                for section_info in context_dict["sections"]:
                    if isinstance(section_info, dict):
                        document_structure["section_list"].append({
                            "name": section_info.get("name", ""),
                            "start_page": section_info.get("start_page", 1),
                            "protection": section_info.get("protected", False),
                            "columns": section_info.get("columns", 1),
                            "header": section_info.get("header", {}),
                            "footer": section_info.get("footer", {})
                        })
            
            # Enhanced: Extract content organization with LibreOffice specifics
            if "styles_used" in context_dict:
                document_structure["styles"] = context_dict["styles_used"]
            if "style_definitions" in context_dict:
                document_structure["style_definitions"] = context_dict["style_definitions"]
            if "references" in context_dict:
                document_structure["references"] = context_dict["references"]
            if "footnotes" in context_dict:
                document_structure["footnotes"] = context_dict["footnotes"]
            if "endnotes" in context_dict:
                document_structure["endnotes"] = context_dict["endnotes"]
            if "bookmarks" in context_dict:
                document_structure["bookmarks"] = context_dict["bookmarks"]
            if "cross_references" in context_dict:
                document_structure["cross_references"] = context_dict["cross_references"]
            
            # Enhanced: Extract page layout information
            if "page_layout" in context_dict:
                document_structure["page_layout"] = context_dict["page_layout"]
            if "headers_footers" in context_dict:
                document_structure["headers_footers"] = context_dict["headers_footers"]
            if "page_breaks" in context_dict:
                document_structure["page_breaks"] = context_dict["page_breaks"]
            
            # Enhanced: Extract track changes and collaboration data
            if "track_changes" in context_dict:
                document_structure["track_changes"] = {
                    "enabled": context_dict["track_changes"].get("enabled", False),
                    "changes": context_dict["track_changes"].get("changes", []),
                    "comments": context_dict["track_changes"].get("comments", [])
                }
            
            # Enhanced: Extract field information (mail merge, formulas, etc.)
            if "fields" in context_dict:
                document_structure["fields"] = context_dict["fields"]
            if "forms" in context_dict:
                document_structure["forms"] = context_dict["forms"]
            
            document_state["document_structure"] = document_structure
            
        except Exception as e:
            self.logger.warning(f"Failed to map document structure: {e}")
    
    async def _map_formatting_state(self, context_dict: Dict[str, Any], 
                                  document_state: DocumentState):
        """Map formatting state information to DocumentState with comprehensive LibreOffice formatting data."""
        try:
            formatting_state = {}
            
            # Extract current character formatting with full LibreOffice support
            if "current_font" in context_dict:
                formatting_state["font_family"] = context_dict["current_font"]
            if "current_font_size" in context_dict:
                formatting_state["font_size"] = context_dict["current_font_size"]
            if "current_style" in context_dict:
                formatting_state["current_style"] = context_dict["current_style"]
            
            # Enhanced: Extract comprehensive text formatting
            character_formatting = {}
            if "is_bold" in context_dict:
                character_formatting["bold"] = context_dict["is_bold"]
            if "is_italic" in context_dict:
                character_formatting["italic"] = context_dict["is_italic"]
            if "is_underline" in context_dict:
                character_formatting["underline"] = context_dict["is_underline"]
            if "is_strikethrough" in context_dict:
                character_formatting["strikethrough"] = context_dict["is_strikethrough"]
            if "font_color" in context_dict:
                character_formatting["color"] = context_dict["font_color"]
            if "background_color" in context_dict:
                character_formatting["background"] = context_dict["background_color"]
            if "superscript" in context_dict:
                character_formatting["superscript"] = context_dict["superscript"]
            if "subscript" in context_dict:
                character_formatting["subscript"] = context_dict["subscript"]
            if "font_weight" in context_dict:
                character_formatting["weight"] = context_dict["font_weight"]
            if "font_style" in context_dict:
                character_formatting["style"] = context_dict["font_style"]
            formatting_state["character_formatting"] = character_formatting
            
            # Enhanced: Extract comprehensive paragraph formatting
            paragraph_formatting = {}
            if "paragraph_alignment" in context_dict:
                paragraph_formatting["alignment"] = context_dict["paragraph_alignment"]
            if "line_spacing" in context_dict:
                paragraph_formatting["line_spacing"] = context_dict["line_spacing"]
            if "line_spacing_value" in context_dict:
                paragraph_formatting["line_spacing_value"] = context_dict["line_spacing_value"]
            if "indent_level" in context_dict:
                paragraph_formatting["indent_level"] = context_dict["indent_level"]
            if "left_indent" in context_dict:
                paragraph_formatting["left_indent"] = context_dict["left_indent"]
            if "right_indent" in context_dict:
                paragraph_formatting["right_indent"] = context_dict["right_indent"]
            if "first_line_indent" in context_dict:
                paragraph_formatting["first_line_indent"] = context_dict["first_line_indent"]
            if "space_before" in context_dict:
                paragraph_formatting["space_before"] = context_dict["space_before"]
            if "space_after" in context_dict:
                paragraph_formatting["space_after"] = context_dict["space_after"]
            if "keep_together" in context_dict:
                paragraph_formatting["keep_together"] = context_dict["keep_together"]
            if "page_break_before" in context_dict:
                paragraph_formatting["page_break_before"] = context_dict["page_break_before"]
            if "widow_control" in context_dict:
                paragraph_formatting["widow_control"] = context_dict["widow_control"]
            if "orphan_control" in context_dict:
                paragraph_formatting["orphan_control"] = context_dict["orphan_control"]
            formatting_state["paragraph_formatting"] = paragraph_formatting
            
            # Enhanced: Extract comprehensive page formatting
            page_formatting = {}
            if "page_margins" in context_dict:
                page_formatting["margins"] = context_dict["page_margins"]
            if "page_orientation" in context_dict:
                page_formatting["orientation"] = context_dict["page_orientation"]
            if "page_size" in context_dict:
                page_formatting["size"] = context_dict["page_size"]
            if "page_width" in context_dict:
                page_formatting["width"] = context_dict["page_width"]
            if "page_height" in context_dict:
                page_formatting["height"] = context_dict["page_height"]
            if "margin_top" in context_dict:
                page_formatting["margin_top"] = context_dict["margin_top"]
            if "margin_bottom" in context_dict:
                page_formatting["margin_bottom"] = context_dict["margin_bottom"]
            if "margin_left" in context_dict:
                page_formatting["margin_left"] = context_dict["margin_left"]
            if "margin_right" in context_dict:
                page_formatting["margin_right"] = context_dict["margin_right"]
            if "header_height" in context_dict:
                page_formatting["header_height"] = context_dict["header_height"]
            if "footer_height" in context_dict:
                page_formatting["footer_height"] = context_dict["footer_height"]
            if "gutter_margin" in context_dict:
                page_formatting["gutter"] = context_dict["gutter_margin"]
            formatting_state["page_formatting"] = page_formatting
            
            # Enhanced: Extract table formatting (if cursor is in table)
            if "table_formatting" in context_dict:
                table_formatting = context_dict["table_formatting"]
                formatting_state["table_formatting"] = {
                    "border_style": table_formatting.get("border_style", ""),
                    "border_width": table_formatting.get("border_width", 0),
                    "border_color": table_formatting.get("border_color", ""),
                    "cell_background": table_formatting.get("cell_background", ""),
                    "cell_padding": table_formatting.get("cell_padding", {}),
                    "column_width": table_formatting.get("column_width", 0),
                    "row_height": table_formatting.get("row_height", 0)
                }
            
            # Enhanced: Extract numbering and bullets
            if "numbering_style" in context_dict:
                formatting_state["numbering"] = {
                    "style": context_dict["numbering_style"],
                    "level": context_dict.get("numbering_level", 0),
                    "format": context_dict.get("numbering_format", ""),
                    "start_value": context_dict.get("numbering_start", 1)
                }
            
            # Enhanced: Extract language and locale settings
            if "language" in context_dict:
                formatting_state["language"] = context_dict["language"]
            if "locale" in context_dict:
                formatting_state["locale"] = context_dict["locale"]
            if "spell_check" in context_dict:
                formatting_state["spell_check"] = context_dict["spell_check"]
            if "hyphenation" in context_dict:
                formatting_state["hyphenation"] = context_dict["hyphenation"]
            
            document_state["formatting_state"] = formatting_state
            
        except Exception as e:
            self.logger.warning(f"Failed to map formatting state: {e}")
    
    async def _map_user_preferences(self, context_dict: Dict[str, Any], 
                                  document_state: DocumentState):
        """Map user preferences to DocumentState."""
        try:
            user_preferences = {}
            
            # Extract user settings
            if "user_language" in context_dict:
                user_preferences["language"] = context_dict["user_language"]
            if "spell_check_enabled" in context_dict:
                user_preferences["spell_check"] = context_dict["spell_check_enabled"]
            if "auto_save_enabled" in context_dict:
                user_preferences["auto_save"] = context_dict["auto_save_enabled"]
            
            # Extract UI preferences
            if "sidebar_visible" in context_dict:
                user_preferences["sidebar_visible"] = context_dict["sidebar_visible"]
            if "zoom_level" in context_dict:
                user_preferences["zoom_level"] = context_dict["zoom_level"]
            if "view_mode" in context_dict:
                user_preferences["view_mode"] = context_dict["view_mode"]
            
            # Extract workflow preferences
            if "preferred_response_time" in context_dict:
                user_preferences["response_time"] = context_dict["preferred_response_time"]
            if "enable_auto_format" in context_dict:
                user_preferences["auto_format"] = context_dict["enable_auto_format"]
            if "collaboration_mode" in context_dict:
                user_preferences["collaboration"] = context_dict["collaboration_mode"]
            
            document_state["user_preferences"] = user_preferences
            
        except Exception as e:
            self.logger.warning(f"Failed to map user preferences: {e}")
    
    async def _stream_agent_processing(self, document_state: DocumentState) -> AsyncGenerator[DocumentState, None]:
        """
        Stream agent processing through DocumentMasterAgent orchestration.
        
        Args:
            document_state: Initial document state
            
        Yields:
            Updated document states from agent processing
        """
        try:
            if not hasattr(self, 'document_master_agent') or not self.document_master_agent:
                self.logger.error("DEBUG: DocumentMasterAgent not available for processing")
                yield document_state
                return
            
            self.logger.info("DEBUG: Starting DocumentMasterAgent processing")
            self.logger.info(f"DEBUG: Input document_state keys: {list(document_state.keys()) if isinstance(document_state, dict) else 'Not a dict'}")
            
            # Create HumanMessage from document state
            user_message = None
            if isinstance(document_state, dict) and 'current_task' in document_state:
                user_message = HumanMessage(content=document_state['current_task'])
                self.logger.info(f"DEBUG: Created HumanMessage with content: {document_state['current_task']}")
            else:
                self.logger.warning("DEBUG: No current_task found in document_state, using empty message")
                user_message = HumanMessage(content="Process request")
            
            # Process through DocumentMasterAgent directly - it handles ALL orchestration internally
            self.logger.info("DEBUG: Calling DocumentMasterAgent.process()")
            agent_result = await self.document_master_agent.process(document_state, user_message)
            
            self.logger.info(f"DEBUG: DocumentMasterAgent.process() completed - success: {agent_result.success}")
            self.logger.info(f"DEBUG: Agent result metadata: {agent_result.metadata}")
            
            # Update document state with the results
            if agent_result.success and agent_result.state_updates:
                self.logger.info(f"DEBUG: Applying state updates: {list(agent_result.state_updates.keys())}")
                if isinstance(document_state, dict):
                    document_state.update(agent_result.state_updates)
                else:
                    # Handle DocumentState object
                    for key, value in agent_result.state_updates.items():
                        setattr(document_state, key, value)
            
            # Add agent result information to state for response generation
            document_state['agent_result'] = {
                'success': agent_result.success,
                'result': agent_result.result,
                'execution_time': agent_result.execution_time,
                'metadata': agent_result.metadata,
                'warnings': agent_result.warnings,
                'error': agent_result.error
            }
            
            self.logger.info("DEBUG: Yielding final document state")
            yield document_state
            
        except Exception as e:
            self.logger.error(f"DocumentMasterAgent processing failed: {e}")
            self.logger.error(f"Exception details: {traceback.format_exc()}")
            yield document_state
    
    async def _convert_agent_state_to_libreoffice_format(self, final_state: DocumentState, 
                                                       request_id: str) -> Dict[str, Any]:
        """
        Convert final agent state to LibreOffice-compatible format.
        
        Extracts information from DocumentMasterAgent orchestration results.
        """
        try:
            self.logger.info("DEBUG: Converting agent state to LibreOffice format")
            
            libreoffice_response = {
                "request_id": request_id,
                "status": "success",
                "operations": [],
                "content_changes": {},
                "formatting_changes": {},
                "response_content": "Request processed successfully.",
                "operation_summaries": [],
                "complexity_detected": "unknown",
                "workflow_path": "unknown", 
                "agents_used": [],
                "execution_time_ms": 0.0,
                "warnings": [],
                "approval_required": [],
                "validation_results": {},
                "performance_metrics": {},
                "optimization_recommendations": [],
                "error_context": None,
                "recovery_options": [],
                "metadata": {}
            }
            
            # Extract information from DocumentMasterAgent result
            if final_state and isinstance(final_state, dict) and "agent_result" in final_state:
                agent_result = final_state["agent_result"]
                self.logger.info(f"DEBUG: Found agent_result in final_state: success={agent_result.get('success')}")
                
                # Extract orchestration metadata from DocumentMaster
                if agent_result.get("metadata") and "orchestration" in agent_result["metadata"]:
                    orchestration = agent_result["metadata"]["orchestration"]
                    libreoffice_response["complexity_detected"] = orchestration.get("complexity", "unknown")
                    libreoffice_response["workflow_path"] = orchestration.get("workflow_path", "unknown")
                    libreoffice_response["agents_used"] = orchestration.get("agents_used", [])
                    libreoffice_response["execution_time_ms"] = orchestration.get("execution_time", 0) * 1000
                    
                    # Performance metrics
                    libreoffice_response["performance_metrics"] = {
                        "execution_time_ms": orchestration.get("execution_time", 0) * 1000,
                        "complexity_detected": orchestration.get("complexity", "unknown"),
                        "performance_target_met": orchestration.get("performance_target_met", False),
                        "analysis_confidence": orchestration.get("analysis_confidence", 0.0),
                        "agents_count": len(orchestration.get("agents_used", []))
                    }
                
                # Extract actual agent results with operations
                if agent_result.get("result") and isinstance(agent_result["result"], dict):
                    result_data = agent_result["result"]
                    
                    # Extract operations from agent results
                    if "operation_results" in result_data:
                        operations = result_data["operation_results"]
                        if isinstance(operations, list):
                            valid_operations = []
                            operation_summaries = []
                            
                            for op in operations:
                                if isinstance(op, dict):
                                    # Create properly formatted operation
                                    clean_op = {
                                        "operation_id": op.get("operation_id", f"op_{len(valid_operations)}"),
                                        "operation_type": op.get("operation_type", "unknown"),
                                        "parameters": op.get("parameters", {}),
                                        "agent_id": op.get("agent_id", "unknown"),
                                        "priority": op.get("priority", 0),
                                        "status": op.get("status", "pending")
                                    }
                                    valid_operations.append(clean_op)
                                    
                                    # Create human-readable summary
                                    summary = self._create_operation_summary(clean_op)
                                    operation_summaries.append(summary)
                            
                            libreoffice_response["operations"] = valid_operations
                            libreoffice_response["operation_summaries"] = operation_summaries
                    
                    # Extract response content
                    if "response_content" in result_data:
                        libreoffice_response["response_content"] = result_data["response_content"]
                    elif "workflow_type" in result_data:
                        workflow_type = result_data["workflow_type"]
                        agents_executed = result_data.get("agents_executed", [])
                        libreoffice_response["response_content"] = f"Processed request using {workflow_type} workflow with agents: {', '.join(agents_executed)}"
                
                # Extract warnings
                if agent_result.get("warnings"):
                    libreoffice_response["warnings"] = agent_result["warnings"]
                
                # Extract error information
                if not agent_result.get("success") and agent_result.get("error"):
                    libreoffice_response["error_context"] = agent_result["error"]
                    libreoffice_response["status"] = "error"
                
                libreoffice_response["execution_time_ms"] = agent_result.get("execution_time", 0) * 1000
            
            # Extract standard operations from pending_operations if available
            if final_state and "pending_operations" in final_state:
                operations = final_state["pending_operations"]
                if isinstance(operations, list) and operations:
                    self.logger.info(f"DEBUG: Found {len(operations)} pending operations")
                    # Only add if we don't already have operations from agent_result
                    if not libreoffice_response["operations"]:
                        valid_operations = []
                        operation_summaries = []
                        
                        for op in operations:
                            if isinstance(op, dict) and "operation_type" in op:
                                clean_op = {
                                    "operation_id": op.get("operation_id", f"op_{len(valid_operations)}"),
                                    "operation_type": op.get("operation_type", "unknown"),
                                    "parameters": op.get("parameters", {}),
                                    "agent_id": op.get("agent_id", "unknown"),
                                    "priority": op.get("priority", 0),
                                    "status": op.get("status", "pending")
                                }
                                valid_operations.append(clean_op)
                                
                                summary = self._create_operation_summary(clean_op)
                                operation_summaries.append(summary)
                        
                        libreoffice_response["operations"] = valid_operations
                        libreoffice_response["operation_summaries"] = operation_summaries
            
            # Add basic metadata
            libreoffice_response["metadata"].update({
                "server_processing_time_ms": libreoffice_response["execution_time_ms"],
                "bridge_integration": "langgraph",
                "agent_orchestration_enabled": True,
                "request_count": self.operation_count,
                "server_uptime_seconds": time.time() - getattr(self, '_start_time', time.time())
            })
            
            self.logger.info(f"DEBUG: Final response - operations: {len(libreoffice_response['operations'])}, " +
                           f"agents_used: {len(libreoffice_response['agents_used'])}, " +
                           f"complexity: {libreoffice_response['complexity_detected']}")
            
            return libreoffice_response
            
        except Exception as e:
            self.logger.error(f"Failed to convert agent state to LibreOffice format: {e}")
            return {
                "request_id": request_id,
                "status": "error",
                "error": str(e),
                "operations": [],
                "content_changes": {},
                "formatting_changes": {},
                "response_content": f"Error processing request: {str(e)}",
                "operation_summaries": [],
                "metadata": {"error": True}
            }
    
    def _create_operation_summary(self, operation: Dict[str, Any]) -> str:
        """Create human-readable summary of an operation for user feedback."""
        try:
            op_type = operation.get("operation_type", "unknown")
            params = operation.get("parameters", {})
            
            # Generate operation-specific summaries
            if op_type == "text_insert" or op_type == "insert_text":
                text = params.get("text", "")[:50]
                return f"Insert text: '{text}...'" if len(text) > 47 else f"Insert text: '{text}'"
            
            elif op_type == "text_format" or op_type == "format_text":
                formatting = []
                if params.get("bold"):
                    formatting.append("bold")
                if params.get("italic"):
                    formatting.append("italic")
                if params.get("underline"):
                    formatting.append("underline")
                if params.get("font_size"):
                    formatting.append(f"size {params['font_size']}")
                return f"Apply formatting: {', '.join(formatting)}" if formatting else "Apply text formatting"
            
            elif op_type == "table_create" or op_type == "create_table":
                rows = params.get("rows", "?")
                cols = params.get("columns", "?")
                return f"Create table: {rows} rows × {cols} columns"
            
            elif op_type == "style_apply" or op_type == "apply_style":
                style = params.get("style_name", params.get("style", "unknown"))
                return f"Apply style: {style}"
            
            elif op_type == "image_insert" or op_type == "insert_image":
                return "Insert image"
            
            elif op_type == "chart_create" or op_type == "create_chart":
                chart_type = params.get("chart_type", "chart")
                return f"Create {chart_type}"
            
            else:
                return f"Execute {op_type.replace('_', ' ')}"
                
        except Exception:
            return f"Execute operation: {op_type}"
    
    def _create_formatting_summary(self, formatting_state: Dict[str, Any]) -> str:
        """Create summary of formatting changes for user feedback."""
        try:
            changes = []
            
            if isinstance(formatting_state, dict):
                # Check character formatting
                char_fmt = formatting_state.get("character_formatting", {})
                if isinstance(char_fmt, dict):
                    if char_fmt.get("bold"):
                        changes.append("bold")
                    if char_fmt.get("italic"):
                        changes.append("italic")
                    if char_fmt.get("underline"):
                        changes.append("underline")
                    if char_fmt.get("color"):
                        changes.append("color")
                
                # Check paragraph formatting
                para_fmt = formatting_state.get("paragraph_formatting", {})
                if isinstance(para_fmt, dict):
                    if para_fmt.get("alignment"):
                        changes.append("alignment")
                    if para_fmt.get("line_spacing"):
                        changes.append("spacing")
                
                # Check font changes
                if formatting_state.get("font_family"):
                    changes.append("font")
                if formatting_state.get("font_size"):
                    changes.append("size")
            
            if changes:
                return f"Formatting applied: {', '.join(changes[:3])}"
            return ""
            
        except Exception:
            return "Formatting changes applied"
    
    async def _send_progress_update(self, request_id: str, stage: str, 
                                  percentage: float, message: str):
        """Send progress update via integration layer."""
        try:
            progress = ProgressUpdate(
                request_id=request_id,
                operation_stage=stage,
                progress_percentage=percentage,
                status_message=message
            )
            
            if self.integration_layer:
                await self.integration_layer.send_progress_update(progress)
            
            self.logger.debug(f"Progress {request_id}: {stage} - {percentage:.1f}% - {message}")
            
        except Exception as e:
            self.logger.warning(f"Failed to send progress update: {e}")
    
    def _update_performance_metrics(self, execution_time_ms: float, success: bool):
        """Update performance monitoring metrics."""
        try:
            self.performance_metrics["total_operations"] += 1
            self.performance_metrics["total_processing_time_ms"] += execution_time_ms
            
            if success:
                self.performance_metrics["successful_operations"] += 1
            else:
                self.performance_metrics["failed_operations"] += 1
            
            # Calculate average response time
            total_ops = self.performance_metrics["total_operations"]
            total_time = self.performance_metrics["total_processing_time_ms"]
            self.performance_metrics["average_response_time_ms"] = total_time / total_ops if total_ops > 0 else 0
            
        except Exception as e:
            self.logger.warning(f"Failed to update performance metrics: {e}")
    
    async def cancel_operation(self, request_id: str) -> bool:
        """
        Cancel an active operation.
        
        Args:
            request_id: ID of operation to cancel
            
        Returns:
            True if cancellation successful
        """
        try:
            if request_id in self.active_operations:
                del self.active_operations[request_id]
                self.progress_callbacks.pop(request_id, None)
                self.logger.info(f"Operation {request_id} cancelled")
                return True
            else:
                self.logger.warning(f"Operation {request_id} not found for cancellation")
                return False
                
        except Exception as e:
            self.logger.error(f"Failed to cancel operation {request_id}: {e}")
            return False


# Global bridge instance
_bridge_instance: Optional[LangGraphBridge] = None
_bridge_lock = threading.Lock()


def get_bridge(config: Optional[BridgeConfiguration] = None) -> LangGraphBridge:
    """
    Get or create global bridge instance.
    
    Args:
        config: Optional configuration for new instance
        
    Returns:
        LangGraphBridge instance
    """
    global _bridge_instance
    
    with _bridge_lock:
        if _bridge_instance is None:
            _bridge_instance = LangGraphBridge(config)
        return _bridge_instance


async def initialize_bridge(config: Optional[BridgeConfiguration] = None) -> bool:
    """
    Initialize the global bridge instance.
    
    Args:
        config: Optional bridge configuration
        
    Returns:
        True if initialization successful
    """
    # Create default config if none provided
    if config is None:
        # Always use HTTP API integration method (pure API mode)
        integration_method = IntegrationMethod.HTTP_API
        
        config = BridgeConfiguration(
            integration_method=integration_method,
            log_level=os.environ.get("AGENT_LOG_LEVEL", "INFO")
        )
    
    bridge = get_bridge(config)
    return await bridge.initialize()


async def shutdown_bridge() -> bool:
    """
    Shutdown the global bridge instance.
    
    Returns:
        True if shutdown successful
    """
    global _bridge_instance
    
    with _bridge_lock:
        if _bridge_instance:
            result = await _bridge_instance.shutdown()
            _bridge_instance = None
            return result
        return True


# Entry point for C++ integration
def process_request_from_cpp(request_id: str, user_message: str, 
                           document_context_json: str) -> str:
    """
    Entry point for processing requests from C++ AgentCoordinator.
    
    This function provides a synchronous interface for C++ integration
    while internally using async processing.
    
    Args:
        request_id: Unique request identifier
        user_message: User's natural language request
        document_context_json: JSON-serialized document context
        
    Returns:
        JSON-serialized response
    """
    try:
        # Parse document context
        document_context = json.loads(document_context_json)
        
        # Get bridge instance
        bridge = get_bridge()
        
        # Create new event loop for this thread if needed
        try:
            loop = asyncio.get_event_loop()
        except RuntimeError:
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
        
        # Process request asynchronously
        response = loop.run_until_complete(
            bridge.process_cpp_request(request_id, user_message, document_context)
        )
        
        return response
        
    except Exception as e:
        error_response = OperationResponse(
            request_id=request_id,
            success=False,
            error_message=f"Request processing failed: {e}"
        )
        return json.dumps(asdict(error_response))


if __name__ == "__main__":
    # Test bridge initialization
    async def test_bridge():
        config = BridgeConfiguration(
            integration_method=IntegrationMethod.HTTP_API,
            log_level="DEBUG"
        )
        
        success = await initialize_bridge(config)
        if success:
            bridge = get_bridge()
            status = bridge.get_status()
            print(f"Bridge Status: {json.dumps(status, indent=2)}")
            
            # Test request processing
            test_response = await bridge.process_cpp_request(
                "test-001", 
                "Create a simple table",
                {"document_type": "text", "cursor_position": {"line": 1, "column": 0}}
            )
            print(f"Test Response: {test_response}")
            
            await shutdown_bridge()
        else:
            print("Bridge initialization failed")
    
    asyncio.run(test_bridge())