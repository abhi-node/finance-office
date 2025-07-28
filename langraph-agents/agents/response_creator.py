"""
Response Creator Node
Creates final human-readable responses and formats JSON output
"""

from typing import Dict, Any
from langchain_core.messages import HumanMessage
from .agent_state import AgentState

class ResponseCreatorNode:
    """
    Final node that creates human-readable responses and validates output
    """
    
    def __init__(self, llm):
        """Initialize with LLM client"""
        self.llm = llm
    
    async def process(self, state: AgentState) -> AgentState:
        """
        Create final response message and validate output
        
        Args:
            state: Current agent state
            
        Returns:
            Updated state with response message
        """
        try:
            # Generate human-readable response
            response_message = await self._generate_response_message(
                state.user_request,
                state.operation_result,
                state.has_financial_data()
            )
            
            # Update state
            state.response_message = response_message
            state.add_processing_step("Created final response message")
            
            return state
            
        except Exception as e:
            state.add_error(f"Response creation failed: {str(e)}")
            
            # Fallback response
            state.response_message = "I've processed your request successfully."
            state.add_processing_step("Using fallback response message")
            
            return state
    
    async def _generate_response_message(self, user_request: str, operation_result, has_financial_data: bool) -> str:
        """
        Generate appropriate human-readable response
        
        Args:
            user_request: Original user request
            operation_result: Result from operation node
            has_financial_data: Whether financial data was used
            
        Returns:
            Human-readable response string
        """
        if not operation_result:
            return "I encountered an issue processing your request."
        
        # Create context for response generation
        operation_context = {
            "operation_type": operation_result.operation_type,
            "user_request": user_request,
            "has_financial_data": has_financial_data
        }
        
        # Add operation-specific context
        if operation_result.content:
            operation_context["content_length"] = len(operation_result.content)
        if operation_result.formatting:
            operation_context["formatting_applied"] = list(operation_result.formatting.keys())
        if operation_result.rows and operation_result.columns:
            operation_context["table_size"] = f"{operation_result.rows}x{operation_result.columns}"
        if operation_result.chart_type:
            operation_context["chart_type"] = operation_result.chart_type
        
        prompt = f"""
        Generate a concise, professional response explaining what was accomplished.
        
        User Request: "{user_request}"
        Operation: {operation_result.operation_type}
        Context: {operation_context}
        
        Create a response that:
        1. Confirms the action was completed
        2. Briefly describes what was done
        3. Is friendly and professional
        4. Is 1-2 sentences maximum
        
        Examples:
        - For content insertion: "I've inserted the requested content about [topic] at your cursor position."
        - For formatting: "I've applied [formatting] to your selected text."
        - For table: "I've created a [rows]x[columns] table for you."
        - For chart: "I've inserted a [type] chart at your cursor position."
        
        If financial data was used, mention it briefly.
        
        Respond with ONLY the response message, no quotes or explanations.
        """
        
        message = HumanMessage(content=prompt)
        response = await self.llm.ainvoke([message])
        
        return response.content.strip()
    
    def _get_default_response(self, operation_type: str) -> str:
        """
        Get default response message based on operation type
        
        Args:
            operation_type: Type of operation performed
            
        Returns:
            Default response message
        """
        default_responses = {
            "insert": "I've inserted the requested content at your cursor position.",
            "format": "I've applied the requested formatting to your selected text.",
            "table": "I've created a table for you.",
            "chart": "I've inserted a chart at your cursor position."
        }
        
        return default_responses.get(operation_type, "I've completed your request successfully.")