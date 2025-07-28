"""
Intent Classifier Node
Analyzes user requests and determines operation type and parameters
"""

import json
import re
from typing import Dict, Any
from langchain_core.messages import HumanMessage
from .agent_state import AgentState, OperationType, IntentClassification

class IntentClassifierNode:
    """
    Intent classifier that determines which operation to perform
    and whether financial data is required
    """
    
    def __init__(self, llm):
        """Initialize with LLM client"""
        self.llm = llm
    
    async def process(self, state: AgentState) -> AgentState:
        """
        Process the user request to classify intent
        
        Args:
            state: Current agent state
            
        Returns:
            Updated state with intent classification
        """
        print(f"🟢 INTENT_CLASSIFIER: Processing request: {state.user_request}")
        print(f"🟢 INTENT_CLASSIFIER: Context: {state.context}")
        
        try:
            # Analyze intent and financial data requirements using LLM
            print(f"🟢 INTENT_CLASSIFIER: Calling LLM for intent classification...")
            intent_result = await self._classify_intent_and_financial_needs(
                state.user_request, 
                state.context
            )
            print(f"🟢 INTENT_CLASSIFIER: LLM result: {intent_result}")
            
            # Create intent classification object
            intent_classification = IntentClassification(
                operation_type=intent_result["operation_type"],
                confidence=intent_result["confidence"],
                requires_financial_data=intent_result["requires_financial_data"],
                extracted_parameters=intent_result["parameters"]
            )
            
            # Update state
            state.intent_classification = intent_classification
            state.add_processing_step(f"Intent classified as: {intent_result['operation_type']}")
            
            if intent_result["requires_financial_data"]:
                state.add_processing_step("Financial data required for this request")
            
            return state
            
        except Exception as e:
            state.add_error(f"Intent classification failed: {str(e)}")
            
            # Fallback to default intent
            fallback_intent = IntentClassification(
                operation_type=OperationType.GENERATE_CONTENT,
                confidence=0.5,
                requires_financial_data=False,
                extracted_parameters={}
            )
            state.intent_classification = fallback_intent
            state.add_processing_step("Using fallback intent: generate_content")
            
            return state
    
    async def _classify_intent_and_financial_needs(self, user_request: str, context: Dict[str, Any]) -> Dict[str, Any]:
        """
        Use LLM to classify the user's intent and determine financial data needs
        
        Args:
            user_request: User's natural language request
            context: Document context
            
        Returns:
            Dict with operation_type, confidence, requires_financial_data, and parameters
        """
        prompt = f"""
        Analyze the user request and classify the intent into one of these categories:
        
        1. GENERATE_CONTENT - User wants to insert new text/content
        2. CLASSIFY_FORMATTING - User wants to format existing selected text
        3. CREATE_CHART - User wants to insert a chart/graph
        4. CREATE_TABLE - User wants to insert a table
        
        Also determine if the request requires financial data (stock prices, market data, earnings, etc.).
        Financial data is ONLY needed for GENERATE_CONTENT operations that specifically mention:
        - Stock symbols (AAPL, GOOGL, etc.)
        - Financial metrics (price, earnings, P/E ratio, market cap, etc.)
        - Market data or financial analysis
        
        User Request: "{user_request}"
        Document Context: {json.dumps(context, indent=2)}
        
        Respond with ONLY a valid JSON object in this format:
        {{
            "operation_type": "generate_content|classify_formatting|create_chart|create_table",
            "confidence": 0.95,
            "requires_financial_data": true|false,
            "reasoning": "Brief explanation of classification and financial data decision",
            "parameters": {{
                "extracted_param": "relevant parameters for the operation"
            }}
        }}
        
        Examples:
        - "Make this text bold" → {{"operation_type": "classify_formatting", "requires_financial_data": false}}
        - "Insert AAPL stock analysis" → {{"operation_type": "generate_content", "requires_financial_data": true}}
        - "Create a bar chart" → {{"operation_type": "create_chart", "requires_financial_data": false}}
        - "Write a quarterly report" → {{"operation_type": "generate_content", "requires_financial_data": false}}
        """
        
        message = HumanMessage(content=prompt)
        response = await self.llm.ainvoke([message])
        
        try:
            # Parse JSON response
            result = json.loads(response.content.strip())
            
            # Validate operation type
            valid_operations = {
                "generate_content": OperationType.GENERATE_CONTENT,
                "classify_formatting": OperationType.CLASSIFY_FORMATTING, 
                "create_chart": OperationType.CREATE_CHART,
                "create_table": OperationType.CREATE_TABLE
            }
            
            operation_str = result.get("operation_type", "generate_content")
            operation_type = valid_operations.get(operation_str, OperationType.GENERATE_CONTENT)
            
            return {
                "operation_type": operation_type,
                "confidence": min(max(result.get("confidence", 0.8), 0.0), 1.0),
                "requires_financial_data": result.get("requires_financial_data", False),
                "reasoning": result.get("reasoning", ""),
                "parameters": result.get("parameters", {})
            }
            
        except (json.JSONDecodeError, KeyError) as e:
            # Fallback parsing if JSON fails
            content = response.content.lower()
            
            if "format" in content or "bold" in content or "italic" in content:
                operation_type = OperationType.CLASSIFY_FORMATTING
            elif "chart" in content or "graph" in content:
                operation_type = OperationType.CREATE_CHART
            elif "table" in content:
                operation_type = OperationType.CREATE_TABLE
            else:
                operation_type = OperationType.GENERATE_CONTENT
            
            return {
                "operation_type": operation_type,
                "confidence": 0.7,
                "requires_financial_data": False,
                "reasoning": "Fallback classification due to parsing error",
                "parameters": {}
            }
    
