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


# Integration layer imports (dynamically loaded)
try:
    import uno
    from com.sun.star.beans import PropertyValue
    from com.sun.star.lang import IllegalArgumentException
    PYUNO_AVAILABLE = True
except ImportError:
    PYUNO_AVAILABLE = False
    # Fallback to ctypes if needed
    import ctypes


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
    integration_method: IntegrationMethod = IntegrationMethod.PYUNO
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
    def extract_uno_context(uno_context: Any) -> Dict[str, Any]:
        """
        Extract document context from UNO structures.
        
        Args:
            uno_context: UNO Any containing document context
            
        Returns:
            Python dictionary with extracted context
        """
        try:
            context = {}
            
            # Extract basic document information
            if hasattr(uno_context, 'getValueTypeName'):
                context['type'] = uno_context.getValueTypeName()
            
            # Try to extract as PropertyValue sequence
            if hasattr(uno_context, 'value'):
                if isinstance(uno_context.value, (list, tuple)):
                    for prop in uno_context.value:
                        if hasattr(prop, 'Name') and hasattr(prop, 'Value'):
                            context[prop.Name] = CppDocumentContext._convert_uno_value(prop.Value)
            
            return context
            
        except Exception as e:
            logging.warning(f"Failed to extract UNO context: {e}")
            return {"error": f"Context extraction failed: {e}"}
    
    @staticmethod
    def _convert_uno_value(value: Any) -> Any:
        """Convert UNO values to Python types."""
        try:
            if hasattr(value, 'value'):
                return CppDocumentContext._convert_uno_value(value.value)
            elif isinstance(value, (str, int, float, bool)):
                return value
            elif hasattr(value, '__iter__') and not isinstance(value, str):
                return [CppDocumentContext._convert_uno_value(item) for item in value]
            else:
                return str(value)
        except Exception:
            return str(value)
    
    @staticmethod
    def create_uno_response(response_data: Dict[str, Any]) -> Any:
        """
        Create UNO-compatible response structure.
        
        Args:
            response_data: Python dictionary with response data
            
        Returns:
            UNO-compatible structure
        """
        if not PYUNO_AVAILABLE:
            return json.dumps(response_data)
        
        try:
            # Create PropertyValue sequence
            properties = []
            for key, value in response_data.items():
                prop = PropertyValue()
                prop.Name = key
                prop.Value = CppDocumentContext._convert_python_to_uno(value)
                properties.append(prop)
            
            return uno.Any("[]com.sun.star.beans.PropertyValue", tuple(properties))
            
        except Exception as e:
            logging.warning(f"Failed to create UNO response: {e}")
            return json.dumps(response_data)
    
    @staticmethod
    def _convert_python_to_uno(value: Any) -> Any:
        """Convert Python values to UNO types."""
        if isinstance(value, dict):
            return json.dumps(value)
        elif isinstance(value, (list, tuple)):
            return [CppDocumentContext._convert_python_to_uno(item) for item in value]
        else:
            return value


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


class PyUnoIntegrationLayer(BaseIntegrationLayer):
    """PyUNO-based integration layer for direct UNO service communication."""
    
    def __init__(self, config: BridgeConfiguration):
        super().__init__(config)
        self.uno_context = None
        self.service_manager = None
        self.component_context = None
    
    async def initialize(self) -> bool:
        """Initialize PyUNO connection."""
        if not PYUNO_AVAILABLE:
            self.logger.error("PyUNO not available")
            return False
        
        try:
            # Initialize UNO context
            import uno
            from com.sun.star.connection import NoConnectException
            
            # Connect to LibreOffice
            local_context = uno.getComponentContext()
            resolver = local_context.ServiceManager.createInstanceWithContext(
                "com.sun.star.bridge.UnoUrlResolver", local_context)
            
            # Attempt connection
            socket_path = self.config.libreoffice_socket_path or "socket,host=localhost,port=2002"
            try:
                self.component_context = resolver.resolve(f"uno:{socket_path};urp;StarOffice.ComponentContext")
                self.service_manager = self.component_context.ServiceManager
                self.logger.info("PyUNO integration initialized successfully")
                return True
            except NoConnectException:
                self.logger.warning("LibreOffice not running, using local context")
                self.component_context = local_context
                self.service_manager = local_context.ServiceManager
                return True
                
        except Exception as e:
            self.logger.error(f"PyUNO initialization failed: {e}")
            return False
    
    async def shutdown(self) -> bool:
        """Shutdown PyUNO connection."""
        try:
            # Cleanup connections
            self.uno_context = None
            self.service_manager = None
            self.component_context = None
            self.logger.info("PyUNO integration shutdown complete")
            return True
        except Exception as e:
            self.logger.error(f"PyUNO shutdown failed: {e}")
            return False
    
    async def send_progress_update(self, update: ProgressUpdate) -> bool:
        """Send progress update via UNO services."""
        try:
            # In a real implementation, this would call specific UNO services
            # to update the LibreOffice UI with progress information
            self.logger.debug(f"Progress update: {update.operation_stage} - {update.progress_percentage:.1f}%")
            return True
        except Exception as e:
            self.logger.error(f"Failed to send progress update: {e}")
            return False
    
    async def send_response(self, response: OperationResponse) -> bool:
        """Send final response via UNO services."""
        try:
            # Convert response to UNO format
            uno_response = CppDocumentContext.create_uno_response(asdict(response))
            self.logger.info(f"Sent response for request {response.request_id}")
            return True
        except Exception as e:
            self.logger.error(f"Failed to send response: {e}")
            return False


class CtypesIntegrationLayer(BaseIntegrationLayer):
    """Ctypes-based integration layer for C++ library communication."""
    
    def __init__(self, config: BridgeConfiguration):
        super().__init__(config)
        self.lib_handle = None
        self.callback_functions = {}
    
    async def initialize(self) -> bool:
        """Initialize ctypes integration."""
        try:
            # In a real implementation, this would load the LibreOffice C++ library
            self.logger.info("Ctypes integration initialized (mock)")
            return True
        except Exception as e:
            self.logger.error(f"Ctypes initialization failed: {e}")
            return False
    
    async def shutdown(self) -> bool:
        """Shutdown ctypes integration."""
        try:
            if self.lib_handle:
                # Cleanup library handle
                self.lib_handle = None
            self.logger.info("Ctypes integration shutdown complete")
            return True
        except Exception as e:
            self.logger.error(f"Ctypes shutdown failed: {e}")
            return False
    
    async def send_progress_update(self, update: ProgressUpdate) -> bool:
        """Send progress update via ctypes calls."""
        try:
            # Mock implementation - would call C++ functions via ctypes
            self.logger.debug(f"Ctypes progress: {update.operation_stage} - {update.progress_percentage:.1f}%")
            return True
        except Exception as e:
            self.logger.error(f"Failed to send ctypes progress update: {e}")
            return False
    
    async def send_response(self, response: OperationResponse) -> bool:
        """Send final response via ctypes calls."""
        try:
            # Mock implementation - would call C++ response functions
            self.logger.info(f"Ctypes sent response for request {response.request_id}")
            return True
        except Exception as e:
            self.logger.error(f"Failed to send ctypes response: {e}")
            return False


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
            if self.config.integration_method == IntegrationMethod.PYUNO:
                self.integration_layer = PyUnoIntegrationLayer(self.config)
            elif self.config.integration_method == IntegrationMethod.CTYPES:
                self.integration_layer = CtypesIntegrationLayer(self.config)
            else:
                raise ValueError(f"Unsupported integration method: {self.config.integration_method}")
            
            return await self.integration_layer.initialize()
            
        except Exception as e:
            self.logger.error(f"Integration layer initialization failed: {e}")
            return False
    
    async def _initialize_agent_graph(self) -> bool:
        """Initialize the LangGraph agent system."""
        try:
            # Create agent instances
            agents = {}
            
            if self.config.enable_all_agents:
                agents["document_master"] = DocumentMasterAgent("document_master_agent")
                agents["context_analysis"] = ContextAnalysisAgent("context_analysis_agent")
                agents["content_generation"] = ContentGenerationAgent("content_generation_agent")
                agents["formatting"] = FormattingAgent("formatting_agent")
                agents["data_integration"] = DataIntegrationAgent("data_integration_agent")
            
            # Build the LangGraph workflow
            workflow = StateGraph(DocumentState)
            
            # Add agent nodes
            for agent_name, agent in agents.items():
                workflow.add_node(agent_name, self._create_agent_node(agent))
            
            # Define workflow edges (simplified for bridge initialization)
            workflow.set_entry_point("document_master")
            workflow.add_edge("document_master", "context_analysis")
            workflow.add_edge("context_analysis", "content_generation")
            workflow.add_edge("content_generation", "formatting")
            workflow.add_edge("formatting", "data_integration")
            workflow.set_finish_point("data_integration")
            
            # Compile the graph
            self.agent_graph = workflow.compile()
            
            self.logger.info(f"Agent graph initialized with {len(agents)} agents")
            return True
            
        except Exception as e:
            self.logger.error(f"Agent graph initialization failed: {e}")
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
            
            # Process through agent graph
            if self.agent_graph:
                # Send progress update - starting processing
                await self._send_progress_update(request_id, "initializing", 0.0, 
                                               "Starting agent processing")
                
                final_state = None
                progress_count = 0
                total_agents = 5  # Approximate number of processing stages
                
                # Stream through LangGraph with progress updates
                async for state_update in self._stream_agent_processing(document_state):
                    progress_count += 1
                    progress_percentage = min((progress_count / total_agents) * 100, 95.0)
                    
                    await self._send_progress_update(
                        request_id, 
                        "processing", 
                        progress_percentage,
                        f"Processing through agent {progress_count}/{total_agents}"
                    )
                    
                    final_state = state_update
                
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
                # Fallback response if agent graph not available
                response = OperationResponse(
                    request_id=request_id,
                    success=False,
                    error_message="Agent graph not initialized"
                )
                self._update_performance_metrics(0, False)
            
            # Store result and cleanup
            self.operation_results[request_id] = response
            self.active_operations.pop(request_id, None)
            
            # Send response via integration layer
            if self.integration_layer:
                await self.integration_layer.send_response(response)
            
            return json.dumps(asdict(response))
            
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
            
            return json.dumps(asdict(error_response))
    
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
            elif PYUNO_AVAILABLE and hasattr(cpp_context, 'getValueTypeName'):
                # UNO Any object
                context_dict = CppDocumentContext.extract_uno_context(cpp_context)
            else:
                # Unknown type, try to convert to string
                self.logger.warning(f"Unknown context type: {type(cpp_context)}")
                context_dict = {"raw_context": str(cpp_context)}
            
            # Map C++ context fields to DocumentState
            await self._map_document_context(context_dict, document_state)
            await self._map_cursor_context(context_dict, document_state)
            await self._map_document_structure(context_dict, document_state)
            await self._map_formatting_state(context_dict, document_state)
            await self._map_user_preferences(context_dict, document_state)
            
            self.logger.debug(f"Successfully converted C++ context to DocumentState")
            return document_state
            
        except Exception as e:
            self.logger.error(f"Failed to convert C++ context: {e}")
            # Return minimal valid DocumentState
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
        """Map document structure information to DocumentState."""
        try:
            document_structure = {}
            
            # Extract basic document structure
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
            
            # Extract detailed structure if available
            if "document_outline" in context_dict:
                document_structure["outline"] = context_dict["document_outline"]
            if "table_of_contents" in context_dict:
                document_structure["toc"] = context_dict["table_of_contents"]
            if "headings" in context_dict:
                document_structure["headings"] = context_dict["headings"]
            
            # Extract content organization
            if "styles_used" in context_dict:
                document_structure["styles"] = context_dict["styles_used"]
            if "references" in context_dict:
                document_structure["references"] = context_dict["references"]
            if "footnotes" in context_dict:
                document_structure["footnotes"] = context_dict["footnotes"]
            
            document_state["document_structure"] = document_structure
            
        except Exception as e:
            self.logger.warning(f"Failed to map document structure: {e}")
    
    async def _map_formatting_state(self, context_dict: Dict[str, Any], 
                                  document_state: DocumentState):
        """Map formatting state information to DocumentState."""
        try:
            formatting_state = {}
            
            # Extract current formatting
            if "current_font" in context_dict:
                formatting_state["font"] = context_dict["current_font"]
            if "current_font_size" in context_dict:
                formatting_state["font_size"] = context_dict["current_font_size"]
            if "current_style" in context_dict:
                formatting_state["style"] = context_dict["current_style"]
            
            # Extract text formatting
            if "is_bold" in context_dict:
                formatting_state["bold"] = context_dict["is_bold"]
            if "is_italic" in context_dict:
                formatting_state["italic"] = context_dict["is_italic"]
            if "is_underline" in context_dict:
                formatting_state["underline"] = context_dict["is_underline"]
            
            # Extract paragraph formatting
            if "paragraph_alignment" in context_dict:
                formatting_state["alignment"] = context_dict["paragraph_alignment"]
            if "line_spacing" in context_dict:
                formatting_state["line_spacing"] = context_dict["line_spacing"]
            if "indent_level" in context_dict:
                formatting_state["indent"] = context_dict["indent_level"]
            
            # Extract page formatting
            if "page_margins" in context_dict:
                formatting_state["margins"] = context_dict["page_margins"]
            if "page_orientation" in context_dict:
                formatting_state["orientation"] = context_dict["page_orientation"]
            if "page_size" in context_dict:
                formatting_state["page_size"] = context_dict["page_size"]
            
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
        Stream agent processing with progress updates.
        
        Args:
            document_state: Initial document state
            
        Yields:
            Updated document states from agent processing
        """
        try:
            if not self.agent_graph:
                yield document_state
                return
            
            # Process through agent graph
            current_state = document_state
            
            # Stream through LangGraph
            async for state_update in self.agent_graph.astream(current_state):
                if state_update:
                    current_state = state_update
                    yield current_state
            
        except Exception as e:
            self.logger.error(f"Agent processing stream failed: {e}")
            yield document_state
    
    async def _convert_agent_state_to_libreoffice_format(self, final_state: DocumentState, 
                                                       request_id: str) -> Dict[str, Any]:
        """
        Convert final agent state to LibreOffice-compatible format.
        
        This method will be fully implemented in subtask 12.5.
        Currently returns a basic conversion.
        """
        try:
            libreoffice_response = {
                "request_id": request_id,
                "status": "success",
                "operations": [],
                "content_changes": {},
                "formatting_changes": {},
                "metadata": {}
            }
            
            # Extract operations from final state
            if final_state and "pending_operations" in final_state:
                libreoffice_response["operations"] = final_state["pending_operations"]
            
            # Extract generated content
            if final_state and "generated_content" in final_state:
                libreoffice_response["content_changes"] = {
                    "generated_content": final_state["generated_content"]
                }
            
            # Extract formatting changes
            if final_state and "formatting_state" in final_state:
                libreoffice_response["formatting_changes"] = final_state["formatting_state"]
            
            # Add metadata
            libreoffice_response["metadata"] = {
                "processing_time": time.time(),
                "agent_count": len(final_state.get("agent_status", {})),
                "final_state_size": len(str(final_state))
            }
            
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
                "metadata": {}
            }
    
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
            integration_method=IntegrationMethod.PYUNO,
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