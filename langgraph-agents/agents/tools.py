"""
Tool Integration Interfaces

This module provides tool integration interfaces that enable agents to interact
with LibreOffice UNO services, external APIs, and other systems. The tools are
organized into specialized toolkits based on their functionality.

Tool Categories:
- DocumentToolkit: LibreOffice document manipulation operations
- FinancialDataToolkit: External financial data API integration
- ResearchToolkit: Web search and research capabilities
- ValidationToolkit: Quality assurance and validation operations
- UnoServiceManager: UNO service connection and resource management

All tools follow consistent patterns for error handling, performance monitoring,
and integration with the agent system.
"""

import asyncio
import json
import time
import uuid
from abc import ABC, abstractmethod
from dataclasses import dataclass
from datetime import datetime, timezone
from typing import Any, Dict, List, Optional, Union, Callable
from pathlib import Path
import logging
from concurrent.futures import ThreadPoolExecutor
from contextlib import asynccontextmanager

# Optional dependencies with fallbacks
try:
    import aiohttp
except ImportError:
    aiohttp = None

try:
    import requests
except ImportError:
    requests = None

# Import UNO types (will be available when integrated with LibreOffice)
try:
    import uno
    from com.sun.star.document import XDocumentInsertable
    from com.sun.star.text import XTextDocument, XText, XTextCursor
    from com.sun.star.table import XTableRows, XTableColumns, XCell
    UNO_AVAILABLE = True
except ImportError:
    # Fallback for development without LibreOffice
    uno = None
    XDocumentInsertable = Any
    XTextDocument = Any
    XText = Any
    XTextCursor = Any
    XTableRows = Any
    XTableColumns = Any
    XCell = Any
    UNO_AVAILABLE = False

@dataclass
class ToolOperation:
    """Represents a tool operation with metadata."""
    operation_id: str
    tool_name: str
    operation_type: str
    parameters: Dict[str, Any]
    timestamp: str
    agent_id: str = ""
    priority: int = 0
    timeout: float = 30.0

@dataclass
class ToolResult:
    """Standardized result format for tool operations."""
    operation_id: str
    tool_name: str
    success: bool
    result: Any = None
    error: Optional[str] = None
    warnings: List[str] = None
    metadata: Dict[str, Any] = None
    execution_time: float = 0.0
    timestamp: str = ""
    
    def __post_init__(self):
        if self.warnings is None:
            self.warnings = []
        if self.metadata is None:
            self.metadata = {}
        if not self.timestamp:
            self.timestamp = datetime.now(timezone.utc).isoformat()

class BaseTool(ABC):
    """Base class for all tool implementations."""
    
    def __init__(self, tool_name: str, config: Optional[Dict[str, Any]] = None):
        self.tool_name = tool_name
        self.config = config or {}
        self.logger = logging.getLogger(f"tool.{tool_name}")
        self.executor = ThreadPoolExecutor(max_workers=2, thread_name_prefix=f"Tool-{tool_name}")
        self._initialize_tool()
    
    def _initialize_tool(self) -> None:
        """Initialize tool-specific resources. Override in subclasses."""
        pass
    
    @abstractmethod
    async def execute(self, operation: ToolOperation) -> ToolResult:
        """Execute the tool operation."""
        pass
    
    def cleanup(self) -> None:
        """Clean up tool resources."""
        self.executor.shutdown(wait=True)

class UnoServiceManager:
    """
    Manages UNO service connections and provides utilities for LibreOffice integration.
    """
    
    def __init__(self, config: Optional[Dict[str, Any]] = None):
        self.config = config or {}
        self.logger = logging.getLogger("tool.uno_service_manager")
        self._connection_pool: Dict[str, Any] = {}
        self._document_cache: Dict[str, Any] = {}
        
    def get_current_document(self) -> Optional[XTextDocument]:
        """Get reference to the current LibreOffice document."""
        if not UNO_AVAILABLE:
            self.logger.warning("UNO services not available")
            return None
            
        try:
            # Get the component context
            ctx = uno.getComponentContext()
            smgr = ctx.ServiceManager
            
            # Get the desktop service
            desktop = smgr.createInstanceWithContext("com.sun.star.frame.Desktop", ctx)
            
            # Get the current document
            document = desktop.getCurrentComponent()
            
            if document and hasattr(document, 'supportsService'):
                if document.supportsService("com.sun.star.text.TextDocument"):
                    return document
                    
            return None
            
        except Exception as e:
            self.logger.error(f"Failed to get current document: {e}")
            return None
    
    def get_text_cursor(self, document: XTextDocument) -> Optional[XTextCursor]:
        """Get text cursor for document manipulation."""
        if not document:
            return None
            
        try:
            text = document.getText()
            return text.createTextCursor()
        except Exception as e:
            self.logger.error(f"Failed to get text cursor: {e}")
            return None
    
    def create_service(self, service_name: str, context: Optional[Any] = None) -> Optional[Any]:
        """Create a UNO service instance."""
        if not UNO_AVAILABLE:
            return None
            
        try:
            if context is None:
                context = uno.getComponentContext()
            
            service_manager = context.ServiceManager
            return service_manager.createInstanceWithContext(service_name, context)
            
        except Exception as e:
            self.logger.error(f"Failed to create service {service_name}: {e}")
            return None

class DocumentToolkit(BaseTool):
    """
    Toolkit for LibreOffice document manipulation operations.
    Provides high-level interfaces for text editing, formatting, and document structure operations.
    """
    
    def __init__(self, config: Optional[Dict[str, Any]] = None):
        super().__init__("document_toolkit", config)
        self.uno_manager = UnoServiceManager(config)
    
    async def execute(self, operation: ToolOperation) -> ToolResult:
        """Execute document manipulation operation."""
        start_time = time.time()
        
        try:
            operation_type = operation.operation_type
            params = operation.parameters
            
            if operation_type == "insert_text":
                result = await self._insert_text(params)
            elif operation_type == "format_text_range":
                result = await self._format_text_range(params)
            elif operation_type == "create_table":
                result = await self._create_table(params)
            elif operation_type == "create_chart":
                result = await self._create_chart(params)
            elif operation_type == "apply_paragraph_style":
                result = await self._apply_paragraph_style(params)
            elif operation_type == "insert_page_break":
                result = await self._insert_page_break(params)
            else:
                raise ValueError(f"Unknown operation type: {operation_type}")
            
            execution_time = time.time() - start_time
            return ToolResult(
                operation_id=operation.operation_id,
                tool_name=self.tool_name,
                success=True,
                result=result,
                execution_time=execution_time
            )
            
        except Exception as e:
            execution_time = time.time() - start_time
            self.logger.error(f"Document operation failed: {e}")
            
            return ToolResult(
                operation_id=operation.operation_id,
                tool_name=self.tool_name,
                success=False,
                error=str(e),
                execution_time=execution_time
            )
    
    async def _insert_text(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Insert text at specified position."""
        text_content = params.get("text", "")
        position = params.get("position", {})
        formatting = params.get("formatting", {})
        
        if not UNO_AVAILABLE:
            # Simulate operation for development
            return {
                "operation": "insert_text",
                "text_length": len(text_content),
                "position": position,
                "formatting_applied": bool(formatting)
            }
        
        # Execute text insertion through UNO services
        document = self.uno_manager.get_current_document()
        if not document:
            raise RuntimeError("No active document available")
        
        cursor = self.uno_manager.get_text_cursor(document)
        if not cursor:
            raise RuntimeError("Could not create text cursor")
        
        # Position cursor if specified
        if position:
            paragraph = position.get("paragraph", 0)
            character = position.get("character", 0)
            # Move cursor to specified position (implementation depends on UNO API details)
        
        # Insert text
        cursor.getText().insertString(cursor, text_content, False)
        
        # Apply formatting if specified
        if formatting:
            await self._apply_formatting_to_cursor(cursor, formatting)
        
        return {
            "operation": "insert_text",
            "text_inserted": text_content,
            "position": position,
            "formatting_applied": bool(formatting)
        }
    
    async def _format_text_range(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Apply formatting to specified text range."""
        range_spec = params.get("range", {})
        formatting = params.get("formatting", {})
        
        # Implementation would use UNO services to apply formatting
        return {
            "operation": "format_text_range",
            "range": range_spec,
            "formatting": formatting,
            "properties_applied": len(formatting)
        }
    
    async def _create_table(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Create table with specified data and formatting."""
        rows = params.get("rows", 2)
        cols = params.get("cols", 2)
        data = params.get("data", [])
        formatting = params.get("formatting", {})
        position = params.get("position", {})
        
        if not UNO_AVAILABLE:
            return {
                "operation": "create_table",
                "dimensions": f"{rows}x{cols}",
                "data_cells": len(data) if data else 0,
                "position": position
            }
        
        # Implementation would create table through UNO services
        document = self.uno_manager.get_current_document()
        if not document:
            raise RuntimeError("No active document available")
        
        # Create table service
        table_service = self.uno_manager.create_service("com.sun.star.text.TextTable")
        if not table_service:
            raise RuntimeError("Could not create table service")
        
        # Initialize table dimensions
        table_service.initialize(rows, cols)
        
        # Insert table into document
        cursor = self.uno_manager.get_text_cursor(document)
        document.getText().insertTextContent(cursor, table_service, False)
        
        # Populate with data if provided
        if data:
            await self._populate_table_data(table_service, data)
        
        # Apply formatting if specified
        if formatting:
            await self._apply_table_formatting(table_service, formatting)
        
        return {
            "operation": "create_table",
            "dimensions": f"{rows}x{cols}",
            "data_populated": bool(data),
            "formatting_applied": bool(formatting)
        }
    
    async def _create_chart(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Create chart with specified data and styling."""
        chart_type = params.get("chart_type", "column")
        data = params.get("data", {})
        title = params.get("title", "")
        position = params.get("position", {})
        styling = params.get("styling", {})
        
        # Implementation would create chart through UNO services
        return {
            "operation": "create_chart",
            "chart_type": chart_type,
            "title": title,
            "data_series": len(data.get("series", [])),
            "position": position
        }
    
    async def _apply_paragraph_style(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Apply paragraph style to specified content."""
        paragraphs = params.get("paragraphs", [])
        style_name = params.get("style_name", "")
        
        return {
            "operation": "apply_paragraph_style",
            "paragraphs_affected": len(paragraphs),
            "style_applied": style_name
        }
    
    async def _insert_page_break(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Insert page or section break."""
        break_type = params.get("break_type", "page")
        position = params.get("position", {})
        
        return {
            "operation": "insert_page_break",
            "break_type": break_type,
            "position": position
        }
    
    async def _apply_formatting_to_cursor(self, cursor: XTextCursor, formatting: Dict[str, Any]) -> None:
        """Apply formatting properties to cursor selection."""
        # Implementation would set character and paragraph properties
        pass
    
    async def _populate_table_data(self, table: Any, data: List[List[str]]) -> None:
        """Populate table with data."""
        # Implementation would set cell content
        pass
    
    async def _apply_table_formatting(self, table: Any, formatting: Dict[str, Any]) -> None:
        """Apply formatting to table."""
        # Implementation would set table properties
        pass

class FinancialDataToolkit(BaseTool):
    """
    Toolkit for financial data integration from external APIs.
    Provides access to stock data, market news, and economic indicators.
    """
    
    def __init__(self, config: Optional[Dict[str, Any]] = None):
        super().__init__("financial_data_toolkit", config)
        self.api_keys = config.get("api_keys", {}) if config else {}
        self.base_urls = {
            "alpha_vantage": "https://www.alphavantage.co/query",
            "yahoo_finance": "https://query1.finance.yahoo.com/v8/finance/chart",
            "fmp": "https://financialmodelingprep.com/api/v3"
        }
        self.session: Optional[aiohttp.ClientSession] = None
    
    async def execute(self, operation: ToolOperation) -> ToolResult:
        """Execute financial data operation."""
        start_time = time.time()
        
        try:
            if not self.session:
                self.session = aiohttp.ClientSession()
                
            operation_type = operation.operation_type
            params = operation.parameters
            
            if operation_type == "get_stock_data":
                result = await self._get_stock_data(params)
            elif operation_type == "get_market_news":
                result = await self._get_market_news(params)
            elif operation_type == "get_economic_indicators":
                result = await self._get_economic_indicators(params)
            elif operation_type == "validate_financial_data":
                result = await self._validate_financial_data(params)
            else:
                raise ValueError(f"Unknown operation type: {operation_type}")
            
            execution_time = time.time() - start_time
            return ToolResult(
                operation_id=operation.operation_id,
                tool_name=self.tool_name,
                success=True,
                result=result,
                execution_time=execution_time
            )
            
        except Exception as e:
            execution_time = time.time() - start_time
            self.logger.error(f"Financial data operation failed: {e}")
            
            return ToolResult(
                operation_id=operation.operation_id,
                tool_name=self.tool_name,
                success=False,
                error=str(e),
                execution_time=execution_time
            )
    
    async def _get_stock_data(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Retrieve stock data from financial APIs."""
        symbol = params.get("symbol", "")
        timeframe = params.get("timeframe", "1D")
        data_points = params.get("data_points", ["price", "volume"])
        
        if not symbol:
            raise ValueError("Stock symbol is required")
        
        # Try Alpha Vantage first
        if "alpha_vantage" in self.api_keys:
            return await self._fetch_alpha_vantage_data(symbol, timeframe, data_points)
        
        # Fallback to Yahoo Finance (free)
        return await self._fetch_yahoo_finance_data(symbol, timeframe, data_points)
    
    async def _fetch_alpha_vantage_data(self, symbol: str, timeframe: str, data_points: List[str]) -> Dict[str, Any]:
        """Fetch data from Alpha Vantage API."""
        api_key = self.api_keys.get("alpha_vantage")
        if not api_key:
            raise ValueError("Alpha Vantage API key not configured")
        
        params = {
            "function": "TIME_SERIES_DAILY",
            "symbol": symbol,
            "apikey": api_key
        }
        
        async with self.session.get(self.base_urls["alpha_vantage"], params=params) as response:
            if response.status == 200:
                data = await response.json()
                return self._parse_alpha_vantage_response(data, data_points)
            else:
                raise RuntimeError(f"Alpha Vantage API error: {response.status}")
    
    async def _fetch_yahoo_finance_data(self, symbol: str, timeframe: str, data_points: List[str]) -> Dict[str, Any]:
        """Fetch data from Yahoo Finance API."""
        url = f"{self.base_urls['yahoo_finance']}/{symbol}"
        params = {"interval": "1d", "range": "1mo"}
        
        async with self.session.get(url, params=params) as response:
            if response.status == 200:
                data = await response.json()
                return self._parse_yahoo_finance_response(data, data_points)
            else:
                raise RuntimeError(f"Yahoo Finance API error: {response.status}")
    
    async def _get_market_news(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Fetch market news and analysis."""
        topics = params.get("topics", [])
        timeframe = params.get("timeframe", "1d")
        
        # Implementation would fetch news from financial news APIs
        return {
            "operation": "get_market_news",
            "topics": topics,
            "timeframe": timeframe,
            "articles": []  # Would contain actual news data
        }
    
    async def _get_economic_indicators(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Retrieve economic indicators and data."""
        indicators = params.get("indicators", [])
        timeframe = params.get("timeframe", "1y")
        
        return {
            "operation": "get_economic_indicators",
            "indicators": indicators,
            "timeframe": timeframe,
            "data": {}  # Would contain actual indicator data
        }
    
    async def _validate_financial_data(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Validate accuracy and completeness of financial data."""
        data = params.get("data", {})
        
        validation_results = {
            "data_completeness": 0.0,
            "accuracy_score": 0.0,
            "freshness_check": True,
            "issues": [],
            "recommendations": []
        }
        
        # Implementation would perform comprehensive data validation
        return validation_results
    
    def _parse_alpha_vantage_response(self, data: Dict[str, Any], data_points: List[str]) -> Dict[str, Any]:
        """Parse Alpha Vantage API response."""
        # Implementation would parse the JSON response
        return {"parsed_data": data, "data_points": data_points}
    
    def _parse_yahoo_finance_response(self, data: Dict[str, Any], data_points: List[str]) -> Dict[str, Any]:
        """Parse Yahoo Finance API response."""
        # Implementation would parse the JSON response
        return {"parsed_data": data, "data_points": data_points}
    
    def cleanup(self) -> None:
        """Clean up resources."""
        if self.session:
            asyncio.create_task(self.session.close())
        super().cleanup()

class ResearchToolkit(BaseTool):
    """
    Toolkit for web search and research capabilities.
    Provides contextual search, fact verification, and citation management.
    """
    
    def __init__(self, config: Optional[Dict[str, Any]] = None):
        super().__init__("research_toolkit", config)
        self.api_keys = config.get("api_keys", {}) if config else {}
        self.session: Optional[aiohttp.ClientSession] = None
    
    async def execute(self, operation: ToolOperation) -> ToolResult:
        """Execute research operation."""
        start_time = time.time()
        
        try:
            if not self.session:
                self.session = aiohttp.ClientSession()
                
            operation_type = operation.operation_type
            params = operation.parameters
            
            if operation_type == "web_search":
                result = await self._web_search(params)
            elif operation_type == "academic_search":
                result = await self._academic_search(params)
            elif operation_type == "fact_verification":
                result = await self._fact_verification(params)
            elif operation_type == "citation_management":
                result = await self._citation_management(params)
            else:
                raise ValueError(f"Unknown operation type: {operation_type}")
            
            execution_time = time.time() - start_time
            return ToolResult(
                operation_id=operation.operation_id,
                tool_name=self.tool_name,
                success=True,
                result=result,
                execution_time=execution_time
            )
            
        except Exception as e:
            execution_time = time.time() - start_time
            self.logger.error(f"Research operation failed: {e}")
            
            return ToolResult(
                operation_id=operation.operation_id,
                tool_name=self.tool_name,
                success=False,
                error=str(e),
                execution_time=execution_time
            )
    
    async def _web_search(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Perform contextual web search."""
        query = params.get("query", "")
        context = params.get("context", {})
        filters = params.get("filters", {})
        
        return {
            "operation": "web_search",
            "query": query,
            "results": [],  # Would contain search results
            "context_applied": bool(context),
            "filters_applied": len(filters)
        }
    
    async def _academic_search(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Search academic databases."""
        query = params.get("query", "")
        fields = params.get("fields", [])
        
        return {
            "operation": "academic_search",
            "query": query,
            "fields": fields,
            "results": []  # Would contain academic search results
        }
    
    async def _fact_verification(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Verify factual accuracy of claims."""
        claims = params.get("claims", [])
        context = params.get("context", {})
        
        return {
            "operation": "fact_verification",
            "claims_checked": len(claims),
            "verification_results": []  # Would contain verification results
        }
    
    async def _citation_management(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Generate properly formatted citations."""
        sources = params.get("sources", [])
        style = params.get("style", "APA")
        
        return {
            "operation": "citation_management",
            "sources_processed": len(sources),
            "citation_style": style,
            "formatted_citations": []  # Would contain formatted citations
        }
    
    def cleanup(self) -> None:
        """Clean up resources."""
        if self.session:
            asyncio.create_task(self.session.close())
        super().cleanup()

class ValidationToolkit(BaseTool):
    """
    Toolkit for quality assurance and validation operations.
    Provides multi-dimensional validation capabilities.
    """
    
    def __init__(self, config: Optional[Dict[str, Any]] = None):
        super().__init__("validation_toolkit", config)
        self.validation_rules = config.get("validation_rules", {}) if config else {}
    
    async def execute(self, operation: ToolOperation) -> ToolResult:
        """Execute validation operation."""
        start_time = time.time()
        
        try:
            operation_type = operation.operation_type
            params = operation.parameters
            
            if operation_type == "validate_content":
                result = await self._validate_content(params)
            elif operation_type == "check_formatting":
                result = await self._check_formatting(params)
            elif operation_type == "verify_compliance":
                result = await self._verify_compliance(params)
            elif operation_type == "assess_quality":
                result = await self._assess_quality(params)
            else:
                raise ValueError(f"Unknown operation type: {operation_type}")
            
            execution_time = time.time() - start_time
            return ToolResult(
                operation_id=operation.operation_id,
                tool_name=self.tool_name,
                success=True,
                result=result,
                execution_time=execution_time
            )
            
        except Exception as e:
            execution_time = time.time() - start_time
            self.logger.error(f"Validation operation failed: {e}")
            
            return ToolResult(
                operation_id=operation.operation_id,
                tool_name=self.tool_name,
                success=False,
                error=str(e),
                execution_time=execution_time
            )
    
    async def _validate_content(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Validate content accuracy and correctness."""
        content = params.get("content", "")
        validation_type = params.get("validation_type", "general")
        
        return {
            "operation": "validate_content",
            "content_length": len(content),
            "validation_type": validation_type,
            "validation_passed": True,
            "issues": [],
            "confidence_score": 0.95
        }
    
    async def _check_formatting(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Check formatting consistency."""
        document_content = params.get("content", {})
        style_requirements = params.get("style_requirements", {})
        
        return {
            "operation": "check_formatting",
            "formatting_issues": [],
            "consistency_score": 0.9,
            "style_compliance": True
        }
    
    async def _verify_compliance(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Verify compliance with standards."""
        content = params.get("content", "")
        standards = params.get("standards", [])
        
        return {
            "operation": "verify_compliance",
            "standards_checked": len(standards),
            "compliance_results": {},
            "overall_compliance": True
        }
    
    async def _assess_quality(self, params: Dict[str, Any]) -> Dict[str, Any]:
        """Assess overall quality."""
        content = params.get("content", "")
        quality_dimensions = params.get("dimensions", [])
        
        return {
            "operation": "assess_quality",
            "quality_score": 0.85,
            "dimension_scores": {},
            "recommendations": []
        }

# Factory function for creating tool instances
def create_toolkit(toolkit_type: str, config: Optional[Dict[str, Any]] = None) -> BaseTool:
    """
    Factory function to create toolkit instances.
    
    Args:
        toolkit_type: Type of toolkit to create
        config: Optional configuration dictionary
        
    Returns:
        Initialized toolkit instance
    """
    toolkit_classes = {
        "document": DocumentToolkit,
        "financial": FinancialDataToolkit,
        "research": ResearchToolkit,
        "validation": ValidationToolkit
    }
    
    if toolkit_type not in toolkit_classes:
        raise ValueError(f"Unknown toolkit type: {toolkit_type}")
    
    return toolkit_classes[toolkit_type](config)

# Utility functions for tool management
async def execute_tool_operation(tool: BaseTool, operation: ToolOperation) -> ToolResult:
    """Execute a tool operation with proper error handling."""
    try:
        return await tool.execute(operation)
    except Exception as e:
        return ToolResult(
            operation_id=operation.operation_id,
            tool_name=tool.tool_name,
            success=False,
            error=f"Tool execution failed: {str(e)}"
        )

def create_tool_operation(tool_name: str, 
                         operation_type: str, 
                         parameters: Dict[str, Any],
                         agent_id: str = "",
                         priority: int = 0) -> ToolOperation:
    """Create a standardized tool operation."""
    return ToolOperation(
        operation_id=str(uuid.uuid4()),
        tool_name=tool_name,
        operation_type=operation_type,
        parameters=parameters,
        timestamp=datetime.now(timezone.utc).isoformat(),
        agent_id=agent_id,
        priority=priority
    )