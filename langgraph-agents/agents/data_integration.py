"""
DataIntegrationAgent - External Data Specialist for Financial APIs

This agent specializes in fetching, processing, and integrating financial data from 
external APIs including Alpha Vantage, Yahoo Finance, and Bloomberg. It provides 
intelligent data retrieval with parallel processing, fallback strategies, comprehensive 
validation, and document-ready formatting.

Key Capabilities:
- Multi-source financial data aggregation with parallel processing
- Intelligent fallback mechanisms for API failures
- Comprehensive data validation and quality assurance
- Real-time and historical financial data integration
- Document-ready formatting with proper attribution
- Rate limiting and caching for optimal performance
- Circuit breaker patterns for service degradation handling
"""

import asyncio
import time
import logging
import json
from datetime import datetime, timezone, timedelta
from typing import Any, Dict, List, Optional, Union, Tuple, Set
from dataclasses import dataclass, field
from enum import Enum
from concurrent.futures import ThreadPoolExecutor
import traceback

# Import base agent classes
from .base import (
    BaseAgent, 
    AgentCapability, 
    AgentResult, 
    AgentError,
    ValidationResult,
    PerformanceMetrics
)

# Import shared caching system
from .shared_cache import SharedCacheMixin, CacheType, InvalidationTrigger

# Import financial data components
from .credential_manager import get_credential_manager, CredentialProvider
from .financial_apis import (
    get_financial_client, DataType, TimeInterval, FinancialDataRequest,
    StockQuote, HistoricalData, CompanyFundamentals, MarketIndex, APIResponse
)
from .rate_limiter import get_rate_limiter, get_cache, RateLimitConfig, CacheConfig

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


class DataIntegrationMode(Enum):
    """Modes for data integration operations."""
    REAL_TIME = "real_time"           # Latest available data
    HISTORICAL = "historical"         # Historical data analysis
    COMPARATIVE = "comparative"       # Multi-symbol comparison
    RESEARCH = "research"             # Research and analysis
    MONITORING = "monitoring"         # Continuous monitoring


class DataQuality(Enum):
    """Data quality levels."""
    HIGH = "high"                     # High quality, recent data
    MEDIUM = "medium"                 # Acceptable quality with minor issues
    LOW = "low"                       # Low quality, may have significant issues
    UNRELIABLE = "unreliable"         # Data not suitable for use


@dataclass
class DataIntegrationRequest:
    """Request for financial data integration."""
    request_id: str
    mode: DataIntegrationMode
    symbols: List[str]
    data_types: List[DataType]
    time_range: Optional[Tuple[datetime, datetime]] = None
    providers: List[CredentialProvider] = field(default_factory=list)
    parameters: Dict[str, Any] = field(default_factory=dict)
    context: Dict[str, Any] = field(default_factory=dict)
    priority: str = "medium"  # low, medium, high, critical
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary representation."""
        return {
            "request_id": self.request_id,
            "mode": self.mode.value,
            "symbols": self.symbols,
            "data_types": [dt.value for dt in self.data_types],
            "time_range": [t.isoformat() for t in self.time_range] if self.time_range else None,
            "providers": [p.value for p in self.providers],
            "parameters": self.parameters,
            "context": self.context,
            "priority": self.priority
        }


@dataclass
class IntegratedFinancialData:
    """Container for integrated financial data results."""
    request_id: str
    symbols: List[str]
    data: Dict[str, Any]  # Symbol -> data mapping
    metadata: Dict[str, Any]
    quality: DataQuality
    sources: List[str]
    retrieved_at: float = field(default_factory=time.time)
    attribution: List[Dict[str, Any]] = field(default_factory=list)
    citations: List[str] = field(default_factory=list)
    success: bool = True
    error_message: Optional[str] = None
    
    def get_document_formatted_data(self) -> Dict[str, Any]:
        """Get data formatted for document integration."""
        return {
            "financial_data": self.data,
            "data_summary": self._generate_summary(),
            "attribution": self.attribution,
            "citations": self.citations,
            "quality_score": self._calculate_quality_score(),
            "last_updated": datetime.fromtimestamp(self.retrieved_at).isoformat(),
            "data_sources": self.sources
        }
    
    def _generate_summary(self) -> Dict[str, Any]:
        """Generate summary of integrated data."""
        summary = {
            "symbols_count": len(self.symbols),
            "data_types": list(set(type(v).__name__ for v in self.data.values())),
            "quality": self.quality.value,
            "sources_count": len(self.sources)
        }
        
        # Add specific summaries based on data types
        quotes = [v for v in self.data.values() if isinstance(v, StockQuote)]
        if quotes:
            summary["quotes"] = {
                "count": len(quotes),
                "avg_price": sum(q.price for q in quotes) / len(quotes),
                "total_volume": sum(q.volume for q in quotes)
            }
        
        return summary
    
    def _calculate_quality_score(self) -> float:
        """Calculate overall data quality score (0.0 to 1.0)."""
        base_score = {
            DataQuality.HIGH: 1.0,
            DataQuality.MEDIUM: 0.7,
            DataQuality.LOW: 0.4,
            DataQuality.UNRELIABLE: 0.1
        }.get(self.quality, 0.5)
        
        # Adjust based on data freshness
        age_hours = (time.time() - self.retrieved_at) / 3600
        freshness_factor = max(0.5, 1.0 - (age_hours / 24.0))
        
        return base_score * freshness_factor


@dataclass
class CircuitBreakerState:
    """State for circuit breaker pattern."""
    provider: CredentialProvider
    failure_count: int = 0
    last_failure_time: Optional[float] = None
    state: str = "closed"  # closed, open, half_open
    failure_threshold: int = 5
    recovery_timeout: float = 300.0  # 5 minutes
    
    def should_allow_request(self) -> bool:
        """Check if request should be allowed through circuit breaker."""
        current_time = time.time()
        
        if self.state == "closed":
            return True
        elif self.state == "open":
            if (self.last_failure_time and 
                current_time - self.last_failure_time > self.recovery_timeout):
                self.state = "half_open"
                return True
            return False
        else:  # half_open
            return True
    
    def record_success(self):
        """Record successful request."""
        self.failure_count = 0
        self.state = "closed"
        self.last_failure_time = None
    
    def record_failure(self):
        """Record failed request."""
        self.failure_count += 1
        self.last_failure_time = time.time()
        
        if self.failure_count >= self.failure_threshold:
            self.state = "open"
        elif self.state == "half_open":
            self.state = "open"


class DataIntegrationAgent(SharedCacheMixin, BaseAgent):
    """
    Specialized agent for external financial data integration.
    
    This agent provides comprehensive financial data retrieval capabilities
    with intelligent parallel processing, fallback strategies, and document-ready
    formatting. It integrates with multiple financial data providers while
    maintaining high performance and reliability.
    
    Performance targets:
    - Real-time data retrieval: ≤2 seconds
    - Historical data analysis: ≤5 seconds
    - Multi-source aggregation: ≤3 seconds
    """
    
    def __init__(self, 
                 agent_id: str = "data_integration_agent",
                 config: Optional[Dict[str, Any]] = None):
        """
        Initialize the DataIntegrationAgent.
        
        Args:
            agent_id: Unique identifier for this agent instance
            config: Optional configuration dictionary
        """
        # Define agent capabilities
        capabilities = [
            AgentCapability.DATA_INTEGRATION,
            AgentCapability.DOCUMENT_ANALYSIS
        ]
        
        # Default configuration
        default_config = {
            "max_retries": 3,
            "retry_delay": 1.0,
            "enable_caching": True,
            "cache_ttl_seconds": 300,  # 5 minutes for financial data
            "max_concurrent_requests": 10,
            "request_timeout_seconds": 30,
            "enable_circuit_breaker": True,
            "fallback_enabled": True,
            "data_validation_enabled": True,
            "quality_threshold": 0.7,
            "preferred_providers": [
                CredentialProvider.YAHOO_FINANCE
            ]
        }
        
        # Merge with provided config
        merged_config = {**default_config, **(config or {})}
        
        # Initialize base agent
        super().__init__(
            agent_id=agent_id,
            capabilities=capabilities,
            config=merged_config
        )
        
        # Financial data integration configuration
        self.max_concurrent_requests = self.config.get("max_concurrent_requests", 10)
        self.request_timeout = self.config.get("request_timeout_seconds", 30)
        self.enable_circuit_breaker = self.config.get("enable_circuit_breaker", True)
        self.fallback_enabled = self.config.get("fallback_enabled", True)
        self.data_validation_enabled = self.config.get("data_validation_enabled", True)
        self.quality_threshold = self.config.get("quality_threshold", 0.7)
        self.preferred_providers = [
            CredentialProvider(p) for p in self.config.get("preferred_providers", [])
        ]
        
        # Initialize components
        self.credential_manager = get_credential_manager()
        self.rate_limiter = get_rate_limiter()
        self.cache = get_cache()
        
        # Circuit breaker states
        self.circuit_breakers: Dict[CredentialProvider, CircuitBreakerState] = {}
        
        # Performance tracking
        self.performance_stats = {
            "requests_processed": 0,
            "cache_hits": 0,
            "fallback_activations": 0,
            "circuit_breaker_trips": 0,
            "avg_response_time_ms": 0.0
        }
        
        # Thread pool for parallel processing
        self.thread_pool = ThreadPoolExecutor(max_workers=self.max_concurrent_requests)
        
        self.logger.info(f"DataIntegrationAgent {agent_id} initialized with financial data capabilities")
    
    def get_required_tools(self) -> List[str]:
        """
        Get list of tools required by this agent.
        
        Returns:
            List of tool names required for data integration
        """
        return [
            "financial_data_client",
            "data_validator",
            "format_converter",
            "attribution_generator",
            "citation_manager",
            "quality_assessor"
        ]
    
    async def process(self, 
                     state: DocumentState, 
                     message: Optional[BaseMessage] = None) -> AgentResult:
        """
        Process a financial data integration request.
        
        Args:
            state: Current document state
            message: Optional message containing data request
            
        Returns:
            AgentResult with integrated financial data and state updates
        """
        operation_id = f"data_integration_{int(time.time() * 1000)}"
        start_time = time.time()
        
        try:
            # Extract document ID for shared state coordination
            document_id = state.get("current_document", {}).get("path", "unknown_doc")
            
            # Extract data integration request
            data_request = self._extract_data_request(state, message)
            
            # Validate request
            validation = self._validate_data_request(data_request)
            if not validation.passed:
                error_messages = [issue.get("message", "") for issue in validation.issues if issue.get("level") == "error"]
                return AgentResult(
                    agent_id=self.agent_id,
                    operation_id=operation_id,
                    success=False,
                    error=f"Invalid data request: {'; '.join(error_messages)}",
                    execution_time=time.time() - start_time
                )
            
            # Check shared cache first
            cache_key = self.generate_cache_key(document_id, data_request.request_id)
            cached_result = self.get_cached_agent_result(document_id, cache_key)
            if cached_result and self.cache_enabled:
                self.logger.debug(f"Shared cache hit for data request {data_request.request_id}")
                self.performance_stats["cache_hits"] += 1
                return cached_result
            
            # Get context from other agents
            content_context = self.get_other_agent_context(document_id, "content_generation_agent") 
            formatting_context = self.get_other_agent_context(document_id, "formatting_agent")
            
            # Process data integration with parallel fetching and fallback strategies
            integrated_data = await self._integrate_financial_data(data_request, content_context, formatting_context)
            
            # Create agent result with state updates
            agent_result = self._create_agent_result(operation_id, integrated_data, start_time)
            
            # Cache the result in shared cache
            if self.cache_enabled and integrated_data.success:
                self.cache_agent_result(document_id, cache_key, agent_result)
            
            # Update shared context with data integration results
            self.update_shared_context(document_id, {
                "last_data_integration": {
                    "request_id": data_request.request_id,
                    "symbols": data_request.symbols,
                    "data_types": [dt.value for dt in data_request.data_types],
                    "integrated_at": time.time(),
                    "quality": integrated_data.quality.value,
                    "sources": integrated_data.sources
                },
                "external_data": integrated_data.get_document_formatted_data(),
                "data_integration_state": "completed",
                "financial_data_available": True
            })
            
            # Invalidate related cache when new data is integrated
            self.invalidate_related_cache(document_id, InvalidationTrigger.CONTENT_MODIFIED)
            
            # Update performance stats
            self.performance_stats["requests_processed"] += 1
            response_time = (time.time() - start_time) * 1000
            self.performance_stats["avg_response_time_ms"] = (
                (self.performance_stats["avg_response_time_ms"] * (self.performance_stats["requests_processed"] - 1) + response_time) / 
                self.performance_stats["requests_processed"]
            )
            
            return agent_result
            
        except Exception as e:
            execution_time = time.time() - start_time
            error_msg = f"Data integration failed: {str(e)}"
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
        Validate input for data integration operations.
        
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
            # Check required fields for data integration
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
        
        # Check credential availability
        available_providers = []
        for provider in [CredentialProvider.YAHOO_FINANCE]:
            if self.credential_manager.get_credential(provider):
                available_providers.append(provider.value)
        
        if not available_providers:
            errors.append("No valid financial API credentials found")
        else:
            warnings.append(f"Available providers: {', '.join(available_providers)}")
        
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
                "available_providers": available_providers,
                "validation_time": datetime.now(timezone.utc).isoformat()
            }
        )
    
    async def _integrate_financial_data(self, 
                                      request: DataIntegrationRequest,
                                      content_context: Optional[Dict[str, Any]] = None,
                                      formatting_context: Optional[Dict[str, Any]] = None) -> IntegratedFinancialData:
        """
        Integrate financial data with parallel processing and fallback strategies.
        
        Args:
            request: Data integration request
            content_context: Context from content generation agent
            formatting_context: Context from formatting agent
            
        Returns:
            Integrated financial data with comprehensive results
        """
        start_time = time.time()
        
        try:
            # Determine providers to use
            providers = self._select_providers(request)
            
            if not providers:
                return IntegratedFinancialData(
                    request_id=request.request_id,
                    symbols=request.symbols,
                    data={},
                    metadata={"error": "No available providers"},
                    quality=DataQuality.UNRELIABLE,
                    sources=[],
                    success=False,
                    error_message="No API providers available"
                )
            
            # Process data requests in parallel
            tasks = []
            for symbol in request.symbols:
                for data_type in request.data_types:
                    task = self._fetch_data_with_fallback(
                        symbol, data_type, providers, request
                    )
                    tasks.append(task)
            
            # Execute with concurrency limit
            semaphore = asyncio.Semaphore(self.max_concurrent_requests)
            
            async def limited_fetch(task):
                async with semaphore:
                    return await task
            
            # Wait for all data with timeout
            try:
                results = await asyncio.wait_for(
                    asyncio.gather(*[limited_fetch(task) for task in tasks]),
                    timeout=self.request_timeout
                )
            except asyncio.TimeoutError:
                self.logger.warning(f"Data integration timeout for request {request.request_id}")
                results = []
            
            # Aggregate and validate results
            integrated_data = self._aggregate_results(request, results)
            
            # Apply data validation if enabled
            if self.data_validation_enabled:
                integrated_data = await self._validate_integrated_data(integrated_data)
            
            # Generate attribution and citations
            integrated_data.attribution = self._generate_attribution(integrated_data)
            integrated_data.citations = self._generate_citations(integrated_data, request)
            
            return integrated_data
            
        except Exception as e:
            self.logger.error(f"Data integration failed: {type(e).__name__}")
            return IntegratedFinancialData(
                request_id=request.request_id,
                symbols=request.symbols,
                data={},
                metadata={"error": str(e)},
                quality=DataQuality.UNRELIABLE,
                sources=[],
                success=False,
                error_message=f"Integration failed: {type(e).__name__}"
            )
    
    async def _fetch_data_with_fallback(self, 
                                      symbol: str,
                                      data_type: DataType,
                                      providers: List[CredentialProvider],
                                      request: DataIntegrationRequest) -> Optional[Tuple[str, DataType, Any, str]]:
        """
        Fetch data with fallback strategies across providers.
        
        Args:
            symbol: Financial symbol
            data_type: Type of data to fetch
            providers: List of providers to try
            request: Original request for context
            
        Returns:
            Tuple of (symbol, data_type, data, source) or None if all failed
        """
        for provider in providers:
            try:
                # Check circuit breaker
                if self.enable_circuit_breaker:
                    circuit_breaker = self._get_circuit_breaker(provider)
                    if not circuit_breaker.should_allow_request():
                        self.logger.debug(f"Circuit breaker open for {provider.value}")
                        continue
                
                # Check rate limiting
                if not await self.rate_limiter.acquire(provider):
                    self.logger.debug(f"Rate limited for {provider.value}")
                    continue
                
                # Fetch data from provider
                client = await get_financial_client(provider)
                
                if data_type == DataType.STOCK_QUOTE:
                    response = await client.get_stock_quote(symbol)
                elif data_type == DataType.HISTORICAL_DATA:
                    response = await client.get_historical_data(
                        symbol,
                        interval=TimeInterval.DAILY,
                        start_date=request.time_range[0] if request.time_range else None,
                        end_date=request.time_range[1] if request.time_range else None
                    )
                elif data_type == DataType.COMPANY_FUNDAMENTALS:
                    response = await client.get_company_fundamentals(symbol)
                else:
                    self.logger.warning(f"Unsupported data type: {data_type}")
                    continue
                
                # Record request result
                self.rate_limiter.record_request(provider, response.success, response.response_time_ms)
                
                if response.success and response.data:
                    # Record circuit breaker success
                    if self.enable_circuit_breaker:
                        circuit_breaker.record_success()
                    
                    return (symbol, data_type, response.data, provider.value)
                else:
                    # Record circuit breaker failure
                    if self.enable_circuit_breaker:
                        circuit_breaker.record_failure()
                
            except Exception as e:
                self.logger.warning(f"Failed to fetch {data_type.value} for {symbol} from {provider.value}: {type(e).__name__}")
                
                # Record failures
                self.rate_limiter.record_request(provider, False)
                if self.enable_circuit_breaker:
                    self._get_circuit_breaker(provider).record_failure()
                
                continue
        
        # All providers failed
        if self.fallback_enabled:
            self.performance_stats["fallback_activations"] += 1
            # Try to get cached data as fallback
            cache_key = f"{symbol}_{data_type.value}"
            cached_data = await self.cache.get(cache_key)
            if cached_data:
                self.logger.info(f"Using cached fallback data for {symbol}:{data_type.value}")
                return (symbol, data_type, cached_data, "cache")
        
        return None
    
    def _select_providers(self, request: DataIntegrationRequest) -> List[CredentialProvider]:
        """Select providers based on availability and preferences."""
        if request.providers:
            # Use requested providers
            available_providers = []
            for provider in request.providers:
                if self.credential_manager.get_credential(provider):
                    available_providers.append(provider)
            return available_providers
        
        # Use preferred providers
        available_providers = []
        for provider in self.preferred_providers:
            if self.credential_manager.get_credential(provider):
                available_providers.append(provider)
        
        # Fallback to any available provider
        if not available_providers:
            for provider in CredentialProvider:
                if self.credential_manager.get_credential(provider):
                    available_providers.append(provider)
        
        return available_providers
    
    def _aggregate_results(self, 
                          request: DataIntegrationRequest,
                          results: List[Optional[Tuple[str, DataType, Any, str]]]) -> IntegratedFinancialData:
        """Aggregate results from parallel data fetching."""
        data = {}
        sources = set()
        successful_results = [r for r in results if r is not None]
        
        for symbol, data_type, result_data, source in successful_results:
            if symbol not in data:
                data[symbol] = {}
            
            data[symbol][data_type.value] = result_data
            sources.add(source)
        
        # Determine overall quality
        total_requested = len(request.symbols) * len(request.data_types)
        successful_count = len(successful_results)
        success_rate = successful_count / total_requested if total_requested > 0 else 0
        
        if success_rate >= 0.9:
            quality = DataQuality.HIGH
        elif success_rate >= 0.7:
            quality = DataQuality.MEDIUM
        elif success_rate >= 0.4:
            quality = DataQuality.LOW
        else:
            quality = DataQuality.UNRELIABLE
        
        return IntegratedFinancialData(
            request_id=request.request_id,
            symbols=request.symbols,
            data=data,
            metadata={
                "success_rate": success_rate,
                "total_requested": total_requested,
                "successful_count": successful_count,
                "request_mode": request.mode.value
            },
            quality=quality,
            sources=list(sources),
            success=successful_count > 0
        )
    
    async def _validate_integrated_data(self, data: IntegratedFinancialData) -> IntegratedFinancialData:
        """Validate integrated data for quality and consistency."""
        # Perform data quality checks
        quality_issues = []
        
        for symbol, symbol_data in data.data.items():
            for data_type, value in symbol_data.items():
                # Check for basic data integrity
                if isinstance(value, StockQuote):
                    if value.price <= 0:
                        quality_issues.append(f"Invalid price for {symbol}: {value.price}")
                    if value.volume < 0:
                        quality_issues.append(f"Invalid volume for {symbol}: {value.volume}")
                
                elif isinstance(value, HistoricalData):
                    if not value.data_points:
                        quality_issues.append(f"No historical data points for {symbol}")
                    else:
                        # Check for data consistency
                        for point in value.data_points:
                            if any(price <= 0 for price in [point.open_price, point.high_price, point.low_price, point.close_price]):
                                quality_issues.append(f"Invalid price data in historical data for {symbol}")
                                break
        
        # Adjust quality based on validation
        if quality_issues:
            self.logger.warning(f"Data quality issues found: {len(quality_issues)} issues")
            if data.quality == DataQuality.HIGH:
                data.quality = DataQuality.MEDIUM
            elif data.quality == DataQuality.MEDIUM:
                data.quality = DataQuality.LOW
        
        data.metadata["validation_issues"] = quality_issues
        data.metadata["validation_performed"] = True
        
        return data
    
    def _generate_attribution(self, data: IntegratedFinancialData) -> List[Dict[str, Any]]:
        """Generate proper attribution for data sources."""
        attributions = []
        
        for source in data.sources:
            if source == "alpha_vantage":
                attributions.append({
                    "provider": "Alpha Vantage",
                    "attribution_text": "Financial data provided by Alpha Vantage",
                    "url": "https://www.alphavantage.co/",
                    "data_types": ["real-time quotes", "historical data", "fundamentals"]
                })
            elif source == "yahoo_finance":
                attributions.append({
                    "provider": "Yahoo Finance",
                    "attribution_text": "Financial data provided by Yahoo Finance",
                    "url": "https://finance.yahoo.com/",
                    "data_types": ["market data", "historical prices"]
                })
            elif source == "cache":
                attributions.append({
                    "provider": "Cached Data",
                    "attribution_text": "Data retrieved from local cache",
                    "data_types": ["cached financial data"]
                })
        
        return attributions
    
    def _generate_citations(self, 
                           data: IntegratedFinancialData, 
                           request: DataIntegrationRequest) -> List[str]:
        """Generate academic-style citations for the data sources."""
        citations = []
        timestamp = datetime.fromtimestamp(data.retrieved_at)
        
        for attribution in data.attribution:
            provider = attribution["provider"]
            
            if provider == "Alpha Vantage":
                citation = f"Alpha Vantage. ({timestamp.year}). Financial market data for {', '.join(request.symbols)}. Retrieved {timestamp.strftime('%B %d, %Y')}, from https://www.alphavantage.co/"
            elif provider == "Yahoo Finance":
                citation = f"Yahoo Finance. ({timestamp.year}). Market data for {', '.join(request.symbols)}. Retrieved {timestamp.strftime('%B %d, %Y')}, from https://finance.yahoo.com/"
            else:
                citation = f"{provider}. ({timestamp.year}). Financial data retrieved {timestamp.strftime('%B %d, %Y')}."
            
            citations.append(citation)
        
        return citations
    
    def _get_circuit_breaker(self, provider: CredentialProvider) -> CircuitBreakerState:
        """Get or create circuit breaker for provider."""
        if provider not in self.circuit_breakers:
            self.circuit_breakers[provider] = CircuitBreakerState(provider=provider)
        return self.circuit_breakers[provider]
    
    def _extract_data_request(self, 
                            state: DocumentState, 
                            message: Optional[BaseMessage]) -> DataIntegrationRequest:
        """Extract data integration request from state and message."""
        request_id = f"data_req_{int(time.time() * 1000)}"
        
        # Default request parameters
        mode = DataIntegrationMode.REAL_TIME
        symbols = ["SPY"]  # Default to S&P 500 ETF
        data_types = [DataType.STOCK_QUOTE]
        
        # Extract request details from message
        if message and isinstance(message, dict):
            content = message.get("content", "")
            
            # Analyze message for symbols and data types
            symbols = self._extract_symbols_from_message(content)
            data_types = self._extract_data_types_from_message(content)
            mode = self._analyze_integration_mode(content)
        
        # Build context from document state
        context = {
            "document_title": state.get("current_document", {}).get("title", ""),
            "document_path": state.get("current_document", {}).get("path", ""),
            "cursor_position": state.get("cursor_position", {}),
            "document_type": self._infer_document_type(state)
        }
        
        return DataIntegrationRequest(
            request_id=request_id,
            mode=mode,
            symbols=symbols,
            data_types=data_types,
            context=context
        )
    
    def _validate_data_request(self, request: DataIntegrationRequest) -> ValidationResult:
        """Validate data integration request parameters."""
        errors = []
        warnings = []
        
        # Validate symbols
        if not request.symbols:
            errors.append("No symbols specified for data integration")
        else:
            for symbol in request.symbols:
                if not symbol or not isinstance(symbol, str) or len(symbol) > 10:
                    errors.append(f"Invalid symbol format: {symbol}")
        
        # Validate data types
        if not request.data_types:
            errors.append("No data types specified")
        
        # Validate mode
        if not isinstance(request.mode, DataIntegrationMode):
            errors.append("Invalid integration mode")
        
        # Check provider availability
        available_providers = self._select_providers(request)
        if not available_providers:
            errors.append("No available data providers with valid credentials")
        
        return ValidationResult(
            agent_id=self.agent_id,
            validation_type="data_request_validation",
            passed=len(errors) == 0,
            confidence=1.0 if len(errors) == 0 else 0.5,
            issues=[{"level": "error", "message": error} for error in errors] + 
                   [{"level": "warning", "message": warning} for warning in warnings],
            recommendations=warnings
        )
    
    def _extract_symbols_from_message(self, content: str) -> List[str]:
        """Extract financial symbols from user message."""
        import re
        
        # Look for common stock symbols (2-5 uppercase letters)
        symbols = re.findall(r'\b[A-Z]{2,5}\b', content)
        
        # Common symbol patterns
        if "apple" in content.lower() or "aapl" in content.lower():
            symbols.append("AAPL")
        if "microsoft" in content.lower() or "msft" in content.lower():
            symbols.append("MSFT")
        if "tesla" in content.lower() or "tsla" in content.lower():
            symbols.append("TSLA")
        if "sp 500" in content.lower() or "s&p 500" in content.lower():
            symbols.append("SPY")
        
        # Remove duplicates and return unique symbols
        return list(set(symbols)) if symbols else ["SPY"]
    
    def _extract_data_types_from_message(self, content: str) -> List[DataType]:
        """Extract requested data types from user message."""
        content_lower = content.lower()
        data_types = []
        
        if any(word in content_lower for word in ["quote", "price", "current", "latest"]):
            data_types.append(DataType.STOCK_QUOTE)
        
        if any(word in content_lower for word in ["historical", "history", "past", "chart"]):
            data_types.append(DataType.HISTORICAL_DATA)
        
        if any(word in content_lower for word in ["fundamental", "earnings", "revenue", "pe ratio"]):
            data_types.append(DataType.COMPANY_FUNDAMENTALS)
        
        if any(word in content_lower for word in ["index", "market", "dow", "nasdaq"]):
            data_types.append(DataType.MARKET_INDEX)
        
        return data_types if data_types else [DataType.STOCK_QUOTE]
    
    def _analyze_integration_mode(self, content: str) -> DataIntegrationMode:
        """Analyze user message to determine integration mode."""
        content_lower = content.lower()
        
        if any(word in content_lower for word in ["historical", "past", "analyze", "trend"]):
            return DataIntegrationMode.HISTORICAL
        elif any(word in content_lower for word in ["compare", "comparison", "versus", "vs"]):
            return DataIntegrationMode.COMPARATIVE
        elif any(word in content_lower for word in ["research", "study", "investigate"]):
            return DataIntegrationMode.RESEARCH
        elif any(word in content_lower for word in ["monitor", "track", "watch"]):
            return DataIntegrationMode.MONITORING
        else:
            return DataIntegrationMode.REAL_TIME
    
    def _infer_document_type(self, state: DocumentState) -> str:
        """Infer document type from title and content."""
        doc_title = state.get("current_document", {}).get("title", "").lower()
        
        if any(word in doc_title for word in ["financial", "report", "quarterly", "annual"]):
            return "financial_report"
        elif any(word in doc_title for word in ["analysis", "research", "market"]):
            return "market_analysis"
        elif any(word in doc_title for word in ["investment", "portfolio"]):
            return "investment_document"
        else:
            return "general_document"
    
    def _create_agent_result(self, 
                           operation_id: str, 
                           integrated_data: IntegratedFinancialData, 
                           start_time: float) -> AgentResult:
        """Create AgentResult from IntegratedFinancialData."""
        execution_time = time.time() - start_time
        
        # Prepare state updates
        state_updates = {
            "external_data": integrated_data.get_document_formatted_data() if integrated_data.success else {},
            "data_integration_status": {
                "request_id": integrated_data.request_id,
                "symbols": integrated_data.symbols,
                "quality": integrated_data.quality.value,
                "sources": integrated_data.sources,
                "success": integrated_data.success,
                "metadata": integrated_data.metadata
            }
        }
        
        return AgentResult(
            agent_id=self.agent_id,
            operation_id=operation_id,
            success=integrated_data.success,
            result=integrated_data.get_document_formatted_data() if integrated_data.success else None,
            error=integrated_data.error_message,
            execution_time=execution_time,
            state_updates=state_updates,
            metadata={
                "data_quality": integrated_data.quality.value,
                "sources_used": integrated_data.sources,
                "symbols_processed": len(integrated_data.symbols),
                "attribution_count": len(integrated_data.attribution),
                "performance_stats": self.performance_stats
            }
        )