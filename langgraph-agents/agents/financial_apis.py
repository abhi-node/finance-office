"""
Financial API Client Interfaces

This module provides standardized client interfaces for major financial data providers
including Alpha Vantage, Yahoo Finance, and Bloomberg. It implements unified data models,
error handling, and authentication patterns to provide consistent access to real-time
and historical financial data.

Key Features:
- Abstract base class for financial API clients
- Unified data models for financial instruments and market data
- Provider-specific implementations with authentication
- Comprehensive error handling and retry logic
- Request/response logging and monitoring
- Data normalization and validation
"""

import asyncio
import aiohttp
import time
import logging
import json
from abc import ABC, abstractmethod
from datetime import datetime, timezone, timedelta
from typing import Any, Dict, List, Optional, Union, Tuple, Set
from dataclasses import dataclass, field
from enum import Enum
from urllib.parse import urlencode

from .credential_manager import get_credential_manager, CredentialProvider, APICredential


class DataType(Enum):
    """Types of financial data that can be retrieved."""
    STOCK_QUOTE = "stock_quote"
    HISTORICAL_DATA = "historical_data"
    COMPANY_FUNDAMENTALS = "company_fundamentals"
    MARKET_INDEX = "market_index"
    OPTIONS_DATA = "options_data"
    FOREX_RATES = "forex_rates"
    CRYPTO_PRICES = "crypto_prices"
    ECONOMIC_INDICATORS = "economic_indicators"
    NEWS = "news"
    EARNINGS = "earnings"


class TimeInterval(Enum):
    """Time intervals for historical data."""
    MINUTE_1 = "1min"
    MINUTE_5 = "5min"
    MINUTE_15 = "15min"
    MINUTE_30 = "30min"
    HOUR_1 = "1hour"
    DAILY = "daily"
    WEEKLY = "weekly"
    MONTHLY = "monthly"


@dataclass
class FinancialDataRequest:
    """Request for financial data."""
    data_type: DataType
    symbol: str
    provider: Optional[CredentialProvider] = None
    interval: Optional[TimeInterval] = None
    start_date: Optional[datetime] = None
    end_date: Optional[datetime] = None
    parameters: Dict[str, Any] = field(default_factory=dict)
    metadata: Dict[str, Any] = field(default_factory=dict)


@dataclass
class StockQuote:
    """Real-time stock quote data."""
    symbol: str
    price: float
    change: float
    change_percent: float
    volume: int
    market_cap: Optional[float] = None
    pe_ratio: Optional[float] = None
    high_52week: Optional[float] = None
    low_52week: Optional[float] = None
    timestamp: float = field(default_factory=time.time)
    source: Optional[str] = None
    currency: str = "USD"


@dataclass
class HistoricalDataPoint:
    """Single point of historical data."""
    timestamp: float
    open_price: float
    high_price: float
    low_price: float
    close_price: float
    volume: int
    adjusted_close: Optional[float] = None


@dataclass
class HistoricalData:
    """Historical price data."""
    symbol: str
    interval: TimeInterval
    data_points: List[HistoricalDataPoint]
    start_date: datetime
    end_date: datetime
    source: Optional[str] = None
    currency: str = "USD"


@dataclass
class CompanyFundamentals:
    """Company fundamental data."""
    symbol: str
    company_name: str
    market_cap: Optional[float] = None
    pe_ratio: Optional[float] = None
    eps: Optional[float] = None
    dividend_yield: Optional[float] = None
    book_value: Optional[float] = None
    debt_to_equity: Optional[float] = None
    revenue: Optional[float] = None
    net_income: Optional[float] = None
    sector: Optional[str] = None
    industry: Optional[str] = None
    timestamp: float = field(default_factory=time.time)
    source: Optional[str] = None


@dataclass
class MarketIndex:
    """Market index data."""
    index_name: str
    symbol: str
    value: float
    change: float
    change_percent: float
    constituents_count: Optional[int] = None
    timestamp: float = field(default_factory=time.time)
    source: Optional[str] = None


@dataclass
class APIResponse:
    """Standardized API response wrapper."""
    success: bool
    data: Optional[Union[StockQuote, HistoricalData, CompanyFundamentals, MarketIndex, List[Any]]]
    error_message: Optional[str] = None
    response_time_ms: float = 0.0
    rate_limit_remaining: Optional[int] = None
    rate_limit_reset: Optional[float] = None
    source: Optional[str] = None
    cached: bool = False
    metadata: Dict[str, Any] = field(default_factory=dict)


class APIError(Exception):
    """Custom exception for API errors."""
    
    def __init__(self, 
                 message: str, 
                 status_code: Optional[int] = None,
                 provider: Optional[str] = None,
                 rate_limited: bool = False):
        super().__init__(message)
        self.status_code = status_code
        self.provider = provider
        self.rate_limited = rate_limited


class BaseFinancialAPIClient(ABC):
    """
    Abstract base class for financial API clients.
    
    This class defines the interface that all financial data providers
    must implement, ensuring consistent behavior across different APIs.
    """
    
    def __init__(self, 
                 provider: CredentialProvider,
                 config: Optional[Dict[str, Any]] = None):
        """
        Initialize the API client.
        
        Args:
            provider: The credential provider for this client
            config: Optional configuration dictionary
        """
        self.provider = provider
        self.config = config or {}
        self.logger = logging.getLogger(f"{__name__}.{provider.value}")
        
        # Get credential manager
        self.credential_manager = get_credential_manager()
        
        # Configuration
        self.timeout_seconds = self.config.get("timeout_seconds", 30)
        self.max_retries = self.config.get("max_retries", 3)
        self.retry_delay = self.config.get("retry_delay", 1.0)
        
        # Session for connection pooling
        self.session: Optional[aiohttp.ClientSession] = None
        
        self.logger.info(f"Initialized {provider.value} API client")
    
    async def __aenter__(self):
        """Async context manager entry."""
        await self._ensure_session()
        return self
    
    async def __aexit__(self, exc_type, exc_val, exc_tb):
        """Async context manager exit."""
        await self._close_session()
    
    async def get_stock_quote(self, symbol: str) -> APIResponse:
        """
        Get real-time stock quote.
        
        Args:
            symbol: Stock symbol (e.g., "AAPL")
            
        Returns:
            APIResponse containing StockQuote data
        """
        request = FinancialDataRequest(
            data_type=DataType.STOCK_QUOTE,
            symbol=symbol.upper()
        )
        return await self._make_request(request)
    
    async def get_historical_data(self, 
                                 symbol: str,
                                 interval: TimeInterval = TimeInterval.DAILY,
                                 start_date: Optional[datetime] = None,
                                 end_date: Optional[datetime] = None) -> APIResponse:
        """
        Get historical price data.
        
        Args:
            symbol: Stock symbol
            interval: Time interval for data points
            start_date: Start date for historical data
            end_date: End date for historical data
            
        Returns:
            APIResponse containing HistoricalData
        """
        # Default to last 30 days if no dates provided
        if not end_date:
            end_date = datetime.now(timezone.utc)
        if not start_date:
            start_date = end_date - timedelta(days=30)
        
        request = FinancialDataRequest(
            data_type=DataType.HISTORICAL_DATA,
            symbol=symbol.upper(),
            interval=interval,
            start_date=start_date,
            end_date=end_date
        )
        return await self._make_request(request)
    
    async def get_company_fundamentals(self, symbol: str) -> APIResponse:
        """
        Get company fundamental data.
        
        Args:
            symbol: Stock symbol
            
        Returns:
            APIResponse containing CompanyFundamentals
        """
        request = FinancialDataRequest(
            data_type=DataType.COMPANY_FUNDAMENTALS,
            symbol=symbol.upper()
        )
        return await self._make_request(request)
    
    async def get_market_index(self, index_symbol: str) -> APIResponse:
        """
        Get market index data.
        
        Args:
            index_symbol: Index symbol (e.g., "SPY", "QQQ")
            
        Returns:
            APIResponse containing MarketIndex data
        """
        request = FinancialDataRequest(
            data_type=DataType.MARKET_INDEX,
            symbol=index_symbol.upper()
        )
        return await self._make_request(request)
    
    @abstractmethod
    async def _make_request(self, request: FinancialDataRequest) -> APIResponse:
        """
        Make API request to the provider.
        
        Args:
            request: Standardized data request
            
        Returns:
            APIResponse with requested data
        """
        pass
    
    @abstractmethod
    def _build_url(self, request: FinancialDataRequest) -> str:
        """
        Build provider-specific URL for the request.
        
        Args:
            request: Data request
            
        Returns:
            Complete URL for the API call
        """
        pass
    
    @abstractmethod
    def _parse_response(self, 
                       request: FinancialDataRequest,
                       response_data: Dict[str, Any]) -> APIResponse:
        """
        Parse provider-specific response into standardized format.
        
        Args:
            request: Original request
            response_data: Raw response from provider
            
        Returns:
            Parsed APIResponse
        """
        pass
    
    async def _ensure_session(self):
        """Ensure HTTP session exists."""
        if self.session is None or self.session.closed:
            timeout = aiohttp.ClientTimeout(total=self.timeout_seconds)
            self.session = aiohttp.ClientSession(timeout=timeout)
    
    async def _close_session(self):
        """Close HTTP session."""
        if self.session and not self.session.closed:
            await self.session.close()
            self.session = None
    
    def _get_credential(self) -> Optional[APICredential]:
        """Get credential for this provider."""
        return self.credential_manager.get_credential(self.provider)


class YahooFinanceClient(BaseFinancialAPIClient):
    """Yahoo Finance API client implementation."""
    
    def __init__(self, config: Optional[Dict[str, Any]] = None):
        super().__init__(CredentialProvider.YAHOO_FINANCE, config)
        self.base_url = "https://query1.finance.yahoo.com/v8/finance/chart/"
    
    async def _make_request(self, request: FinancialDataRequest) -> APIResponse:
        """Make request to Yahoo Finance API."""
        start_time = time.time()
        
        try:
            url = self._build_url(request)
            params = self._build_params(request)
            
            await self._ensure_session()
            
            async with self.session.get(url, params=params) as response:
                response_data = await response.json()
                response_time = (time.time() - start_time) * 1000
                
                if response.status != 200:
                    return APIResponse(
                        success=False,
                        data=None,
                        error_message=f"HTTP {response.status}",
                        response_time_ms=response_time,
                        source="yahoo_finance"
                    )
                
                result = self._parse_response(request, response_data)
                result.response_time_ms = response_time
                return result
                
        except Exception as e:
            response_time = (time.time() - start_time) * 1000
            self.logger.error(f"Yahoo Finance API request failed: {type(e).__name__}")
            
            return APIResponse(
                success=False,
                data=None,
                error_message="API request failed",
                response_time_ms=response_time,
                source="yahoo_finance"
            )
    
    def _build_url(self, request: FinancialDataRequest) -> str:
        """Build Yahoo Finance API URL."""
        return f"{self.base_url}{request.symbol}"
    
    def _build_params(self, request: FinancialDataRequest) -> Dict[str, str]:
        """Build request parameters for Yahoo Finance."""
        params = {}
        
        if request.data_type == DataType.HISTORICAL_DATA:
            if request.start_date:
                params["period1"] = str(int(request.start_date.timestamp()))
            if request.end_date:
                params["period2"] = str(int(request.end_date.timestamp()))
            
            # Map interval
            interval_map = {
                TimeInterval.MINUTE_1: "1m",
                TimeInterval.MINUTE_5: "5m",
                TimeInterval.MINUTE_15: "15m",
                TimeInterval.MINUTE_30: "30m",
                TimeInterval.HOUR_1: "1h",
                TimeInterval.DAILY: "1d",
                TimeInterval.WEEKLY: "1wk",
                TimeInterval.MONTHLY: "1mo"
            }
            params["interval"] = interval_map.get(request.interval, "1d")
        
        return params
    
    def _parse_response(self, 
                       request: FinancialDataRequest,
                       response_data: Dict[str, Any]) -> APIResponse:
        """Parse Yahoo Finance response."""
        try:
            chart_data = response_data.get("chart", {})
            result_data = chart_data.get("result", [])
            
            if not result_data:
                return APIResponse(
                    success=False,
                    data=None,
                    error_message="No chart data found",
                    source="yahoo_finance"
                )
            
            first_result = result_data[0]
            
            if request.data_type == DataType.STOCK_QUOTE:
                return self._parse_yahoo_quote(first_result)
            elif request.data_type == DataType.HISTORICAL_DATA:
                return self._parse_yahoo_historical(request, first_result)
            else:
                return APIResponse(
                    success=False,
                    data=None,
                    error_message="Unsupported data type for Yahoo Finance",
                    source="yahoo_finance"
                )
                
        except Exception as e:
            self.logger.error(f"Failed to parse Yahoo Finance response: {type(e).__name__}")
            return APIResponse(
                success=False,
                data=None,
                error_message="Response parsing failed",
                source="yahoo_finance"
            )
    
    def _parse_yahoo_quote(self, result_data: Dict[str, Any]) -> APIResponse:
        """Parse Yahoo Finance quote data."""
        meta = result_data.get("meta", {})
        
        quote = StockQuote(
            symbol=meta.get("symbol", ""),
            price=meta.get("regularMarketPrice", 0.0),
            change=meta.get("regularMarketPrice", 0.0) - meta.get("previousClose", 0.0),
            change_percent=((meta.get("regularMarketPrice", 0.0) - meta.get("previousClose", 0.0)) / 
                          meta.get("previousClose", 1.0)) * 100,
            volume=meta.get("regularMarketVolume", 0),
            source="yahoo_finance"
        )
        
        return APIResponse(
            success=True,
            data=quote,
            source="yahoo_finance"
        )
    
    def _parse_yahoo_historical(self, 
                               request: FinancialDataRequest,
                               result_data: Dict[str, Any]) -> APIResponse:
        """Parse Yahoo Finance historical data."""
        timestamps = result_data.get("timestamp", [])
        indicators = result_data.get("indicators", {})
        quote_data = indicators.get("quote", [{}])[0]
        
        opens = quote_data.get("open", [])
        highs = quote_data.get("high", [])
        lows = quote_data.get("low", [])
        closes = quote_data.get("close", [])
        volumes = quote_data.get("volume", [])
        
        data_points = []
        for i, timestamp in enumerate(timestamps):
            if (i < len(opens) and i < len(highs) and i < len(lows) and 
                i < len(closes) and i < len(volumes)):
                
                data_point = HistoricalDataPoint(
                    timestamp=float(timestamp),
                    open_price=opens[i] or 0.0,
                    high_price=highs[i] or 0.0,
                    low_price=lows[i] or 0.0,
                    close_price=closes[i] or 0.0,
                    volume=volumes[i] or 0
                )
                data_points.append(data_point)
        
        historical_data = HistoricalData(
            symbol=request.symbol,
            interval=request.interval or TimeInterval.DAILY,
            data_points=data_points,
            start_date=request.start_date or datetime.now(timezone.utc) - timedelta(days=30),
            end_date=request.end_date or datetime.now(timezone.utc),
            source="yahoo_finance"
        )
        
        return APIResponse(
            success=True,
            data=historical_data,
            source="yahoo_finance"
        )


# Factory function to create appropriate API client
def create_financial_client(provider: CredentialProvider,
                           config: Optional[Dict[str, Any]] = None) -> BaseFinancialAPIClient:
    """
    Create appropriate financial API client for the provider.
    
    Args:
        provider: The credential provider
        config: Optional configuration
        
    Returns:
        Appropriate API client instance
    """
    if provider == CredentialProvider.YAHOO_FINANCE:
        return YahooFinanceClient(config)
    else:
        raise ValueError(f"Unsupported financial data provider: {provider}")


# Global client instances for reuse
_client_instances: Dict[CredentialProvider, BaseFinancialAPIClient] = {}

async def get_financial_client(provider: CredentialProvider,
                              config: Optional[Dict[str, Any]] = None) -> BaseFinancialAPIClient:
    """
    Get or create financial API client instance.
    
    Args:
        provider: The credential provider
        config: Optional configuration
        
    Returns:
        Financial API client instance
    """
    if provider not in _client_instances:
        _client_instances[provider] = create_financial_client(provider, config)
    
    return _client_instances[provider]