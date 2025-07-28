"""
LangGraph Agent Orchestrator
Main orchestration logic for the 7-node agent workflow
"""

import asyncio
import time
from typing import Dict, Any
from langgraph.graph import StateGraph, END
from langchain_openai import ChatOpenAI
from langchain_core.messages import HumanMessage

from .agent_state import AgentState, OperationType, IntentClassification, FinancialData, OperationResult
from .intent_classifier import IntentClassifierNode
from .data_analyst_agent import DataAnalystAgent
from .operation_nodes import (
    GenerateContentNode,
    ClassifyFormattingNode, 
    CreateChartNode,
    CreateTableNode
)
from .response_creator import ResponseCreatorNode

class AgentOrchestrator:
    """
    Main orchestrator class that manages the LangGraph workflow
    Coordinates all agent nodes and manages state transitions
    """
    
    def __init__(self):
        """Initialize the orchestrator with all nodes and workflow"""
        self.setup_llm_clients()
        self.setup_nodes()
        self.setup_workflow()
    
    def setup_llm_clients(self):
        """Initialize LLM clients for different nodes"""
        # GPT-4 for intent classification (high accuracy needed)
        self.intent_llm = ChatOpenAI(
            model="gpt-4",
            temperature=0.1,
            timeout=60
        )
        
        # GPT-4 for content generation (high quality needed)
        self.content_llm = ChatOpenAI(
            model="gpt-4",
            temperature=0.3,
            timeout=60
        )
        
        # GPT-4 for other operations (consistency across all nodes)
        self.operation_llm = ChatOpenAI(
            model="gpt-4",
            temperature=0.2,
            timeout=60
        )
    
    def setup_nodes(self):
        """Initialize all agent nodes"""
        self.intent_classifier = IntentClassifierNode(self.intent_llm)
        self.data_analyst = DataAnalystAgent(self.operation_llm)
        self.generate_content = GenerateContentNode(self.content_llm)
        self.classify_formatting = ClassifyFormattingNode(self.operation_llm)
        self.create_chart = CreateChartNode(self.operation_llm)
        self.create_table = CreateTableNode(self.operation_llm)
        self.response_creator = ResponseCreatorNode(self.operation_llm)
    
    def setup_workflow(self):
        """Setup the LangGraph workflow with conditional routing"""
        
        # Create the state graph
        workflow = StateGraph(AgentState)
        
        # Add all nodes
        workflow.add_node("intent_classifier", self.intent_classifier_node)
        workflow.add_node("data_analyst", self.data_analyst_node)
        workflow.add_node("generate_content", self.generate_content_node)
        workflow.add_node("classify_formatting", self.classify_formatting_node)
        workflow.add_node("create_chart", self.create_chart_node)
        workflow.add_node("create_table", self.create_table_node)
        workflow.add_node("response_creator", self.response_creator_node)
        
        # Set entry point
        workflow.set_entry_point("intent_classifier")
        
        # Add conditional routing from intent classifier
        workflow.add_conditional_edges(
            "intent_classifier",
            self.route_after_intent_classification,
            {
                "data_analyst": "data_analyst",
                "generate_content": "generate_content",
                "classify_formatting": "classify_formatting",
                "create_chart": "create_chart",
                "create_table": "create_table"
            }
        )
        
        # Route from data analyst to generate content
        workflow.add_edge("data_analyst", "generate_content")
        
        # All operation nodes go to response creator
        workflow.add_edge("generate_content", "response_creator")
        workflow.add_edge("classify_formatting", "response_creator")
        workflow.add_edge("create_chart", "response_creator")
        workflow.add_edge("create_table", "response_creator")
        
        # Response creator is the end
        workflow.add_edge("response_creator", END)
        
        # Compile the workflow
        self.app = workflow.compile()
    
    def route_after_intent_classification(self, state: AgentState) -> str:
        """
        Conditional routing logic after intent classification
        Determines next node based on intent and financial data requirements
        """
        if not state.intent_classification:
            return "generate_content"  # Default fallback
        
        operation_type = state.intent_classification.operation_type
        requires_financial = state.intent_classification.requires_financial_data
        
        # If generate content needs financial data, go to data analyst first
        if operation_type == OperationType.GENERATE_CONTENT and requires_financial:
            return "data_analyst"
        
        # Otherwise route directly to operation nodes
        operation_routing = {
            OperationType.GENERATE_CONTENT: "generate_content",
            OperationType.CLASSIFY_FORMATTING: "classify_formatting",
            OperationType.CREATE_CHART: "create_chart",
            OperationType.CREATE_TABLE: "create_table"
        }
        
        return operation_routing.get(operation_type, "generate_content")
    
    # Node wrapper functions for LangGraph
    async def intent_classifier_node(self, state: AgentState) -> AgentState:
        """Intent classifier node wrapper"""
        state.add_processing_step("Starting intent classification")
        return await self.intent_classifier.process(state)
    
    async def data_analyst_node(self, state: AgentState) -> AgentState:
        """Data analyst node wrapper"""
        state.add_processing_step("Fetching financial data")
        return await self.data_analyst.process(state)
    
    async def generate_content_node(self, state: AgentState) -> AgentState:
        """Generate content node wrapper"""
        state.add_processing_step("Generating content")
        return await self.generate_content.process(state)
    
    async def classify_formatting_node(self, state: AgentState) -> AgentState:
        """Classify formatting node wrapper"""
        state.add_processing_step("Classifying formatting")
        return await self.classify_formatting.process(state)
    
    async def create_chart_node(self, state: AgentState) -> AgentState:
        """Create chart node wrapper"""
        state.add_processing_step("Creating chart")
        return await self.create_chart.process(state)
    
    async def create_table_node(self, state: AgentState) -> AgentState:
        """Create table node wrapper"""
        state.add_processing_step("Creating table")
        return await self.create_table.process(state)
    
    async def response_creator_node(self, state: AgentState) -> AgentState:
        """Response creator node wrapper"""
        state.add_processing_step("Creating final response")
        return await self.response_creator.process(state)
    
    async def process_request(self, user_request: str, request_id: str, context: Dict[str, Any]) -> Dict[str, Any]:
        """
        Main entry point for processing agent requests
        
        Args:
            user_request: Natural language request from user
            request_id: Unique identifier for request
            context: Document context (cursor position, selected text, etc.)
            
        Returns:
            Dict containing the processed response
        """
        print(f"🟡 ORCHESTRATOR: Starting request {request_id}")
        print(f"🟡 ORCHESTRATOR: User request: {user_request}")
        print(f"🟡 ORCHESTRATOR: Context: {context}")
        
        start_time = time.time()
        
        # Initialize agent state
        initial_state = AgentState(
            user_request=user_request,
            request_id=request_id,
            context=context,
            processing_steps=[],
            errors=[]
        )
        
        print(f"🟡 ORCHESTRATOR: Initial state created: {initial_state}")
        
        try:
            # Run the workflow
            print(f"🟡 ORCHESTRATOR: Invoking LangGraph workflow...")
            final_state_dict = await self.app.ainvoke(initial_state)
            print(f"🟡 ORCHESTRATOR: Workflow completed. Result type: {type(final_state_dict)}")
            print(f"🟡 ORCHESTRATOR: Final state dict: {final_state_dict}")
            
            # Calculate execution time
            execution_time = int((time.time() - start_time) * 1000)
            
            # Convert dict back to AgentState object
            print(f"🟡 ORCHESTRATOR: Converting dict back to AgentState...")
            final_state = AgentState(**final_state_dict)
            final_state.execution_time_ms = execution_time
            
            print(f"🟡 ORCHESTRATOR: Final state object: {final_state}")
            print(f"🟡 ORCHESTRATOR: Operation result: {final_state.operation_result}")
            
            # Convert to response dictionary
            print(f"🟡 ORCHESTRATOR: Converting to response dict...")
            response = final_state.to_response_dict()
            print(f"🟡 ORCHESTRATOR: Final response: {response}")
            
            return response
            
        except Exception as e:
            # Handle errors gracefully
            error_response = {
                "type": "insert",
                "response": f"I encountered an error processing your request: {str(e)}",
                "request_id": request_id,
                "content": "Error occurred during processing",
                "metadata": {
                    "error": str(e),
                    "execution_time_ms": int((time.time() - start_time) * 1000),
                    "processing_steps": initial_state.processing_steps
                }
            }
            
            return error_response