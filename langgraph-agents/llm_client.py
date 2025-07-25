"""
LLM Client Manager for LibreOffice AI Writing Assistant

This module provides a centralized interface for LLM interactions across all agents,
supporting multiple providers (OpenAI, Anthropic) with consistent API and error handling.
"""

import asyncio
import time
import logging
from typing import Dict, Any, List, Optional, Union
from dataclasses import dataclass
from enum import Enum
import json

try:
    import openai
except ImportError:
    openai = None

try:
    import anthropic
except ImportError:
    anthropic = None

from config import get_config


class LLMProvider(Enum):
    """Supported LLM providers."""
    OPENAI = "openai"
    ANTHROPIC = "anthropic"
    

class AnalysisMode(Enum):
    """Analysis complexity modes for different prompt strategies."""
    LIGHTWEIGHT = "lightweight"
    FOCUSED = "focused"
    COMPREHENSIVE = "comprehensive"


@dataclass
class LLMResponse:
    """Standardized LLM response container."""
    content: str
    success: bool
    provider: LLMProvider
    model: str
    tokens_used: Optional[int] = None
    execution_time_ms: float = 0.0
    error: Optional[str] = None
    metadata: Dict[str, Any] = None


class LLMClient:
    """
    Unified LLM client for consistent AI interactions across all agents.
    
    Provides automatic fallback between providers, rate limiting, caching,
    and standardized prompt templates for different analysis types.
    """
    
    def __init__(self):
        """Initialize LLM client with configuration."""
        self.config = get_config()
        self.logger = logging.getLogger(__name__)
        
        # Initialize clients based on available API keys
        self.openai_client = None
        self.anthropic_client = None
        self.primary_provider = None
        
        self._setup_clients()
        
        # Rate limiting
        self._last_request_time = 0.0
        self._min_request_interval = 60.0 / self.config.api.api_requests_per_minute
        
    def _setup_clients(self):
        """Set up LLM clients based on available API keys."""
        if self.config.api.openai_api_key and openai:
            try:
                self.openai_client = openai.OpenAI(
                    api_key=self.config.api.openai_api_key
                )
                self.primary_provider = LLMProvider.OPENAI
                self.logger.info("OpenAI client initialized successfully")
            except Exception as e:
                self.logger.error(f"Failed to initialize OpenAI client: {e}")
        
        if self.config.api.anthropic_api_key and anthropic:
            try:
                self.anthropic_client = anthropic.Anthropic(
                    api_key=self.config.api.anthropic_api_key
                )
                if not self.primary_provider:
                    self.primary_provider = LLMProvider.ANTHROPIC
                self.logger.info("Anthropic client initialized successfully")
            except Exception as e:
                self.logger.error(f"Failed to initialize Anthropic client: {e}")
        
        if not self.primary_provider:
            raise ValueError("No LLM provider available. Please set OPENAI_API_KEY or ANTHROPIC_API_KEY")
    
    async def analyze_document_context(self, 
                                     document_state: Dict[str, Any],
                                     analysis_mode: AnalysisMode,
                                     user_message: Optional[str] = None) -> LLMResponse:
        """
        Analyze document context using LLM with mode-specific prompts.
        
        Args:
            document_state: Current document state information
            analysis_mode: Analysis complexity level
            user_message: Optional user request for analysis
            
        Returns:
            LLMResponse with analysis results
        """
        prompt = self._build_context_analysis_prompt(document_state, analysis_mode, user_message)
        
        return await self._make_llm_request(
            prompt=prompt,
            max_tokens=self._get_max_tokens_for_mode(analysis_mode),
            temperature=0.3,  # Lower temperature for more consistent analysis
            system_message=self._get_system_message_for_context_analysis()
        )
    
    async def route_user_request(self, 
                               user_message: str,
                               document_state: Dict[str, Any]) -> LLMResponse:
        """
        Route user request to appropriate agent workflow.
        
        Args:
            user_message: User's natural language request
            document_state: Current document context
            
        Returns:
            LLMResponse with routing decision and reasoning
        """
        prompt = self._build_routing_prompt(user_message, document_state)
        
        return await self._make_llm_request(
            prompt=prompt,
            max_tokens=500,
            temperature=0.1,  # Very low temperature for consistent routing
            system_message=self._get_system_message_for_routing()
        )
    
    async def generate_content_suggestions(self,
                                         content_type: str,
                                         context: Dict[str, Any]) -> LLMResponse:
        """
        Generate content suggestions based on document context.
        
        Args:
            content_type: Type of content to generate (text, table, chart, etc.)
            context: Contextual information for generation
            
        Returns:
            LLMResponse with content suggestions
        """
        prompt = self._build_content_generation_prompt(content_type, context)
        
        return await self._make_llm_request(
            prompt=prompt,
            max_tokens=1000,
            temperature=0.7,  # Higher temperature for creative content
            system_message=self._get_system_message_for_content_generation()
        )
    
    async def _make_llm_request(self,
                              prompt: str,
                              max_tokens: int = 1000,
                              temperature: float = 0.5,
                              system_message: Optional[str] = None) -> LLMResponse:
        """
        Make LLM request with automatic provider fallback and rate limiting.
        
        Args:
            prompt: The prompt to send to the LLM
            max_tokens: Maximum tokens to generate
            temperature: Response randomness (0.0-1.0)
            system_message: Optional system message
            
        Returns:
            LLMResponse with results
        """
        start_time = time.time()
        
        # Rate limiting
        await self._apply_rate_limit()
        
        # Try primary provider first
        response = await self._try_provider(
            self.primary_provider, prompt, max_tokens, temperature, system_message
        )
        
        if response.success:
            response.execution_time_ms = (time.time() - start_time) * 1000
            return response
        
        # Fallback to secondary provider
        fallback_provider = (
            LLMProvider.ANTHROPIC if self.primary_provider == LLMProvider.OPENAI 
            else LLMProvider.OPENAI
        )
        
        if self._is_provider_available(fallback_provider):
            self.logger.warning(f"Primary provider failed, falling back to {fallback_provider.value}")
            response = await self._try_provider(
                fallback_provider, prompt, max_tokens, temperature, system_message
            )
        
        response.execution_time_ms = (time.time() - start_time) * 1000
        return response
    
    async def _try_provider(self,
                          provider: LLMProvider,
                          prompt: str,
                          max_tokens: int,
                          temperature: float,
                          system_message: Optional[str]) -> LLMResponse:
        """Try making request with specific provider."""
        try:
            if provider == LLMProvider.OPENAI and self.openai_client:
                return await self._call_openai(prompt, max_tokens, temperature, system_message)
            elif provider == LLMProvider.ANTHROPIC and self.anthropic_client:
                return await self._call_anthropic(prompt, max_tokens, temperature, system_message)
            else:
                return LLMResponse(
                    content="",
                    success=False,
                    provider=provider,
                    model="unavailable",
                    error=f"Provider {provider.value} not available"
                )
        except Exception as e:
            self.logger.error(f"LLM request failed with {provider.value}: {e}")
            return LLMResponse(
                content="",
                success=False,
                provider=provider,
                model="error",
                error=str(e)
            )
    
    async def _call_openai(self,
                         prompt: str,
                         max_tokens: int,
                         temperature: float,
                         system_message: Optional[str]) -> LLMResponse:
        """Make OpenAI API call."""
        messages = []
        
        if system_message:
            messages.append({"role": "system", "content": system_message})
        
        messages.append({"role": "user", "content": prompt})
        
        response = await asyncio.to_thread(
            self.openai_client.chat.completions.create,
            model=self.config.api.default_model,
            messages=messages,
            max_tokens=max_tokens,
            temperature=temperature
        )
        
        return LLMResponse(
            content=response.choices[0].message.content,
            success=True,
            provider=LLMProvider.OPENAI,
            model=self.config.api.default_model,
            tokens_used=response.usage.total_tokens if response.usage else None
        )
    
    async def _call_anthropic(self,
                            prompt: str,
                            max_tokens: int,
                            temperature: float,
                            system_message: Optional[str]) -> LLMResponse:
        """Make Anthropic API call."""
        # Anthropic API implementation would go here
        # For now, return error since we're focusing on OpenAI
        return LLMResponse(
            content="",
            success=False,
            provider=LLMProvider.ANTHROPIC,
            model="claude-3-sonnet",
            error="Anthropic integration not yet implemented"
        )
    
    def _is_provider_available(self, provider: LLMProvider) -> bool:
        """Check if provider is available."""
        if provider == LLMProvider.OPENAI:
            return self.openai_client is not None
        elif provider == LLMProvider.ANTHROPIC:
            return self.anthropic_client is not None
        return False
    
    async def _apply_rate_limit(self):
        """Apply rate limiting between requests."""
        current_time = time.time()
        time_since_last = current_time - self._last_request_time
        
        if time_since_last < self._min_request_interval:
            sleep_time = self._min_request_interval - time_since_last
            await asyncio.sleep(sleep_time)
        
        self._last_request_time = time.time()
    
    def _build_context_analysis_prompt(self,
                                     document_state: Dict[str, Any],
                                     analysis_mode: AnalysisMode,
                                     user_message: Optional[str]) -> str:
        """Build context analysis prompt based on mode and state."""
        base_context = f"""
Document Context:
- Title: {document_state.get('current_document', {}).get('title', 'Unknown')}
- Path: {document_state.get('current_document', {}).get('path', 'Unknown')}
- Cursor Position: Paragraph {document_state.get('cursor_position', {}).get('paragraph', 0)}, Character {document_state.get('cursor_position', {}).get('character', 0)}
- Selected Text: "{document_state.get('selected_text', '')}"
- Document Structure: {document_state.get('document_structure', {})}
"""
        
        if user_message:
            base_context += f"\nUser Request: {user_message}"
        
        if analysis_mode == AnalysisMode.LIGHTWEIGHT:
            return f"""{base_context}

Provide a lightweight analysis focusing on:
1. Current cursor context and immediate surroundings
2. Basic document navigation information
3. Simple contextual information for user operations

Return your analysis as JSON with keys: cursor_context, navigation_info, analysis_mode."""
        
        elif analysis_mode == AnalysisMode.FOCUSED:
            return f"""{base_context}

Provide a focused analysis including:
1. Document structure analysis and section context
2. Content relationships and cross-references
3. Formatting and style context
4. Relevant suggestions for user workflow

Return your analysis as JSON with keys: document_structure, content_relationships, formatting_context, suggestions, analysis_mode."""
        
        else:  # COMPREHENSIVE
            return f"""{base_context}

Provide a comprehensive analysis including:
1. Complete document semantic understanding
2. Content themes, topics, and purpose analysis
3. Structural relationships and document flow
4. Contextual recommendations and insights
5. Potential improvements and optimizations

Return your analysis as JSON with keys: semantic_analysis, content_themes, structural_analysis, recommendations, optimization_suggestions, analysis_mode."""
    
    def _build_routing_prompt(self, user_message: str, document_state: Dict[str, Any]) -> str:
        """Build routing prompt for user request classification."""
        return f"""
User Request: "{user_message}"

Document Context:
- Current document: {document_state.get('current_document', {}).get('title', 'Unknown')}
- Cursor position: Paragraph {document_state.get('cursor_position', {}).get('paragraph', 0)}
- Selected text: "{document_state.get('selected_text', '')}"

Available Agents:
1. ContextAnalysisAgent - Document analysis, cursor tracking, semantic understanding
2. ContentGenerationAgent - Text generation, writing assistance, content creation
3. FormattingAgent - Document formatting, styling, layout optimization
4. ResearchAgent - External data integration, fact-checking, citations
5. FinancialAgent - Financial calculations, data analysis, reporting

Analyze the user request and determine:
1. Which agent(s) should handle this request
2. What type of operation this is (simple, moderate, complex)
3. Any special requirements or constraints

Return your routing decision as JSON with keys: primary_agent, secondary_agents, operation_complexity, reasoning, estimated_time_seconds.
"""
    
    def _build_content_generation_prompt(self, content_type: str, context: Dict[str, Any]) -> str:
        """Build content generation prompt."""
        return f"""
Content Type: {content_type}
Context: {json.dumps(context, indent=2)}

Generate appropriate content based on the type and context provided.
Consider the document's current style, tone, and purpose.

Return your response as JSON with keys: generated_content, style_notes, suggestions, content_type.
"""
    
    def _get_system_message_for_context_analysis(self) -> str:
        """Get system message for context analysis."""
        return """You are an expert document analysis assistant for LibreOffice. You specialize in understanding document structure, content relationships, and user context. Always return well-structured JSON responses with the requested keys. Be precise and focus on actionable insights."""
    
    def _get_system_message_for_routing(self) -> str:
        """Get system message for request routing."""
        return """You are an intelligent request router for a LibreOffice AI writing assistant. Analyze user requests and route them to the most appropriate agent(s). Consider the complexity, required capabilities, and expected response time. Always return valid JSON with clear reasoning."""
    
    def _get_system_message_for_content_generation(self) -> str:
        """Get system message for content generation."""
        return """You are a professional writing assistant specializing in document content generation. Create high-quality, contextually appropriate content that matches the document's style and purpose. Always return valid JSON responses."""
    
    def _get_max_tokens_for_mode(self, mode: AnalysisMode) -> int:
        """Get appropriate token limit for analysis mode."""
        return {
            AnalysisMode.LIGHTWEIGHT: 300,
            AnalysisMode.FOCUSED: 800,
            AnalysisMode.COMPREHENSIVE: 1500
        }.get(mode, 500)


# Global LLM client instance
_llm_client = None

def get_llm_client() -> LLMClient:
    """Get the global LLM client instance."""
    global _llm_client
    if _llm_client is None:
        _llm_client = LLMClient()
    return _llm_client