"""
ContentGenerationAgent - AI-Powered Content Creation and Writing Assistant

This agent specializes in intelligent content generation for LibreOffice documents,
providing sophisticated text creation, rewriting, and structured content generation
capabilities with a focus on professional and financial document creation.

Key Capabilities:
- AI-powered text generation and content creation
- Content rewriting and enhancement for clarity and professionalism  
- Structured content generation (lists, sections, summaries)
- Financial document templates and standardized content
- Context-aware content suggestions based on document analysis
- Integration with LibreOffice UNO services for content insertion
"""

import asyncio
import time
import logging
import json
import hashlib
from datetime import datetime, timezone
from typing import Any, Dict, List, Optional, Union, Tuple
from dataclasses import dataclass, field
from enum import Enum

# Import base agent classes
from .base import (
    BaseAgent, 
    AgentCapability, 
    AgentResult, 
    AgentError,
    ValidationResult,
    PerformanceMetrics
)

# Import LLM client for AI-powered content generation
from llm_client import get_llm_client

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

# Import shared caching system
from .shared_cache import SharedCacheMixin, CacheType, InvalidationTrigger


class ContentType(Enum):
    """Types of content that can be generated."""
    TEXT_PARAGRAPH = "text_paragraph"
    EXECUTIVE_SUMMARY = "executive_summary"
    FINANCIAL_ANALYSIS = "financial_analysis"
    BULLET_LIST = "bullet_list"
    NUMBERED_LIST = "numbered_list"
    TABLE_CONTENT = "table_content"
    CHART_DESCRIPTION = "chart_description"
    SECTION_HEADING = "section_heading"
    CONCLUSION = "conclusion"
    RECOMMENDATION = "recommendation"


class GenerationMode(Enum):
    """Content generation complexity modes."""
    QUICK = "quick"          # Simple completion, rephrasing (≤1s)
    STANDARD = "standard"    # Structured content, summaries (≤2s)
    COMPREHENSIVE = "comprehensive"  # Complex analysis, reports (≤3s)


@dataclass
class ContentRequest:
    """Request for content generation."""
    request_id: str
    content_type: ContentType
    generation_mode: GenerationMode
    context: Dict[str, Any]
    user_prompt: Optional[str] = None
    target_length: Optional[int] = None
    tone: str = "professional"
    language: str = "en"
    template_name: Optional[str] = None


@dataclass
class GeneratedContent:
    """Container for generated content results."""
    request_id: str
    content_type: ContentType
    generated_text: str
    metadata: Dict[str, Any] = field(default_factory=dict)
    suggestions: List[str] = field(default_factory=list)
    quality_score: Optional[float] = None
    word_count: int = 0
    execution_time_ms: float = 0.0
    success: bool = True
    error_message: Optional[str] = None


class ContentGenerationAgent(SharedCacheMixin, BaseAgent):
    """
    Specialized agent for AI-powered content generation and writing assistance.
    
    This agent provides intelligent content creation capabilities ranging from
    simple text completion to comprehensive document generation, with special
    focus on financial and professional document types.
    
    Performance targets:
    - Quick generation: ≤1 second
    - Standard generation: ≤2 seconds  
    - Comprehensive generation: ≤3 seconds
    """
    
    def __init__(self, 
                 agent_id: str = "content_generation_agent",
                 config: Optional[Dict[str, Any]] = None):
        """
        Initialize the ContentGenerationAgent.
        
        Args:
            agent_id: Unique identifier for this agent instance
            config: Optional configuration dictionary
        """
        # Define agent capabilities
        capabilities = [
            AgentCapability.CONTENT_GENERATION,
            AgentCapability.FORMATTING
        ]
        
        # Default configuration
        default_config = {
            "max_retries": 2,
            "retry_delay": 0.5,
            "enable_caching": True,
            "cache_ttl_seconds": 600,  # 10 minutes for generated content
            "max_content_length": 5000,
            "performance_targets": {
                GenerationMode.QUICK: 1000,         # ms
                GenerationMode.STANDARD: 2000,      # ms
                GenerationMode.COMPREHENSIVE: 3000  # ms
            },
            "supported_languages": ["en", "es", "fr", "de"],
            "default_tone": "professional",
            "enable_quality_scoring": True,
            "financial_templates_enabled": True
        }
        
        # Merge with provided config
        merged_config = {**default_config, **(config or {})}
        
        # Initialize base agent
        super().__init__(
            agent_id=agent_id,
            capabilities=capabilities,
            config=merged_config
        )
        
        # Content generation configuration
        self.max_content_length = self.config.get("max_content_length", 5000)
        self.supported_languages = self.config.get("supported_languages", ["en"])
        self.default_tone = self.config.get("default_tone", "professional")
        self.enable_quality_scoring = self.config.get("enable_quality_scoring", True)
        
        # Performance targets
        self.performance_targets = self.config.get("performance_targets", {})
        
        # Content caching for reuse and performance
        self.content_cache: Dict[str, GeneratedContent] = {}
        self.cache_enabled = self.config.get("enable_caching", True)
        self.cache_ttl_seconds = self.config.get("cache_ttl_seconds", 600)
        
        # LLM client for AI-powered generation
        self.llm_client = get_llm_client()
        
        # Content templates for financial documents
        self.financial_templates = self._initialize_financial_templates()
        
        # Quality assessment patterns
        self.quality_metrics = self._initialize_quality_metrics()
        
        self.logger.info(f"ContentGenerationAgent {agent_id} initialized with content generation capabilities")
    
    def get_required_tools(self) -> List[str]:
        """
        Get list of tools required by this agent.
        
        Returns:
            List of tool names required for content generation
        """
        return [
            "text_generator",
            "content_formatter", 
            "quality_assessor",
            "template_engine",
            "language_detector"
        ]
    
    async def process(self, 
                     state: DocumentState, 
                     message: Optional[BaseMessage] = None) -> AgentResult:
        """
        Process a content generation request.
        
        Args:
            state: Current document state
            message: Optional message containing generation request
            
        Returns:
            AgentResult with generated content and state updates
        """
        operation_id = f"content_gen_{int(time.time() * 1000)}"
        start_time = time.time()
        
        try:
            # Extract document ID for shared state coordination
            document_id = state.get("current_document", {}).get("path", "unknown_doc")
            
            # Extract content generation request
            content_request = self._extract_content_request(state, message)
            
            # Validate request
            validation = self._validate_content_request(content_request)
            if not validation.passed:
                error_messages = [issue.get("message", "") for issue in validation.issues if issue.get("level") == "error"]
                return AgentResult(
                    agent_id=self.agent_id,
                    operation_id=operation_id,
                    success=False,
                    error=f"Invalid content request: {'; '.join(error_messages)}",
                    execution_time=time.time() - start_time
                )
            
            # Check shared cache first
            cache_key = self.generate_cache_key(document_id, content_request.request_id)
            cached_result = self.get_cached_agent_result(document_id, cache_key)
            if cached_result and self.cache_enabled:
                self.logger.debug(f"Shared cache hit for content request {content_request.request_id}")
                return cached_result
            
            # Get context from other agents (like FormattingAgent)
            formatting_context = self.get_other_agent_context(document_id, "formatting_agent")
            
            # Generate content based on request and cross-agent context
            generated_content = await self._generate_content(content_request, state, formatting_context)
            
            # Create agent result with state updates
            agent_result = self._create_agent_result(operation_id, generated_content, start_time)
            
            # Cache the result in shared cache
            if self.cache_enabled and generated_content.success:
                self.cache_agent_result(document_id, cache_key, agent_result)
            
            # Update shared context with content generation results
            self.update_shared_context(document_id, {
                "last_generated_content": {
                    "request_id": content_request.request_id,
                    "content_type": content_request.content_type.value,
                    "generated_at": time.time(),
                    "content_preview": generated_content.generated_text[:100] + "..." if len(generated_content.generated_text) > 100 else generated_content.generated_text
                },
                "content_generation_state": "completed",
                "available_for_formatting": True
            })
            
            # Invalidate related cache when new content is generated
            self.invalidate_related_cache(document_id, InvalidationTrigger.CONTENT_MODIFIED)
            
            return agent_result
            
        except Exception as e:
            execution_time = time.time() - start_time
            error_msg = f"Content generation failed: {str(e)}"
            self.logger.error(error_msg, exc_info=True)
            
            return AgentResult(
                agent_id=self.agent_id,
                operation_id=operation_id,
                success=False,
                error=error_msg,
                execution_time=execution_time,
                metadata={"error_type": type(e).__name__}
            )
    
    def validate_input(self, 
                      state: DocumentState, 
                      message: Optional[BaseMessage] = None) -> ValidationResult:
        """
        Validate input for content generation.
        
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
            # Check required fields for content generation
            required_fields = ["current_document"]
            for field in required_fields:
                if field not in state:
                    errors.append(f"Missing required field: {field}")
        
        # Validate message format if provided
        if message:
            if not isinstance(message, dict):
                errors.append("Message must be a dictionary")
            elif "content" not in message:
                warnings.append("Message missing content field")
        
        return ValidationResult(
            agent_id=self.agent_id,
            validation_type="input_validation",
            passed=len(errors) == 0,
            confidence=1.0 if len(errors) == 0 else 0.0,
            issues=[{"level": "error", "message": error} for error in errors] + 
                   [{"level": "warning", "message": warning} for warning in warnings],
            recommendations=warnings,
            metadata={
                "agent": self.agent_id,
                "validation_time": datetime.now(timezone.utc).isoformat()
            }
        )
    
    def _extract_content_request(self, 
                               state: DocumentState, 
                               message: Optional[BaseMessage]) -> ContentRequest:
        """Extract content generation request from state and message."""
        request_id = f"req_{int(time.time() * 1000)}"
        
        # Default request parameters
        content_type = ContentType.TEXT_PARAGRAPH
        generation_mode = GenerationMode.STANDARD
        user_prompt = None
        
        # Extract request details from message
        if message and isinstance(message, dict):
            content = message.get("content", "")
            user_prompt = content
            
            # Analyze message to determine content type and mode
            content_type = self._analyze_content_type(content)
            generation_mode = self._analyze_generation_mode(content)
        
        # Build context from document state
        context = {
            "document_title": state.get("current_document", {}).get("title", ""),
            "document_path": state.get("current_document", {}).get("path", ""),
            "cursor_position": state.get("cursor_position", {}),
            "selected_text": state.get("selected_text", ""),
            "document_structure": state.get("document_structure", {}),
            "current_section": self._get_current_section(state),
            "document_type": self._infer_document_type(state)
        }
        
        return ContentRequest(
            request_id=request_id,
            content_type=content_type,
            generation_mode=generation_mode,
            context=context,
            user_prompt=user_prompt,
            tone=self.default_tone
        )
    
    def _validate_content_request(self, request: ContentRequest) -> ValidationResult:
        """Validate content generation request."""
        errors = []
        warnings = []
        
        # Validate content type
        if not isinstance(request.content_type, ContentType):
            errors.append("Invalid content type")
        
        # Validate generation mode
        if not isinstance(request.generation_mode, GenerationMode):
            errors.append("Invalid generation mode")
        
        # Validate language support
        if request.language not in self.supported_languages:
            warnings.append(f"Language {request.language} not fully supported")
        
        # Validate target length
        if request.target_length and request.target_length > self.max_content_length:
            warnings.append(f"Target length exceeds maximum ({self.max_content_length})")
        
        return ValidationResult(
            agent_id=self.agent_id,
            validation_type="content_request_validation",
            passed=len(errors) == 0,
            confidence=1.0 if len(errors) == 0 else 0.5,
            issues=[{"level": "error", "message": error} for error in errors] + 
                   [{"level": "warning", "message": warning} for warning in warnings],
            recommendations=warnings
        )
    
    async def _generate_content(self, 
                              request: ContentRequest, 
                              state: DocumentState,
                              formatting_context: Optional[Dict[str, Any]] = None) -> GeneratedContent:
        """Generate content based on request parameters using advanced prompts."""
        start_time = time.time()
        
        try:
            # Get appropriate prompt for content type and generation mode
            prompt = self._build_generation_prompt(request, state)
            
            # Generate content using LLM with advanced prompt
            # Try the new method first, fallback to existing method
            try:
                if hasattr(self.llm_client, 'generate_content_with_prompt'):
                    llm_response = await self.llm_client.generate_content_with_prompt(
                        prompt=prompt,
                        content_type=request.content_type.value,
                        generation_mode=request.generation_mode.value,
                        context=request.context
                    )
                else:
                    # Fallback to existing method
                    llm_response = await self.llm_client.generate_content_suggestions(
                        content_type=request.content_type.value,
                        context={**request.context, "custom_prompt": prompt}
                    )
            except AttributeError:
                # Fallback for missing method
                llm_response = await self.llm_client.generate_content_suggestions(
                    content_type=request.content_type.value,
                    context={**request.context, "custom_prompt": prompt}
                )
            
            if llm_response.success:
                # Parse LLM response - handle both JSON and plain text
                try:
                    content_data = json.loads(llm_response.content)
                    generated_text = content_data.get("generated_content", "")
                    suggestions = content_data.get("suggestions", [])
                    
                    # If generated_text is empty, try other fields
                    if not generated_text:
                        generated_text = content_data.get("content", content_data.get("text", ""))
                    
                    # If still empty, use the whole response as generated text
                    if not generated_text:
                        generated_text = llm_response.content
                    
                    # Calculate quality score if enabled
                    quality_score = None
                    if self.enable_quality_scoring:
                        quality_score = self._calculate_quality_score(generated_text, request)
                    
                    return GeneratedContent(
                        request_id=request.request_id,
                        content_type=request.content_type,
                        generated_text=generated_text,
                        metadata={
                            "llm_provider": llm_response.provider.value,
                            "llm_model": llm_response.model,
                            "tokens_used": llm_response.tokens_used,
                            "generation_mode": request.generation_mode.value,
                            "tone": request.tone
                        },
                        suggestions=suggestions,
                        quality_score=quality_score,
                        word_count=len(generated_text.split()),
                        execution_time_ms=(time.time() - start_time) * 1000,
                        success=True
                    )
                    
                except json.JSONDecodeError:
                    # Use raw response as generated content
                    return GeneratedContent(
                        request_id=request.request_id,
                        content_type=request.content_type,
                        generated_text=llm_response.content,
                        word_count=len(llm_response.content.split()),
                        execution_time_ms=(time.time() - start_time) * 1000,
                        success=True
                    )
            else:
                # LLM generation failed, create error result
                return GeneratedContent(
                    request_id=request.request_id,
                    content_type=request.content_type,
                    generated_text="",
                    execution_time_ms=(time.time() - start_time) * 1000,
                    success=False,
                    error_message=f"LLM generation failed: {llm_response.error}"
                )
                
        except Exception as e:
            return GeneratedContent(
                request_id=request.request_id,
                content_type=request.content_type,
                generated_text="",
                execution_time_ms=(time.time() - start_time) * 1000,
                success=False,
                error_message=str(e)
            )
    
    def _analyze_content_type(self, content: str) -> ContentType:
        """Analyze user message to determine content type."""
        content_lower = content.lower()
        
        # Content type detection patterns
        if any(word in content_lower for word in ["summary", "summarize", "overview"]):
            return ContentType.EXECUTIVE_SUMMARY
        elif any(word in content_lower for word in ["list", "bullet", "points"]):
            return ContentType.BULLET_LIST
        elif any(word in content_lower for word in ["table", "data", "columns"]):
            return ContentType.TABLE_CONTENT
        elif any(word in content_lower for word in ["conclusion", "conclude", "final"]):
            return ContentType.CONCLUSION
        elif any(word in content_lower for word in ["recommend", "suggest", "advice"]):
            return ContentType.RECOMMENDATION
        elif any(word in content_lower for word in ["financial", "analysis", "revenue", "profit"]):
            return ContentType.FINANCIAL_ANALYSIS
        elif any(word in content_lower for word in ["heading", "title", "section"]):
            return ContentType.SECTION_HEADING
        else:
            return ContentType.TEXT_PARAGRAPH
    
    def _analyze_generation_mode(self, content: str) -> GenerationMode:
        """Analyze user message to determine generation complexity."""
        content_lower = content.lower()
        
        # Mode detection patterns
        if any(word in content_lower for word in ["quick", "brief", "short", "simple"]):
            return GenerationMode.QUICK
        elif any(word in content_lower for word in ["comprehensive", "detailed", "complete", "thorough"]):
            return GenerationMode.COMPREHENSIVE
        else:
            return GenerationMode.STANDARD
    
    def _get_current_section(self, state: DocumentState) -> Optional[Dict[str, Any]]:
        """Get current document section based on cursor position."""
        cursor_pos = state.get("cursor_position", {})
        document_structure = state.get("document_structure", {})
        sections = document_structure.get("sections", [])
        
        if not cursor_pos or not sections:
            return None
        
        current_paragraph = cursor_pos.get("paragraph", 0)
        for section in sections:
            if isinstance(section, dict):
                start_para = section.get("start_paragraph", 0)
                end_para = section.get("end_paragraph", float('inf'))
                if start_para <= current_paragraph <= end_para:
                    return section
        
        return None
    
    def _infer_document_type(self, state: DocumentState) -> str:
        """Infer document type from title and content."""
        doc_title = state.get("current_document", {}).get("title", "").lower()
        
        if any(word in doc_title for word in ["financial", "report", "quarterly", "annual"]):
            return "financial_report"
        elif any(word in doc_title for word in ["business", "plan", "strategy"]):
            return "business_plan"
        elif any(word in doc_title for word in ["memo", "memorandum"]):
            return "memorandum"
        elif any(word in doc_title for word in ["proposal", "contract"]):
            return "proposal"
        else:
            return "general_document"
    
    def _check_cache(self, request: ContentRequest) -> Optional[GeneratedContent]:
        """Check if content is cached."""
        if not self.cache_enabled:
            return None
        
        cache_key = self._generate_cache_key(request)
        cached_content = self.content_cache.get(cache_key)
        
        if cached_content:
            # Check TTL
            age_seconds = (time.time() * 1000 - cached_content.execution_time_ms) / 1000
            if age_seconds < self.cache_ttl_seconds:
                return cached_content
            else:
                # Remove expired entry
                del self.content_cache[cache_key]
        
        return None
    
    def _cache_content(self, request: ContentRequest, content: GeneratedContent):
        """Cache generated content."""
        if not self.cache_enabled:
            return
        
        cache_key = self._generate_cache_key(request)
        self.content_cache[cache_key] = content
        
        # Simple cache size management
        if len(self.content_cache) > 100:  # Max 100 cached items
            oldest_key = min(self.content_cache.keys(), 
                           key=lambda k: self.content_cache[k].execution_time_ms)
            del self.content_cache[oldest_key]
    
    def _generate_cache_key(self, request: ContentRequest) -> str:
        """Generate cache key for content request."""
        key_data = {
            "content_type": request.content_type.value,
            "generation_mode": request.generation_mode.value,
            "user_prompt": request.user_prompt,
            "context_hash": hash(str(sorted(request.context.items())))
        }
        return hashlib.md5(str(key_data).encode()).hexdigest()
    
    def _calculate_quality_score(self, text: str, request: ContentRequest) -> float:
        """Calculate comprehensive quality score for generated content."""
        if not text:
            return 0.0
        
        # Use enhanced quality scoring
        return self._calculate_comprehensive_quality(text, request.content_type, request.generation_mode, request.context)
    
    def _calculate_comprehensive_quality(self, 
                                       text: str, 
                                       content_type: ContentType, 
                                       generation_mode: GenerationMode,
                                       context: Dict[str, Any]) -> float:
        """Calculate comprehensive quality score with multiple dimensions."""
        if not text:
            return 0.0
        
        score = 0.0
        text_lower = text.lower()
        words = text.split()
        word_count = len(words)
        
        # 1. Length and Structure Quality (0.25 weight)
        structure_score = self._evaluate_structure_quality(text, content_type, generation_mode)
        score += 0.25 * structure_score
        
        # 2. Content Relevance and Context Appropriateness (0.25 weight)
        relevance_score = self._evaluate_content_relevance(text, content_type, context)
        score += 0.25 * relevance_score
        
        # 3. Language Quality and Professionalism (0.25 weight)
        language_score = self._evaluate_language_quality(text, content_type)
        score += 0.25 * language_score
        
        # 4. Content Type Specific Quality (0.25 weight)
        type_specific_score = self._evaluate_type_specific_quality(text, content_type, context)
        score += 0.25 * type_specific_score
        
        return min(score, 1.0)
    
    def _evaluate_structure_quality(self, text: str, content_type: ContentType, generation_mode: GenerationMode) -> float:
        """Evaluate structural quality of the content."""
        score = 0.0
        words = text.split()
        word_count = len(words)
        
        # Length appropriateness based on generation mode
        mode_targets = {
            GenerationMode.QUICK: (10, 50),
            GenerationMode.STANDARD: (50, 200),
            GenerationMode.COMPREHENSIVE: (150, 400)
        }
        
        min_words, max_words = mode_targets.get(generation_mode, (20, 150))
        if min_words <= word_count <= max_words:
            score += 0.4
        elif word_count >= min_words * 0.8 and word_count <= max_words * 1.2:
            score += 0.3
        
        # Sentence structure
        sentences = text.count('.') + text.count('!') + text.count('?')
        if sentences > 0:
            avg_words_per_sentence = word_count / sentences
            if 8 <= avg_words_per_sentence <= 25:
                score += 0.3
            elif 5 <= avg_words_per_sentence <= 35:
                score += 0.2
        
        # Completeness
        if text.strip().endswith(('.', '!', '?')):
            score += 0.2
        if not text.strip().endswith('...'):
            score += 0.1
        
        return min(score, 1.0)
    
    def _evaluate_content_relevance(self, text: str, content_type: ContentType, context: Dict[str, Any]) -> float:
        """Evaluate how relevant the content is to the request and context."""
        score = 0.0
        text_lower = text.lower()
        
        # Context keyword matching
        document_title = context.get('document_title', '').lower()
        selected_text = context.get('selected_text', '').lower()
        
        # Document title relevance
        if document_title:
            title_words = document_title.split()
            matching_words = sum(1 for word in title_words if len(word) > 3 and word in text_lower)
            if title_words:
                score += 0.3 * (matching_words / len(title_words))
        
        # Selected text context relevance
        if selected_text and len(selected_text) > 10:
            selected_words = [w for w in selected_text.split() if len(w) > 3]
            if selected_words:
                matching_words = sum(1 for word in selected_words if word in text_lower)
                score += 0.3 * (matching_words / len(selected_words))
        else:
            score += 0.3  # No selected text constraint
        
        # Content type appropriateness
        content_type_keywords = {
            ContentType.EXECUTIVE_SUMMARY: ['summary', 'overview', 'highlights', 'key', 'performance'],
            ContentType.FINANCIAL_ANALYSIS: ['analysis', 'financial', 'revenue', 'profit', 'margin', 'growth'],
            ContentType.BULLET_LIST: ['•', '-', '*', '\n-', '\n•', '\n*'],
            ContentType.CONCLUSION: ['conclusion', 'summary', 'therefore', 'overall', 'results']
        }
        
        expected_keywords = content_type_keywords.get(content_type, [])
        if expected_keywords:
            keyword_matches = sum(1 for keyword in expected_keywords if keyword in text_lower)
            score += 0.4 * min(keyword_matches / len(expected_keywords), 1.0)
        else:
            score += 0.4  # No specific keywords required
        
        return min(score, 1.0)
    
    def _evaluate_language_quality(self, text: str, content_type: ContentType) -> float:
        """Evaluate language quality and professionalism."""
        score = 0.0
        text_lower = text.lower()
        
        # Professional language indicators
        professional_words = [
            'however', 'therefore', 'furthermore', 'additionally', 'consequently',
            'nevertheless', 'moreover', 'specifically', 'particularly', 'significantly'
        ]
        professional_count = sum(1 for word in professional_words if word in text_lower)
        score += min(0.3, professional_count * 0.05)
        
        # Financial terminology (for financial content types)
        if content_type in [ContentType.FINANCIAL_ANALYSIS, ContentType.EXECUTIVE_SUMMARY]:
            financial_terms = [
                'revenue', 'profit', 'margin', 'growth', 'performance', 'analysis',
                'metrics', 'earnings', 'assets', 'liability', 'return', 'investment'
            ]
            financial_count = sum(1 for term in financial_terms if term in text_lower)
            score += min(0.3, financial_count * 0.03)
        else:
            score += 0.3  # Not financial content
        
        # Grammar and coherence indicators
        # Check for proper capitalization at sentence starts
        sentences = text.split('.')
        proper_caps = sum(1 for s in sentences if s.strip() and s.strip()[0].isupper())
        if sentences:
            score += 0.2 * (proper_caps / len(sentences))
        
        # Variety in sentence structure
        conjunctions = ['and', 'but', 'or', 'however', 'therefore', 'furthermore']
        conjunction_count = sum(1 for conj in conjunctions if conj in text_lower)
        score += min(0.2, conjunction_count * 0.05)
        
        return min(score, 1.0)
    
    def _evaluate_type_specific_quality(self, text: str, content_type: ContentType, context: Dict[str, Any]) -> float:
        """Evaluate quality specific to the content type."""
        score = 0.0
        text_lower = text.lower()
        
        if content_type == ContentType.EXECUTIVE_SUMMARY:
            # Should be concise but comprehensive
            word_count = len(text.split())
            if 75 <= word_count <= 250:
                score += 0.4
            
            # Should mention key metrics or results
            key_phrases = ['performance', 'results', 'growth', 'revenue', 'highlights']
            phrase_count = sum(1 for phrase in key_phrases if phrase in text_lower)
            score += min(0.6, phrase_count * 0.15)
            
        elif content_type == ContentType.FINANCIAL_ANALYSIS:
            # Should include quantitative elements
            numbers_pattern = any(char.isdigit() for char in text)
            if numbers_pattern:
                score += 0.3
            
            # Should have analytical language
            analytical_words = ['analysis', 'indicates', 'shows', 'demonstrates', 'reveals']
            analytical_count = sum(1 for word in analytical_words if word in text_lower)
            score += min(0.4, analytical_count * 0.1)
            
            # Should discuss financial metrics
            metrics = ['margin', 'ratio', 'percentage', 'growth', 'decline', 'increase']
            metric_count = sum(1 for metric in metrics if metric in text_lower)
            score += min(0.3, metric_count * 0.1)
            
        elif content_type == ContentType.BULLET_LIST:
            # Should have bullet points or list structure
            list_indicators = text.count('•') + text.count('-') + text.count('*')
            if list_indicators >= 2:
                score += 0.5
            
            # Each point should be concise
            lines = [line.strip() for line in text.split('\n') if line.strip()]
            if lines:
                avg_line_length = sum(len(line.split()) for line in lines) / len(lines)
                if 5 <= avg_line_length <= 15:
                    score += 0.5
            
        elif content_type == ContentType.CONCLUSION:
            # Should have conclusive language
            conclusive_words = ['conclusion', 'therefore', 'thus', 'overall', 'in summary']
            conclusive_count = sum(1 for word in conclusive_words if word in text_lower)
            score += min(0.5, conclusive_count * 0.2)
            
            # Should reference previous content
            reference_words = ['above', 'previous', 'discussed', 'analysis', 'results']
            reference_count = sum(1 for word in reference_words if word in text_lower)
            score += min(0.5, reference_count * 0.15)
            
        else:
            # General text paragraph quality
            score += 0.5  # Default for general text
            
            # Check for coherent flow
            transition_words = ['first', 'second', 'then', 'next', 'finally', 'however']
            transition_count = sum(1 for word in transition_words if word in text_lower)
            score += min(0.5, transition_count * 0.1)
        
        return min(score, 1.0)
    
    def _create_agent_result(self, 
                           operation_id: str, 
                           content: GeneratedContent, 
                           start_time: float,
                           from_cache: bool = False) -> AgentResult:
        """Create AgentResult from GeneratedContent."""
        execution_time = time.time() - start_time
        
        # Prepare state updates
        state_updates = {
            "generated_content": [content.generated_text] if content.success else [],
            "content_generation": {
                "request_id": content.request_id,
                "content_type": content.content_type.value,
                "word_count": content.word_count,
                "quality_score": content.quality_score,
                "suggestions": content.suggestions,
                "success": content.success,
                "from_cache": from_cache,
                "metadata": content.metadata
            }
        }
        
        return AgentResult(
            agent_id=self.agent_id,
            operation_id=operation_id,
            success=content.success,
            result=content.generated_text if content.success else None,
            error=content.error_message,
            execution_time=execution_time,
            state_updates=state_updates,
            metadata={
                "content_type": content.content_type.value,
                "generation_performance": {
                    "target_ms": self.performance_targets.get(
                        GenerationMode.STANDARD, 2000
                    ),
                    "actual_ms": content.execution_time_ms,
                    "performance_met": content.execution_time_ms <= self.performance_targets.get(
                        GenerationMode.STANDARD, 2000
                    )
                },
                "cache_used": from_cache
            }
        )
    
    def _initialize_financial_templates(self) -> Dict[str, str]:
        """Initialize financial document templates and prompts."""
        return {
            "executive_summary": """
This executive summary provides a comprehensive overview of {company_name}'s 
financial performance for {period}. Key highlights include revenue growth of 
{revenue_growth}%, operational efficiency improvements, and strategic initiatives 
that position the company for continued success.
            """.strip(),
            
            "financial_analysis": """
Financial Analysis for {period}:

Revenue: {revenue}
- Growth rate: {growth_rate}%
- Primary drivers: {revenue_drivers}

Profitability:
- Gross margin: {gross_margin}%
- Operating margin: {operating_margin}%
- Net margin: {net_margin}%

Key Performance Indicators:
- {kpi_1}: {value_1}
- {kpi_2}: {value_2}
- {kpi_3}: {value_3}
            """.strip()
        }
    
    def _get_advanced_generation_prompts(self) -> Dict[str, Dict[str, str]]:
        """Get advanced prompt templates for different content types and generation modes."""
        return {
            ContentType.EXECUTIVE_SUMMARY.value: {
                GenerationMode.QUICK.value: """
Create a brief executive summary for this financial document. 
Context: {context}
Document type: {document_type}
Selected text: {selected_text}

Requirements:
- 2-3 concise sentences
- Professional tone
- Focus on key financial highlights
- Maximum 50 words

Generate only the summary text, no additional commentary.
                """.strip(),
                
                GenerationMode.STANDARD.value: """
Create a comprehensive executive summary for this financial document.
Context: {context}
Document type: {document_type}
Current section: {current_section}
Selected text: {selected_text}

Requirements:
- Professional financial language
- Include key performance metrics
- Highlight growth trends and challenges
- 100-150 words
- Structure: Overview, key results, outlook

Generate a well-structured executive summary that captures the essence of the financial performance.
                """.strip(),
                
                GenerationMode.COMPREHENSIVE.value: """
Create a detailed executive summary for this financial document that meets professional standards.
Context: {context}
Document type: {document_type}
Document structure: {document_structure}
Current section: {current_section}
Selected text: {selected_text}

Requirements:
- Executive-level professional language
- Comprehensive coverage of financial performance
- Include quantitative metrics and qualitative insights
- Address growth drivers, challenges, and strategic initiatives
- 200-300 words
- Structure: Company overview, financial highlights, operational performance, strategic outlook
- Compliance with financial reporting standards

Generate a comprehensive executive summary suitable for board presentations and investor communications.
                """.strip()
            },
            
            ContentType.FINANCIAL_ANALYSIS.value: {
                GenerationMode.QUICK.value: """
Generate a quick financial analysis based on the provided context.
Context: {context}
Selected text: {selected_text}

Requirements:
- Focus on key financial metrics
- Professional analytical tone
- 1-2 paragraphs maximum
- Include specific numbers if available

Generate concise financial analysis.
                """.strip(),
                
                GenerationMode.STANDARD.value: """
Create a comprehensive financial analysis for this section.
Context: {context}
Document type: {document_type}
Current section: {current_section}
Selected text: {selected_text}

Requirements:
- Detailed financial metrics analysis
- Include revenue, profitability, and efficiency metrics
- Professional analytical language
- 3-4 paragraphs
- Provide insights and trends
- Use financial terminology appropriately

Generate a thorough financial analysis with actionable insights.
                """.strip(),
                
                GenerationMode.COMPREHENSIVE.value: """
Create an in-depth financial analysis suitable for professional financial reporting.
Context: {context}
Document type: {document_type}
Document structure: {document_structure}
Current section: {current_section}
Selected text: {selected_text}

Requirements:
- Comprehensive financial metrics coverage
- Include year-over-year comparisons
- Analyze revenue streams, cost structure, profitability
- Assess financial health and performance trends
- Professional investment-grade analysis
- 300-500 words
- Include specific metrics, ratios, and percentages
- Provide forward-looking insights
- Maintain objectivity and analytical rigor

Generate a comprehensive financial analysis that meets institutional investor standards.
                """.strip()
            },
            
            ContentType.TEXT_PARAGRAPH.value: {
                GenerationMode.QUICK.value: """
Generate a professional paragraph based on the context.
Context: {context}
User request: {user_prompt}
Selected text: {selected_text}

Requirements:
- Match the document's tone and style
- Professional language
- 2-3 sentences
- Relevant to the document context

Generate appropriate paragraph text.
                """.strip(),
                
                GenerationMode.STANDARD.value: """
Create a well-structured paragraph that fits naturally in this document.
Context: {context}
Document type: {document_type}
User request: {user_prompt}
Current section: {current_section}
Selected text: {selected_text}

Requirements:
- Maintain document consistency
- Professional writing style
- 4-6 sentences
- Logical flow and coherence
- Appropriate for the document type

Generate a coherent paragraph that enhances the document.
                """.strip(),
                
                GenerationMode.COMPREHENSIVE.value: """
Create detailed paragraph content that integrates seamlessly with the existing document.
Context: {context}
Document type: {document_type}
Document structure: {document_structure}
User request: {user_prompt}
Current section: {current_section}
Selected text: {selected_text}

Requirements:
- Maintain stylistic consistency throughout
- Professional, polished language
- 6-10 sentences
- Logical progression of ideas
- Support main document themes
- Include specific details and examples where appropriate
- Ensure smooth transitions with surrounding content

Generate comprehensive paragraph content that elevates the overall document quality.
                """.strip()
            }
        }
    
    def _build_generation_prompt(self, request: ContentRequest, state: DocumentState) -> str:
        """Build an advanced generation prompt based on content type and mode."""
        prompts = self._get_advanced_generation_prompts()
        
        # Get the specific prompt template
        content_type_prompts = prompts.get(request.content_type.value, {})
        prompt_template = content_type_prompts.get(
            request.generation_mode.value,
            prompts[ContentType.TEXT_PARAGRAPH.value][GenerationMode.STANDARD.value]
        )
        
        # Extract context information
        context_str = self._format_context_for_prompt(request.context)
        document_type = request.context.get("document_type", "unknown")
        current_section = request.context.get("current_section", {})
        current_section_str = current_section.get("title", "unknown") if isinstance(current_section, dict) else str(current_section)
        selected_text = request.context.get("selected_text", "")
        document_structure = request.context.get("document_structure", {})
        
        # Format the prompt with context
        try:
            formatted_prompt = prompt_template.format(
                context=context_str,
                document_type=document_type,
                current_section=current_section_str,
                selected_text=selected_text,
                document_structure=str(document_structure),
                user_prompt=request.user_prompt or ""
            )
        except KeyError as e:
            # Fallback to basic formatting if template variables are missing
            self.logger.warning(f"Prompt formatting failed: {e}, using basic template")
            formatted_prompt = f"""
Generate {request.content_type.value} content for this document.
Context: {context_str}
User request: {request.user_prompt or 'Generate appropriate content'}
Generation mode: {request.generation_mode.value}

Please generate professional, contextually appropriate content.
            """.strip()
        
        return formatted_prompt
    
    def _format_context_for_prompt(self, context: Dict[str, Any]) -> str:
        """Format context information for inclusion in prompts."""
        context_parts = []
        
        # Document information
        if context.get("document_title"):
            context_parts.append(f"Document: {context['document_title']}")
        
        # Cursor position
        cursor_pos = context.get("cursor_position", {})
        if cursor_pos:
            context_parts.append(f"Position: paragraph {cursor_pos.get('paragraph', 'unknown')}")
        
        # Selected text
        selected_text = context.get("selected_text", "")
        if selected_text and len(selected_text) > 0:
            # Truncate if too long
            if len(selected_text) > 200:
                selected_text = selected_text[:200] + "..."
            context_parts.append(f"Selected: \"{selected_text}\"")
        
        # Document structure
        doc_structure = context.get("document_structure", {})
        if doc_structure.get("paragraphs"):
            context_parts.append(f"Document has {doc_structure['paragraphs']} paragraphs")
        
        return "; ".join(context_parts) if context_parts else "No specific context available"
    
    async def rewrite_content(self, 
                            content: str, 
                            rewrite_instructions: str, 
                            context: Dict[str, Any] = None) -> GeneratedContent:
        """
        Rewrite existing content based on specific instructions.
        
        Args:
            content: Original content to rewrite
            rewrite_instructions: Specific instructions for how to rewrite
            context: Optional context for the rewrite operation
            
        Returns:
            GeneratedContent with rewritten text
        """
        start_time = time.time()
        request_id = f"rewrite_{int(time.time() * 1000)}"
        
        # Build rewrite prompt
        rewrite_prompt = f"""
Rewrite the following content according to the specific instructions provided.

Original content:
{content}

Rewrite instructions:
{rewrite_instructions}

Context: {self._format_context_for_prompt(context or {})}

Requirements:
- Maintain the core message and factual accuracy
- Apply the rewrite instructions precisely
- Ensure professional tone and language
- Preserve any technical or financial terminology
- Keep the content length appropriate

Provide only the rewritten content, no additional commentary.
        """.strip()
        
        try:
            # Use LLM to rewrite content
            if hasattr(self.llm_client, 'generate_text_completion'):
                llm_response = await self.llm_client.generate_text_completion(
                    prompt=rewrite_prompt,
                    max_tokens=1000
                )
            else:
                # Fallback to content generation method
                llm_response = await self.llm_client.generate_content_suggestions(
                    content_type="text_rewrite",
                    context={"custom_prompt": rewrite_prompt, "original_content": content}
                )
            
            if llm_response.success:
                # Handle both JSON and plain text responses
                try:
                    content_data = json.loads(llm_response.content)
                    rewritten_text = content_data.get("generated_content", content_data.get("content", content_data.get("text", llm_response.content)))
                except json.JSONDecodeError:
                    rewritten_text = llm_response.content
                
                rewritten_text = rewritten_text.strip()
                
                # Calculate quality score for rewritten content
                quality_score = self._calculate_rewrite_quality(content, rewritten_text, rewrite_instructions)
                
                return GeneratedContent(
                    request_id=request_id,
                    content_type=ContentType.TEXT_PARAGRAPH,
                    generated_text=rewritten_text,
                    metadata={
                        "operation_type": "content_rewrite",
                        "original_length": len(content.split()),
                        "rewritten_length": len(rewritten_text.split()),
                        "instructions": rewrite_instructions
                    },
                    quality_score=quality_score,
                    word_count=len(rewritten_text.split()),
                    execution_time_ms=(time.time() - start_time) * 1000,
                    success=True
                )
            else:
                return GeneratedContent(
                    request_id=request_id,
                    content_type=ContentType.TEXT_PARAGRAPH,
                    generated_text="",
                    execution_time_ms=(time.time() - start_time) * 1000,
                    success=False,
                    error_message=f"Content rewrite failed: {llm_response.error}"
                )
                
        except Exception as e:
            return GeneratedContent(
                request_id=request_id,
                content_type=ContentType.TEXT_PARAGRAPH,
                generated_text="",
                execution_time_ms=(time.time() - start_time) * 1000,
                success=False,
                error_message=f"Content rewrite error: {str(e)}"
            )
    
    async def enhance_content(self, 
                            content: str, 
                            enhancement_type: str = "professional", 
                            context: Dict[str, Any] = None) -> GeneratedContent:
        """
        Enhance existing content for better quality, clarity, and professionalism.
        
        Args:
            content: Original content to enhance
            enhancement_type: Type of enhancement (professional, clarity, conciseness, detail)
            context: Optional context for enhancement
            
        Returns:
            GeneratedContent with enhanced text
        """
        start_time = time.time()
        request_id = f"enhance_{int(time.time() * 1000)}"
        
        # Enhancement type specific instructions
        enhancement_instructions = {
            "professional": "Enhance for professional tone, formal language, and business communication standards",
            "clarity": "Improve clarity, readability, and logical flow while maintaining all key information",
            "conciseness": "Make more concise while preserving all essential information and meaning",
            "detail": "Add appropriate detail, examples, and supporting information for better comprehension",
            "financial": "Enhance for financial document standards with precise terminology and professional presentation"
        }
        
        instructions = enhancement_instructions.get(enhancement_type, enhancement_instructions["professional"])
        
        # Build enhancement prompt
        enhancement_prompt = f"""
Enhance the following content according to the specified enhancement type.

Original content:
{content}

Enhancement type: {enhancement_type}
Enhancement instructions: {instructions}

Context: {self._format_context_for_prompt(context or {})}

Requirements:
- Maintain all factual accuracy and core information
- Apply the specified enhancement type consistently
- Ensure the enhanced version is clearly better than the original
- Preserve any technical terminology and data
- Keep appropriate length for the content type

Provide only the enhanced content, no additional commentary.
        """.strip()
        
        try:
            # Use LLM to enhance content
            if hasattr(self.llm_client, 'generate_text_completion'):
                llm_response = await self.llm_client.generate_text_completion(
                    prompt=enhancement_prompt,
                    max_tokens=1200
                )
            else:
                # Fallback to content generation method
                llm_response = await self.llm_client.generate_content_suggestions(
                    content_type="text_enhancement",
                    context={"custom_prompt": enhancement_prompt, "original_content": content}
                )
            
            if llm_response.success:
                # Handle both JSON and plain text responses
                try:
                    content_data = json.loads(llm_response.content)
                    enhanced_text = content_data.get("generated_content", content_data.get("content", content_data.get("text", llm_response.content)))
                except json.JSONDecodeError:
                    enhanced_text = llm_response.content
                
                enhanced_text = enhanced_text.strip()
                
                # Calculate enhancement quality score
                quality_score = self._calculate_enhancement_quality(content, enhanced_text, enhancement_type)
                
                return GeneratedContent(
                    request_id=request_id,
                    content_type=ContentType.TEXT_PARAGRAPH,
                    generated_text=enhanced_text,
                    metadata={
                        "operation_type": "content_enhancement",
                        "enhancement_type": enhancement_type,
                        "original_length": len(content.split()),
                        "enhanced_length": len(enhanced_text.split()),
                        "improvement_ratio": len(enhanced_text.split()) / max(len(content.split()), 1)
                    },
                    quality_score=quality_score,
                    word_count=len(enhanced_text.split()),
                    execution_time_ms=(time.time() - start_time) * 1000,
                    success=True
                )
            else:
                return GeneratedContent(
                    request_id=request_id,
                    content_type=ContentType.TEXT_PARAGRAPH,
                    generated_text="",
                    execution_time_ms=(time.time() - start_time) * 1000,
                    success=False,
                    error_message=f"Content enhancement failed: {llm_response.error}"
                )
                
        except Exception as e:
            return GeneratedContent(
                request_id=request_id,
                content_type=ContentType.TEXT_PARAGRAPH,
                generated_text="",
                execution_time_ms=(time.time() - start_time) * 1000,
                success=False,
                error_message=f"Content enhancement error: {str(e)}"
            )
    
    def _calculate_rewrite_quality(self, original: str, rewritten: str, instructions: str) -> float:
        """Calculate quality score for rewritten content."""
        if not rewritten or not original:
            return 0.0
        
        score = 0.0
        original_words = original.split()
        rewritten_words = rewritten.split()
        
        # Length appropriateness (0.25 weight)
        length_ratio = len(rewritten_words) / max(len(original_words), 1)
        if 0.8 <= length_ratio <= 1.5:  # Reasonable length change
            score += 0.25
        elif 0.5 <= length_ratio <= 2.0:  # Still acceptable
            score += 0.15
        
        # Instruction following (0.35 weight) - heuristic based on keywords
        instruction_lower = instructions.lower()
        rewritten_lower = rewritten.lower()
        
        if "professional" in instruction_lower:
            professional_indicators = ["furthermore", "however", "consequently", "therefore"]
            if any(word in rewritten_lower for word in professional_indicators):
                score += 0.35
        elif "concise" in instruction_lower or "brief" in instruction_lower:
            if len(rewritten_words) < len(original_words):
                score += 0.35
        elif "detail" in instruction_lower or "expand" in instruction_lower:
            if len(rewritten_words) > len(original_words):
                score += 0.35
        else:
            score += 0.25  # Default for following instructions
        
        # Content completeness (0.25 weight)
        if rewritten.strip().endswith(('.', '!', '?')):
            score += 0.125
        if len(rewritten_words) >= 5:  # Has substance
            score += 0.125
        
        # Coherence (0.15 weight)
        sentences = rewritten.count('.') + rewritten.count('!') + rewritten.count('?')
        if sentences > 0:
            avg_words_per_sentence = len(rewritten_words) / sentences
            if 5 <= avg_words_per_sentence <= 30:
                score += 0.15
        
        return min(score, 1.0)
    
    def _calculate_enhancement_quality(self, original: str, enhanced: str, enhancement_type: str) -> float:
        """Calculate quality score for enhanced content."""
        if not enhanced or not original:
            return 0.0
        
        score = 0.0
        original_words = original.split()
        enhanced_words = enhanced.split()
        
        # Basic quality metrics (0.3 weight)
        if enhanced.strip().endswith(('.', '!', '?')):
            score += 0.15
        if len(enhanced_words) >= max(3, len(original_words) * 0.8):
            score += 0.15
        
        # Enhancement type specific scoring (0.4 weight)
        enhanced_lower = enhanced.lower()
        
        if enhancement_type == "professional":
            professional_words = ["furthermore", "however", "consequently", "therefore", "additionally"]
            professional_count = sum(1 for word in professional_words if word in enhanced_lower)
            score += min(0.4, professional_count * 0.1)
        elif enhancement_type == "clarity":
            # Check for clear structure and transitions
            transition_words = ["first", "second", "furthermore", "in addition", "however", "therefore"]
            clarity_score = sum(1 for word in transition_words if word in enhanced_lower)
            score += min(0.4, clarity_score * 0.08)
        elif enhancement_type == "conciseness":
            # Reward for being more concise while maintaining content
            if len(enhanced_words) <= len(original_words) and len(enhanced_words) >= len(original_words) * 0.7:
                score += 0.4
        elif enhancement_type == "detail":
            # Reward for adding appropriate detail
            if len(enhanced_words) > len(original_words) * 1.2:
                score += 0.4
        elif enhancement_type == "financial":
            # Check for financial terminology
            financial_terms = ["revenue", "profit", "margin", "growth", "analysis", "performance", "metrics"]
            financial_score = sum(1 for term in financial_terms if term in enhanced_lower)
            score += min(0.4, financial_score * 0.06)
        
        # Improvement over original (0.3 weight)
        # Heuristic: longer content with good structure indicates enhancement
        structure_indicators = enhanced.count('.') + enhanced.count(',') + enhanced.count(';')
        original_structure = original.count('.') + original.count(',') + original.count(';')
        
        if structure_indicators >= original_structure:
            score += 0.15
        if len(enhanced_words) != len(original_words):  # Content was actually changed
            score += 0.15
        
        return min(score, 1.0)
    
    async def generate_structured_financial_content(self, 
                                                   structure_type: str,
                                                   financial_data: Dict[str, Any] = None,
                                                   context: Dict[str, Any] = None) -> GeneratedContent:
        """
        Generate structured content specifically for financial documents.
        
        Args:
            structure_type: Type of structure (executive_summary, financial_analysis, 
                          quarterly_report, investment_thesis, risk_analysis)
            financial_data: Financial data to incorporate
            context: Document context
            
        Returns:
            GeneratedContent with structured financial content
        """
        start_time = time.time()
        request_id = f"structured_{int(time.time() * 1000)}"
        
        # Financial structure templates
        structure_templates = {
            "executive_summary": {
                "sections": ["Company Overview", "Financial Highlights", "Key Achievements", "Outlook"],
                "word_target": 200,
                "tone": "executive"
            },
            "financial_analysis": {
                "sections": ["Revenue Analysis", "Profitability Analysis", "Efficiency Metrics", "Growth Trends"],
                "word_target": 400,
                "tone": "analytical"
            },
            "quarterly_report": {
                "sections": ["Quarterly Highlights", "Revenue Performance", "Operating Metrics", "Balance Sheet Summary", "Forward Guidance"],
                "word_target": 600,
                "tone": "formal"
            },
            "investment_thesis": {
                "sections": ["Investment Highlights", "Market Position", "Financial Strengths", "Growth Catalysts", "Risk Factors"],
                "word_target": 500,
                "tone": "persuasive"
            },
            "risk_analysis": {
                "sections": ["Market Risks", "Operational Risks", "Financial Risks", "Regulatory Risks", "Mitigation Strategies"],
                "word_target": 350,
                "tone": "cautious"
            }
        }
        
        template = structure_templates.get(structure_type, structure_templates["financial_analysis"])
        
        # Build structured content prompt
        financial_data_str = self._format_financial_data(financial_data or {})
        context_str = self._format_context_for_prompt(context or {})
        
        structured_prompt = f"""
Generate a structured {structure_type} for a financial document with the following sections:

Required Sections: {', '.join(template['sections'])}
Target Length: {template['word_target']} words
Tone: {template['tone']}

Financial Data Available:
{financial_data_str}

Document Context:
{context_str}

Structure Requirements:
- Use clear section headings
- Maintain professional financial language
- Include specific metrics and data points where available
- Ensure logical flow between sections
- Use bullet points where appropriate for clarity
- Include quantitative analysis where possible

Generate the complete structured content with proper headings and formatting.
        """.strip()
        
        try:
            # Generate structured content using LLM
            if hasattr(self.llm_client, 'generate_text_completion'):
                llm_response = await self.llm_client.generate_text_completion(
                    prompt=structured_prompt,
                    max_tokens=1500
                )
            else:
                # Fallback to content generation method
                llm_response = await self.llm_client.generate_content_suggestions(
                    content_type=structure_type,
                    context={"custom_prompt": structured_prompt, "financial_data": financial_data}
                )
            
            if llm_response.success:
                # Handle both JSON and plain text responses
                try:
                    content_data = json.loads(llm_response.content)
                    structured_text = content_data.get("generated_content", content_data.get("content", content_data.get("text", llm_response.content)))
                    
                    # Handle case where structured_text might be a dict
                    if isinstance(structured_text, dict):
                        # Try to extract text from common structure keys
                        for key in ["content", "text", "generated_content", "summary"]:
                            if key in structured_text:
                                structured_text = structured_text[key]
                                break
                        else:
                            # If no standard key found, convert dict to string
                            structured_text = str(structured_text)
                            
                except json.JSONDecodeError:
                    structured_text = llm_response.content
                
                # Ensure structured_text is a string
                if not isinstance(structured_text, str):
                    structured_text = str(structured_text)
                    
                structured_text = structured_text.strip()
                
                # Calculate structure quality score
                quality_score = self._calculate_structure_quality(structured_text, template)
                
                return GeneratedContent(
                    request_id=request_id,
                    content_type=ContentType.FINANCIAL_ANALYSIS,
                    generated_text=structured_text,
                    metadata={
                        "operation_type": "structured_financial_content",
                        "structure_type": structure_type,
                        "sections_required": template['sections'],
                        "target_words": template['word_target'],
                        "tone": template['tone'],
                        "financial_data_used": bool(financial_data)
                    },
                    quality_score=quality_score,
                    word_count=len(structured_text.split()),
                    execution_time_ms=(time.time() - start_time) * 1000,
                    success=True
                )
            else:
                return GeneratedContent(
                    request_id=request_id,
                    content_type=ContentType.FINANCIAL_ANALYSIS,
                    generated_text="",
                    execution_time_ms=(time.time() - start_time) * 1000,
                    success=False,
                    error_message=f"Structured content generation failed: {llm_response.error}"
                )
                
        except Exception as e:
            return GeneratedContent(
                request_id=request_id,
                content_type=ContentType.FINANCIAL_ANALYSIS,
                generated_text="",
                execution_time_ms=(time.time() - start_time) * 1000,
                success=False,
                error_message=f"Structured content generation error: {str(e)}"
            )
    
    def _format_financial_data(self, financial_data: Dict[str, Any]) -> str:
        """Format financial data for inclusion in prompts."""
        if not financial_data:
            return "No specific financial data provided"
        
        formatted_parts = []
        
        # Revenue data
        if 'revenue' in financial_data:
            formatted_parts.append(f"Revenue: {financial_data['revenue']}")
        
        # Profitability data
        for metric in ['gross_margin', 'operating_margin', 'net_margin']:
            if metric in financial_data:
                formatted_parts.append(f"{metric.replace('_', ' ').title()}: {financial_data[metric]}")
        
        # Growth data
        for metric in ['revenue_growth', 'profit_growth', 'market_growth']:
            if metric in financial_data:
                formatted_parts.append(f"{metric.replace('_', ' ').title()}: {financial_data[metric]}")
        
        # Key metrics
        if 'kpis' in financial_data:
            kpis = financial_data['kpis']
            if isinstance(kpis, dict):
                for kpi, value in kpis.items():
                    formatted_parts.append(f"{kpi}: {value}")
        
        # Market data
        if 'market_data' in financial_data:
            formatted_parts.append(f"Market Information: {financial_data['market_data']}")
        
        return "; ".join(formatted_parts) if formatted_parts else "Basic financial context provided"
    
    def _calculate_structure_quality(self, content: str, template: Dict[str, Any]) -> float:
        """Calculate quality score for structured content."""
        if not content:
            return 0.0
        
        score = 0.0
        content_lower = content.lower()
        word_count = len(content.split())
        
        # Section coverage (0.4 weight)
        required_sections = template.get('sections', [])
        sections_found = 0
        for section in required_sections:
            # Check if section title or keywords appear in content
            section_lower = section.lower()
            if section_lower in content_lower or any(word in content_lower for word in section_lower.split()):
                sections_found += 1
        
        section_coverage = sections_found / max(len(required_sections), 1)
        score += 0.4 * section_coverage
        
        # Length appropriateness (0.2 weight)
        target_words = template.get('word_target', 300)
        length_ratio = word_count / target_words
        if 0.8 <= length_ratio <= 1.3:  # Within reasonable range
            score += 0.2
        elif 0.6 <= length_ratio <= 1.6:  # Still acceptable
            score += 0.1
        
        # Financial terminology (0.2 weight)
        financial_terms = ['revenue', 'profit', 'margin', 'growth', 'performance', 'analysis', 
                          'metrics', 'earnings', 'cash flow', 'return', 'assets', 'liability']
        financial_score = sum(1 for term in financial_terms if term in content_lower)
        score += min(0.2, financial_score * 0.02)
        
        # Structure quality (0.2 weight)
        # Check for proper structure indicators
        structure_indicators = content.count('#') + content.count('*') + content.count('-')  # Headings/bullets
        paragraphs = content.count('\n\n') + 1
        
        if structure_indicators >= len(required_sections):  # Has structured elements
            score += 0.1
        if paragraphs >= 3:  # Has multiple paragraphs
            score += 0.1
        
        return min(score, 1.0)
    
    def _initialize_quality_metrics(self) -> Dict[str, Any]:
        """Initialize quality assessment metrics."""
        return {
            "min_word_count": 5,
            "max_word_count": 1000,
            "sentence_length_range": (8, 25),
            "professional_indicators": [
                "however", "therefore", "furthermore", "additionally",
                "consequently", "nevertheless", "moreover", "specifically"
            ],
            "quality_thresholds": {
                "excellent": 0.8,
                "good": 0.6,
                "acceptable": 0.4,
                "poor": 0.2
            }
        }