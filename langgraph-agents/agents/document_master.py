"""
DocumentMasterAgent - Intelligent Supervisor and Orchestrator

This module implements the DocumentMasterAgent, which serves as the central 
orchestrator for all LibreOffice AI Writing Assistant operations. It analyzes
user requests, determines optimal workflow paths, coordinates specialized agents,
and aggregates results into coherent responses.

The agent implements intelligent routing based on operation complexity:
- Simple Operations (1-2 seconds): Direct path with minimal agents
- Moderate Operations (2-4 seconds): Focused agent subset  
- Complex Operations (3-5 seconds): Full agent orchestration

Key Responsibilities:
- Request analysis and intent classification
- Complexity assessment and routing decisions
- Agent coordination with parallel processing
- Result aggregation and response synthesis
- Performance optimization and resource management
"""

import asyncio
import re
import time
from dataclasses import dataclass, field
from datetime import datetime, timezone
from enum import Enum
from typing import Any, Dict, List, Optional, Union, Set, Callable, Tuple
import logging
from concurrent.futures import ThreadPoolExecutor

# Import LLM client for intelligent routing
from llm_client import get_llm_client
import json

# Import base agent framework
from .base import (
    BaseAgent,
    AgentResult,
    AgentCapability,
    AgentLifecycleState,
    ValidationResult,
    ToolCallResult
)

# Import specialized agents for integration
try:
    from .validation import ValidationAgent, ValidationRequest, ValidationLevel, ValidationCategory
    from .execution import ExecutionAgent, ExecutionOperation, OperationType, OperationPriority
    from .context_analysis import ContextAnalysisAgent
    from .content_generation import ContentGenerationAgent
    from .formatting import FormattingAgent
    from .data_integration import DataIntegrationAgent
    AGENTS_AVAILABLE = True
except ImportError as e:
    # Fallback for development without all agents
    ValidationAgent = None
    ExecutionAgent = None
    ContextAnalysisAgent = None
    ContentGenerationAgent = None
    FormattingAgent = None
    DataIntegrationAgent = None
    AGENTS_AVAILABLE = False

# Import LangGraph types
try:
    from langchain_core.messages import BaseMessage, HumanMessage, AIMessage
except ImportError:
    BaseMessage = Dict[str, Any]
    HumanMessage = Dict[str, Any]
    AIMessage = Dict[str, Any]

# Import state management
try:
    from state.document_state import DocumentState, AgentStatus
except ImportError:
    DocumentState = Dict[str, Any]
    AgentStatus = Any

class OperationComplexity(Enum):
    """Operation complexity classification for routing decisions."""
    SIMPLE = "simple"      # 1-2 seconds, minimal agents
    MODERATE = "moderate"  # 2-4 seconds, focused agents  
    COMPLEX = "complex"    # 3-5 seconds, full orchestration

class WorkflowPath(Enum):
    """Workflow execution paths based on operation type."""
    DIRECT = "direct"              # Simple operations, direct execution
    FOCUSED = "focused"            # Moderate operations, focused agents
    ORCHESTRATED = "orchestrated" # Complex operations, full coordination
    PARALLEL = "parallel"         # Complex operations with parallel processing

@dataclass
class OperationPlan:
    """Execution plan for agent coordination."""
    operation_id: str
    complexity: OperationComplexity
    workflow_path: WorkflowPath
    required_agents: List[str]
    parallel_agents: List[List[str]] = field(default_factory=list)
    estimated_time: float = 0.0
    optimization_hints: Dict[str, Any] = field(default_factory=dict)
    fallback_strategy: Optional[str] = None
    user_approval_required: bool = False

@dataclass
class RequestAnalysis:
    """Analysis results for user requests."""
    request_id: str
    intent_classification: str
    operation_type: str
    complexity: OperationComplexity
    confidence: float
    extracted_parameters: Dict[str, Any]
    context_requirements: List[str]
    performance_target: float
    risk_assessment: str = "low"

class DocumentMasterAgent(BaseAgent):
    """
    Central orchestrator and intelligent supervisor for all document operations.
    
    This agent serves as the primary entry point for user requests and coordinates
    all other specialized agents to provide comprehensive document assistance.
    It implements sophisticated routing logic to optimize performance while
    maintaining high-quality results.
    """
    
    def __init__(self, 
                 agent_id: str = "document_master",
                 config: Optional[Dict[str, Any]] = None):
        """
        Initialize the DocumentMasterAgent.
        
        Args:
            agent_id: Unique identifier for this agent instance
            config: Optional configuration dictionary
        """
        super().__init__(
            agent_id=agent_id,
            capabilities=[AgentCapability.ORCHESTRATION],
            config=config or {},
            tools={}
        )
        
        # Agent registry and coordination
        self.registered_agents: Dict[str, BaseAgent] = {}
        self.agent_capabilities: Dict[str, List[AgentCapability]] = {}
        self.agent_performance_history: Dict[str, List[float]] = {}
        
        # Initialize specialized agents
        self._initialize_specialized_agents()
        
        # Request analysis patterns
        self.simple_patterns = self._initialize_simple_patterns()
        self.moderate_patterns = self._initialize_moderate_patterns()
        self.complex_patterns = self._initialize_complex_patterns()
        
        # Performance targets (from PRD requirements)
        self.performance_targets = {
            OperationComplexity.SIMPLE: 2.0,    # 1-2 seconds
            OperationComplexity.MODERATE: 4.0,  # 2-4 seconds  
            OperationComplexity.COMPLEX: 5.0    # 3-5 seconds
        }
        
        # Workflow optimization
        self.workflow_cache: Dict[str, OperationPlan] = {}
        self.parallel_executor = ThreadPoolExecutor(
            max_workers=4, 
            thread_name_prefix=f"DocumentMaster-{agent_id}"
        )
        
        # Initialize LLM client for intelligent routing
        self.llm_client = get_llm_client()
        
        # Result aggregation
        self.result_aggregators: Dict[str, Callable] = {
            "simple": self._aggregate_simple_results,
            "moderate": self._aggregate_moderate_results,
            "complex": self._aggregate_complex_results
        }
        
        self.logger.info(f"DocumentMasterAgent {agent_id} initialized with orchestration capabilities")
    
    def _initialize_specialized_agents(self):
        """Initialize and register specialized agents"""
        if not AGENTS_AVAILABLE:
            self.logger.warning("Specialized agents not available, skipping initialization")
            return
        
        try:
            # Initialize ContextAnalysisAgent
            if ContextAnalysisAgent:
                context_agent = ContextAnalysisAgent("context_analysis_agent")
                self.register_agent(context_agent)
                self.logger.info("Registered ContextAnalysisAgent")
            
            # Initialize ContentGenerationAgent
            if ContentGenerationAgent:
                content_agent = ContentGenerationAgent("content_generation_agent")
                self.register_agent(content_agent)
                self.logger.info("Registered ContentGenerationAgent")
            
            # Initialize FormattingAgent
            if FormattingAgent:
                formatting_agent = FormattingAgent("formatting_agent")
                self.register_agent(formatting_agent)
                self.logger.info("Registered FormattingAgent")
            
            # Initialize DataIntegrationAgent
            if DataIntegrationAgent:
                data_agent = DataIntegrationAgent("data_integration_agent")
                self.register_agent(data_agent)
                self.logger.info("Registered DataIntegrationAgent")
            
            # Initialize ValidationAgent
            if ValidationAgent:
                validation_agent = ValidationAgent("validation_agent")
                self.register_agent(validation_agent)
                self.logger.info("Registered ValidationAgent")
            
            # Initialize ExecutionAgent
            if ExecutionAgent:
                execution_agent = ExecutionAgent("execution_agent")
                self.register_agent(execution_agent)
                self.logger.info("Registered ExecutionAgent")
                
        except Exception as e:
            self.logger.error(f"Failed to initialize specialized agents: {e}")
    
    def _initialize_simple_patterns(self) -> Dict[str, List[str]]:
        """Initialize patterns for simple operations (1-2 seconds)."""
        return {
            "chart_creation": [
                r"create.*chart", r"insert.*graph", r"add.*chart", r"make.*chart",
                r"generate.*chart", r"new.*chart", r"simple.*chart"
            ],
            "basic_formatting": [
                r"make.*bold", r"change.*font", r"font.*size", r"italic", r"underline",
                r"align.*text", r"indent", r"bullet.*points", r"numbering"
            ],
            "table_operations": [
                r"insert.*table", r"create.*table", r"add.*table", r"new.*table",
                r"table.*with.*\d+.*rows", r"simple.*table"
            ],
            "direct_editing": [
                r"delete.*text", r"cut", r"paste", r"copy", r"select.*all",
                r"find.*replace", r"undo", r"redo"
            ],
            "page_operations": [
                r"page.*break", r"new.*page", r"insert.*break", r"section.*break"
            ]
        }
    
    def _initialize_moderate_patterns(self) -> Dict[str, List[str]]:
        """Initialize patterns for moderate operations (2-4 seconds)."""
        return {
            "content_writing": [
                r"write.*summary", r"create.*paragraph", r"improve.*text", r"rewrite",
                r"summarize.*section", r"explain.*concept", r"expand.*text"
            ],
            "document_styling": [
                r"format.*document", r"apply.*style", r"fix.*formatting", r"consistent.*styling",
                r"professional.*format", r"style.*guide", r"template.*apply"
            ],
            "simple_research": [
                r"define.*term", r"explain.*concept", r"clarify.*meaning", r"what.*is",
                r"lookup.*information", r"brief.*research"
            ],
            "content_analysis": [
                r"analyze.*text", r"check.*grammar", r"review.*content", r"assess.*quality",
                r"content.*suggestions", r"readability.*check"
            ],
            "structure_operations": [
                r"organize.*document", r"structure.*content", r"create.*outline",
                r"rearrange.*sections", r"improve.*organization"
            ]
        }
    
    def _initialize_complex_patterns(self) -> Dict[str, List[str]]:
        """Initialize patterns for complex operations (3-5 seconds)."""
        return {
            "financial_analysis": [
                r"financial.*report", r"market.*analysis", r"investment.*summary",
                r"stock.*data", r"financial.*chart", r"economic.*analysis", r"portfolio.*review",
                r"stock.*performance", r"comprehensive.*analysis.*stock"
            ],
            "research_integration": [
                r"research.*and.*write", r"analyze.*and.*create", r"comprehensive.*report",
                r"detailed.*analysis", r"in-depth.*study", r"thorough.*investigation",
                r"comprehensive.*analysis"
            ],
            "multi_step_operations": [
                r"restructure.*document", r"restructure.*entire", r"complete.*analysis", r"full.*review",
                r"comprehensive.*formatting", r"end-to-end.*process", r"complex.*workflow",
                r"entire.*document", r"whole.*document"
            ],
            "data_driven_content": [
                r"data.*visualization", r"statistical.*analysis", r"trend.*analysis",
                r"performance.*metrics", r"comparative.*analysis", r"dashboard.*creation"
            ],
            "collaborative_operations": [
                r"review.*and.*approve", r"collaborative.*editing", r"multi-user.*workflow",
                r"approval.*process", r"stakeholder.*review"
            ]
        }
    
    async def process(self, 
                     state: DocumentState, 
                     message: Optional[BaseMessage] = None) -> AgentResult:
        """
        Main processing method - analyzes request and orchestrates appropriate workflow.
        
        Args:
            state: Current document state with all context
            message: User message triggering this processing
            
        Returns:
            AgentResult: Orchestrated results from coordinated agents
        """
        start_time = time.time()
        
        try:
            # Extract user request
            user_request = self._extract_user_request(message, state)
            if not user_request:
                return self._create_error_result("unknown", "No user request found in message or state")
            
            # Analyze request to determine routing strategy
            analysis = await self._analyze_request(user_request, state)
            
            # Create execution plan
            plan = await self._create_execution_plan(analysis, state)
            
            # Execute workflow based on complexity
            if plan.complexity == OperationComplexity.SIMPLE:
                result = await self._execute_simple_workflow(plan, state, message)
            elif plan.complexity == OperationComplexity.MODERATE:
                result = await self._execute_moderate_workflow(plan, state, message)
            else:
                result = await self._execute_complex_workflow(plan, state, message)
            
            # Enhance result with orchestration metadata
            execution_time = time.time() - start_time
            result.metadata.update({
                "orchestration": {
                    "complexity": plan.complexity.value,
                    "workflow_path": plan.workflow_path.value,
                    "agents_used": plan.required_agents,
                    "execution_time": execution_time,
                    "performance_target_met": execution_time <= plan.estimated_time,
                    "analysis_confidence": analysis.confidence
                }
            })
            
            # Update performance history
            self._update_performance_history(plan, execution_time)
            
            return result
            
        except Exception as e:
            execution_time = time.time() - start_time
            self.logger.error(f"DocumentMaster orchestration failed: {e}")
            
            return AgentResult(
                agent_id=self.agent_id,
                success=False,
                error=f"Orchestration failed: {str(e)}",
                execution_time=execution_time,
                metadata={"error_type": type(e).__name__}
            )
    
    def _extract_user_request(self, 
                            message: Optional[BaseMessage], 
                            state: DocumentState) -> Optional[str]:
        """Extract user request from message or state."""
        if message and hasattr(message, 'content'):
            return message.content
        elif message and isinstance(message, dict) and 'content' in message:
            return message['content']
        elif isinstance(state, dict) and 'current_task' in state:
            return state.get('current_task', '')
        elif hasattr(state, 'current_task'):
            return state.current_task
        
        return None
    
    async def _analyze_request(self, 
                             user_request: str, 
                             state: DocumentState) -> RequestAnalysis:
        """
        Analyze user request to determine intent, complexity, and routing strategy.
        
        Args:
            user_request: Raw user request text
            state: Current document state for context
            
        Returns:
            RequestAnalysis: Comprehensive analysis of the request
        """
        request_id = f"req_{int(time.time() * 1000)}"
        
        # Classify intent and determine complexity
        intent_info = self._classify_intent(user_request)
        complexity = self._assess_complexity(user_request, intent_info, state)
        
        # Extract parameters from request
        parameters = self._extract_parameters(user_request, intent_info)
        
        # Determine context requirements
        context_reqs = self._analyze_context_requirements(user_request, state)
        
        # Calculate performance target
        performance_target = self.performance_targets[complexity]
        
        # Assess risk level
        risk = self._assess_risk_level(user_request, complexity, state)
        
        return RequestAnalysis(
            request_id=request_id,
            intent_classification=intent_info["category"],
            operation_type=intent_info["operation"],
            complexity=complexity,
            confidence=intent_info["confidence"],
            extracted_parameters=parameters,
            context_requirements=context_reqs,
            performance_target=performance_target,
            risk_assessment=risk
        )
    
    def _classify_intent(self, user_request: str) -> Dict[str, Any]:
        """Classify user intent using pattern matching."""
        user_request_lower = user_request.lower()
        
        # Check complex patterns first (to catch complex operations before simpler ones)
        for category, patterns in self.complex_patterns.items():
            for pattern in patterns:
                if re.search(pattern, user_request_lower):
                    return {
                        "category": "complex",
                        "operation": category,
                        "confidence": 0.9,
                        "matched_pattern": pattern
                    }
        
        # Check moderate patterns next
        for category, patterns in self.moderate_patterns.items():
            for pattern in patterns:
                if re.search(pattern, user_request_lower):
                    return {
                        "category": "moderate",
                        "operation": category,
                        "confidence": 0.8,
                        "matched_pattern": pattern
                    }
        
        # Check simple patterns last (most common, but less specific)
        for category, patterns in self.simple_patterns.items():
            for pattern in patterns:
                if re.search(pattern, user_request_lower):
                    return {
                        "category": "simple",
                        "operation": category,
                        "confidence": 0.7,
                        "matched_pattern": pattern
                    }
        
        # Default to moderate complexity for unrecognized patterns
        return {
            "category": "moderate",
            "operation": "general_assistance",
            "confidence": 0.5,
            "matched_pattern": None
        }
    
    def _assess_complexity(self, 
                         user_request: str, 
                         intent_info: Dict[str, Any], 
                         state: DocumentState) -> OperationComplexity:
        """Assess operation complexity based on multiple factors."""
        base_complexity = intent_info["category"]
        
        # Complexity indicators that increase complexity
        complexity_indicators = [
            r"comprehensive", r"detailed", r"complex", r"advanced",
            r"multiple", r"all", r"entire", r"complete", r"thorough"
        ]
        
        # Simplicity indicators that decrease complexity  
        simplicity_indicators = [
            r"simple", r"basic", r"quick", r"fast", r"easy", r"minimal"
        ]
        
        user_request_lower = user_request.lower()
        
        # Check for complexity modifiers
        complexity_score = 0
        for indicator in complexity_indicators:
            if re.search(indicator, user_request_lower):
                complexity_score += 1
        
        for indicator in simplicity_indicators:
            if re.search(indicator, user_request_lower):
                complexity_score -= 1
        
        # Adjust based on document context
        if isinstance(state, dict):
            # Large documents increase complexity
            doc_structure = state.get("document_structure", {})
            if isinstance(doc_structure, dict):
                paragraphs = doc_structure.get("paragraphs", 0)
                if paragraphs > 100:
                    complexity_score += 1
                
            # Multiple pending operations increase complexity
            pending_ops = state.get("pending_operations", [])
            if len(pending_ops) > 3:
                complexity_score += 1
        
        # Determine final complexity
        if base_complexity == "simple" and complexity_score <= 0:
            return OperationComplexity.SIMPLE
        elif base_complexity == "complex" or complexity_score >= 2:
            return OperationComplexity.COMPLEX
        else:
            return OperationComplexity.MODERATE
    
    def _extract_parameters(self, 
                          user_request: str, 
                          intent_info: Dict[str, Any]) -> Dict[str, Any]:
        """Extract operation parameters from user request."""
        parameters = {}
        operation = intent_info.get("operation", "")
        
        # Extract numbers (for tables, charts, etc.)
        numbers = re.findall(r'\d+', user_request)
        if numbers:
            parameters["numbers"] = [int(n) for n in numbers]
        
        # Extract specific parameters based on operation type
        if "table" in operation:
            # Look for table dimensions
            table_match = re.search(r'(\d+).*(?:by|x|\*).*(\d+)', user_request.lower())
            if table_match:
                parameters["rows"] = int(table_match.group(1))
                parameters["cols"] = int(table_match.group(2))
        
        elif "chart" in operation:
            # Look for chart types
            chart_types = ["bar", "line", "pie", "scatter", "column"]
            for chart_type in chart_types:
                if chart_type in user_request.lower():
                    parameters["chart_type"] = chart_type
                    break
        
        elif "format" in operation:
            # Look for formatting instructions
            format_terms = ["bold", "italic", "underline", "font", "size", "color"]
            found_formats = [term for term in format_terms if term in user_request.lower()]
            if found_formats:
                parameters["formatting_types"] = found_formats
        
        # Extract quoted text (specific content to work with)
        quoted_text = re.findall(r'"([^"]*)"', user_request)
        if quoted_text:
            parameters["quoted_content"] = quoted_text
        
        return parameters
    
    def _analyze_context_requirements(self, 
                                    user_request: str, 
                                    state: DocumentState) -> List[str]:
        """Analyze what context information is needed for the operation."""
        requirements = []
        
        # Check if operation needs document structure analysis
        structure_keywords = ["document", "structure", "organize", "outline", "sections"]
        if any(keyword in user_request.lower() for keyword in structure_keywords):
            requirements.append("document_structure")
        
        # Check if operation needs content analysis
        content_keywords = ["text", "content", "writing", "paragraph", "analyze"]
        if any(keyword in user_request.lower() for keyword in content_keywords):
            requirements.append("content_analysis")
        
        # Check if operation needs cursor/selection context
        selection_keywords = ["selected", "current", "here", "this", "cursor"]
        if any(keyword in user_request.lower() for keyword in selection_keywords):
            requirements.append("cursor_context")
        
        # Check if operation needs external data
        data_keywords = ["data", "research", "information", "stock", "financial", "market"]
        if any(keyword in user_request.lower() for keyword in data_keywords):
            requirements.append("external_data")
        
        return requirements
    
    def _assess_risk_level(self, 
                         user_request: str, 
                         complexity: OperationComplexity, 
                         state: DocumentState) -> str:
        """Assess risk level of the operation."""
        risk_keywords = [
            "delete", "remove", "clear", "replace", "overwrite", 
            "reset", "erase", "restructure", "major"
        ]
        
        if any(keyword in user_request.lower() for keyword in risk_keywords):
            return "high"
        elif complexity == OperationComplexity.COMPLEX:
            return "medium"
        else:
            return "low"
    
    async def _create_execution_plan(self, 
                                   analysis: RequestAnalysis, 
                                   state: DocumentState) -> OperationPlan:
        """Create detailed execution plan based on request analysis."""
        
        # Determine workflow path
        if analysis.complexity == OperationComplexity.SIMPLE:
            workflow_path = WorkflowPath.DIRECT
            required_agents = self._get_simple_workflow_agents(analysis.operation_type)
        elif analysis.complexity == OperationComplexity.MODERATE:
            workflow_path = WorkflowPath.FOCUSED
            required_agents = self._get_moderate_workflow_agents(analysis.operation_type)
        else:
            # Determine if parallel processing would be beneficial
            if self._should_use_parallel_processing(analysis, state):
                workflow_path = WorkflowPath.PARALLEL
                required_agents, parallel_groups = self._get_complex_parallel_workflow(analysis.operation_type)
            else:
                workflow_path = WorkflowPath.ORCHESTRATED
                required_agents = self._get_complex_workflow_agents(analysis.operation_type)
                parallel_groups = []
        
        # Calculate estimated execution time
        estimated_time = self._estimate_execution_time(analysis, required_agents)
        
        # Determine optimization hints
        optimization_hints = self._generate_optimization_hints(analysis, required_agents)
        
        # Check if user approval is required
        user_approval = analysis.risk_assessment == "high" or analysis.complexity == OperationComplexity.COMPLEX
        
        return OperationPlan(
            operation_id=analysis.request_id,
            complexity=analysis.complexity,
            workflow_path=workflow_path,
            required_agents=required_agents,
            parallel_agents=parallel_groups if workflow_path == WorkflowPath.PARALLEL else [],
            estimated_time=estimated_time,
            optimization_hints=optimization_hints,
            fallback_strategy=self._determine_fallback_strategy(analysis),
            user_approval_required=user_approval
        )
    
    def _get_simple_workflow_agents(self, operation_type: str) -> List[str]:
        """Get agent list for simple workflow (minimal agents)."""
        base_agents = ["context_analysis", "execution"]
        
        if "chart" in operation_type or "table" in operation_type:
            return ["context_analysis", "formatting", "validation", "execution"]
        elif "format" in operation_type:
            return ["context_analysis", "formatting", "execution"]
        else:
            return base_agents
    
    def _get_moderate_workflow_agents(self, operation_type: str) -> List[str]:
        """Get agent list for moderate workflow (focused agents)."""
        base_agents = []
        
        if "content" in operation_type or "writing" in operation_type:
            base_agents = ["context_analysis", "content_generation", "formatting"]
        elif "research" in operation_type:
            base_agents = ["context_analysis", "data_integration", "content_generation"]
        elif "styling" in operation_type or "format" in operation_type:
            base_agents = ["context_analysis", "formatting"]
        else:
            base_agents = ["context_analysis", "content_generation", "formatting"]
        
        # Add validation and execution agents if available
        if AGENTS_AVAILABLE:
            if "validation_agent" in self.registered_agents:
                base_agents.append("validation_agent")
            if "execution_agent" in self.registered_agents:
                base_agents.append("execution_agent")
        
        return base_agents
    
    def _get_complex_workflow_agents(self, operation_type: str) -> List[str]:
        """Get agent list for complex workflow (full orchestration)."""
        base_agents = [
            "context_analysis",
            "data_integration", 
            "content_generation",
            "formatting"
        ]
        
        # Add validation and execution agents if available
        if AGENTS_AVAILABLE:
            if "validation_agent" in self.registered_agents:
                base_agents.append("validation_agent")
            if "execution_agent" in self.registered_agents:
                base_agents.append("execution_agent")
        
        return base_agents
    
    def _get_complex_parallel_workflow(self, operation_type: str) -> tuple[List[str], List[List[str]]]:
        """Get agents and parallel groups for complex parallel workflow."""
        all_agents = [
            "context_analysis",
            "data_integration",
            "content_generation", 
            "formatting"
        ]
        
        # Add validation and execution agents if available
        if AGENTS_AVAILABLE:
            if "validation_agent" in self.registered_agents:
                all_agents.append("validation_agent")
            if "execution_agent" in self.registered_agents:
                all_agents.append("execution_agent")
        
        # Define parallel execution groups
        parallel_groups = [
            ["context_analysis", "data_integration"],  # Can run in parallel
            ["content_generation"],                    # Depends on both above
            ["formatting"],                           # Depends on content
            ["validation"],                           # Depends on formatting
            ["execution"]                             # Final execution
        ]
        
        return all_agents, parallel_groups
    
    def _should_use_parallel_processing(self, 
                                      analysis: RequestAnalysis, 
                                      state: DocumentState) -> bool:
        """Determine if parallel processing would be beneficial."""
        # Use parallel processing for:
        # 1. Complex operations with data integration
        # 2. Operations that benefit from parallel context + data fetching
        # 3. Large documents where analysis can be parallelized
        
        if "data" in analysis.operation_type or "research" in analysis.operation_type:
            return True
        
        if isinstance(state, dict):
            doc_structure = state.get("document_structure", {})
            if isinstance(doc_structure, dict) and doc_structure.get("paragraphs", 0) > 50:
                return True
        
        return False
    
    def _estimate_execution_time(self, 
                               analysis: RequestAnalysis, 
                               required_agents: List[str]) -> float:
        """Estimate execution time based on agents and complexity."""
        base_time = {
            OperationComplexity.SIMPLE: 1.0,
            OperationComplexity.MODERATE: 2.5,
            OperationComplexity.COMPLEX: 4.0
        }
        
        # Add time per agent (with some parallelization benefit)
        agent_time = len(required_agents) * 0.3
        
        # Consider historical performance
        historical_factor = 1.0
        for agent_id in required_agents:
            if agent_id in self.agent_performance_history:
                recent_times = self.agent_performance_history[agent_id][-5:]
                if recent_times:
                    avg_time = sum(recent_times) / len(recent_times)
                    historical_factor += (avg_time / len(required_agents))
        
        return base_time[analysis.complexity] + agent_time + historical_factor
    
    def _generate_optimization_hints(self, 
                                   analysis: RequestAnalysis, 
                                   required_agents: List[str]) -> Dict[str, Any]:
        """Generate optimization hints for agent execution."""
        hints = {
            "cache_document_analysis": "context_analysis" in required_agents,
            "parallel_data_fetch": "data_integration" in required_agents,
            "streaming_response": analysis.complexity == OperationComplexity.COMPLEX,
            "defer_heavy_validation": analysis.risk_assessment == "low",
            "use_performance_mode": analysis.performance_target < 3.0
        }
        
        return hints
    
    def _determine_fallback_strategy(self, analysis: RequestAnalysis) -> str:
        """Determine fallback strategy for error scenarios."""
        if analysis.complexity == OperationComplexity.SIMPLE:
            return "retry_with_basic_agents"
        elif analysis.complexity == OperationComplexity.MODERATE:
            return "fallback_to_simple_workflow"
        else:
            return "graceful_degradation_with_user_notification"
    
    # Task 18: Intelligent Agent Routing System Implementation
    
    def _get_simple_workflow_agents(self, operation_type: str) -> List[str]:
        """
        Determine agents for simple operations (1-2 seconds).
        Optimized for minimal agent involvement with direct execution.
        """
        # Simple operations skip unnecessary agents for performance
        routing_map = {
            "chart_creation": ["context_analysis_agent", "formatting_agent", "execution_agent"],
            "basic_formatting": ["formatting_agent", "execution_agent"],
            "table_operations": ["context_analysis_agent", "formatting_agent", "execution_agent"],
            "direct_editing": ["execution_agent"],
            "page_operations": ["formatting_agent", "execution_agent"]
        }
        
        return routing_map.get(operation_type, ["context_analysis_agent", "execution_agent"])
    
    def _get_moderate_workflow_agents(self, operation_type: str) -> List[str]:
        """
        Determine agents for moderate operations (2-4 seconds).
        Focused agent subset with streamlined validation.
        """
        routing_map = {
            "content_writing": ["context_analysis_agent", "content_generation_agent", "formatting_agent", "validation_agent", "execution_agent"],
            "document_styling": ["context_analysis_agent", "formatting_agent", "validation_agent", "execution_agent"],
            "simple_research": ["context_analysis_agent", "data_integration_agent", "content_generation_agent", "formatting_agent", "execution_agent"],
            "content_analysis": ["context_analysis_agent", "content_generation_agent", "validation_agent", "execution_agent"],
            "structure_operations": ["context_analysis_agent", "content_generation_agent", "formatting_agent", "validation_agent", "execution_agent"]
        }
        
        return routing_map.get(operation_type, ["context_analysis_agent", "content_generation_agent", "formatting_agent", "execution_agent"])
    
    def _get_complex_workflow_agents(self, operation_type: str) -> List[str]:
        """
        Determine agents for complex operations (3-5 seconds).
        Full agent orchestration with comprehensive validation.
        """
        routing_map = {
            "financial_analysis": ["context_analysis_agent", "data_integration_agent", "content_generation_agent", "formatting_agent", "validation_agent", "execution_agent"],
            "research_integration": ["context_analysis_agent", "data_integration_agent", "content_generation_agent", "formatting_agent", "validation_agent", "execution_agent"],
            "multi_step_operations": ["context_analysis_agent", "content_generation_agent", "formatting_agent", "validation_agent", "execution_agent"],
            "data_driven_content": ["context_analysis_agent", "data_integration_agent", "content_generation_agent", "formatting_agent", "validation_agent", "execution_agent"],
            "collaborative_operations": ["context_analysis_agent", "content_generation_agent", "formatting_agent", "validation_agent", "execution_agent"]
        }
        
        # Complex operations always use full agent network
        return routing_map.get(operation_type, ["context_analysis_agent", "data_integration_agent", "content_generation_agent", "formatting_agent", "validation_agent", "execution_agent"])
    
    def _get_complex_parallel_workflow(self, operation_type: str) -> Tuple[List[str], List[List[str]]]:
        """
        Determine agents and parallel groups for complex operations requiring parallel processing.
        Returns tuple of (all_agents, parallel_groups) for coordinated execution.
        """
        all_agents = self._get_complex_workflow_agents(operation_type)
        
        # Define parallel execution groups for performance optimization
        parallel_groups = []
        
        if operation_type in ["financial_analysis", "data_driven_content"]:
            # Parallel: Context analysis + Data integration
            parallel_groups = [
                ["context_analysis_agent", "data_integration_agent"],
                ["content_generation_agent"],
                ["formatting_agent"],
                ["validation_agent"],
                ["execution_agent"]
            ]
        elif operation_type == "research_integration":
            # Parallel: Context analysis + Data integration
            parallel_groups = [
                ["context_analysis_agent", "data_integration_agent"],
                ["content_generation_agent"],
                ["formatting_agent"],
                ["validation_agent"],
                ["execution_agent"]
            ]
        else:
            # Sequential execution for other complex operations
            parallel_groups = [[agent] for agent in all_agents]
        
        return all_agents, parallel_groups
    
    def _should_use_parallel_processing(self, analysis: RequestAnalysis, state: DocumentState) -> bool:
        """
        Determine if parallel processing would benefit the operation.
        Based on operation type, complexity, and system resources.
        """
        # Enable parallel processing for data-intensive operations
        parallel_operations = [
            "financial_analysis", 
            "research_integration", 
            "data_driven_content"
        ]
        
        # Check if operation benefits from parallelization
        if analysis.operation_type in parallel_operations:
            return True
        
        # Check system resources (simplified - could be more sophisticated)
        # Enable parallel processing if multiple heavy operations are detected
        if "data" in analysis.user_request.lower() and "analysis" in analysis.user_request.lower():
            return True
        
        return False
    
    async def _execute_simple_workflow(self, 
                                     plan: OperationPlan, 
                                     state: DocumentState, 
                                     message: Optional[BaseMessage]) -> AgentResult:
        """Execute simple workflow with minimal agents (1-2 seconds target)."""
        self.logger.info(f"Executing simple workflow: {plan.required_agents}")
        
        results = []
        current_state = state
        
        # Execute agents sequentially with optimization
        for agent_id in plan.required_agents:
            if agent_id not in self.registered_agents:
                # Mock agent execution for now
                result = AgentResult(
                    agent_id=agent_id,
                    success=True,
                    result={"simulated": True, "agent": agent_id, "operation": "simple"},
                    execution_time=0.2
                )
            else:
                agent = self.registered_agents[agent_id]
                result = await agent.execute_with_monitoring(current_state, message)
            
            results.append(result)
            
            # Update state with results if successful
            if result.success and result.state_updates:
                current_state = self._merge_state_updates(current_state, result.state_updates)
        
        # Aggregate results
        return self._aggregate_simple_results(results, plan)
    
    async def _execute_moderate_workflow(self, 
                                       plan: OperationPlan, 
                                       state: DocumentState, 
                                       message: Optional[BaseMessage]) -> AgentResult:
        """Execute moderate workflow with focused agents (2-4 seconds target)."""
        self.logger.info(f"Executing moderate workflow: {plan.required_agents}")
        
        results = []
        current_state = state
        
        # Execute with some optimization hints applied
        for agent_id in plan.required_agents:
            if agent_id not in self.registered_agents:
                # Mock agent execution
                result = AgentResult(
                    agent_id=agent_id,
                    success=True,
                    result={"simulated": True, "agent": agent_id, "operation": "moderate"},
                    execution_time=0.4
                )
            else:
                agent = self.registered_agents[agent_id]
                result = await agent.execute_with_monitoring(current_state, message)
            
            results.append(result)
            
            # Update state
            if result.success and result.state_updates:
                current_state = self._merge_state_updates(current_state, result.state_updates)
        
        return self._aggregate_moderate_results(results, plan)
    
    async def _execute_complex_workflow(self, 
                                      plan: OperationPlan, 
                                      state: DocumentState, 
                                      message: Optional[BaseMessage]) -> AgentResult:
        """Execute complex workflow with full orchestration (3-5 seconds target)."""
        self.logger.info(f"Executing complex workflow: {plan.workflow_path.value}")
        
        if plan.workflow_path == WorkflowPath.PARALLEL:
            return await self._execute_parallel_workflow(plan, state, message)
        else:
            return await self._execute_orchestrated_workflow(plan, state, message)
    
    async def _execute_parallel_workflow(self, 
                                       plan: OperationPlan, 
                                       state: DocumentState, 
                                       message: Optional[BaseMessage]) -> AgentResult:
        """Execute workflow with parallel agent coordination."""
        self.logger.info(f"Executing parallel workflow with {len(plan.parallel_agents)} groups")
        
        all_results = []
        current_state = state
        
        # Execute parallel groups sequentially, but agents within groups in parallel
        for group in plan.parallel_agents:
            if len(group) == 1:
                # Single agent in group
                agent_id = group[0]
                if agent_id not in self.registered_agents:
                    result = AgentResult(
                        agent_id=agent_id,
                        success=True,
                        result={"simulated": True, "agent": agent_id, "operation": "parallel"},
                        execution_time=0.6
                    )
                else:
                    agent = self.registered_agents[agent_id]
                    result = await agent.execute_with_monitoring(current_state, message)
                
                all_results.append(result)
                
                # Update state
                if result.success and result.state_updates:
                    current_state = self._merge_state_updates(current_state, result.state_updates)
            
            else:
                # Multiple agents in parallel
                tasks = []
                for agent_id in group:
                    if agent_id not in self.registered_agents:
                        # Create mock coroutine
                        async def mock_agent_execution(aid=agent_id):
                            await asyncio.sleep(0.1)  # Simulate work
                            return AgentResult(
                                agent_id=aid,
                                success=True,
                                result={"simulated": True, "agent": aid, "operation": "parallel"},
                                execution_time=0.1
                            )
                        tasks.append(mock_agent_execution())
                    else:
                        agent = self.registered_agents[agent_id]
                        tasks.append(agent.execute_with_monitoring(current_state, message))
                
                # Wait for all agents in group to complete
                group_results = await asyncio.gather(*tasks, return_exceptions=True)
                
                # Process results
                for i, result in enumerate(group_results):
                    if isinstance(result, Exception):
                        # Create error result
                        error_result = AgentResult(
                            agent_id=group[i],
                            success=False,
                            error=str(result),
                            execution_time=0.0
                        )
                        all_results.append(error_result)
                    else:
                        all_results.append(result)
                        
                        # Merge successful state updates
                        if result.success and result.state_updates:
                            current_state = self._merge_state_updates(current_state, result.state_updates)
        
        return self._aggregate_complex_results(all_results, plan)
    
    async def _execute_orchestrated_workflow(self, 
                                           plan: OperationPlan, 
                                           state: DocumentState, 
                                           message: Optional[BaseMessage]) -> AgentResult:
        """Execute fully orchestrated workflow with validation and execution."""
        self.logger.info(f"Executing orchestrated workflow: {plan.required_agents}")
        
        results = []
        current_state = state
        
        # Phase 1: Execute specialist agents (Context, Content, Formatting, Data)
        specialist_agents = [aid for aid in plan.required_agents if aid not in ['validation_agent', 'execution_agent']]
        
        for agent_id in specialist_agents:
            if agent_id not in self.registered_agents:
                result = AgentResult(
                    agent_id=agent_id,
                    success=True,
                    result={"simulated": True, "agent": agent_id, "operation": "orchestrated"},
                    execution_time=0.8
                )
            else:
                agent = self.registered_agents[agent_id]
                result = await agent.execute_with_monitoring(current_state, message)
            
            results.append(result)
            
            # Update state and check for errors
            if result.success and result.state_updates:
                current_state = self._merge_state_updates(current_state, result.state_updates)
            elif not result.success:
                # Handle error with fallback strategy
                fallback_result = await self._execute_fallback_strategy(
                    plan.fallback_strategy, agent_id, result, current_state, message
                )
                if fallback_result:
                    results[-1] = fallback_result  # Replace failed result
                    current_state = self._merge_state_updates(current_state, fallback_result.state_updates)
        
        # Phase 2: Validation (if ValidationAgent is available and required)
        if 'validation_agent' in self.registered_agents:
            validation_result = await self._execute_validation_phase(current_state, message)
            results.append(validation_result)
            
            if not validation_result.success:
                self.logger.warning("Validation failed, stopping workflow")
                return self._aggregate_complex_results(results, plan)
            
            # Check if approval is required
            if (validation_result.result and 
                validation_result.result.get('approval_required', False)):
                self.logger.info("Operation requires approval, pausing workflow")
                # In a real implementation, this would trigger user approval workflow
                current_state['approval_required'] = True
                current_state['approval_pending'] = True
            
            # Update state with validation results
            if validation_result.state_updates:
                current_state = self._merge_state_updates(current_state, validation_result.state_updates)
        
        # Phase 3: Execution (if ExecutionAgent is available and validation passed)
        if ('execution_agent' in self.registered_agents and 
            not current_state.get('approval_pending', False)):
            execution_result = await self._execute_execution_phase(current_state, message)
            results.append(execution_result)
            
            # Update state with execution results
            if execution_result.state_updates:
                current_state = self._merge_state_updates(current_state, execution_result.state_updates)
        
        return self._aggregate_complex_results(results, plan)
    
    async def _execute_validation_phase(self, state: DocumentState, message: Optional[BaseMessage]) -> AgentResult:
        """Execute validation phase using ValidationAgent"""
        try:
            validation_agent = self.registered_agents['validation_agent']
            
            # Create validation request based on current state
            validation_level = self._determine_validation_level(state)
            categories = self._determine_validation_categories(state)
            
            # Execute validation through agent
            result = await validation_agent.execute_with_monitoring(state, message)
            
            self.logger.info(f"Validation phase completed: success={result.success}")
            return result
            
        except Exception as e:
            self.logger.error(f"Validation phase failed: {e}")
            return AgentResult(
                agent_id="validation_agent",
                success=False,
                result={},
                error=f"Validation phase error: {str(e)}",
                execution_time=0
            )
    
    async def _execute_execution_phase(self, state: DocumentState, message: Optional[BaseMessage]) -> AgentResult:
        """Execute operations using ExecutionAgent"""
        try:
            execution_agent = self.registered_agents['execution_agent']
            
            # Begin undo group for the operation
            if hasattr(execution_agent, 'undo_redo_manager'):
                operation_name = state.get('current_task', 'Document Operation')
                execution_agent.undo_redo_manager.begin_undo_group(operation_name)
            
            # Execute operations through agent
            result = await execution_agent.execute_with_monitoring(state, message)
            
            # End undo group
            if hasattr(execution_agent, 'undo_redo_manager'):
                execution_agent.undo_redo_manager.end_undo_group()
            
            self.logger.info(f"Execution phase completed: success={result.success}")
            return result
            
        except Exception as e:
            self.logger.error(f"Execution phase failed: {e}")
            return AgentResult(
                agent_id="execution_agent",
                success=False,
                result={},
                error=f"Execution phase error: {str(e)}",
                execution_time=0
            )
    
    def _determine_validation_level(self, state: DocumentState) -> 'ValidationLevel':
        """Determine appropriate validation level based on state"""
        if not AGENTS_AVAILABLE:
            return None
        
        # Check for financial content
        external_data = state.get('external_data', {})
        if 'financial_data' in external_data:
            return ValidationLevel.COMPREHENSIVE
        
        # Check for complex operations
        pending_ops = state.get('pending_operations', [])
        if len(pending_ops) > 3:
            return ValidationLevel.COMPREHENSIVE
        
        # Check for simple operations
        current_task = state.get('current_task', '').lower()
        if any(keyword in current_task for keyword in ['quick', 'fast', 'simple']):
            return ValidationLevel.FAST
        
        return ValidationLevel.STANDARD
    
    def _determine_validation_categories(self, state: DocumentState) -> Set['ValidationCategory']:
        """Determine validation categories based on state"""
        if not AGENTS_AVAILABLE:
            return set()
        
        categories = set()
        
        # Always check content accuracy
        categories.add(ValidationCategory.CONTENT_ACCURACY)
        
        # Check for formatting operations
        pending_ops = state.get('pending_operations', [])
        if any(op.get('type', '').startswith('format') for op in pending_ops):
            categories.add(ValidationCategory.FORMATTING_CONSISTENCY)
        
        # Check for financial content
        external_data = state.get('external_data', {})
        if 'financial_data' in external_data:
            categories.add(ValidationCategory.FINANCIAL_COMPLIANCE)
        
        # Check for accessibility requirements
        if state.get('user_preferences', {}).get('accessibility_required', False):
            categories.add(ValidationCategory.ACCESSIBILITY)
        
        return categories
    
    async def _execute_fallback_strategy(self, 
                                       strategy: str, 
                                       failed_agent: str, 
                                       error_result: AgentResult,
                                       state: DocumentState, 
                                       message: Optional[BaseMessage]) -> Optional[AgentResult]:
        """Execute fallback strategy when agent fails."""
        self.logger.warning(f"Executing fallback strategy '{strategy}' for failed agent '{failed_agent}'")
        
        if strategy == "retry_with_basic_agents":
            # Retry with simplified approach
            return AgentResult(
                agent_id=failed_agent,
                success=True,
                result={"fallback": True, "strategy": "basic_retry"},
                execution_time=0.1,
                warnings=[f"Used fallback strategy for {failed_agent}"]
            )
        
        elif strategy == "fallback_to_simple_workflow":
            # Fallback to simpler approach
            return AgentResult(
                agent_id=failed_agent,
                success=True,
                result={"fallback": True, "strategy": "simple_workflow"},
                execution_time=0.2,
                warnings=[f"Simplified execution for {failed_agent}"]
            )
        
        elif strategy == "graceful_degradation_with_user_notification":
            # Continue with degraded functionality
            return AgentResult(
                agent_id=failed_agent,
                success=True,
                result={"fallback": True, "strategy": "graceful_degradation"},
                execution_time=0.1,
                warnings=[f"Continuing with reduced functionality due to {failed_agent} failure"]
            )
        
        return None
    
    def _merge_state_updates(self, 
                           current_state: DocumentState, 
                           updates: Dict[str, Any]) -> DocumentState:
        """Merge state updates into current state."""
        if isinstance(current_state, dict):
            merged_state = current_state.copy()
            for key, value in updates.items():
                if key in merged_state:
                    if isinstance(merged_state[key], list) and isinstance(value, list):
                        merged_state[key].extend(value)
                    elif isinstance(merged_state[key], dict) and isinstance(value, dict):
                        merged_state[key].update(value)
                    else:
                        merged_state[key] = value
                else:
                    merged_state[key] = value
            return merged_state
        else:
            # Handle TypedDict or other state formats
            return current_state
    
    def _aggregate_simple_results(self, 
                                results: List[AgentResult], 
                                plan: OperationPlan) -> AgentResult:
        """Aggregate results from simple workflow."""
        successful_results = [r for r in results if r.success]
        failed_results = [r for r in results if not r.success]
        
        # Combine all results
        combined_result = {
            "workflow_type": "simple",
            "agents_executed": [r.agent_id for r in results],
            "successful_agents": [r.agent_id for r in successful_results],
            "failed_agents": [r.agent_id for r in failed_results],
            "operation_results": [r.result for r in successful_results if r.result]
        }
        
        # Calculate total execution time
        total_time = sum(r.execution_time for r in results)
        
        # Combine state updates
        combined_updates = {}
        for result in successful_results:
            if result.state_updates:
                for key, value in result.state_updates.items():
                    if key in combined_updates:
                        if isinstance(combined_updates[key], dict) and isinstance(value, dict):
                            combined_updates[key].update(value)
                        elif isinstance(combined_updates[key], list) and isinstance(value, list):
                            combined_updates[key].extend(value)
                        else:
                            combined_updates[key] = value
                    else:
                        combined_updates[key] = value
        
        return AgentResult(
            agent_id=self.agent_id,
            success=len(failed_results) == 0,
            result=combined_result,
            execution_time=total_time,
            state_updates=combined_updates,
            warnings=[w for r in results for w in r.warnings],
            metadata={
                "workflow_plan": {
                    "complexity": plan.complexity.value,
                    "estimated_time": plan.estimated_time,
                    "actual_time": total_time,
                    "performance_met": total_time <= plan.estimated_time
                }
            }
        )
    
    def _aggregate_moderate_results(self, 
                                  results: List[AgentResult], 
                                  plan: OperationPlan) -> AgentResult:
        """Aggregate results from moderate workflow."""
        return self._aggregate_simple_results(results, plan)  # Similar logic for now
    
    def _aggregate_complex_results(self, 
                                 results: List[AgentResult], 
                                 plan: OperationPlan) -> AgentResult:
        """Aggregate results from complex workflow."""
        return self._aggregate_simple_results(results, plan)  # Similar logic for now
    
    def _update_performance_history(self, plan: OperationPlan, execution_time: float) -> None:
        """Update performance history for continuous improvement."""
        for agent_id in plan.required_agents:
            if agent_id not in self.agent_performance_history:
                self.agent_performance_history[agent_id] = []
            
            self.agent_performance_history[agent_id].append(execution_time / len(plan.required_agents))
            
            # Keep only recent history
            if len(self.agent_performance_history[agent_id]) > 20:
                self.agent_performance_history[agent_id] = self.agent_performance_history[agent_id][-10:]
    
    def validate_input(self, 
                      state: DocumentState, 
                      message: Optional[BaseMessage] = None) -> ValidationResult:
        """Validate input for orchestration processing."""
        issues = []
        
        # Check for user request
        user_request = self._extract_user_request(message, state)
        if not user_request or len(user_request.strip()) == 0:
            issues.append({
                "type": "missing_request",
                "message": "No user request found in message or state"
            })
        
        # Check state structure
        if isinstance(state, dict):
            required_fields = ["current_document", "agent_status"]
            for field in required_fields:
                if field not in state:
                    issues.append({
                        "type": "missing_state_field",
                        "message": f"Required state field '{field}' is missing"
                    })
        
        return ValidationResult(
            agent_id=self.agent_id,
            validation_type="orchestration_input",
            passed=len(issues) == 0,
            confidence=1.0 if len(issues) == 0 else 0.0,
            issues=issues
        )
    
    def get_required_tools(self) -> List[str]:
        """Get required tools for orchestration."""
        return []  # DocumentMaster doesn't use tools directly
    
    def register_agent(self, agent: BaseAgent) -> bool:
        """Register a specialized agent for orchestration."""
        try:
            self.registered_agents[agent.agent_id] = agent
            self.agent_capabilities[agent.agent_id] = agent.get_capabilities()
            self.logger.info(f"Registered agent: {agent.agent_id} with capabilities: {agent.get_capabilities()}")
            return True
        except Exception as e:
            self.logger.error(f"Failed to register agent {agent.agent_id}: {e}")
            return False
    
    def unregister_agent(self, agent_id: str) -> bool:
        """Unregister an agent."""
        if agent_id in self.registered_agents:
            del self.registered_agents[agent_id]
            del self.agent_capabilities[agent_id]
            if agent_id in self.agent_performance_history:
                del self.agent_performance_history[agent_id]
            self.logger.info(f"Unregistered agent: {agent_id}")
            return True
        return False
    
    def get_registered_agents(self) -> Dict[str, List[AgentCapability]]:
        """Get list of registered agents and their capabilities."""
        return self.agent_capabilities.copy()
    
    def get_orchestration_summary(self) -> Dict[str, Any]:
        """Get comprehensive orchestration status summary."""
        return {
            "agent_id": self.agent_id,
            "registered_agents": len(self.registered_agents),
            "agent_capabilities": self.agent_capabilities,
            "performance_targets": {k.value: v for k, v in self.performance_targets.items()},
            "workflow_cache_size": len(self.workflow_cache),
            "performance_history_agents": len(self.agent_performance_history),
            "lifecycle_state": self.lifecycle_state.value,
            "operation_count": self.performance_metrics.operation_count,
            "success_rate": self.performance_metrics.success_rate
        }
    
    def _create_error_result(self, operation_id: str, error_message: str, execution_time: float = 0.0) -> AgentResult:
        """Create standardized error result."""
        return AgentResult(
            agent_id=self.agent_id,
            operation_id=operation_id,
            success=False,
            error=error_message,
            execution_time=execution_time
        )
    
    def cleanup(self) -> None:
        """Clean up orchestration resources."""
        try:
            # Cleanup parallel executor
            self.parallel_executor.shutdown(wait=True)
            
            # Clear caches
            self.workflow_cache.clear()
            self.agent_performance_history.clear()
            
            # Cleanup registered agents
            for agent in self.registered_agents.values():
                try:
                    agent.cleanup()
                except Exception as e:
                    self.logger.error(f"Error cleaning up agent {agent.agent_id}: {e}")
            
            self.registered_agents.clear()
            self.agent_capabilities.clear()
            
        except Exception as e:
            self.logger.error(f"Error during DocumentMaster cleanup: {e}")
        
        # Call parent cleanup
        super().cleanup()
    
    async def _analyze_user_request(self, user_request: str, state: DocumentState) -> RequestAnalysis:
        """
        Alias for _analyze_request method to maintain backward compatibility.
        
        Args:
            user_request: Raw user request text
            state: Current document state for context
            
        Returns:
            RequestAnalysis: Comprehensive analysis of the request
        """
        # Call the async version directly
        return await self._analyze_request(user_request, state)