"""
Agent State Definition for LangGraph Orchestrator
Defines the shared state structure used across all agent nodes
"""

from typing import Dict, Any, Optional, List
from pydantic import BaseModel
from enum import Enum

class OperationType(str, Enum):
    """Supported operation types"""
    GENERATE_CONTENT = "generate_content"
    CLASSIFY_FORMATTING = "classify_formatting"
    CREATE_CHART = "create_chart"
    CREATE_TABLE = "create_table"

class IntentClassification(BaseModel):
    """Intent classification result"""
    operation_type: OperationType
    confidence: float
    requires_financial_data: bool
    extracted_parameters: Dict[str, Any]

class FinancialData(BaseModel):
    """Financial data structure from Data Analyst Agent"""
    stock_prices: Optional[Dict[str, Any]] = None
    financial_metrics: Optional[Dict[str, Any]] = None
    formatted_summary: Optional[str] = None
    data_source: Optional[str] = None
    timestamp: Optional[str] = None

class OperationResult(BaseModel):
    """Result from operation nodes"""
    operation_type: str
    content: Optional[str] = None
    formatting: Optional[Dict[str, Any]] = None
    rows: Optional[int] = None
    columns: Optional[int] = None
    chart_type: Optional[str] = None
    metadata: Optional[Dict[str, Any]] = None

class AgentState(BaseModel):
    """
    Shared state object passed between all agent nodes
    Contains all information needed for processing requests
    """
    # Original request information
    user_request: str
    request_id: str
    context: Dict[str, Any]
    
    # Intent classification results
    intent_classification: Optional[IntentClassification] = None
    
    # Financial data (if required)
    financial_data: Optional[FinancialData] = None
    
    # Operation results
    operation_result: Optional[OperationResult] = None
    
    # Final response
    response_message: Optional[str] = None
    
    # Processing metadata
    processing_steps: List[str] = []
    errors: List[str] = []
    execution_time_ms: Optional[int] = None
    
    def add_processing_step(self, step: str):
        """Add a processing step to the log"""
        self.processing_steps.append(step)
    
    def add_error(self, error: str):
        """Add an error to the error log"""
        self.errors.append(error)
    
    def has_errors(self) -> bool:
        """Check if there are any processing errors"""
        return len(self.errors) > 0
    
    def get_operation_type(self) -> Optional[OperationType]:
        """Get the classified operation type"""
        if self.intent_classification:
            return self.intent_classification.operation_type
        return None
    
    def requires_financial_data(self) -> bool:
        """Check if financial data is required for this request"""
        if self.intent_classification:
            return self.intent_classification.requires_financial_data
        return False
    
    def has_financial_data(self) -> bool:
        """Check if financial data has been fetched"""
        return self.financial_data is not None
    
    def to_response_dict(self) -> Dict[str, Any]:
        """Convert state to final response dictionary matching data-models.md format"""
        if not self.operation_result:
            raise ValueError("No operation result available")
        
        # Build response according to data-models.md specification
        response = {
            "type": self.operation_result.operation_type,
            "response": self.response_message or "Operation completed successfully"
        }
        
        # Add operation-specific fields based on type (data-models.md format)
        if self.operation_result.operation_type == "format":
            # Format operation: type, response, formatting
            if self.operation_result.formatting:
                response["formatting"] = self.operation_result.formatting
            
        elif self.operation_result.operation_type == "insert":
            # Insert operation: type, response, content
            if self.operation_result.content:
                response["content"] = self.operation_result.content
                
        elif self.operation_result.operation_type == "table":
            # Table operation: type, response, rows, columns
            if self.operation_result.rows is not None:
                response["rows"] = self.operation_result.rows
            if self.operation_result.columns is not None:
                response["columns"] = self.operation_result.columns
                
        elif self.operation_result.operation_type == "chart":
            # Chart operation: type, response, chart_type
            if self.operation_result.chart_type:
                response["chart_type"] = self.operation_result.chart_type
        
        return response