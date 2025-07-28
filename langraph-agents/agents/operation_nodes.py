"""
Operation Nodes
Implements the 4 core operation types: Generate Content, Classify Formatting, Create Chart, Create Table
"""

import json
from typing import Dict, Any
from langchain_core.messages import HumanMessage
from .agent_state import AgentState, OperationResult

class GenerateContentNode:
    """
    Generate content operation - enhanced with financial data support
    """
    
    def __init__(self, llm):
        """Initialize with LLM client"""
        self.llm = llm
    
    async def process(self, state: AgentState) -> AgentState:
        """
        Generate text content for insertion
        
        Args:
            state: Current agent state
            
        Returns:
            Updated state with operation result
        """
        try:
            # Generate content using LLM
            content = await self._generate_content(
                state.user_request,
                state.context,
                state.financial_data
            )
            
            # Create operation result
            operation_result = OperationResult(
                operation_type="insert",
                content=content,
                metadata={
                    "content_type": "text",
                    "length": len(content),
                    "has_financial_data": state.has_financial_data()
                }
            )
            
            state.operation_result = operation_result
            state.add_processing_step(f"Generated content ({len(content)} characters)")
            
            return state
            
        except Exception as e:
            state.add_error(f"Content generation failed: {str(e)}")
            
            # Fallback content
            fallback_content = "I apologize, but I encountered an error generating the requested content."
            operation_result = OperationResult(
                operation_type="insert",
                content=fallback_content,
                metadata={"error": str(e)}
            )
            state.operation_result = operation_result
            
            return state
    
    async def _generate_content(self, user_request: str, context: Dict[str, Any], financial_data=None) -> str:
        """
        Use LLM to generate appropriate content
        
        Args:
            user_request: User's request
            context: Document context
            financial_data: Optional financial data
            
        Returns:
            Generated text content
        """
        # Build prompt with context
        prompt_parts = [
            f"Generate appropriate text content for this request: {user_request}",
            f"Document context: {json.dumps(context, indent=2)}"
        ]
        
        # Add financial data if available
        if financial_data and financial_data.formatted_summary:
            prompt_parts.append(f"Financial data: {financial_data.formatted_summary}")
            
            if financial_data.stock_prices:
                prompt_parts.append(f"Stock prices: {json.dumps(financial_data.stock_prices, indent=2)}")
        
        prompt_parts.extend([
            "",
            "Generate clear, professional content that directly addresses the user's request.",
            "If financial data is provided, incorporate it naturally into the content.",
            "Keep the content concise and relevant to the document context.",
            "Respond with ONLY the content text, no explanations or meta-commentary."
        ])
        
        prompt = "\n".join(prompt_parts)
        
        message = HumanMessage(content=prompt)
        response = await self.llm.ainvoke([message])
        
        return response.content.strip()

class ClassifyFormattingNode:
    """
    Classify formatting operation - determines formatting specifications
    """
    
    def __init__(self, llm):
        """Initialize with LLM client"""
        self.llm = llm
    
    async def process(self, state: AgentState) -> AgentState:
        """
        Classify formatting requirements
        
        Args:
            state: Current agent state
            
        Returns:
            Updated state with operation result
        """
        try:
            # Classify formatting using LLM
            formatting = await self._classify_formatting(
                state.user_request,
                state.context
            )
            
            # Create operation result
            operation_result = OperationResult(
                operation_type="format",
                formatting=formatting,
                metadata={
                    "selected_text_length": len(state.context.get("selected_text", "")),
                    "formatting_scope": "selection"
                }
            )
            
            state.operation_result = operation_result
            state.add_processing_step("Classified formatting requirements")
            
            return state
            
        except Exception as e:
            state.add_error(f"Formatting classification failed: {str(e)}")
            
            # Fallback formatting
            fallback_formatting = {
                "bold": "false",
                "italic": "false",
                "underline": "false",
                "font_size": "12",
                "color": "#000000"
            }
            
            operation_result = OperationResult(
                operation_type="format",
                formatting=fallback_formatting,
                metadata={"error": str(e)}
            )
            state.operation_result = operation_result
            
            return state
    
    async def _classify_formatting(self, user_request: str, context: Dict[str, Any]) -> Dict[str, str]:
        """
        Use LLM to determine formatting specifications
        
        Args:
            user_request: User's formatting request
            context: Document context
            
        Returns:
            Dict with formatting properties
        """
        selected_text = context.get("selected_text", "")
        
        prompt = f"""
        Analyze this formatting request and determine the formatting properties:
        
        Request: "{user_request}"
        Selected text: "{selected_text}"
        
        Respond with ONLY a valid JSON object with these exact fields:
        {{
            "bold": "true|false",
            "italic": "true|false",
            "underline": "true|false",
            "strikethrough": "true|false",
            "font_size": "12",
            "color": "#000000"
        }}
        
        Examples:
        - "Make this bold" → {{"bold": "true", "italic": "false", "underline": "false", "strikethrough": "false", "font_size": "12", "color": "#000000"}}
        - "Remove formatting" → all properties set to default values
        - "Make it red and italic" → {{"bold": "false", "italic": "true", "underline": "false", "strikethrough": "false", "font_size": "12", "color": "#FF0000"}}
        """
        
        message = HumanMessage(content=prompt)
        response = await self.llm.ainvoke([message])
        
        try:
            formatting = json.loads(response.content.strip())
            
            # Validate and set defaults
            validated_formatting = {
                "bold": str(formatting.get("bold", "false")).lower(),
                "italic": str(formatting.get("italic", "false")).lower(),
                "underline": str(formatting.get("underline", "false")).lower(),
                "strikethrough": str(formatting.get("strikethrough", "false")).lower(),
                "font_size": str(formatting.get("font_size", "12")),
                "color": formatting.get("color", "#000000")
            }
            
            return validated_formatting
            
        except json.JSONDecodeError:
            # Fallback parsing
            content = response.content.lower()
            return {
                "bold": "true" if "bold" in content else "false",
                "italic": "true" if "italic" in content else "false",
                "underline": "true" if "underline" in content else "false",
                "strikethrough": "true" if "strikethrough" in content else "false",
                "font_size": "12",
                "color": "#FF0000" if "red" in content else "#000000"
            }

class CreateChartNode:
    """
    Create chart operation - generates blank chart specifications
    """
    
    def __init__(self, llm):
        """Initialize with LLM client"""
        self.llm = llm
    
    async def process(self, state: AgentState) -> AgentState:
        """
        Create chart specifications
        
        Args:
            state: Current agent state
            
        Returns:
            Updated state with operation result
        """
        try:
            # Determine chart type
            chart_type = await self._determine_chart_type(state.user_request)
            
            # Create operation result
            operation_result = OperationResult(
                operation_type="chart",
                chart_type=chart_type,
                metadata={
                    "chart_category": "blank",
                    "complexity": "simple"
                }
            )
            
            state.operation_result = operation_result
            state.add_processing_step(f"Created {chart_type} chart specification")
            
            return state
            
        except Exception as e:
            state.add_error(f"Chart creation failed: {str(e)}")
            
            # Fallback chart
            operation_result = OperationResult(
                operation_type="chart",
                chart_type="bar",
                metadata={"error": str(e)}
            )
            state.operation_result = operation_result
            
            return state
    
    async def _determine_chart_type(self, user_request: str) -> str:
        """
        Use LLM to determine appropriate chart type from request
        
        Args:
            user_request: User's chart request
            
        Returns:
            Chart type string
        """
        prompt = f"""
        Analyze this chart creation request and determine the most appropriate chart type:
        
        Request: "{user_request}"
        
        Available chart types:
        - bar: Good for comparing categories/values
        - line: Good for trends over time
        - pie: Good for showing parts of a whole (percentages)
        - column: Good for vertical comparisons
        
        Consider the context and purpose of the chart request.
        
        Respond with ONLY one of these exact words: bar, line, pie, column
        
        Examples:
        - "Create a sales chart" → bar
        - "Show trends over time" → line
        - "Display market share" → pie
        - "Compare quarterly results" → column
        """
        
        message = HumanMessage(content=prompt)
        response = await self.llm.ainvoke([message])
        
        chart_type = response.content.strip().lower()
        
        # Validate response
        valid_types = ["bar", "line", "pie", "column"]
        if chart_type in valid_types:
            return chart_type
        else:
            return "bar"  # Default fallback

class CreateTableNode:
    """
    Create table operation - generates blank table specifications
    """
    
    def __init__(self, llm):
        """Initialize with LLM client"""
        self.llm = llm
    
    async def process(self, state: AgentState) -> AgentState:
        """
        Create table specifications
        
        Args:
            state: Current agent state
            
        Returns:
            Updated state with operation result
        """
        try:
            # Determine table dimensions
            dimensions = await self._determine_table_dimensions(state.user_request)
            
            # Create operation result
            operation_result = OperationResult(
                operation_type="table",
                rows=dimensions["rows"],
                columns=dimensions["columns"],
                metadata={
                    "table_purpose": "blank",
                    "estimated_size": "medium" if dimensions["rows"] * dimensions["columns"] > 12 else "small"
                }
            )
            
            state.operation_result = operation_result
            state.add_processing_step(f"Created {dimensions['rows']}x{dimensions['columns']} table specification")
            
            return state
            
        except Exception as e:
            state.add_error(f"Table creation failed: {str(e)}")
            
            # Fallback table
            operation_result = OperationResult(
                operation_type="table",
                rows=3,
                columns=3,
                metadata={"error": str(e)}
            )
            state.operation_result = operation_result
            
            return state
    
    async def _determine_table_dimensions(self, user_request: str) -> Dict[str, int]:
        """
        Use LLM to extract table dimensions from request
        
        Args:
            user_request: User's table request
            
        Returns:
            Dict with rows and columns
        """
        prompt = f"""
        Analyze this table creation request and determine appropriate dimensions:
        
        Request: "{user_request}"
        
        Extract or infer:
        1. Number of rows needed (1-50 range)
        2. Number of columns needed (1-20 range)
        
        If specific dimensions are mentioned, use those.
        If not specified, suggest reasonable dimensions based on the purpose.
        
        Respond with ONLY this format:
        ROWS: [number]
        COLUMNS: [number]
        
        Examples:
        - "Create a 3x4 table" → ROWS: 3, COLUMNS: 4
        - "Add a table for quarterly data" → ROWS: 5, COLUMNS: 4
        - "Insert a table" → ROWS: 3, COLUMNS: 3
        - "Make a contact list table" → ROWS: 6, COLUMNS: 3
        """
        
        message = HumanMessage(content=prompt)
        response = await self.llm.ainvoke([message])
        
        content = response.content.strip()
        
        # Extract dimensions from response
        rows = 3  # Default
        columns = 3  # Default
        
        if "ROWS:" in content:
            try:
                rows_line = content.split("ROWS:")[1].split("\n")[0].strip()
                rows = int(''.join(filter(str.isdigit, rows_line)))
                rows = min(max(rows, 1), 50)  # Limit to 1-50
            except (ValueError, IndexError):
                rows = 3
        
        if "COLUMNS:" in content:
            try:
                columns_line = content.split("COLUMNS:")[1].split("\n")[0].strip()
                columns = int(''.join(filter(str.isdigit, columns_line)))
                columns = min(max(columns, 1), 20)  # Limit to 1-20
            except (ValueError, IndexError):
                columns = 3
        
        return {"rows": rows, "columns": columns}