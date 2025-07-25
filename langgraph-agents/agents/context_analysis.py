"""
ContextAnalysisAgent - Document Context Analysis and Semantic Understanding

This agent specializes in analyzing document structure, cursor position tracking,
and semantic content understanding for the LibreOffice AI Writing Assistant.
It provides lightweight analysis for simple operations and comprehensive analysis
for complex operations with performance optimization and caching.

Key Capabilities:
- Document structure analysis (paragraphs, sections, tables, charts)
- Cursor position tracking and context extraction
- Semantic content understanding and relationships
- LibreOffice UNO service integration
- Analysis result caching for performance optimization
"""

import asyncio
import time
import logging
from datetime import datetime, timezone
from typing import Any, Dict, List, Optional, Union, Tuple
from dataclasses import dataclass, field
from enum import Enum
import json
import hashlib
import weakref
from collections import OrderedDict
from threading import Lock

# Import base agent classes
from .base import (
    BaseAgent, 
    AgentCapability, 
    AgentResult, 
    AgentError,
    ValidationResult,
    PerformanceMetrics
)

# Import LLM client for AI-powered analysis
from llm_client import get_llm_client, AnalysisMode as LLMAnalysisMode

# Import state management
try:
    from state.document_state import DocumentState, DocumentStateManager
    from langchain_core.messages import BaseMessage, HumanMessage, AIMessage
except ImportError:
    # Fallback for development
    DocumentState = Dict[str, Any]
    DocumentStateManager = Any
    BaseMessage = Dict[str, Any]
    HumanMessage = Dict[str, Any]
    AIMessage = Dict[str, Any]


class AnalysisMode(Enum):
    """Analysis mode classification for performance optimization."""
    LIGHTWEIGHT = "lightweight"      # Simple operations: cursor position, basic context
    FOCUSED = "focused"              # Moderate operations: section context, content structure  
    COMPREHENSIVE = "comprehensive"  # Complex operations: full semantic analysis


class ContextType(Enum):
    """Types of document context that can be analyzed."""
    CURSOR_POSITION = "cursor_position"
    PARAGRAPH_CONTEXT = "paragraph_context"
    SECTION_CONTEXT = "section_context"
    DOCUMENT_STRUCTURE = "document_structure"
    CONTENT_SEMANTICS = "content_semantics"
    FORMATTING_CONTEXT = "formatting_context"
    TABLE_CONTEXT = "table_context"
    CHART_CONTEXT = "chart_context"


@dataclass
class AnalysisRequest:
    """Request for document context analysis."""
    request_id: str
    analysis_mode: AnalysisMode
    context_types: List[ContextType]
    target_position: Optional[Dict[str, Any]] = None
    content_focus: Optional[str] = None
    performance_target_ms: int = 2000
    cache_key: Optional[str] = None


@dataclass
class AnalysisResult:
    """Result of document context analysis."""
    request_id: str
    analysis_mode: AnalysisMode
    success: bool
    execution_time_ms: float
    context_data: Dict[str, Any] = field(default_factory=dict)
    semantic_insights: Dict[str, Any] = field(default_factory=dict)
    cursor_context: Dict[str, Any] = field(default_factory=dict)
    document_structure: Dict[str, Any] = field(default_factory=dict)
    error_message: Optional[str] = None
    cache_hit: bool = False
    metadata: Dict[str, Any] = field(default_factory=dict)


@dataclass
class CacheEntry:
    """Cache entry for analysis results."""
    cache_key: str
    result: AnalysisResult
    timestamp: datetime
    access_count: int = 0
    ttl_seconds: int = 300  # 5 minutes default


class AnalysisCache:
    """
    Intelligent caching system for analysis results with cache invalidation
    strategies and performance monitoring.
    
    Features:
    - LRU-based cache eviction with configurable size limits
    - TTL-based expiration for temporal consistency
    - Document change detection for cache invalidation
    - Performance metrics collection for optimization insights
    - Thread-safe operations for concurrent access
    """
    
    def __init__(self, max_size: int = 1000, default_ttl: int = 300):
        """
        Initialize the analysis cache.
        
        Args:
            max_size: Maximum number of entries to cache
            default_ttl: Default time-to-live in seconds
        """
        self.max_size = max_size
        self.default_ttl = default_ttl
        
        # Thread-safe cache storage
        self._cache: OrderedDict[str, CacheEntry] = OrderedDict()
        self._lock = Lock()
        
        # Performance metrics
        self._hits = 0
        self._misses = 0
        self._evictions = 0
        self._invalidations = 0
        
        # Document tracking for invalidation
        self._document_versions: Dict[str, str] = {}
        
    def cache_analysis_result(self, 
                            cache_key: str, 
                            result: AnalysisResult, 
                            ttl_seconds: Optional[int] = None) -> None:
        """
        Cache an analysis result with automatic eviction management.
        
        Args:
            cache_key: Unique key for the cached result
            result: Analysis result to cache
            ttl_seconds: Optional custom TTL, uses default if None
        """
        with self._lock:
            # Remove existing entry if present
            if cache_key in self._cache:
                del self._cache[cache_key]
            
            # Create cache entry
            entry = CacheEntry(
                cache_key=cache_key,
                result=result,
                timestamp=datetime.now(timezone.utc),
                ttl_seconds=ttl_seconds or self.default_ttl
            )
            
            # Add to cache (most recent)
            self._cache[cache_key] = entry
            
            # Evict oldest entries if over size limit
            while len(self._cache) > self.max_size:
                oldest_key = next(iter(self._cache))
                del self._cache[oldest_key]
                self._evictions += 1
    
    def get_cached_analysis(self, cache_key: str) -> Optional[AnalysisResult]:
        """
        Retrieve cached analysis result if valid.
        
        Args:
            cache_key: Unique key for the cached result
            
        Returns:
            Cached analysis result or None if not found/expired
        """
        with self._lock:
            entry = self._cache.get(cache_key)
            
            if entry is None:
                self._misses += 1
                return None
            
            # Check TTL expiration
            age_seconds = (datetime.now(timezone.utc) - entry.timestamp).total_seconds()
            if age_seconds > entry.ttl_seconds:
                del self._cache[cache_key]
                self._misses += 1
                return None
            
            # Update access pattern (move to end for LRU)
            self._cache.move_to_end(cache_key)
            entry.access_count += 1
            
            # Mark as cache hit
            result = entry.result
            result.cache_hit = True
            self._hits += 1
            
            return result
    
    def invalidate_cache(self, 
                        pattern: Optional[str] = None, 
                        document_path: Optional[str] = None) -> int:
        """
        Invalidate cached entries based on pattern or document changes.
        
        Args:
            pattern: Optional pattern to match cache keys
            document_path: Optional document path for document-specific invalidation
            
        Returns:
            Number of entries invalidated
        """
        invalidated = 0
        
        with self._lock:
            keys_to_remove = []
            
            for cache_key in self._cache:
                should_invalidate = False
                
                # Pattern-based invalidation
                if pattern and pattern in cache_key:
                    should_invalidate = True
                
                # Document-based invalidation
                if document_path and cache_key.startswith(f"doc:{document_path}"):
                    should_invalidate = True
                
                if should_invalidate:
                    keys_to_remove.append(cache_key)
            
            # Remove invalidated entries
            for key in keys_to_remove:
                del self._cache[key]
                invalidated += 1
                self._invalidations += 1
        
        return invalidated
    
    def update_document_version(self, document_path: str, version_hash: str) -> bool:
        """
        Update document version and invalidate cache if changed.
        
        Args:
            document_path: Path to the document
            version_hash: New version hash of the document
            
        Returns:
            True if document changed and cache was invalidated
        """
        with self._lock:
            old_version = self._document_versions.get(document_path)
            
            if old_version != version_hash:
                # Document changed, invalidate related cache entries
                self._document_versions[document_path] = version_hash
                invalidated = self.invalidate_cache(document_path=document_path)
                return invalidated > 0
            
            return False
    
    def get_performance_metrics(self) -> Dict[str, Any]:
        """
        Get cache performance metrics for optimization insights.
        
        Returns:
            Dictionary containing performance metrics
        """
        with self._lock:
            total_requests = self._hits + self._misses
            hit_rate = (self._hits / total_requests) if total_requests > 0 else 0.0
            
            return {
                "cache_size": len(self._cache),
                "max_size": self.max_size,
                "hit_rate": hit_rate,
                "hits": self._hits,
                "misses": self._misses,
                "evictions": self._evictions,
                "invalidations": self._invalidations,
                "total_requests": total_requests
            }
    
    def clear_cache(self) -> int:
        """
        Clear all cached entries.
        
        Returns:
            Number of entries cleared
        """
        with self._lock:
            cleared = len(self._cache)
            self._cache.clear()
            self._document_versions.clear()
            return cleared
    
    def generate_cache_key(self, 
                          document_path: str, 
                          analysis_mode: str, 
                          context_data: Dict[str, Any]) -> str:
        """
        Generate a deterministic cache key for analysis parameters.
        
        Args:
            document_path: Path to the document being analyzed
            analysis_mode: Type of analysis being performed
            context_data: Context information for the analysis
            
        Returns:
            Deterministic cache key string
        """
        # Create deterministic hash of key parameters
        key_data = {
            "document": document_path,
            "mode": analysis_mode,
            "context": context_data
        }
        
        key_string = json.dumps(key_data, sort_keys=True)
        key_hash = hashlib.sha256(key_string.encode()).hexdigest()[:16]
        
        return f"analysis:{analysis_mode}:{key_hash}"


class ContextAnalysisAgent(BaseAgent):
    """
    Specialized agent for document context analysis and semantic understanding.
    
    This agent provides three levels of analysis based on operation complexity:
    - Lightweight: Fast cursor position and basic context analysis
    - Focused: Moderate analysis including section context and content structure
    - Comprehensive: Full semantic analysis with document relationships
    
    Performance targets:
    - Lightweight analysis: <200ms
    - Focused analysis: <1000ms  
    - Comprehensive analysis: <2000ms
    """
    
    def __init__(self, 
                 agent_id: str = "context_analysis_agent",
                 config: Optional[Dict[str, Any]] = None):
        """
        Initialize the ContextAnalysisAgent.
        
        Args:
            agent_id: Unique identifier for this agent instance
            config: Optional configuration dictionary
        """
        # Define agent capabilities
        capabilities = [
            AgentCapability.DOCUMENT_ANALYSIS
        ]
        
        # Default configuration
        default_config = {
            "max_retries": 2,
            "retry_delay": 0.5,
            "cache_enabled": True,
            "cache_max_entries": 100,
            "cache_ttl_seconds": 300,
            "performance_targets": {
                AnalysisMode.LIGHTWEIGHT: 200,      # ms
                AnalysisMode.FOCUSED: 1000,         # ms
                AnalysisMode.COMPREHENSIVE: 2000    # ms
            },
            "uno_connection_timeout": 5.0,
            "enable_semantic_analysis": True,
            "debug_mode": False
        }
        
        # Merge with provided config
        merged_config = {**default_config, **(config or {})}
        
        # Initialize base agent
        super().__init__(
            agent_id=agent_id,
            capabilities=capabilities,
            config=merged_config
        )
        
        # Enhanced Analysis Cache
        self.cache_enabled = self.config.get("cache_enabled", True)
        cache_max_entries = self.config.get("cache_max_entries", 100)
        cache_ttl_seconds = self.config.get("cache_ttl_seconds", 300)
        
        self.analysis_cache = AnalysisCache(
            max_size=cache_max_entries,
            default_ttl=cache_ttl_seconds
        ) if self.cache_enabled else None
        
        # Performance targets
        self.performance_targets = self.config.get("performance_targets", {})
        
        # LibreOffice UNO connection 
        self.uno_bridge = None
        self.document_service = None
        self.swdoc_model = None
        self.uno_connection_timeout = self.config.get("uno_connection_timeout", 5.0)
        self.uno_retry_attempts = self.config.get("uno_retry_attempts", 3)
        
        # Semantic analysis components (placeholder for future implementation)
        self.semantic_analyzer = None
        
        self.logger.info(f"ContextAnalysisAgent {agent_id} initialized with cache_enabled={self.cache_enabled}")
    
    def _initialize_agent(self) -> None:
        """Initialize agent-specific components."""
        super()._initialize_agent()
        
        # Initialize UNO bridge connection (placeholder)
        self._initialize_uno_bridge()
        
        # Initialize semantic analysis components (placeholder)
        self._initialize_semantic_analyzer()
        
        # Initialize LLM client for AI-powered analysis
        self.llm_client = get_llm_client()
        
        self.logger.info("ContextAnalysisAgent initialization completed")
    
    def _initialize_uno_bridge(self) -> None:
        """Initialize LibreOffice UNO bridge connection."""
        try:
            # Connect to LibreOffice UNO bridge
            self.uno_bridge = self._connect_to_uno_bridge()
            
            if self.uno_bridge:
                # Access document service
                self.document_service = self._get_document_service()
                self.logger.info("UNO bridge connection established successfully")
            else:
                self.logger.warning("UNO bridge connection failed - operating in fallback mode")
                
        except Exception as e:
            self.logger.error(f"UNO bridge initialization failed: {e}")
            self.uno_bridge = None
            self.document_service = None
    
    def _initialize_semantic_analyzer(self) -> None:
        """Initialize semantic analysis components (placeholder)."""
        # TODO: Implement semantic analysis in Task 13.3
        self.logger.debug("Semantic analyzer initialization (placeholder)")
        self.semantic_analyzer = None
    
    async def process(self, 
                     state: DocumentState, 
                     message: Optional[BaseMessage] = None) -> AgentResult:
        """
        Process a document context analysis request.
        
        Args:
            state: Current document state
            message: Optional message containing analysis request
            
        Returns:
            AgentResult with analysis results and state updates
        """
        operation_id = f"context_analysis_{int(time.time() * 1000)}"
        start_time = time.time()
        
        try:
            # Extract analysis request from message or state
            analysis_request = self._extract_analysis_request(state, message)
            
            # Determine analysis mode if not specified
            if not analysis_request.analysis_mode:
                analysis_request.analysis_mode = self._determine_analysis_mode(state, message)
            
            # Check for document changes and invalidate cache if needed
            if self.analysis_cache:
                document_path = state.get("current_document", {}).get("path", "")
                if document_path:
                    current_hash = self.calculate_document_hash(state)
                    self.update_document_version(document_path, current_hash)
            
            # Check cache first
            cache_result = await self._check_cache(analysis_request)
            if cache_result:
                self.logger.debug(f"Cache hit for analysis request {analysis_request.request_id}")
                return self._create_agent_result(operation_id, cache_result, start_time)
            
            # Perform analysis based on mode
            analysis_result = await self._perform_analysis(analysis_request, state)
            
            # Cache the result
            if self.cache_enabled and analysis_result.success:
                await self._cache_result(analysis_request, analysis_result)
            
            # Create agent result with state updates
            return self._create_agent_result(operation_id, analysis_result, start_time)
            
        except Exception as e:
            execution_time = time.time() - start_time
            error_msg = f"Context analysis failed: {str(e)}"
            self.logger.error(error_msg, exc_info=True)
            
            return AgentResult(
                agent_id=self.agent_id,
                operation_id=operation_id,
                success=False,
                error=error_msg,
                execution_time=execution_time,
                metadata={"analysis_mode": "unknown", "error_type": type(e).__name__}
            )
    
    def validate_input(self, 
                      state: DocumentState, 
                      message: Optional[BaseMessage] = None) -> ValidationResult:
        """
        Validate input for context analysis.
        
        Args:
            state: Document state to validate
            message: Optional message to validate
            
        Returns:
            ValidationResult indicating if input is valid
        """
        errors = []
        warnings = []
        
        # Validate document state structure
        if not isinstance(state, dict):
            errors.append("Document state must be a dictionary")
        else:
            # Check required fields
            required_fields = [
                "current_document", 
                "cursor_position", 
                "document_structure"
            ]
            
            for field in required_fields:
                if field not in state:
                    errors.append(f"Missing required field: {field}")
            
            # Validate cursor position format
            cursor_pos = state.get("cursor_position", {})
            if not isinstance(cursor_pos, dict):
                errors.append("cursor_position must be a dictionary")
            elif "paragraph" not in cursor_pos or "character" not in cursor_pos:
                warnings.append("cursor_position missing paragraph or character position")
        
        # Validate message format if provided
        if message:
            if not isinstance(message, dict):
                errors.append("Message must be a dictionary")
            elif "content" not in message:
                warnings.append("Message missing content field")
        
        return ValidationResult(
            agent_id=self.agent_id,
            validation_type="context_analysis_input",
            passed=len(errors) == 0,
            issues=[{"type": "error", "message": error} for error in errors] + 
                   [{"type": "warning", "message": warning} for warning in warnings],
            metadata={"validator": "ContextAnalysisAgent", "error_count": len(errors), "warning_count": len(warnings)}
        )
    
    def get_required_tools(self) -> List[str]:
        """
        Get list of required tools for context analysis.
        
        Returns:
            List of required tool names
        """
        return [
            "document_reader",
            "cursor_tracker", 
            "structure_analyzer",
            "semantic_analyzer",
            "uno_bridge"
        ]
    
    def _extract_analysis_request(self, 
                                 state: DocumentState, 
                                 message: Optional[BaseMessage]) -> AnalysisRequest:
        """Extract analysis request from state and message."""
        request_id = f"req_{int(time.time() * 1000)}"
        
        # Default analysis request
        analysis_request = AnalysisRequest(
            request_id=request_id,
            analysis_mode=None,  # Will be determined later
            context_types=[ContextType.CURSOR_POSITION, ContextType.PARAGRAPH_CONTEXT]
        )
        
        # Extract from message if provided
        if message and isinstance(message, dict):
            content = message.get("content", "")
            
            # Determine context types based on message content
            if "comprehensive" in content.lower():
                analysis_request.context_types = [
                    ContextType.DOCUMENT_STRUCTURE,
                    ContextType.CONTENT_SEMANTICS,
                    ContextType.CURSOR_POSITION,
                    ContextType.PARAGRAPH_CONTEXT,
                    ContextType.SECTION_CONTEXT
                ]
            elif "structure" in content.lower():
                analysis_request.context_types = [
                    ContextType.DOCUMENT_STRUCTURE,
                    ContextType.SECTION_CONTEXT,
                    ContextType.CURSOR_POSITION
                ]
            elif "format" in content.lower():
                analysis_request.context_types = [
                    ContextType.FORMATTING_CONTEXT,
                    ContextType.CURSOR_POSITION
                ]
        
        # Generate cache key
        analysis_request.cache_key = self._generate_cache_key(analysis_request, state)
        
        return analysis_request
    
    def _determine_analysis_mode(self, 
                                state: DocumentState, 
                                message: Optional[BaseMessage]) -> AnalysisMode:
        """Determine appropriate analysis mode based on request complexity."""
        if message and isinstance(message, dict):
            content = message.get("content", "").lower()
            
            # Comprehensive analysis keywords
            comprehensive_keywords = [
                "comprehensive", "complete", "full", "detailed", 
                "semantic", "relationship", "analysis"
            ]
            
            # Focused analysis keywords
            focused_keywords = [
                "structure", "section", "format", "style", 
                "context", "summary"
            ]
            
            if any(keyword in content for keyword in comprehensive_keywords):
                return AnalysisMode.COMPREHENSIVE
            elif any(keyword in content for keyword in focused_keywords):
                return AnalysisMode.FOCUSED
        
        # Default to lightweight for simple operations
        return AnalysisMode.LIGHTWEIGHT
    
    def _generate_cache_key(self, 
                           request: AnalysisRequest, 
                           state: DocumentState) -> str:
        """Generate cache key for analysis request."""
        if not self.analysis_cache:
            # Fallback cache key generation
            cache_data = {
                "document_path": state.get("current_document", {}).get("path", ""),
                "cursor_position": state.get("cursor_position", {}),
                "analysis_mode": request.analysis_mode.value if request.analysis_mode else "auto",
                "context_types": [ct.value for ct in request.context_types]
            }
            cache_string = json.dumps(cache_data, sort_keys=True)
            return hashlib.md5(cache_string.encode()).hexdigest()
        
        # Use enhanced cache key generation
        document_path = state.get("current_document", {}).get("path", "")
        analysis_mode = request.analysis_mode.value if request.analysis_mode else "auto"
        context_data = {
            "cursor_position": state.get("cursor_position", {}),
            "context_types": [ct.value for ct in request.context_types],
            "content_focus": request.content_focus
        }
        
        return self.analysis_cache.generate_cache_key(
            document_path=document_path,
            analysis_mode=analysis_mode,
            context_data=context_data
        )
    
    async def _check_cache(self, request: AnalysisRequest) -> Optional[AnalysisResult]:
        """Check if analysis result is cached."""
        if not self.cache_enabled or not self.analysis_cache or not request.cache_key:
            return None
        
        # Use the enhanced cache system
        cached_result = self.analysis_cache.get_cached_analysis(request.cache_key)
        
        if cached_result:
            self.logger.debug(f"Cache hit for analysis request {request.request_id}")
            return cached_result
        
        return None
    
    async def _cache_result(self, request: AnalysisRequest, result: AnalysisResult) -> None:
        """Cache analysis result."""
        if not self.cache_enabled or not self.analysis_cache or not request.cache_key:
            return
        
        # Use the enhanced cache system
        self.analysis_cache.cache_analysis_result(
            cache_key=request.cache_key,
            result=result
        )
        
        self.logger.debug(f"Cached analysis result with key {request.cache_key}")
    
    async def _perform_analysis(self, 
                               request: AnalysisRequest, 
                               state: DocumentState) -> AnalysisResult:
        """Perform document context analysis based on request mode."""
        start_time = time.time()
        
        # Create result container
        result = AnalysisResult(
            request_id=request.request_id,
            analysis_mode=request.analysis_mode,
            success=True,
            execution_time_ms=0
        )
        
        try:
            # Perform analysis based on mode
            if request.analysis_mode == AnalysisMode.LIGHTWEIGHT:
                await self._perform_lightweight_analysis(request, state, result)
            elif request.analysis_mode == AnalysisMode.FOCUSED:
                await self._perform_focused_analysis(request, state, result)
            elif request.analysis_mode == AnalysisMode.COMPREHENSIVE:
                await self._perform_comprehensive_analysis(request, state, result)
            
            result.execution_time_ms = (time.time() - start_time) * 1000
            
            # Check performance target
            target_ms = self.performance_targets.get(request.analysis_mode, 2000)
            if result.execution_time_ms > target_ms:
                self.logger.warning(
                    f"Analysis exceeded target: {result.execution_time_ms:.1f}ms > {target_ms}ms"
                )
            
            return result
            
        except Exception as e:
            result.success = False
            result.error_message = str(e)
            result.execution_time_ms = (time.time() - start_time) * 1000
            
            self.logger.error(f"Analysis failed: {e}", exc_info=True)
            return result
    
    async def _perform_lightweight_analysis(self, 
                                           request: AnalysisRequest, 
                                           state: DocumentState, 
                                           result: AnalysisResult) -> None:
        """Perform lightweight analysis for simple operations using LLM."""
        try:
            # Get user message for context
            user_message = None
            if state.get("messages"):
                user_message = state["messages"][-1].get("content", "")
            
            # Use LLM for intelligent lightweight analysis
            llm_response = await self.llm_client.analyze_document_context(
                document_state=state,
                analysis_mode=LLMAnalysisMode.LIGHTWEIGHT,
                user_message=user_message
            )
            
            if llm_response.success:
                # Parse LLM response (expecting JSON)
                import json
                try:
                    analysis_data = json.loads(llm_response.content)
                    result.cursor_context = analysis_data.get("cursor_context", {})
                    result.context_data = analysis_data.get("navigation_info", {})
                    result.metadata = {
                        "llm_provider": llm_response.provider.value,
                        "llm_model": llm_response.model,
                        "tokens_used": llm_response.tokens_used,
                        "analysis_mode": "lightweight"
                    }
                except json.JSONDecodeError:
                    # Fallback to using raw content
                    result.context_data = {"analysis": llm_response.content}
                    result.cursor_context = {"raw_analysis": True}
            else:
                # Fallback to basic analysis if LLM fails
                self.logger.warning(f"LLM analysis failed: {llm_response.error}, using fallback")
                cursor_context = await self.analyze_cursor_position(state)
                result.cursor_context = cursor_context
                basic_context = await self.get_basic_context(state)
                result.context_data = basic_context
                
        except Exception as e:
            self.logger.error(f"Lightweight analysis failed: {e}")
            # Fallback to basic analysis
            cursor_context = await self.analyze_cursor_position(state)
            result.cursor_context = cursor_context
            basic_context = await self.get_basic_context(state)
            result.context_data = basic_context
        
        # Minimal semantic understanding
        simple_semantics = await self.extract_simple_semantics(state)
        result.semantic_insights = simple_semantics
        
        # Efficient document structure overview
        structure_overview = await self.get_structure_overview(state)
        result.document_structure = structure_overview
        
        self.logger.debug("Completed lightweight analysis with enhanced methods")
    
    async def _perform_focused_analysis(self, 
                                       request: AnalysisRequest, 
                                       state: DocumentState, 
                                       result: AnalysisResult) -> None:
        """Perform focused analysis for moderate operations using LLM."""
        try:
            # Get user message for context
            user_message = None
            if state.get("messages"):
                user_message = state["messages"][-1].get("content", "")
            
            # Use LLM for intelligent focused analysis
            llm_response = await self.llm_client.analyze_document_context(
                document_state=state,
                analysis_mode=LLMAnalysisMode.FOCUSED,
                user_message=user_message
            )
            
            if llm_response.success:
                # Parse LLM response (expecting JSON)
                import json
                try:
                    analysis_data = json.loads(llm_response.content)
                    result.document_structure = analysis_data.get("document_structure", {})
                    result.context_data = analysis_data.get("content_relationships", {})
                    result.cursor_context = analysis_data.get("formatting_context", {})
                    result.semantic_insights = analysis_data.get("suggestions", {})
                    result.metadata = {
                        "llm_provider": llm_response.provider.value,
                        "llm_model": llm_response.model,
                        "tokens_used": llm_response.tokens_used,
                        "analysis_mode": "focused"
                    }
                except json.JSONDecodeError:
                    # Fallback to using raw content
                    result.context_data = {"analysis": llm_response.content}
                    result.document_structure = {"raw_analysis": True}
            else:
                # Fallback to basic analysis if LLM fails
                self.logger.warning(f"LLM focused analysis failed: {llm_response.error}, using fallback")
                await self._perform_focused_analysis_fallback(request, state, result)
                
        except Exception as e:
            self.logger.error(f"Focused analysis failed: {e}")
            # Fallback to basic analysis
            await self._perform_focused_analysis_fallback(request, state, result)
    
    async def _perform_focused_analysis_fallback(self, 
                                               request: AnalysisRequest, 
                                               state: DocumentState, 
                                               result: AnalysisResult) -> None:
        """Fallback focused analysis without LLM."""
        # Include lightweight analysis
        await self._perform_lightweight_analysis(request, state, result)
        
        # Enhanced context analysis
        doc_structure = state.get("document_structure", {})
        result.document_structure.update({
            "sections": doc_structure.get("sections", []),
            "tables": doc_structure.get("tables", []),
            "charts": doc_structure.get("charts", []),
            "analysis_level": "focused"
        })
        
        # Section context
        cursor_paragraph = result.cursor_context.get("paragraph", 0)
        sections = doc_structure.get("sections", [])
        current_section = None
        
        for section in sections:
            if isinstance(section, dict):
                start_para = section.get("start_paragraph", 0)
                end_para = section.get("end_paragraph", float('inf'))
                if start_para <= cursor_paragraph <= end_para:
                    current_section = section
                    break
        
        result.context_data.update({
            "current_section": current_section,
            "section_context": "available" if current_section else "none",
            "analysis_scope": "section_and_structure"
        })
        
        # Formatting context
        formatting_state = state.get("formatting_state", {})
        result.context_data["formatting_context"] = {
            "current_style": formatting_state.get("current_style", "Normal"),
            "font": formatting_state.get("font", "Unknown"),
            "size": formatting_state.get("size", 12)
        }
        
        self.logger.debug("Completed focused analysis")
    
    async def _perform_comprehensive_analysis(self, 
                                            request: AnalysisRequest, 
                                            state: DocumentState, 
                                            result: AnalysisResult) -> None:
        """Perform comprehensive analysis for complex operations using LLM."""
        try:
            # Get user message for context
            user_message = None
            if state.get("messages"):
                user_message = state["messages"][-1].get("content", "")
            
            # Use LLM for intelligent comprehensive analysis
            llm_response = await self.llm_client.analyze_document_context(
                document_state=state,
                analysis_mode=LLMAnalysisMode.COMPREHENSIVE,
                user_message=user_message
            )
            
            if llm_response.success:
                # Parse LLM response (expecting JSON)
                import json
                try:
                    analysis_data = json.loads(llm_response.content)
                    result.semantic_insights = analysis_data.get("semantic_analysis", {})
                    result.context_data = analysis_data.get("content_themes", {})
                    result.document_structure = analysis_data.get("structural_analysis", {})
                    
                    # Add LLM-specific insights
                    result.metadata = {
                        "llm_provider": llm_response.provider.value,
                        "llm_model": llm_response.model,
                        "tokens_used": llm_response.tokens_used,
                        "analysis_mode": "comprehensive",
                        "recommendations": analysis_data.get("recommendations", []),
                        "optimization_suggestions": analysis_data.get("optimization_suggestions", [])
                    }
                except json.JSONDecodeError:
                    # Fallback to using raw content
                    result.semantic_insights = {"analysis": llm_response.content}
                    result.document_structure = {"raw_analysis": True}
            else:
                # Fallback to basic analysis if LLM fails
                self.logger.warning(f"LLM comprehensive analysis failed: {llm_response.error}, using fallback")
                await self._perform_comprehensive_analysis_fallback(request, state, result)
                
        except Exception as e:
            self.logger.error(f"Comprehensive analysis failed: {e}")
            # Fallback to basic analysis
            await self._perform_comprehensive_analysis_fallback(request, state, result)
    
    async def _perform_comprehensive_analysis_fallback(self, 
                                                     request: AnalysisRequest, 
                                                     state: DocumentState, 
                                                     result: AnalysisResult) -> None:
        """Fallback comprehensive analysis without LLM."""
        # Include focused analysis
        await self._perform_focused_analysis(request, state, result)
        
        # Deep document structure analysis
        comprehensive_structure = await self.analyze_document_structure(state)
        result.document_structure.update(comprehensive_structure)
        
        # Advanced semantic content analysis
        semantic_content = await self.extract_semantic_content(state)
        result.semantic_insights.update(semantic_content)
        
        # Document relationship mapping
        relationships = await self.map_document_relationships(state)
        result.semantic_insights["document_relationships"] = relationships
        
        # Enhanced content analysis
        generated_content = state.get("generated_content", [])
        content_analysis = state.get("content_analysis", {})
        
        result.context_data.update({
            "content_analysis": content_analysis,
            "generated_content_count": len(generated_content),
            "document_complexity": comprehensive_structure.get("complexity_assessment", "high"),
            "analysis_scope": "comprehensive_semantic"
        })
        
        # Performance metadata
        result.metadata.update({
            "semantic_analysis_enabled": self.config.get("enable_semantic_analysis", True),
            "uno_bridge_available": self.uno_bridge is not None,
            "cache_enabled": self.cache_enabled,
            "comprehensive_features": {
                "structure_analysis": True,
                "semantic_extraction": True,
                "relationship_mapping": True
            }
        })
        
        self.logger.debug("Completed comprehensive analysis with enhanced methods")
    
    def _create_agent_result(self, 
                            operation_id: str, 
                            analysis_result: AnalysisResult, 
                            start_time: float) -> AgentResult:
        """Create AgentResult from AnalysisResult."""
        execution_time = time.time() - start_time
        
        # Create state updates
        state_updates = {
            "content_analysis": {
                "context_data": analysis_result.context_data,
                "cursor_context": analysis_result.cursor_context,
                "document_structure": analysis_result.document_structure,
                "semantic_insights": analysis_result.semantic_insights,
                "analysis_mode": analysis_result.analysis_mode.value,
                "execution_time_ms": analysis_result.execution_time_ms,
                "timestamp": datetime.now(timezone.utc).isoformat()
            },
            "agent_status": {
                "context_analysis": "completed" if analysis_result.success else "failed"
            }
        }
        
        # Add performance metrics
        performance_metrics = PerformanceMetrics(
            agent_id=self.agent_id,
            operation_count=1,
            total_execution_time=execution_time,
            average_execution_time=execution_time,
            success_rate=1.0 if analysis_result.success else 0.0
        )
        
        return AgentResult(
            agent_id=self.agent_id,
            operation_id=operation_id,
            success=analysis_result.success,
            result=analysis_result.context_data,
            error=analysis_result.error_message,
            execution_time=execution_time,
            state_updates=state_updates,
            performance_metrics=performance_metrics,
            metadata={
                "analysis_mode": analysis_result.analysis_mode.value,
                "cache_hit": analysis_result.cache_hit,
                "context_types": [ct.value for ct in request.context_types] if 'request' in locals() else [],
                "performance_target_met": analysis_result.execution_time_ms <= self.performance_targets.get(analysis_result.analysis_mode, 2000)
            }
        )
    
    def get_cache_stats(self) -> Dict[str, Any]:
        """Get cache statistics."""
        if not self.cache_enabled:
            return {"cache_enabled": False}
        
        total_access_count = sum(entry.access_count for entry in self.analysis_cache.values())
        
        return {
            "cache_enabled": True,
            "cache_entries": len(self.analysis_cache),
            "max_entries": self.cache_max_entries,
            "total_access_count": total_access_count,
            "cache_hit_rate": total_access_count / max(1, len(self.analysis_cache)),
            "ttl_seconds": self.cache_ttl_seconds
        }
    
    def clear_cache(self) -> None:
        """Clear the analysis cache."""
        self.analysis_cache.clear()
        self.logger.info("Analysis cache cleared")
    
    async def shutdown(self) -> None:
        """Shutdown the agent and clean up resources."""
        self.logger.info(f"Shutting down ContextAnalysisAgent {self.agent_id}")
        
        # Clear cache
        self.clear_cache()
        
        # Close UNO bridge connection
        if self.uno_bridge:
            # TODO: Implement UNO bridge cleanup in Task 13.4
            pass
        
        # Call parent shutdown
        if hasattr(super(), 'shutdown'):
            await super().shutdown()
        
        self.logger.info(f"ContextAnalysisAgent {self.agent_id} shutdown completed")
    
    # === TASK 13.2: Lightweight Analysis Methods ===
    
    async def analyze_cursor_position(self, state: DocumentState) -> Dict[str, Any]:
        """
        Analyze cursor position and immediate context for lightweight operations.
        Performance target: <50ms
        """
        cursor_pos = state.get("cursor_position", {})
        doc_structure = state.get("document_structure", {})
        
        # Basic cursor information
        paragraph_num = cursor_pos.get("paragraph", 0)
        character_pos = cursor_pos.get("character", 0)
        line_num = cursor_pos.get("line", 0)
        
        # Calculate relative position in document
        total_paragraphs = doc_structure.get("paragraphs", 1)
        relative_position = paragraph_num / max(total_paragraphs, 1)
        
        # Determine document section based on cursor
        current_section = None
        sections = doc_structure.get("sections", [])
        for section in sections:
            if isinstance(section, dict):
                start_para = section.get("start_paragraph", 0)
                end_para = section.get("end_paragraph", float('inf'))
                if start_para <= paragraph_num <= end_para:
                    current_section = section.get("title", "Unknown Section")
                    break
        
        # Check if cursor is near special elements
        tables = doc_structure.get("tables", [])
        charts = doc_structure.get("charts", [])
        
        near_table = any(
            abs(table.get("paragraph", 0) - paragraph_num) <= 2 
            for table in tables if isinstance(table, dict)
        )
        
        near_chart = any(
            abs(chart.get("paragraph", 0) - paragraph_num) <= 2 
            for chart in charts if isinstance(chart, dict)
        )
        
        return {
            "paragraph": paragraph_num,
            "character": character_pos,
            "line": line_num,
            "relative_position": relative_position,
            "document_section": current_section,
            "context_elements": {
                "near_table": near_table,
                "near_chart": near_chart,
                "section_boundary": paragraph_num in [s.get("start_paragraph", -1) for s in sections] or
                                  paragraph_num in [s.get("end_paragraph", -1) for s in sections]
            },
            "navigation_context": {
                "is_document_start": paragraph_num == 0,
                "is_document_end": paragraph_num >= total_paragraphs - 1,
                "paragraphs_remaining": max(0, total_paragraphs - paragraph_num - 1)
            },
            "analysis_type": "lightweight_cursor"
        }
    
    async def get_basic_context(self, state: DocumentState) -> Dict[str, Any]:
        """
        Extract basic document context for simple operations.
        Performance target: <100ms
        """
        current_doc = state.get("current_document", {})
        doc_structure = state.get("document_structure", {})
        formatting_state = state.get("formatting_state", {})
        messages = state.get("messages", [])
        
        # Document basic info
        doc_info = {
            "title": current_doc.get("title", "Unknown"),
            "path": current_doc.get("path", ""),
            "type": self._infer_document_type(current_doc.get("path", "")),
            "estimated_size": "small" if doc_structure.get("paragraphs", 0) < 10 else
                             "medium" if doc_structure.get("paragraphs", 0) < 50 else "large"
        }
        
        # Current formatting context
        format_context = {
            "current_style": formatting_state.get("current_style", "Normal"),
            "font": formatting_state.get("font", "Arial"),
            "size": formatting_state.get("size", 12),
            "formatting_available": len(formatting_state) > 0
        }
        
        # Recent user intent from messages
        user_intent = "unknown"
        if messages:
            last_message = messages[-1]
            if isinstance(last_message, dict) and last_message.get("role") == "user":
                content = last_message.get("content", "").lower()
                if any(word in content for word in ["format", "style", "bold", "italic"]):
                    user_intent = "formatting"
                elif any(word in content for word in ["chart", "table", "data"]):
                    user_intent = "data_visualization"
                elif any(word in content for word in ["write", "generate", "create"]):
                    user_intent = "content_creation"
                elif any(word in content for word in ["analyze", "review", "check"]):
                    user_intent = "analysis"
        
        # Content complexity estimation
        content_complexity = self._estimate_content_complexity(doc_structure)
        
        return {
            "document_info": doc_info,
            "formatting_context": format_context,
            "user_intent": user_intent,
            "content_complexity": content_complexity,
            "analysis_scope": "basic_context",
            "context_timestamp": time.time(),
            "quick_stats": {
                "paragraphs": doc_structure.get("paragraphs", 0),
                "sections": len(doc_structure.get("sections", [])),
                "tables": len(doc_structure.get("tables", [])),
                "charts": len(doc_structure.get("charts", []))
            }
        }
    
    async def extract_simple_semantics(self, state: DocumentState) -> Dict[str, Any]:
        """
        Extract minimal semantic understanding for lightweight operations.
        Performance target: <50ms
        """
        messages = state.get("messages", [])
        doc_structure = state.get("document_structure", {})
        current_task = state.get("current_task", "")
        
        # Simple keyword extraction from recent messages
        keywords = set()
        key_phrases = []
        
        if messages:
            for message in messages[-3:]:  # Only analyze last 3 messages for speed
                if isinstance(message, dict):
                    content = message.get("content", "").lower()
                    # Extract simple keywords
                    words = content.split()
                    financial_keywords = {"revenue", "profit", "chart", "data", "analysis", "report", "financial"}
                    formatting_keywords = {"bold", "italic", "format", "style", "font", "color"}
                    content_keywords = {"write", "create", "generate", "insert", "add"}
                    
                    keywords.update(word for word in words if word in financial_keywords.union(formatting_keywords).union(content_keywords))
                    
                    # Extract simple phrases (2-3 words)
                    for i in range(len(words) - 1):
                        phrase = " ".join(words[i:i+2])
                        if any(kw in phrase for kw in ["financial report", "quarterly data", "market analysis", "data chart"]):
                            key_phrases.append(phrase)
        
        # Infer document purpose from structure and keywords
        document_purpose = "general"
        if keywords.intersection({"revenue", "profit", "financial", "quarterly"}):
            document_purpose = "financial_report"
        elif keywords.intersection({"chart", "data", "analysis"}):
            document_purpose = "data_analysis"
        elif doc_structure.get("tables", []) or doc_structure.get("charts", []):
            document_purpose = "data_presentation"
        
        # Simple content themes based on sections
        content_themes = []
        sections = doc_structure.get("sections", [])
        for section in sections:
            if isinstance(section, dict):
                title = section.get("title", "").lower()
                if "introduction" in title or "summary" in title:
                    content_themes.append("overview")
                elif "analysis" in title or "results" in title:
                    content_themes.append("analysis")
                elif "conclusion" in title or "recommendation" in title:
                    content_themes.append("conclusion")
        
        # Current operation semantic context
        operation_context = "neutral"
        if current_task:
            task_lower = current_task.lower()
            if "format" in task_lower:
                operation_context = "formatting_operation"
            elif "data" in task_lower or "chart" in task_lower:
                operation_context = "data_operation"
            elif "write" in task_lower or "generate" in task_lower:
                operation_context = "content_operation"
        
        return {
            "keywords": list(keywords),
            "key_phrases": key_phrases[:5],  # Limit to top 5 for performance
            "document_purpose": document_purpose,
            "content_themes": content_themes,
            "operation_context": operation_context,
            "semantic_confidence": "low",  # Lightweight analysis has low confidence
            "analysis_type": "simple_semantic",
            "processing_time_ms": 0  # Will be updated by caller
        }
    
    async def get_structure_overview(self, state: DocumentState) -> Dict[str, Any]:
        """
        Get efficient document structure overview for lightweight operations.
        Performance target: <30ms
        """
        doc_structure = state.get("document_structure", {})
        cursor_pos = state.get("cursor_position", {})
        
        # Basic structure counts
        paragraph_count = doc_structure.get("paragraphs", 0)
        section_count = len(doc_structure.get("sections", []))
        table_count = len(doc_structure.get("tables", []))
        chart_count = len(doc_structure.get("charts", []))
        
        # Structure complexity assessment
        complexity_score = 0
        if section_count > 5: complexity_score += 2
        if table_count > 3: complexity_score += 2
        if chart_count > 3: complexity_score += 2
        if paragraph_count > 100: complexity_score += 1
        
        complexity_level = "simple" if complexity_score <= 2 else "moderate" if complexity_score <= 4 else "complex"
        
        # Current position context in structure
        current_paragraph = cursor_pos.get("paragraph", 0)
        structure_position = {
            "current_section_index": 0,
            "current_section_title": "Unknown",
            "progress_percentage": (current_paragraph / max(paragraph_count, 1)) * 100
        }
        
        # Find current section
        sections = doc_structure.get("sections", [])
        for i, section in enumerate(sections):
            if isinstance(section, dict):
                start_para = section.get("start_paragraph", 0)
                end_para = section.get("end_paragraph", float('inf'))
                if start_para <= current_paragraph <= end_para:
                    structure_position["current_section_index"] = i
                    structure_position["current_section_title"] = section.get("title", f"Section {i+1}")
                    break
        
        # Navigation hints for lightweight operations
        navigation_hints = []
        if current_paragraph < paragraph_count * 0.1:
            navigation_hints.append("near_document_start")
        elif current_paragraph > paragraph_count * 0.9:
            navigation_hints.append("near_document_end")
        
        if table_count > 0:
            tables = doc_structure.get("tables", [])
            nearest_table = min(tables, key=lambda t: abs(t.get("paragraph", 0) - current_paragraph), default=None)
            if nearest_table and abs(nearest_table.get("paragraph", 0) - current_paragraph) <= 5:
                navigation_hints.append("near_table")
        
        return {
            "total_paragraphs": paragraph_count,
            "current_paragraph": current_paragraph,
            "structure_counts": {
                "sections": section_count,
                "tables": table_count,
                "charts": chart_count
            },
            "complexity_level": complexity_level,
            "complexity_score": complexity_score,
            "structure_position": structure_position,
            "navigation_hints": navigation_hints,
            "analysis_level": "lightweight_structure"
        }
    
    def _infer_document_type(self, path: str) -> str:
        """Infer document type from file path."""
        if not path:
            return "unknown"
        
        path_lower = path.lower()
        if path_lower.endswith(('.odt', '.docx', '.doc')):
            return "text_document"
        elif path_lower.endswith(('.ods', '.xlsx', '.xls')):
            return "spreadsheet"
        elif path_lower.endswith(('.odp', '.pptx', '.ppt')):
            return "presentation"
        else:
            return "unknown"
    
    def _estimate_content_complexity(self, doc_structure: Dict[str, Any]) -> str:
        """Estimate content complexity based on structure."""
        paragraphs = doc_structure.get("paragraphs", 0)
        sections = len(doc_structure.get("sections", []))
        tables = len(doc_structure.get("tables", []))
        charts = len(doc_structure.get("charts", []))
        
        total_elements = paragraphs + sections + tables + charts
        
        if total_elements < 20:
            return "simple"
        elif total_elements < 100:
            return "moderate"
        else:
            return "complex"
    
    # === TASK 13.3: Comprehensive Document Structure Analysis ===
    
    async def analyze_document_structure(self, state: DocumentState) -> Dict[str, Any]:
        """
        Analyze complete document structure with deep hierarchy parsing.
        Performance target: <1000ms
        """
        doc_structure = state.get("document_structure", {})
        cursor_pos = state.get("cursor_position", {})
        formatting_state = state.get("formatting_state", {})
        
        # Deep structure analysis
        structure_analysis = {
            "hierarchy_depth": 0,
            "section_hierarchy": [],
            "content_distribution": {},
            "structural_patterns": [],
            "document_flow": {},
            "complexity_assessment": "moderate"
        }
        
        # Analyze section hierarchy
        sections = doc_structure.get("sections", [])
        if sections:
            structure_analysis["section_hierarchy"] = await self._analyze_section_hierarchy(sections)
            structure_analysis["hierarchy_depth"] = len(structure_analysis["section_hierarchy"])
        
        # Content distribution analysis
        paragraphs = doc_structure.get("paragraphs", 0)
        tables = doc_structure.get("tables", [])
        charts = doc_structure.get("charts", [])
        
        structure_analysis["content_distribution"] = {
            "text_paragraphs": paragraphs - len(tables) - len(charts),
            "data_tables": len(tables),
            "visualizations": len(charts),
            "text_to_data_ratio": (paragraphs - len(tables) - len(charts)) / max(len(tables) + len(charts), 1),
            "content_density": self._calculate_content_density(paragraphs, sections)
        }
        
        # Identify structural patterns
        structure_analysis["structural_patterns"] = await self._identify_structural_patterns(doc_structure)
        
        # Document flow analysis
        structure_analysis["document_flow"] = await self._analyze_document_flow(doc_structure, cursor_pos)
        
        # Complexity assessment
        complexity_factors = {
            "section_count": len(sections),
            "table_count": len(tables),
            "chart_count": len(charts),
            "paragraph_count": paragraphs,
            "hierarchy_depth": structure_analysis["hierarchy_depth"]
        }
        
        structure_analysis["complexity_assessment"] = self._assess_structural_complexity(complexity_factors)
        
        # Content hierarchy (placeholder for UNO bridge integration)
        structure_analysis["content_hierarchy"] = {
            "document_root": {
                "type": "document",
                "children": self._build_content_hierarchy_mock(doc_structure)
            },
            "uno_integration": "placeholder"  # TODO: Task 13.4
        }
        
        # Cross-references detection
        structure_analysis["cross_references"] = await self._detect_cross_references(state)
        
        # Document metadata extraction
        structure_analysis["document_metadata"] = {
            "estimated_word_count": paragraphs * 50,  # Rough estimate
            "document_type": self._classify_document_type(doc_structure),
            "structure_integrity": self._assess_structure_integrity(doc_structure),
            "navigation_complexity": self._assess_navigation_complexity(sections, tables, charts)
        }
        
        structure_analysis["analysis_level"] = "comprehensive"
        return structure_analysis
    
    async def extract_semantic_content(self, state: DocumentState) -> Dict[str, Any]:
        """
        Extract deep semantic understanding from document content.
        Performance target: <800ms
        """
        messages = state.get("messages", [])
        doc_structure = state.get("document_structure", {})
        current_task = state.get("current_task", "")
        generated_content = state.get("generated_content", [])
        
        semantic_analysis = {
            "content_themes": [],
            "key_concepts": [],
            "semantic_structure": {},
            "content_intent": {},
            "knowledge_domains": [],
            "semantic_confidence": "medium"
        }
        
        # Advanced content themes analysis
        semantic_analysis["content_themes"] = await self._extract_content_themes(messages, doc_structure, generated_content)
        
        # Key concepts extraction with confidence scoring
        semantic_analysis["key_concepts"] = await self._extract_key_concepts(messages, doc_structure)
        
        # Semantic structure mapping
        semantic_analysis["semantic_structure"] = await self._map_semantic_structure(doc_structure)
        
        # Content intent analysis
        semantic_analysis["content_intent"] = await self._analyze_content_intent(messages, current_task)
        
        # Knowledge domain identification
        semantic_analysis["knowledge_domains"] = await self._identify_knowledge_domains(messages, doc_structure)
        
        # Semantic relationships
        semantic_analysis["semantic_relationships"] = await self._map_semantic_relationships(doc_structure, messages)
        
        # Content quality assessment
        semantic_analysis["content_quality"] = {
            "coherence_score": self._assess_content_coherence(doc_structure),
            "completeness_score": self._assess_content_completeness(doc_structure),
            "relevance_score": self._assess_content_relevance(messages, doc_structure)
        }
        
        semantic_analysis["analysis_level"] = "comprehensive"
        return semantic_analysis
    
    async def map_document_relationships(self, state: DocumentState) -> Dict[str, Any]:
        """
        Map relationships between document elements and external references.
        Performance target: <500ms
        """
        doc_structure = state.get("document_structure", {})
        external_data = state.get("external_data", {})
        research_citations = state.get("research_citations", [])
        
        relationships = {
            "internal_references": [],
            "external_data_links": {},
            "citation_network": {},
            "content_dependencies": {},
            "reference_integrity": {}
        }
        
        # Internal document references
        relationships["internal_references"] = await self._map_internal_references(doc_structure)
        
        # External data relationships
        if external_data:
            relationships["external_data_links"] = await self._map_external_data_links(doc_structure, external_data)
        
        # Citation network analysis
        if research_citations:
            relationships["citation_network"] = await self._analyze_citation_network(research_citations)
        
        # Content dependencies
        relationships["content_dependencies"] = await self._map_content_dependencies(doc_structure)
        
        # Reference integrity check
        relationships["reference_integrity"] = await self._check_reference_integrity(doc_structure, external_data)
        
        # Document coherence mapping
        relationships["coherence_map"] = {
            "narrative_flow": self._assess_narrative_flow(doc_structure),
            "logical_structure": self._assess_logical_structure(doc_structure),
            "information_hierarchy": self._assess_information_hierarchy(doc_structure)
        }
        
        return relationships
    
    # Helper methods for comprehensive analysis
    
    async def _analyze_section_hierarchy(self, sections: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
        """Analyze hierarchical structure of document sections."""
        hierarchy = []
        
        for i, section in enumerate(sections):
            if isinstance(section, dict):
                section_info = {
                    "index": i,
                    "title": section.get("title", f"Section {i+1}"),
                    "start_paragraph": section.get("start_paragraph", 0),
                    "end_paragraph": section.get("end_paragraph", 0),
                    "length": section.get("end_paragraph", 0) - section.get("start_paragraph", 0),
                    "level": self._infer_section_level(section.get("title", "")),
                    "content_type": self._classify_section_content(section)
                }
                hierarchy.append(section_info)
        
        return hierarchy
    
    def _calculate_content_density(self, paragraphs: int, sections: List[Dict[str, Any]]) -> float:
        """Calculate content density (paragraphs per section)."""
        if not sections:
            return paragraphs
        return paragraphs / len(sections)
    
    async def _identify_structural_patterns(self, doc_structure: Dict[str, Any]) -> List[str]:
        """Identify common structural patterns in the document."""
        patterns = []
        
        sections = doc_structure.get("sections", [])
        tables = doc_structure.get("tables", [])
        charts = doc_structure.get("charts", [])
        
        # Pattern: Introduction-Body-Conclusion
        if len(sections) >= 3:
            first_title = sections[0].get("title", "").lower() if sections else ""
            last_title = sections[-1].get("title", "").lower() if sections else ""
            
            if any(word in first_title for word in ["intro", "summary", "overview"]):
                patterns.append("introduction_pattern")
            
            if any(word in last_title for word in ["conclusion", "summary", "final"]):
                patterns.append("conclusion_pattern")
        
        # Pattern: Data-heavy document
        if len(tables) + len(charts) > len(sections) * 0.5:
            patterns.append("data_heavy")
        
        # Pattern: Narrative document
        if len(tables) + len(charts) < 2 and len(sections) > 5:
            patterns.append("narrative_heavy")
        
        # Pattern: Mixed content
        if tables and charts and len(sections) > 3:
            patterns.append("mixed_content")
        
        return patterns
    
    async def _analyze_document_flow(self, doc_structure: Dict[str, Any], cursor_pos: Dict[str, Any]) -> Dict[str, Any]:
        """Analyze the logical flow of the document."""
        sections = doc_structure.get("sections", [])
        current_para = cursor_pos.get("paragraph", 0)
        
        flow_analysis = {
            "current_position": "unknown",
            "logical_progression": [],
            "content_transitions": [],
            "flow_integrity": "good"
        }
        
        # Determine current position in document flow
        for i, section in enumerate(sections):
            if isinstance(section, dict):
                start = section.get("start_paragraph", 0)
                end = section.get("end_paragraph", 0)
                if start <= current_para <= end:
                    flow_analysis["current_position"] = f"section_{i+1}"
                    break
        
        # Analyze logical progression
        for i, section in enumerate(sections):
            if isinstance(section, dict):
                section_type = self._classify_section_content(section)
                flow_analysis["logical_progression"].append({
                    "section_index": i,
                    "content_type": section_type,
                    "title": section.get("title", f"Section {i+1}")
                })
        
        return flow_analysis
    
    def _assess_structural_complexity(self, factors: Dict[str, int]) -> str:
        """Assess overall structural complexity based on various factors."""
        complexity_score = 0
        
        # Weight different factors
        complexity_score += factors.get("section_count", 0) * 0.5
        complexity_score += factors.get("table_count", 0) * 1.5
        complexity_score += factors.get("chart_count", 0) * 1.5
        complexity_score += factors.get("paragraph_count", 0) * 0.1
        complexity_score += factors.get("hierarchy_depth", 0) * 2
        
        if complexity_score < 10:
            return "simple"
        elif complexity_score < 30:
            return "moderate"
        else:
            return "complex"
    
    def _build_content_hierarchy_mock(self, doc_structure: Dict[str, Any]) -> List[Dict[str, Any]]:
        """Build mock content hierarchy (placeholder for UNO bridge)."""
        sections = doc_structure.get("sections", [])
        hierarchy = []
        
        for section in sections:
            if isinstance(section, dict):
                hierarchy.append({
                    "type": "section",
                    "title": section.get("title", "Unknown"),
                    "start_paragraph": section.get("start_paragraph", 0),
                    "end_paragraph": section.get("end_paragraph", 0),
                    "children": []  # Would be populated by UNO bridge
                })
        
        return hierarchy
    
    async def _detect_cross_references(self, state: DocumentState) -> List[Dict[str, Any]]:
        """Detect cross-references within the document."""
        # Placeholder implementation - would use UNO bridge for real detection
        references = []
        
        # Mock some common cross-reference patterns
        messages = state.get("messages", [])
        for message in messages:
            if isinstance(message, dict):
                content = message.get("content", "").lower()
                if any(phrase in content for phrase in ["see table", "as shown in", "refer to"]):
                    references.append({
                        "type": "table_reference",
                        "source": "user_message",
                        "confidence": 0.7
                    })
        
        return references
    
    def _classify_document_type(self, doc_structure: Dict[str, Any]) -> str:
        """Classify the overall document type based on structure."""
        sections = doc_structure.get("sections", [])
        tables = doc_structure.get("tables", [])
        charts = doc_structure.get("charts", [])
        
        # Financial report pattern
        if any("financial" in section.get("title", "").lower() for section in sections if isinstance(section, dict)):
            return "financial_report"
        
        # Data analysis pattern
        if len(tables) + len(charts) > 3:
            return "data_analysis"
        
        # Research document pattern
        if len(sections) > 5 and len(tables) + len(charts) < 2:
            return "research_document"
        
        # Mixed business document
        if len(sections) > 2 and (tables or charts):
            return "business_document"
        
        return "general_document"
    
    def _assess_structure_integrity(self, doc_structure: Dict[str, Any]) -> str:
        """Assess the integrity of document structure."""
        sections = doc_structure.get("sections", [])
        
        # Check for overlapping sections
        for i in range(len(sections) - 1):
            if isinstance(sections[i], dict) and isinstance(sections[i+1], dict):
                current_end = sections[i].get("end_paragraph", 0)
                next_start = sections[i+1].get("start_paragraph", 0)
                if current_end > next_start:
                    return "poor"
        
        # Check for significant gaps
        total_paragraphs = doc_structure.get("paragraphs", 0)
        covered_paragraphs = sum(
            section.get("end_paragraph", 0) - section.get("start_paragraph", 0)
            for section in sections if isinstance(section, dict)
        )
        
        coverage_ratio = covered_paragraphs / max(total_paragraphs, 1)
        
        if coverage_ratio > 0.9:
            return "excellent"
        elif coverage_ratio > 0.7:
            return "good"
        elif coverage_ratio > 0.5:
            return "fair"
        else:
            return "poor"
    
    def _assess_navigation_complexity(self, sections: List[Dict[str, Any]], tables: List[Dict[str, Any]], charts: List[Dict[str, Any]]) -> str:
        """Assess how complex it is to navigate the document."""
        total_elements = len(sections) + len(tables) + len(charts)
        
        if total_elements < 5:
            return "simple"
        elif total_elements < 15:
            return "moderate"
        else:
            return "complex"
    
    async def _extract_content_themes(self, messages: List[Dict[str, Any]], doc_structure: Dict[str, Any], generated_content: List[Dict[str, Any]]) -> List[str]:
        """Extract major content themes from the document."""
        themes = set()
        
        # Theme extraction from section titles
        sections = doc_structure.get("sections", [])
        for section in sections:
            if isinstance(section, dict):
                title = section.get("title", "").lower()
                if any(word in title for word in ["financial", "revenue", "profit", "earnings"]):
                    themes.add("financial_performance")
                elif any(word in title for word in ["analysis", "review", "assessment"]):
                    themes.add("analytical_content")
                elif any(word in title for word in ["summary", "overview", "introduction"]):
                    themes.add("overview_content")
                elif any(word in title for word in ["conclusion", "recommendation", "next"]):
                    themes.add("conclusive_content")
        
        # Theme extraction from messages
        for message in messages[-5:]:  # Last 5 messages
            if isinstance(message, dict):
                content = message.get("content", "").lower()
                if any(word in content for word in ["chart", "graph", "visualization"]):
                    themes.add("data_visualization")
                elif any(word in content for word in ["format", "style", "appearance"]):
                    themes.add("formatting_content")
                elif any(word in content for word in ["data", "numbers", "statistics"]):
                    themes.add("quantitative_content")
        
        return list(themes)
    
    async def _extract_key_concepts(self, messages: List[Dict[str, Any]], doc_structure: Dict[str, Any]) -> List[Dict[str, Any]]:
        """Extract key concepts with confidence scoring."""
        concepts = []
        
        # Financial concepts
        financial_terms = {"revenue", "profit", "margin", "growth", "performance", "quarterly", "annual"}
        
        # Technical concepts  
        technical_terms = {"analysis", "data", "chart", "table", "visualization", "report"}
        
        # Extract from messages
        all_text = ""
        for message in messages:
            if isinstance(message, dict):
                all_text += " " + message.get("content", "")
        
        words = all_text.lower().split()
        word_freq = {}
        for word in words:
            word_freq[word] = word_freq.get(word, 0) + 1
        
        # Score concepts based on frequency and domain relevance
        for word, freq in word_freq.items():
            confidence = min(freq * 0.1, 1.0)  # Cap at 1.0
            
            if word in financial_terms:
                concepts.append({
                    "concept": word,
                    "domain": "financial",
                    "confidence": confidence,
                    "frequency": freq
                })
            elif word in technical_terms:
                concepts.append({
                    "concept": word,
                    "domain": "technical",
                    "confidence": confidence,
                    "frequency": freq
                })
        
        # Sort by confidence and return top concepts
        concepts.sort(key=lambda x: x["confidence"], reverse=True)
        return concepts[:10]  # Top 10 concepts
    
    async def _map_semantic_structure(self, doc_structure: Dict[str, Any]) -> Dict[str, Any]:
        """Map the semantic structure of the document."""
        sections = doc_structure.get("sections", [])
        
        semantic_map = {
            "information_flow": [],
            "concept_hierarchy": {},
            "semantic_density": {}
        }
        
        # Map information flow through sections
        for i, section in enumerate(sections):
            if isinstance(section, dict):
                title = section.get("title", "").lower()
                flow_type = "content"
                
                if any(word in title for word in ["intro", "overview", "summary"]):
                    flow_type = "introduction"
                elif any(word in title for word in ["analysis", "results", "findings"]):
                    flow_type = "analysis"
                elif any(word in title for word in ["conclusion", "recommendation"]):
                    flow_type = "conclusion"
                
                semantic_map["information_flow"].append({
                    "section_index": i,
                    "flow_type": flow_type,
                    "title": section.get("title", f"Section {i+1}")
                })
        
        return semantic_map
    
    async def _analyze_content_intent(self, messages: List[Dict[str, Any]], current_task: str) -> Dict[str, Any]:
        """Analyze the intent behind the content."""
        intent_analysis = {
            "primary_intent": "unknown",
            "secondary_intents": [],
            "confidence": 0.0,
            "context_clues": []
        }
        
        # Analyze current task
        if current_task:
            task_lower = current_task.lower()
            if any(word in task_lower for word in ["create", "generate", "write"]):
                intent_analysis["primary_intent"] = "content_creation"
            elif any(word in task_lower for word in ["format", "style", "appearance"]):
                intent_analysis["primary_intent"] = "formatting"
            elif any(word in task_lower for word in ["analyze", "review", "examine"]):
                intent_analysis["primary_intent"] = "analysis"
        
        # Analyze recent messages for additional context
        recent_messages = messages[-3:] if messages else []
        for message in recent_messages:
            if isinstance(message, dict):
                content = message.get("content", "").lower()
                if "chart" in content or "graph" in content:
                    intent_analysis["secondary_intents"].append("data_visualization")
                if "professional" in content or "business" in content:
                    intent_analysis["secondary_intents"].append("professional_presentation")
        
        return intent_analysis
    
    async def _identify_knowledge_domains(self, messages: List[Dict[str, Any]], doc_structure: Dict[str, Any]) -> List[str]:
        """Identify knowledge domains present in the document."""
        domains = set()
        
        # Domain keywords
        domain_keywords = {
            "finance": {"revenue", "profit", "financial", "earnings", "quarterly", "budget"},
            "business": {"strategy", "management", "operations", "performance", "growth"},
            "technology": {"software", "system", "technical", "implementation", "development"},
            "research": {"analysis", "study", "research", "methodology", "findings"},
            "marketing": {"market", "customer", "sales", "campaign", "brand"}
        }
        
        # Collect all text
        all_text = ""
        for message in messages:
            if isinstance(message, dict):
                all_text += " " + message.get("content", "")
        
        # Add section titles
        sections = doc_structure.get("sections", [])
        for section in sections:
            if isinstance(section, dict):
                all_text += " " + section.get("title", "")
        
        text_lower = all_text.lower()
        
        # Check for domain keywords
        for domain, keywords in domain_keywords.items():
            if any(keyword in text_lower for keyword in keywords):
                domains.add(domain)
        
        return list(domains)
    
    async def _map_semantic_relationships(self, doc_structure: Dict[str, Any], messages: List[Dict[str, Any]]) -> Dict[str, Any]:
        """Map semantic relationships between document elements."""
        relationships = {
            "section_relationships": [],
            "concept_connections": {},
            "thematic_links": []
        }
        
        sections = doc_structure.get("sections", [])
        
        # Analyze relationships between sections
        for i in range(len(sections) - 1):
            if isinstance(sections[i], dict) and isinstance(sections[i+1], dict):
                current_title = sections[i].get("title", "").lower()
                next_title = sections[i+1].get("title", "").lower()
                
                relationship_type = "sequential"
                if any(word in current_title for word in ["intro", "overview"]) and "analysis" in next_title:
                    relationship_type = "introduction_to_content"
                elif "analysis" in current_title and any(word in next_title for word in ["conclusion", "summary"]):
                    relationship_type = "content_to_conclusion"
                
                relationships["section_relationships"].append({
                    "from_section": i,
                    "to_section": i + 1,
                    "relationship_type": relationship_type
                })
        
        return relationships
    
    def _assess_content_coherence(self, doc_structure: Dict[str, Any]) -> float:
        """Assess how coherent the content structure is."""
        sections = doc_structure.get("sections", [])
        
        if not sections:
            return 0.5
        
        coherence_score = 1.0
        
        # Check for logical section ordering
        section_types = []
        for section in sections:
            if isinstance(section, dict):
                title = section.get("title", "").lower()
                if any(word in title for word in ["intro", "overview", "summary"]):
                    section_types.append("intro")
                elif any(word in title for word in ["conclusion", "final", "summary"]):
                    section_types.append("conclusion")
                else:
                    section_types.append("content")
        
        # Penalize if conclusion comes before content
        if "conclusion" in section_types and "content" in section_types:
            conclusion_index = section_types.index("conclusion")
            last_content_index = len(section_types) - 1 - section_types[::-1].index("content")
            if conclusion_index < last_content_index:
                coherence_score -= 0.3
        
        return max(0.0, min(1.0, coherence_score))
    
    def _assess_content_completeness(self, doc_structure: Dict[str, Any]) -> float:
        """Assess how complete the document structure appears."""
        sections = doc_structure.get("sections", [])
        tables = doc_structure.get("tables", [])
        charts = doc_structure.get("charts", [])
        
        completeness_score = 0.0
        
        # Basic structure completeness
        if sections:
            completeness_score += 0.4
        if tables or charts:
            completeness_score += 0.3
        
        # Check for introduction and conclusion
        section_titles = [section.get("title", "").lower() for section in sections if isinstance(section, dict)]
        
        has_intro = any(word in title for title in section_titles for word in ["intro", "overview", "summary"])
        has_conclusion = any(word in title for title in section_titles for word in ["conclusion", "final", "summary"])
        
        if has_intro:
            completeness_score += 0.15
        if has_conclusion:
            completeness_score += 0.15
        
        return min(1.0, completeness_score)
    
    def _assess_content_relevance(self, messages: List[Dict[str, Any]], doc_structure: Dict[str, Any]) -> float:
        """Assess how relevant the document content is to user intent."""
        if not messages:
            return 0.5
        
        # Get user intent keywords from recent messages
        user_keywords = set()
        for message in messages[-3:]:
            if isinstance(message, dict) and message.get("role") == "user":
                content = message.get("content", "").lower()
                words = content.split()
                user_keywords.update(words)
        
        if not user_keywords:
            return 0.5
        
        # Check how many user keywords appear in section titles
        section_titles = [section.get("title", "").lower() for section in doc_structure.get("sections", []) if isinstance(section, dict)]
        title_text = " ".join(section_titles)
        
        matching_keywords = sum(1 for keyword in user_keywords if keyword in title_text)
        relevance_score = matching_keywords / len(user_keywords)
        
        return min(1.0, relevance_score)
    
    async def _map_internal_references(self, doc_structure: Dict[str, Any]) -> List[Dict[str, Any]]:
        """Map internal references within the document."""
        # Placeholder implementation - would use UNO bridge for real analysis
        references = []
        
        tables = doc_structure.get("tables", [])
        charts = doc_structure.get("charts", [])
        
        # Mock some table references
        for i, table in enumerate(tables):
            if isinstance(table, dict):
                references.append({
                    "type": "table_reference",
                    "id": table.get("id", f"table_{i}"),
                    "paragraph": table.get("paragraph", 0),
                    "reference_type": "data_table"
                })
        
        # Mock some chart references
        for i, chart in enumerate(charts):
            if isinstance(chart, dict):
                references.append({
                    "type": "chart_reference", 
                    "id": chart.get("id", f"chart_{i}"),
                    "paragraph": chart.get("paragraph", 0),
                    "reference_type": "visualization"
                })
        
        return references
    
    async def _map_external_data_links(self, doc_structure: Dict[str, Any], external_data: Dict[str, Any]) -> Dict[str, Any]:
        """Map relationships to external data sources."""
        data_links = {
            "api_connections": [],
            "data_dependencies": [],
            "update_relationships": []
        }
        
        # Analyze external data connections
        for data_source, data_info in external_data.items():
            if isinstance(data_info, dict):
                data_links["api_connections"].append({
                    "source": data_source,
                    "status": data_info.get("status", "unknown"),
                    "last_updated": data_info.get("timestamp", "unknown")
                })
        
        return data_links
    
    async def _analyze_citation_network(self, citations: List[Dict[str, Any]]) -> Dict[str, Any]:
        """Analyze the network of citations and references."""
        network = {
            "citation_count": len(citations),
            "source_types": {},
            "citation_quality": "unknown"
        }
        
        # Analyze citation types
        for citation in citations:
            if isinstance(citation, dict):
                source_type = citation.get("type", "unknown")
                network["source_types"][source_type] = network["source_types"].get(source_type, 0) + 1
        
        return network
    
    async def _map_content_dependencies(self, doc_structure: Dict[str, Any]) -> Dict[str, Any]:
        """Map dependencies between content sections."""
        dependencies = {
            "section_dependencies": [],
            "data_dependencies": [],
            "reference_chains": []
        }
        
        sections = doc_structure.get("sections", [])
        
        # Simple dependency mapping based on section order and type
        for i, section in enumerate(sections):
            if isinstance(section, dict):
                title = section.get("title", "").lower()
                
                # Analysis sections typically depend on data sections
                if "analysis" in title and i > 0:
                    dependencies["section_dependencies"].append({
                        "dependent_section": i,
                        "dependency_section": i - 1,
                        "dependency_type": "sequential"
                    })
        
        return dependencies
    
    async def _check_reference_integrity(self, doc_structure: Dict[str, Any], external_data: Dict[str, Any]) -> Dict[str, Any]:
        """Check the integrity of references and data links."""
        integrity = {
            "broken_references": [],
            "missing_data": [],
            "integrity_score": 1.0,
            "recommendations": []
        }
        
        # Check if referenced data sources are available
        tables = doc_structure.get("tables", [])
        charts = doc_structure.get("charts", [])
        
        total_elements = len(tables) + len(charts)
        available_data_sources = len(external_data) if external_data else 0
        
        if total_elements > available_data_sources:
            integrity["missing_data"].append("Some visualizations may lack data sources")
            integrity["integrity_score"] -= 0.2
        
        if integrity["integrity_score"] < 1.0:
            integrity["recommendations"].append("Review data source connections")
        
        return integrity
    
    def _assess_narrative_flow(self, doc_structure: Dict[str, Any]) -> float:
        """Assess the narrative flow of the document."""
        sections = doc_structure.get("sections", [])
        
        if not sections:
            return 0.5
        
        # Check for logical narrative progression
        section_order_score = 1.0
        
        intro_found = False
        conclusion_found = False
        
        for i, section in enumerate(sections):
            if isinstance(section, dict):
                title = section.get("title", "").lower()
                
                # Introduction should come early
                if any(word in title for word in ["intro", "overview"]):
                    if i > len(sections) * 0.3:  # If intro comes after 30% of sections
                        section_order_score -= 0.2
                    intro_found = True
                
                # Conclusion should come late
                elif any(word in title for word in ["conclusion", "summary", "final"]):
                    if i < len(sections) * 0.7:  # If conclusion comes before 70% of sections
                        section_order_score -= 0.2
                    conclusion_found = True
        
        # Bonus for having both intro and conclusion
        if intro_found and conclusion_found:
            section_order_score += 0.1
        
        return max(0.0, min(1.0, section_order_score))
    
    def _assess_logical_structure(self, doc_structure: Dict[str, Any]) -> float:
        """Assess the logical structure of the document."""
        sections = doc_structure.get("sections", [])
        tables = doc_structure.get("tables", [])
        charts = doc_structure.get("charts", [])
        
        logical_score = 0.5  # Base score
        
        # Documents with both data and analysis sections score higher
        has_data = len(tables) + len(charts) > 0
        has_analysis = any("analysis" in section.get("title", "").lower() for section in sections if isinstance(section, dict))
        
        if has_data and has_analysis:
            logical_score += 0.3
        
        # Sequential section numbering or clear hierarchy adds to logic
        if len(sections) > 2:
            logical_score += 0.2
        
        return min(1.0, logical_score)
    
    def _assess_information_hierarchy(self, doc_structure: Dict[str, Any]) -> float:
        """Assess the information hierarchy of the document."""
        sections = doc_structure.get("sections", [])
        
        if not sections:
            return 0.3
        
        hierarchy_score = 0.5
        
        # Clear section structure adds to hierarchy
        if len(sections) >= 3:
            hierarchy_score += 0.2
        
        # Mix of content types suggests good information organization
        tables = doc_structure.get("tables", [])
        charts = doc_structure.get("charts", [])
        
        if tables and charts:
            hierarchy_score += 0.2
        elif tables or charts:
            hierarchy_score += 0.1
        
        return min(1.0, hierarchy_score)
    
    def _infer_section_level(self, title: str) -> int:
        """Infer the hierarchical level of a section from its title."""
        title_lower = title.lower()
        
        # Level 1: Major sections
        if any(word in title_lower for word in ["introduction", "conclusion", "summary", "overview"]):
            return 1
        
        # Level 2: Content sections
        elif any(word in title_lower for word in ["analysis", "results", "discussion", "methodology"]):
            return 2
        
        # Level 3: Subsections
        else:
            return 3
    
    def _classify_section_content(self, section: Dict[str, Any]) -> str:
        """Classify the type of content in a section."""
        title = section.get("title", "").lower()
        
        if any(word in title for word in ["intro", "overview", "summary"]):
            return "introductory"
        elif any(word in title for word in ["analysis", "results", "findings"]):
            return "analytical"
        elif any(word in title for word in ["method", "approach", "process"]):
            return "methodological"
        elif any(word in title for word in ["conclusion", "recommendation", "final"]):
            return "conclusive"
        else:
            return "content"
    
    # === TASK 13.4: LibreOffice UNO Services Bridge Integration ===
    
    def _connect_to_uno_bridge(self) -> Optional[Any]:
        """
        Establish connection to LibreOffice UNO bridge.
        Connects to the AgentCoordinator service in LibreOffice.
        """
        try:
            # Try to connect to existing LibreOffice instance
            import uno
            from com.sun.star.connection import NoConnectException
            from com.sun.star.lang import DisposedException
            
            # Get UNO context - connects to LibreOffice instance
            local_context = uno.getComponentContext()
            resolver = local_context.getServiceManager().createInstanceWithContext(
                "com.sun.star.bridge.UnoUrlResolver", local_context
            )
            
            # Connect to LibreOffice with retry logic
            connection_string = "uno:socket,host=localhost,port=2002;urp;StarOffice.ComponentContext"
            
            for attempt in range(self.uno_retry_attempts):
                try:
                    context = resolver.resolve(connection_string)
                    self.logger.info(f"UNO bridge connected successfully (attempt {attempt + 1})")
                    return context
                    
                except NoConnectException as e:
                    if attempt < self.uno_retry_attempts - 1:
                        self.logger.warning(f"UNO connection attempt {attempt + 1} failed, retrying...")
                        time.sleep(1.0 * (attempt + 1))  # Exponential backoff
                    else:
                        self.logger.error(f"Failed to connect to UNO bridge after {self.uno_retry_attempts} attempts: {e}")
                        
        except ImportError:
            # UNO not available - create mock connection for development
            self.logger.warning("UNO module not available, using mock connection")
            return self._create_mock_uno_bridge()
            
        except Exception as e:
            self.logger.error(f"UNO bridge connection error: {e}")
            
        return None
    
    def _create_mock_uno_bridge(self) -> Dict[str, Any]:
        """Create mock UNO bridge for development without LibreOffice."""
        mock_bridge = {
            "connection_type": "mock",
            "connected": True,
            "service_manager": self._create_mock_service_manager(),
            "document_model": self._create_mock_document_model()
        }
        
        self.logger.info("Created mock UNO bridge for development")
        return mock_bridge
    
    def _create_mock_service_manager(self) -> Dict[str, Any]:
        """Create mock service manager for development."""
        return {
            "ai_agent_coordinator": {
                "service_name": "com.sun.star.ai.AgentCoordinator",
                "available": True,
                "methods": ["processRequest", "getDocumentContext", "setConfiguration"]
            },
            "document_model": {
                "service_name": "com.sun.star.text.TextDocument",
                "available": True,
                "methods": ["getText", "getTextCursor", "insertString"]
            }
        }
    
    def _create_mock_document_model(self) -> Dict[str, Any]:
        """Create mock document model for development."""
        return {
            "document_type": "text_document",
            "paragraphs": [],
            "sections": [],
            "tables": [],
            "frames": [],
            "text_nodes": [],
            "style_families": {}
        }
    
    def _get_document_service(self) -> Optional[Any]:
        """Get document service from UNO bridge."""
        if not self.uno_bridge:
            return None
            
        try:
            if isinstance(self.uno_bridge, dict) and self.uno_bridge.get("connection_type") == "mock":
                # Return mock document service
                return self.uno_bridge.get("service_manager", {}).get("document_model")
            
            # Real UNO connection
            service_manager = self.uno_bridge.getServiceManager()
            
            # Try to get AgentCoordinator service first
            try:
                agent_coordinator = service_manager.createInstanceWithContext(
                    "com.sun.star.ai.AgentCoordinator", self.uno_bridge
                )
                self.logger.info("Connected to AgentCoordinator service")
                return agent_coordinator
                
            except Exception as e:
                self.logger.warning(f"AgentCoordinator service not available: {e}")
                
                # Fallback to direct document access
                try:
                    desktop = service_manager.createInstanceWithContext(
                        "com.sun.star.frame.Desktop", self.uno_bridge
                    )
                    self.logger.info("Connected to Desktop service as fallback")
                    return desktop
                    
                except Exception as e2:
                    self.logger.error(f"Failed to connect to Desktop service: {e2}")
                    
        except Exception as e:
            self.logger.error(f"Error getting document service: {e}")
            
        return None
    
    async def connect_to_uno_bridge(self) -> bool:
        """
        Public method to establish UNO bridge connection.
        Returns True if connection successful, False otherwise.
        """
        try:
            self.uno_bridge = self._connect_to_uno_bridge()
            
            if self.uno_bridge:
                self.document_service = self._get_document_service() 
                
                # Test connection
                connection_valid = await self._test_uno_connection()
                
                if connection_valid:
                    self.logger.info("UNO bridge connection established and validated")
                    return True
                else:
                    self.logger.warning("UNO bridge connection validation failed")
                    
        except Exception as e:
            self.logger.error(f"Failed to connect to UNO bridge: {e}")
            
        return False
    
    async def _test_uno_connection(self) -> bool:
        """Test UNO connection validity."""
        if not self.uno_bridge or not self.document_service:
            return False
            
        try:
            if isinstance(self.uno_bridge, dict) and self.uno_bridge.get("connection_type") == "mock":
                # Mock connection is always valid
                return True
                
            # Test real UNO connection by checking service availability
            if hasattr(self.document_service, 'getImplementationName'):
                service_name = self.document_service.getImplementationName()
                self.logger.debug(f"UNO service available: {service_name}")
                return True
                
        except Exception as e:
            self.logger.error(f"UNO connection test failed: {e}")
            
        return False
    
    async def access_swdoc_model(self, state: DocumentState) -> Optional[Dict[str, Any]]:
        """
        Access SwDoc model through UNO bridge for document structure analysis.
        Returns document model information or None if unavailable.
        """
        if not self.document_service:
            self.logger.warning("No UNO document service available")
            return None
            
        try:
            # Mock implementation for development
            if isinstance(self.uno_bridge, dict) and self.uno_bridge.get("connection_type") == "mock":
                return await self._get_mock_swdoc_model(state)
                
            # Real UNO implementation
            return await self._get_real_swdoc_model(state)
            
        except Exception as e:
            self.logger.error(f"Error accessing SwDoc model: {e}")
            return None
    
    async def _get_mock_swdoc_model(self, state: DocumentState) -> Dict[str, Any]:
        """Get mock SwDoc model for development."""
        doc_structure = state.get("document_structure", {})
        
        mock_model = {
            "model_type": "mock_swdoc",
            "document_info": {
                "title": state.get("current_document", {}).get("title", "Unknown"),
                "path": state.get("current_document", {}).get("path", ""),
                "paragraph_count": doc_structure.get("paragraphs", 0),
                "section_count": len(doc_structure.get("sections", []))
            },
            "text_nodes": await self._create_mock_text_nodes(doc_structure),
            "paragraph_hierarchy": await self._create_mock_paragraph_hierarchy(doc_structure),
            "style_information": await self._get_mock_style_info(state),
            "uno_integration_status": "mock_mode"
        }
        
        self.logger.debug(f"Generated mock SwDoc model with {len(mock_model['text_nodes'])} text nodes")
        return mock_model
    
    async def _get_real_swdoc_model(self, state: DocumentState) -> Dict[str, Any]:
        """Get real SwDoc model from LibreOffice UNO."""
        try:
            # Access current document through AgentCoordinator if available
            if hasattr(self.document_service, 'getDocumentContext'):
                doc_context = self.document_service.getDocumentContext()
                
                swdoc_model = {
                    "model_type": "real_swdoc",
                    "document_info": {
                        "title": getattr(doc_context, 'title', 'Unknown'),
                        "url": getattr(doc_context, 'url', ''),
                        "paragraph_count": getattr(doc_context, 'paragraphCount', 0)
                    },
                    "text_nodes": await self._extract_real_text_nodes(doc_context),
                    "paragraph_hierarchy": await self._extract_real_hierarchy(doc_context),
                    "style_information": await self._extract_real_styles(doc_context),
                    "uno_integration_status": "real_mode"
                }
                
                self.logger.info("Successfully accessed real SwDoc model")
                return swdoc_model
                
            else:
                # Fallback to Desktop service
                self.logger.warning("AgentCoordinator not available, using Desktop fallback")
                return await self._get_desktop_document_model(state)
                
        except Exception as e:
            self.logger.error(f"Error accessing real SwDoc model: {e}")
            # Fallback to mock model
            return await self._get_mock_swdoc_model(state)
    
    async def _get_desktop_document_model(self, state: DocumentState) -> Dict[str, Any]:
        """Get document model through Desktop service."""
        try:
            # Get current document from Desktop
            current_doc = self.document_service.getCurrentComponent()
            
            if current_doc and hasattr(current_doc, 'getText'):
                text = current_doc.getText()
                
                desktop_model = {
                    "model_type": "desktop_swdoc",
                    "document_info": {
                        "title": getattr(current_doc, 'getTitle', lambda: 'Unknown')(),
                        "url": getattr(current_doc, 'getURL', lambda: '')(),
                        "has_text": text is not None
                    },
                    "text_nodes": await self._extract_desktop_text_nodes(text),
                    "paragraph_hierarchy": [],  # Limited access through Desktop
                    "style_information": {},    # Limited access through Desktop
                    "uno_integration_status": "desktop_mode"
                }
                
                self.logger.info("Successfully accessed document through Desktop service")
                return desktop_model
                
        except Exception as e:
            self.logger.error(f"Error accessing document through Desktop: {e}")
            
        # Final fallback to mock
        return await self._get_mock_swdoc_model(state)
    
    async def traverse_text_nodes(self, swdoc_model: Dict[str, Any]) -> List[Dict[str, Any]]:
        """
        Traverse text nodes in the document for detailed analysis.
        Returns list of text node information.
        """
        if not swdoc_model:
            return []
            
        try:
            text_nodes = swdoc_model.get("text_nodes", [])
            
            enhanced_nodes = []
            for i, node in enumerate(text_nodes):
                enhanced_node = {
                    "node_index": i,
                    "node_type": node.get("type", "text"),
                    "content": node.get("content", ""),
                    "start_position": node.get("start", 0),
                    "end_position": node.get("end", 0),
                    "paragraph_index": node.get("paragraph", 0),
                    "style_name": node.get("style", "Standard"),
                    "formatting": node.get("formatting", {}),
                    "parent_section": node.get("section", None),
                    "node_depth": node.get("depth", 0)
                }
                
                # Add semantic information if available
                if "semantic_info" in node:
                    enhanced_node["semantic_info"] = node["semantic_info"]
                    
                enhanced_nodes.append(enhanced_node)
            
            self.logger.debug(f"Traversed {len(enhanced_nodes)} text nodes")
            return enhanced_nodes
            
        except Exception as e:
            self.logger.error(f"Error traversing text nodes: {e}")
            return []
    
    async def extract_styling_info(self, swdoc_model: Dict[str, Any]) -> Dict[str, Any]:
        """
        Extract document styling information from SwDoc model.
        Returns comprehensive styling data.
        """
        if not swdoc_model:
            return {}
            
        try:
            style_info = swdoc_model.get("style_information", {})
            
            comprehensive_styles = {
                "character_styles": style_info.get("character_styles", {}),
                "paragraph_styles": style_info.get("paragraph_styles", {}),
                "page_styles": style_info.get("page_styles", {}),
                "numbering_styles": style_info.get("numbering_styles", {}),
                "table_styles": style_info.get("table_styles", {}),
                "current_style_context": {
                    "active_paragraph_style": style_info.get("current_paragraph_style", "Standard"),
                    "active_character_style": style_info.get("current_character_style", "Default"),
                    "cursor_style_info": style_info.get("cursor_style", {})
                },
                "style_hierarchy": style_info.get("style_hierarchy", {}),
                "custom_styles": style_info.get("custom_styles", []),
                "style_usage_count": style_info.get("usage_stats", {}),
                "uno_integration_mode": swdoc_model.get("uno_integration_status", "unknown")
            }
            
            self.logger.debug(f"Extracted styling info with {len(comprehensive_styles['paragraph_styles'])} paragraph styles")
            return comprehensive_styles
            
        except Exception as e:
            self.logger.error(f"Error extracting styling info: {e}")
            return {}
    
    # Helper methods for mock UNO implementation
    
    async def _create_mock_text_nodes(self, doc_structure: Dict[str, Any]) -> List[Dict[str, Any]]:
        """Create mock text nodes based on document structure."""
        nodes = []
        paragraph_count = doc_structure.get("paragraphs", 0)
        sections = doc_structure.get("sections", [])
        
        current_section = None
        for para_idx in range(paragraph_count):
            # Determine current section
            for section in sections:
                if isinstance(section, dict):
                    start = section.get("start_paragraph", 0)
                    end = section.get("end_paragraph", 0)
                    if start <= para_idx <= end:
                        current_section = section.get("title", f"Section {start}")
                        break
            
            node = {
                "type": "paragraph",
                "content": f"Mock paragraph {para_idx + 1} content",
                "start": para_idx * 100,
                "end": (para_idx + 1) * 100 - 1,
                "paragraph": para_idx,
                "style": "Standard" if para_idx % 5 != 0 else "Heading 2",
                "formatting": {
                    "font_name": "Arial",
                    "font_size": 12 if para_idx % 5 != 0 else 14,
                    "bold": para_idx % 5 == 0
                },
                "section": current_section,
                "depth": 0
            }
            nodes.append(node)
        
        return nodes
    
    async def _create_mock_paragraph_hierarchy(self, doc_structure: Dict[str, Any]) -> List[Dict[str, Any]]:
        """Create mock paragraph hierarchy."""
        hierarchy = []
        sections = doc_structure.get("sections", [])
        
        for i, section in enumerate(sections):
            if isinstance(section, dict):
                hierarchy_item = {
                    "section_index": i,
                    "title": section.get("title", f"Section {i+1}"),
                    "start_paragraph": section.get("start_paragraph", 0),
                    "end_paragraph": section.get("end_paragraph", 0),
                    "level": 1,  # Mock level
                    "style": "Heading 1" if i == 0 else "Heading 2",
                    "child_elements": []
                }
                hierarchy.append(hierarchy_item)
        
        return hierarchy
    
    async def _get_mock_style_info(self, state: DocumentState) -> Dict[str, Any]:
        """Get mock styling information."""
        formatting_state = state.get("formatting_state", {})
        
        return {
            "character_styles": {
                "Default": {"font_name": "Arial", "font_size": 12},
                "Emphasis": {"font_name": "Arial", "font_size": 12, "italic": True}
            },
            "paragraph_styles": {
                "Standard": {"font_name": "Arial", "font_size": 12, "line_spacing": 1.0},
                "Heading 1": {"font_name": "Arial", "font_size": 16, "bold": True},
                "Heading 2": {"font_name": "Arial", "font_size": 14, "bold": True}
            },
            "page_styles": {
                "Default": {"page_width": "21cm", "page_height": "29.7cm", "margins": "2cm"}
            },
            "current_paragraph_style": formatting_state.get("current_style", "Standard"),
            "current_character_style": "Default",
            "cursor_style": {
                "font": formatting_state.get("font", "Arial"),
                "size": formatting_state.get("size", 12)
            }
        }
    
    async def _extract_real_text_nodes(self, doc_context: Any) -> List[Dict[str, Any]]:
        """Extract real text nodes from LibreOffice document."""
        # Placeholder for real UNO text node extraction
        # This would use actual UNO API calls to traverse document
        nodes = []
        
        try:
            # This is where real UNO API calls would go
            # For now, return empty list as this requires actual LibreOffice integration
            pass
            
        except Exception as e:
            self.logger.error(f"Error extracting real text nodes: {e}")
            
        return nodes
    
    async def _extract_real_hierarchy(self, doc_context: Any) -> List[Dict[str, Any]]:
        """Extract real paragraph hierarchy from LibreOffice document."""
        # Placeholder for real UNO hierarchy extraction
        return []
    
    async def _extract_real_styles(self, doc_context: Any) -> Dict[str, Any]:
        """Extract real style information from LibreOffice document.""" 
        # Placeholder for real UNO style extraction
        return {}
    
    async def _extract_desktop_text_nodes(self, text: Any) -> List[Dict[str, Any]]:
        """Extract text nodes through Desktop service."""
        nodes = []
        
        try:
            if text and hasattr(text, 'createTextCursor'):
                cursor = text.createTextCursor()
                cursor.gotoStart(False)
                
                para_count = 0
                while cursor.gotoNextParagraph(False):
                    cursor.gotoEndOfParagraph(True)  # Select paragraph
                    content = cursor.getString()
                    
                    node = {
                        "type": "paragraph",
                        "content": content,
                        "paragraph": para_count,
                        "start": para_count * 100,
                        "end": (para_count + 1) * 100,
                        "style": "Unknown"  # Limited access through Desktop
                    }
                    nodes.append(node)
                    para_count += 1
                    
                    cursor.collapseToEnd()
                    
                    # Limit to prevent infinite loops
                    if para_count > 1000:
                        break
                        
        except Exception as e:
            self.logger.error(f"Error extracting desktop text nodes: {e}")
            
        return nodes
    
    def get_uno_connection_status(self) -> Dict[str, Any]:
        """Get current UNO connection status information."""
        return {
            "uno_bridge_connected": self.uno_bridge is not None,
            "document_service_available": self.document_service is not None,
            "connection_type": "mock" if isinstance(self.uno_bridge, dict) else "real" if self.uno_bridge else "none",
            "swdoc_model_accessible": self.swdoc_model is not None,
            "connection_timeout": self.uno_connection_timeout,
            "retry_attempts": self.uno_retry_attempts,
            "last_connection_test": getattr(self, '_last_connection_test', None)
        }
    
    def invalidate_cache_for_document(self, document_path: str) -> int:
        """
        Invalidate cached analysis results for a specific document.
        
        Args:
            document_path: Path to the document whose cache should be invalidated
            
        Returns:
            Number of cache entries invalidated
        """
        if not self.analysis_cache:
            return 0
        
        return self.analysis_cache.invalidate_cache(document_path=document_path)
    
    def update_document_version(self, document_path: str, content_hash: str) -> bool:
        """
        Update document version tracking and invalidate cache if changed.
        
        Args:
            document_path: Path to the document
            content_hash: Hash of current document content
            
        Returns:
            True if document changed and cache was invalidated
        """
        if not self.analysis_cache:
            return False
        
        return self.analysis_cache.update_document_version(document_path, content_hash)
    
    def get_cache_performance_metrics(self) -> Dict[str, Any]:
        """
        Get cache performance metrics for optimization insights.
        
        Returns:
            Dictionary containing cache performance data
        """
        if not self.analysis_cache:
            return {
                "cache_enabled": False,
                "message": "Cache not available"
            }
        
        metrics = self.analysis_cache.get_performance_metrics()
        metrics["cache_enabled"] = True
        
        return metrics
    
    def clear_analysis_cache(self) -> int:
        """
        Clear all cached analysis results.
        
        Returns:
            Number of entries cleared
        """
        if not self.analysis_cache:
            return 0
        
        return self.analysis_cache.clear_cache()
    
    def calculate_document_hash(self, state: DocumentState) -> str:
        """
        Calculate a hash of document content for change detection.
        
        Args:
            state: Current document state
            
        Returns:
            Document content hash string
        """
        # Extract relevant document content for hashing
        content_data = {
            "document_path": state.get("current_document", {}).get("path", ""),
            "document_title": state.get("current_document", {}).get("title", ""),
            "paragraph_count": state.get("document_structure", {}).get("paragraphs", 0),
            "sections": state.get("document_structure", {}).get("sections", []),
            "tables": state.get("document_structure", {}).get("tables", []),
            "charts": state.get("document_structure", {}).get("charts", []),
            "selected_text": state.get("selected_text", ""),
            "last_modified": state.get("current_document", {}).get("last_modified", "")
        }
        
        content_string = json.dumps(content_data, sort_keys=True)
        return hashlib.sha256(content_string.encode()).hexdigest()[:16]
    
    async def optimize_cache_performance(self) -> Dict[str, Any]:
        """
        Analyze and optimize cache performance based on usage patterns.
        
        Returns:
            Dictionary containing optimization results and recommendations
        """
        if not self.analysis_cache:
            return {"message": "Cache not available for optimization"}
        
        metrics = self.analysis_cache.get_performance_metrics()
        
        # Analyze performance and provide recommendations
        recommendations = []
        
        if metrics["hit_rate"] < 0.3:
            recommendations.append("Low cache hit rate - consider increasing cache size or TTL")
        
        if metrics["evictions"] > metrics["hits"] * 0.1:
            recommendations.append("High eviction rate - consider increasing cache size")
        
        if metrics["cache_size"] < metrics["max_size"] * 0.1:
            recommendations.append("Cache underutilized - may reduce cache size")
        
        optimization_result = {
            "current_metrics": metrics,
            "recommendations": recommendations,
            "optimal_cache_size": min(1000, max(100, metrics["total_requests"] // 10)),
            "suggested_ttl": 300 if metrics["hit_rate"] > 0.5 else 600
        }
        
        return optimization_result
    
    async def analyze_document_context(self, request: AnalysisRequest) -> AnalysisResult:
        """
        Analyze document context based on the provided request.
        
        This is an alias method that wraps the main process() method to provide
        a more specific interface for direct analysis requests.
        
        Args:
            request: AnalysisRequest with analysis parameters
            
        Returns:
            AnalysisResult: Analysis results
        """
        # Create a minimal state for processing
        mock_state = {
            'current_document': {'path': 'test.odt', 'title': 'Test Document'},
            'cursor_position': request.target_position or {'line': 1, 'column': 1},
            'selected_text': request.content_focus or '',
            'document_structure': {},
            'formatting_state': {},
            'messages': [],
            'current_task': f"Analysis: {request.analysis_mode.value}",
            'task_history': [],
            'agent_status': {},
            'content_analysis': {},
            'generated_content': [],
            'content_suggestions': [],
            'external_data': {},
            'research_citations': [],
            'api_usage': {},
            'pending_operations': [],
            'completed_operations': [],
            'validation_results': {},
            'last_error': None,
            'retry_count': 0,
            'error_recovery': {},
            'rollback_points': [],
            'user_preferences': {},
            'interaction_history': [],
            'approval_required': [],
            'performance_metrics': {},
            'resource_utilization': {},
            'optimization_recommendations': []
        }
        
        # Process the request using the main processing pipeline
        result = await self.process(mock_state)
        
        # Convert AgentResult to AnalysisResult
        analysis_result = AnalysisResult(
            request_id=request.request_id,
            analysis_mode=request.analysis_mode,
            success=result.success,
            execution_time_ms=result.execution_time * 1000,  # Convert to ms
            context_data=result.data.get('context_data', {}),
            semantic_insights=result.data.get('semantic_insights', {}),
            cursor_context=result.data.get('cursor_context', {}),
            document_structure=result.data.get('document_structure', {}),
            error_message=result.error,
            cache_hit=result.data.get('cache_hit', False),
            metadata=result.metadata or {}
        )
        
        return analysis_result