"""
Data Analyst Agent
Fetches and processes financial data from external APIs (Alpha Vantage)
"""

import json
import os
import requests
import time
from datetime import datetime
from typing import Dict, Any, List, Optional
from langchain_core.messages import HumanMessage
from .agent_state import AgentState, FinancialData
import logging

logger = logging.getLogger(__name__)

class DataAnalystAgent:
    """
    Specialized agent for fetching and processing financial data
    Uses Alpha Vantage API and LLM for parameter extraction
    """
    
    def __init__(self, llm):
        """Initialize with LLM client"""
        self.llm = llm
        self.default_symbols = ["AAPL", "GOOGL", "MSFT", "AMZN", "TSLA"]
        self.alpha_vantage_api_key = os.getenv("ALPHA_VANTAGE_API_KEY")
        self.base_url = "https://www.alphavantage.co/query"
        
        if not self.alpha_vantage_api_key:
            logger.warning("ALPHA_VANTAGE_API_KEY not found in environment variables")
            print("⚠️ WARNING: ALPHA_VANTAGE_API_KEY not set. Financial data fetching will use mock data.")
    
    async def process(self, state: AgentState) -> AgentState:
        """
        Process financial data requirements and fetch data
        
        Args:
            state: Current agent state
            
        Returns:
            Updated state with financial data
        """
        print(f"🟠 DATA_ANALYST: Processing financial data request")
        print(f"🟠 DATA_ANALYST: User request: {state.user_request}")
        
        try:
            # Extract financial parameters using LLM
            print(f"🟠 DATA_ANALYST: Extracting financial parameters...")
            params = await self._extract_financial_params(state.user_request)
            print(f"🟠 DATA_ANALYST: Extracted params: {params}")
            
            # Fetch financial data
            print(f"🟠 DATA_ANALYST: Starting financial data fetch with params: {params}")
            financial_data = await self._fetch_financial_data(params)
            print(f"🟠 DATA_ANALYST: Financial data fetch completed: {financial_data}")
            
            # Update state
            state.financial_data = financial_data
            state.add_processing_step(f"Fetched financial data for: {params.get('symbols', [])}")
            
            print(f"🟠 DATA_ANALYST: State updated with financial data")
            return state
            
        except Exception as e:
            state.add_error(f"Financial data fetch failed: {str(e)}")
            
            # Create minimal fallback financial data
            fallback_data = FinancialData(
                formatted_summary="Financial data temporarily unavailable",
                data_source="fallback",
                timestamp=datetime.now().isoformat()
            )
            state.financial_data = fallback_data
            state.add_processing_step("Using fallback financial data due to error")
            
            return state
    
    async def _extract_financial_params(self, user_request: str) -> Dict[str, Any]:
        """
        Use LLM to extract financial parameters from user request
        
        Args:
            user_request: User's natural language request
            
        Returns:
            Dict with extracted parameters
        """
        prompt = f"""
        Extract financial parameters from this request:
        
        Request: "{user_request}"
        
        Identify:
        1. Stock symbols (e.g., AAPL, GOOGL, MSFT)
        2. Time period (e.g., daily, weekly, monthly, yearly)
        3. Data types needed (price, volume, financials, news)
        
        Respond with ONLY a valid JSON object:
        {{
            "symbols": ["AAPL", "GOOGL"],
            "period": "1d",
            "data_types": ["price", "volume"],
            "analysis_focus": "brief description of what user wants to know"
        }}
        
        Default to AAPL if no symbols found.
        Default to "1d" period if not specified.
        """
        
        message = HumanMessage(content=prompt)
        response = await self.llm.ainvoke([message])
        
        try:
            params = json.loads(response.content.strip())
            
            # Validate and set defaults
            symbols = params.get("symbols", ["AAPL"])
            if not symbols or not isinstance(symbols, list):
                symbols = ["AAPL"]
            
            period = params.get("period", "1d")
            if period not in ["1d", "5d", "1mo", "3mo", "6mo", "1y", "2y", "5y", "10y", "ytd", "max"]:
                period = "1d"
            
            return {
                "symbols": symbols[:5],  # Limit to 5 symbols
                "period": period,
                "data_types": params.get("data_types", ["price"]),
                "analysis_focus": params.get("analysis_focus", "general financial information")
            }
            
        except (json.JSONDecodeError, KeyError):
            # LLM-powered fallback extraction
            fallback_params = await self._llm_fallback_extraction(user_request)
            return fallback_params
    
    async def _llm_fallback_extraction(self, user_request: str) -> Dict[str, Any]:
        """
        LLM-powered fallback method for parameter extraction
        
        Args:
            user_request: User's request
            
        Returns:
            Dict with extracted parameters
        """
        prompt = f"""
        Extract financial parameters from this request as a fallback when JSON parsing fails:
        
        Request: "{user_request}"
        
        Analyze the request and extract:
        1. Any stock symbols mentioned (common symbols: AAPL, GOOGL, MSFT, AMZN, TSLA, META, NFLX, NVDA)
        2. Time period if specified (default to "1d")
        3. What type of financial analysis is needed
        
        If no specific symbols are mentioned, default to AAPL.
        
        Respond with a simple list format:
        SYMBOLS: [list of symbols]
        PERIOD: [time period]
        ANALYSIS: [brief description]
        """
        
        message = HumanMessage(content=prompt)
        response = await self.llm.ainvoke([message])
        
        # Parse the simple response format
        content = response.content.strip()
        
        # Extract symbols
        symbols = ["AAPL"]  # Default
        if "SYMBOLS:" in content:
            symbols_line = content.split("SYMBOLS:")[1].split("\n")[0].strip()
            # Simple extraction of symbols from the response
            import re
            found_symbols = re.findall(r'\b[A-Z]{2,5}\b', symbols_line)
            if found_symbols:
                symbols = found_symbols[:5]
        
        # Extract period
        period = "1d"  # Default
        if "PERIOD:" in content:
            period_line = content.split("PERIOD:")[1].split("\n")[0].strip()
            valid_periods = ["1d", "5d", "1mo", "3mo", "6mo", "1y", "2y", "5y", "10y", "ytd", "max"]
            for p in valid_periods:
                if p in period_line:
                    period = p
                    break
        
        # Extract analysis focus
        analysis_focus = "general financial information"
        if "ANALYSIS:" in content:
            analysis_focus = content.split("ANALYSIS:")[1].split("\n")[0].strip()
        
        return {
            "symbols": symbols,
            "period": period,
            "data_types": ["price"],
            "analysis_focus": analysis_focus
        }
    
    async def _fetch_financial_data(self, params: Dict[str, Any]) -> FinancialData:
        """
        Fetch financial data from Alpha Vantage API
        
        Args:
            params: Parameters extracted from user request
            
        Returns:
            FinancialData object with fetched information
        """
        symbols = params["symbols"]
        period = params["period"]
        
        print(f"🔴 ALPHA_VANTAGE: Starting fetch for symbols: {symbols}, period: {period}")
        
        # Check if API key is available
        if not self.alpha_vantage_api_key:
            print(f"🔴 ALPHA_VANTAGE: No API key found, using mock data")
            return self._get_mock_financial_data(symbols)
        
        stock_prices = {}
        financial_metrics = {}
        summaries = []
        
        for symbol in symbols:
            print(f"🔴 ALPHA_VANTAGE: Processing symbol: {symbol}")
            try:
                # Rate limiting: Alpha Vantage free tier allows 5 requests per minute
                time.sleep(12)  # Wait 12 seconds between requests to stay within limits
                
                # Get quote data (current price, change, etc.)
                quote_params = {
                    "function": "GLOBAL_QUOTE",
                    "symbol": symbol,
                    "apikey": self.alpha_vantage_api_key
                }
                
                print(f"🔴 ALPHA_VANTAGE: Fetching quote for {symbol}")
                quote_response = requests.get(self.base_url, params=quote_params, timeout=10)
                quote_data = quote_response.json()
                
                if "Global Quote" in quote_data and quote_data["Global Quote"]:
                    quote = quote_data["Global Quote"]
                    
                    current_price = float(quote.get("05. price", 0))
                    change = float(quote.get("09. change", 0))
                    change_percent = quote.get("10. change percent", "0%").rstrip('%')
                    volume = int(quote.get("06. volume", 0))
                    
                    print(f"🔴 ALPHA_VANTAGE: {symbol} - Current: ${current_price:.2f}, Change: {change_percent}%")
                    
                    # Store price data
                    stock_prices[symbol] = {
                        "current_price": round(current_price, 2),
                        "change": round(change, 2),
                        "change_percent": f"{float(change_percent):+.1f}%",
                        "volume": volume,
                        "currency": "USD"
                    }
                    
                    # Get company overview for additional metrics
                    time.sleep(12)  # Rate limiting
                    overview_params = {
                        "function": "OVERVIEW",
                        "symbol": symbol,
                        "apikey": self.alpha_vantage_api_key
                    }
                    
                    overview_response = requests.get(self.base_url, params=overview_params, timeout=10)
                    overview_data = overview_response.json()
                    
                    # Store financial metrics
                    financial_metrics[symbol] = {
                        "market_cap": int(overview_data.get("MarketCapitalization", 0)) if overview_data.get("MarketCapitalization") else None,
                        "pe_ratio": float(overview_data.get("PERatio", 0)) if overview_data.get("PERatio") and overview_data.get("PERatio") != "None" else None,
                        "forward_pe": float(overview_data.get("ForwardPE", 0)) if overview_data.get("ForwardPE") and overview_data.get("ForwardPE") != "None" else None,
                        "dividend_yield": float(overview_data.get("DividendYield", 0)) if overview_data.get("DividendYield") else None,
                        "52_week_high": float(overview_data.get("52WeekHigh", 0)) if overview_data.get("52WeekHigh") else None,
                        "52_week_low": float(overview_data.get("52WeekLow", 0)) if overview_data.get("52WeekLow") else None
                    }
                    
                    # Create summary
                    company_name = overview_data.get("Name", symbol)
                    summary = f"{company_name} ({symbol}) is trading at ${current_price:.2f}, {float(change_percent):+.1f}%"
                    
                    if overview_data.get("MarketCapitalization"):
                        market_cap_b = int(overview_data["MarketCapitalization"]) / 1e9
                        summary += f" with a market cap of ${market_cap_b:.1f}B"
                    
                    summaries.append(summary)
                else:
                    # API limit reached or invalid response
                    print(f"🔴 ALPHA_VANTAGE: No data for {symbol}, may have hit rate limit")
                    summaries.append(f"{symbol}: Data unavailable (API limit or invalid symbol)")
            
            except Exception as e:
                # Handle individual symbol errors
                print(f"🔴 ALPHA_VANTAGE: Error fetching {symbol}: {str(e)}")
                summaries.append(f"{symbol}: Data unavailable ({str(e)[:50]})")
        
        # Create formatted summary
        formatted_summary = ". ".join(summaries) if summaries else "No financial data available"
        
        print(f"🔴 ALPHA_VANTAGE: Final result - Stock prices: {stock_prices}")
        print(f"🔴 ALPHA_VANTAGE: Final result - Financial metrics: {financial_metrics}")
        print(f"🔴 ALPHA_VANTAGE: Final result - Summary: {formatted_summary}")
        
        financial_data = FinancialData(
            stock_prices=stock_prices,
            financial_metrics=financial_metrics,
            formatted_summary=formatted_summary,
            data_source="alpha_vantage",
            timestamp=datetime.now().isoformat()
        )
        
        print(f"🔴 ALPHA_VANTAGE: Created FinancialData object: {financial_data}")
        return financial_data
    
    def _get_mock_financial_data(self, symbols: List[str]) -> FinancialData:
        """
        Generate mock financial data for testing when API is not available
        
        Args:
            symbols: List of stock symbols
            
        Returns:
            FinancialData object with mock data
        """
        import random
        
        stock_prices = {}
        financial_metrics = {}
        summaries = []
        
        for symbol in symbols:
            # Generate realistic mock data
            base_price = random.uniform(50, 500)
            change_percent = random.uniform(-5, 5)
            change = base_price * (change_percent / 100)
            
            stock_prices[symbol] = {
                "current_price": round(base_price, 2),
                "change": round(change, 2),
                "change_percent": f"{change_percent:+.1f}%",
                "volume": random.randint(1000000, 50000000),
                "currency": "USD"
            }
            
            financial_metrics[symbol] = {
                "market_cap": random.randint(10000000000, 3000000000000),
                "pe_ratio": round(random.uniform(10, 40), 2),
                "forward_pe": round(random.uniform(10, 35), 2),
                "dividend_yield": round(random.uniform(0, 5), 2) / 100,
                "52_week_high": round(base_price * random.uniform(1.1, 1.5), 2),
                "52_week_low": round(base_price * random.uniform(0.6, 0.9), 2)
            }
            
            market_cap_b = financial_metrics[symbol]["market_cap"] / 1e9
            summaries.append(
                f"{symbol} is trading at ${base_price:.2f}, {change_percent:+.1f}% "
                f"with a market cap of ${market_cap_b:.1f}B"
            )
        
        financial_data = FinancialData(
            stock_prices=stock_prices,
            financial_metrics=financial_metrics,
            formatted_summary=". ".join(summaries),
            data_source="mock_data",
            timestamp=datetime.now().isoformat()
        )
        
        return financial_data