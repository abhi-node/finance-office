"""
FastAPI server for LangGraph Agent Orchestrator
Handles requests from LibreOffice backend and routes to agent system
"""

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from typing import Dict, Any, Optional
import logging
import uvicorn
import os
from dotenv import load_dotenv
from agents.agent_orchestrator import AgentOrchestrator

# Load environment variables from .env file
load_dotenv()

# Check for required API key
if not os.getenv("OPENAI_API_KEY"):
    raise ValueError("OPENAI_API_KEY environment variable not set. Please add it to your .env file.")

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

app = FastAPI(
    title="LangGraph Agent Orchestrator",
    description="AI Agent system for LibreOffice Finance Office",
    version="1.0.0"
)

# Initialize the agent orchestrator
agent_orchestrator = AgentOrchestrator()

class DocumentContext(BaseModel):
    """LibreOffice document context model"""
    document_available: bool = True
    cursor_position: Optional[Dict[str, int]] = None
    selected_text: str = ""
    document_structure: Optional[Dict[str, int]] = None
    document_content: str = ""
    formatting_state: Optional[Dict[str, bool]] = None

class AgentRequest(BaseModel):
    """Request model for agent processing (matches AgentCoordinator.cxx format)"""
    request: str
    request_id: str
    context: Optional[DocumentContext] = None

class AgentResponse(BaseModel):
    """Response model for agent processing (matches data-models.md exactly)"""
    type: str
    response: str
    # Operation-specific fields (only present based on type)
    content: Optional[str] = None              # for insert operations
    formatting: Optional[Dict[str, str]] = None # for format operations  
    rows: Optional[int] = None                 # for table operations
    columns: Optional[int] = None              # for table operations
    chart_type: Optional[str] = None           # for chart operations

@app.get("/")
async def root():
    """Health check endpoint"""
    return {"message": "LangGraph Agent Orchestrator is running"}

@app.get("/health")
async def health_check():
    """Detailed health check"""
    return {
        "status": "healthy",
        "service": "langraph-agent-orchestrator",
        "version": "1.0.0"
    }

@app.post("/api/agent", response_model=AgentResponse)
async def process_agent_request(request: AgentRequest):
    """
    Main endpoint to process agent requests from LibreOffice backend
    
    Args:
        request: AgentRequest containing user request, ID, and context
        
    Returns:
        AgentResponse: Processed response with operation data
    """
    try:
        logger.info(f"🔵 SERVER: Processing request {request.request_id}: {request.request[:100]}...")
        logger.info(f"🔵 SERVER: Request type check - request object: {type(request)}")
        logger.info(f"🔵 SERVER: Full request: {request}")
        
        # Convert DocumentContext to dict for agent processing
        context_dict = {}
        if request.context:
            context_dict = request.context.model_dump()
            logger.info(f"🔵 SERVER: Context converted to dict: {context_dict}")
        else:
            logger.info(f"🔵 SERVER: No context provided")
        
        # Process the request through the agent orchestrator
        logger.info(f"🔵 SERVER: Sending to agent orchestrator...")
        result = await agent_orchestrator.process_request(
            user_request=request.request,
            request_id=request.request_id,
            context=context_dict
        )
        
        logger.info(f"🔵 SERVER: Agent orchestrator returned: {type(result)}")
        logger.info(f"🔵 SERVER: Result: {result}")
        logger.info(f"🔵 SERVER: Request {request.request_id} processed successfully")
        return result
        
    except Exception as e:
        logger.error(f"Error processing request {request.request_id}: {str(e)}")
        raise HTTPException(
            status_code=500,
            detail=f"Agent processing failed: {str(e)}"
        )

@app.post("/agent/test")
async def test_agent():
    """Test endpoint for agent functionality"""
    test_context = DocumentContext(
        document_available=True,
        cursor_position={"node_index": 1, "content_index": 0},
        selected_text="Hello World",
        document_structure={"paragraph_count": 1, "page_count": 1, "word_count": 2, "character_count": 11},
        document_content="Hello World",
        formatting_state={"has_selection": True}
    )
    test_request = AgentRequest(
        request="Format the selected text as bold",
        request_id="test_001",
        context=test_context
    )
    
    try:
        result = await process_agent_request(test_request)
        return {"test_status": "success", "result": result}
    except Exception as e:
        return {"test_status": "failed", "error": str(e)}

if __name__ == "__main__":
    uvicorn.run(
        "server:app",
        host="127.0.0.1",
        port=8000,
        reload=True,
        log_level="info"
    )