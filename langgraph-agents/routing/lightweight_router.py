"""
Task 18.2: Lightweight Agent Chain Router

Implements routing logic for simple operations (1-2 seconds) that uses minimal
agent chains for basic document operations. Handles cursor movements, simple
text insertions, basic formatting, and quick document queries.

Features:
- Streamlined agent workflows bypassing complex analysis
- Integration with ContextAnalysisAgent for basic position tracking
- Direct ExecutionAgent for UNO operations
- Performance optimization for sub-2-second completion
- Minimal state management overhead
"""

import asyncio
import logging
from typing import Dict, List, Any, Optional, Set
from dataclasses import dataclass, field
from datetime import datetime
import time

try:
    from state.document_state import DocumentState
    from agents.base import BaseAgent, AgentResult
    STATE_AVAILABLE = True
except ImportError:
    DocumentState = Dict[str, Any]
    BaseAgent = Any
    AgentResult = Dict[str, Any]
    STATE_AVAILABLE = False

from .complexity_analyzer import OperationType, OperationComplexity

@dataclass
class LightweightWorkflow:
    """Defines a lightweight workflow for simple operations."""
    operation_type: OperationType
    required_agents: List[str]
    estimated_time: float
    max_parallel: int = 2
    skip_validation: bool = True
    direct_execution: bool = True

@dataclass
class LightweightResult:
    """Result from lightweight routing."""
    success: bool
    operation_type: OperationType
    agents_used: List[str]
    execution_time: float
    result_data: Dict[str, Any]
    performance_metrics: Dict[str, Any]

class LightweightRouter:
    """
    Task 18.2 Implementation: Lightweight Agent Chain Router
    
    Handles simple operations (1-2 seconds) with minimal agent chains
    for optimal performance and responsiveness.
    """
    
    def __init__(self, config: Optional[Dict[str, Any]] = None):
        """Initialize the lightweight router."""
        self.config = config or {}
        self.logger = logging.getLogger("lightweight_router")
        
        # Performance targets
        self.max_execution_time = 2.0  # 2 seconds maximum
        self.target_execution_time = 1.5  # 1.5 seconds target
        
        # Agent registry (will be populated by DocumentMasterAgent)
        self.available_agents: Dict[str, BaseAgent] = {}
        
        # Initialize workflow definitions
        self._init_lightweight_workflows()
        
        # Performance tracking
        self.execution_history: List[Dict[str, Any]] = []
        self.performance_stats = {
            "total_executions": 0,
            "avg_execution_time": 0.0,
            "success_rate": 1.0,
            "agent_usage_stats": {}
        }
        
        self.logger.info("LightweightRouter initialized successfully")
    
    def _init_lightweight_workflows(self):
        """Initialize lightweight workflow definitions."""
        
        self.workflows = {
            OperationType.BASIC_FORMATTING: LightweightWorkflow(
                operation_type=OperationType.BASIC_FORMATTING,
                required_agents=["formatting_agent", "execution_agent"],
                estimated_time=0.8,
                max_parallel=1,
                skip_validation=True,
                direct_execution=True
            ),
            
            OperationType.CURSOR_MOVEMENT: LightweightWorkflow(
                operation_type=OperationType.CURSOR_MOVEMENT,
                required_agents=["context_analysis_agent", "execution_agent"],
                estimated_time=0.5,
                max_parallel=1,
                skip_validation=True,
                direct_execution=True
            ),
            
            OperationType.TEXT_INSERTION: LightweightWorkflow(
                operation_type=OperationType.TEXT_INSERTION,
                required_agents=["execution_agent"],
                estimated_time=0.6,
                max_parallel=1,
                skip_validation=True,
                direct_execution=True
            ),
            
            OperationType.SIMPLE_EDITING: LightweightWorkflow(
                operation_type=OperationType.SIMPLE_EDITING,
                required_agents=["context_analysis_agent", "execution_agent"],
                estimated_time=0.7,
                max_parallel=1,
                skip_validation=True,
                direct_execution=True
            )
        }
        
        # Optimization patterns for common operations
        self.optimization_patterns = {
            "bold_text": {
                "agents": ["execution_agent"],
                "direct_uno_call": "SwEditShell::SetAttr",
                "parameters": {"bold": True},
                "estimated_time": 0.3
            },
            "italic_text": {
                "agents": ["execution_agent"],
                "direct_uno_call": "SwEditShell::SetAttr", 
                "parameters": {"italic": True},
                "estimated_time": 0.3
            },
            "change_font": {
                "agents": ["formatting_agent", "execution_agent"],
                "preparation_required": True,
                "estimated_time": 0.6
            },
            "move_cursor": {
                "agents": ["context_analysis_agent", "execution_agent"],
                "context_required": True,
                "estimated_time": 0.4
            }
        }
    
    def register_agent(self, agent_id: str, agent: BaseAgent):
        """Register an agent for lightweight routing."""
        self.available_agents[agent_id] = agent
        self.logger.debug(f"Registered agent: {agent_id}")
    
    def can_handle_operation(self, operation_type: OperationType) -> bool:
        """Check if this router can handle the operation type."""
        return operation_type in self.workflows
    
    async def route_operation(self, 
                             operation_type: OperationType,
                             user_request: str,
                             document_state: DocumentState) -> LightweightResult:
        """
        Route and execute a simple operation through lightweight workflow.
        
        Args:
            operation_type: Type of operation to execute
            user_request: Original user request
            document_state: Current document state
            
        Returns:
            LightweightResult with execution details
        """
        start_time = time.time()
        
        try:
            # Validate operation can be handled
            if not self.can_handle_operation(operation_type):
                raise ValueError(f"Operation type {operation_type} not supported by lightweight router")
            
            # Get workflow definition
            workflow = self.workflows[operation_type]
            
            # Check for optimization patterns
            optimized_result = await self._try_optimized_execution(
                user_request, document_state, workflow
            )
            
            if optimized_result:
                execution_time = time.time() - start_time
                self._track_execution_performance(operation_type, execution_time, True, len(optimized_result.get("agents_used", [])))
                
                return LightweightResult(
                    success=True,
                    operation_type=operation_type,
                    agents_used=optimized_result.get("agents_used", []),
                    execution_time=execution_time,
                    result_data=optimized_result,
                    performance_metrics={
                        "optimized": True,
                        "workflow_type": "direct_execution"
                    }
                )
            
            # Execute standard lightweight workflow
            result = await self._execute_lightweight_workflow(
                workflow, user_request, document_state
            )
            
            execution_time = time.time() - start_time
            self._track_execution_performance(operation_type, execution_time, result.get("success", False), len(workflow.required_agents))
            
            return LightweightResult(
                success=result.get("success", False),
                operation_type=operation_type,
                agents_used=workflow.required_agents,
                execution_time=execution_time,
                result_data=result,
                performance_metrics={
                    "optimized": False,
                    "workflow_type": "lightweight_chain"
                }
            )
            
        except Exception as e:
            execution_time = time.time() - start_time
            self.logger.error(f"Lightweight routing failed for {operation_type}: {e}")
            self._track_execution_performance(operation_type, execution_time, False, 0)
            
            return LightweightResult(
                success=False,
                operation_type=operation_type,
                agents_used=[],
                execution_time=execution_time,
                result_data={"error": str(e)},
                performance_metrics={"error": True}
            )
    
    async def _try_optimized_execution(self, 
                                     user_request: str,
                                     document_state: DocumentState,
                                     workflow: LightweightWorkflow) -> Optional[Dict[str, Any]]:
        """Try optimized execution patterns for common operations."""
        
        user_request_lower = user_request.lower()
        
        # Check for direct optimization patterns
        for pattern_name, pattern_config in self.optimization_patterns.items():
            if self._matches_optimization_pattern(user_request_lower, pattern_name):
                return await self._execute_optimized_pattern(
                    pattern_name, pattern_config, user_request, document_state
                )
        
        return None
    
    def _matches_optimization_pattern(self, request_lower: str, pattern_name: str) -> bool:
        """Check if request matches an optimization pattern."""
        
        pattern_triggers = {
            "bold_text": ["make bold", "bold", "make it bold"],
            "italic_text": ["make italic", "italic", "italicize"],
            "change_font": ["change font", "font", "font family"],
            "move_cursor": ["move cursor", "go to", "navigate to", "cursor to"]
        }
        
        triggers = pattern_triggers.get(pattern_name, [])
        return any(trigger in request_lower for trigger in triggers)
    
    async def _execute_optimized_pattern(self, 
                                       pattern_name: str,
                                       pattern_config: Dict[str, Any],
                                       user_request: str,
                                       document_state: DocumentState) -> Dict[str, Any]:
        """Execute an optimized pattern."""
        
        agents_used = pattern_config.get("agents", [])
        
        # Handle direct UNO calls
        if "direct_uno_call" in pattern_config:
            return await self._execute_direct_uno_call(
                pattern_config, user_request, document_state
            )
        
        # Handle patterns requiring preparation
        if pattern_config.get("preparation_required", False):
            return await self._execute_prepared_pattern(
                pattern_name, pattern_config, user_request, document_state
            )
        
        # Handle patterns requiring context
        if pattern_config.get("context_required", False):
            return await self._execute_context_pattern(
                pattern_name, pattern_config, user_request, document_state
            )
        
        # Default execution
        return {
            "success": True,
            "pattern": pattern_name,
            "agents_used": agents_used,
            "optimization": "direct"
        }
    
    async def _execute_direct_uno_call(self, 
                                     pattern_config: Dict[str, Any],
                                     user_request: str,
                                     document_state: DocumentState) -> Dict[str, Any]:
        """Execute direct UNO API call for maximum performance."""
        
        # Get execution agent
        execution_agent = self.available_agents.get("execution_agent")
        if not execution_agent:
            raise ValueError("ExecutionAgent not available for direct UNO call")
        
        # Prepare UNO operation
        uno_call = pattern_config["direct_uno_call"]
        parameters = pattern_config.get("parameters", {})
        
        # Execute through execution agent
        try:
            # Create minimal operation for execution agent
            operation_data = {
                "type": "direct_uno",
                "method": uno_call,
                "parameters": parameters,
                "context": document_state
            }
            
            # Execute operation
            if hasattr(execution_agent, 'execute_direct_operation'):
                result = await execution_agent.execute_direct_operation(operation_data)
            else:
                # Fallback to standard execution
                result = await execution_agent.process(document_state)
            
            return {
                "success": True,
                "method": "direct_uno",
                "uno_call": uno_call,
                "agents_used": ["execution_agent"],
                "result": result
            }
            
        except Exception as e:
            self.logger.error(f"Direct UNO call failed: {e}")
            return {
                "success": False,
                "error": str(e),
                "method": "direct_uno_failed"
            }
    
    async def _execute_prepared_pattern(self, 
                                      pattern_name: str,
                                      pattern_config: Dict[str, Any],
                                      user_request: str,
                                      document_state: DocumentState) -> Dict[str, Any]:
        """Execute pattern that requires preparation (e.g., font changes)."""
        
        agents_used = pattern_config["agents"]
        
        # Execute agents in sequence
        current_state = document_state
        results = []
        
        for agent_id in agents_used:
            agent = self.available_agents.get(agent_id)
            if not agent:
                continue
            
            # Process with agent
            agent_result = await agent.process(current_state)
            results.append(agent_result)
            
            # Update state if needed
            if hasattr(agent_result, 'state_updates') and agent_result.state_updates:
                current_state = self._merge_state_updates(current_state, agent_result.state_updates)
        
        return {
            "success": True,
            "pattern": pattern_name,
            "agents_used": agents_used,
            "results": results,
            "method": "prepared_execution"
        }
    
    async def _execute_context_pattern(self, 
                                     pattern_name: str,
                                     pattern_config: Dict[str, Any],
                                     user_request: str,
                                     document_state: DocumentState) -> Dict[str, Any]:
        """Execute pattern that requires context analysis (e.g., cursor movement)."""
        
        # Get context analysis first
        context_agent = self.available_agents.get("context_analysis_agent")
        if context_agent:
            # Quick context analysis for cursor operations
            context_result = await context_agent.analyze_cursor_position(document_state)
            
            # Update document state with context
            enhanced_state = {
                **document_state,
                "cursor_context": context_result
            }
        else:
            enhanced_state = document_state
        
        # Execute remaining agents
        agents_used = pattern_config["agents"]
        results = []
        
        for agent_id in agents_used:
            if agent_id == "context_analysis_agent":
                continue  # Already processed
            
            agent = self.available_agents.get(agent_id)
            if agent:
                agent_result = await agent.process(enhanced_state)
                results.append(agent_result)
        
        return {
            "success": True,
            "pattern": pattern_name,
            "agents_used": agents_used,
            "results": results,
            "method": "context_execution"
        }
    
    async def _execute_lightweight_workflow(self, 
                                          workflow: LightweightWorkflow,
                                          user_request: str,
                                          document_state: DocumentState) -> Dict[str, Any]:
        """Execute standard lightweight workflow."""
        
        results = []
        current_state = document_state
        
        # Parallel execution for independent agents
        if workflow.max_parallel > 1 and len(workflow.required_agents) > 1:
            # Execute agents in parallel where possible
            parallel_results = await self._execute_parallel_agents(
                workflow.required_agents[:workflow.max_parallel],
                current_state
            )
            results.extend(parallel_results)
            
            # Execute remaining agents sequentially
            remaining_agents = workflow.required_agents[workflow.max_parallel:]
            for agent_id in remaining_agents:
                agent = self.available_agents.get(agent_id)
                if agent:
                    agent_result = await agent.process(current_state)
                    results.append(agent_result)
        else:
            # Sequential execution
            for agent_id in workflow.required_agents:
                agent = self.available_agents.get(agent_id)
                if agent:
                    agent_result = await agent.process(current_state)
                    results.append(agent_result)
                    
                    # Update state for next agent
                    if hasattr(agent_result, 'state_updates') and agent_result.state_updates:
                        current_state = self._merge_state_updates(current_state, agent_result.state_updates)
        
        # Aggregate results
        success = all(getattr(result, 'success', True) for result in results)
        
        return {
            "success": success,
            "workflow": workflow.operation_type.value,
            "agents_executed": workflow.required_agents,
            "results": results,
            "final_state": current_state
        }
    
    async def _execute_parallel_agents(self, 
                                     agent_ids: List[str],
                                     document_state: DocumentState) -> List[Any]:
        """Execute multiple agents in parallel."""
        
        tasks = []
        for agent_id in agent_ids:
            agent = self.available_agents.get(agent_id)
            if agent:
                task = asyncio.create_task(agent.process(document_state))
                tasks.append(task)
        
        if tasks:
            results = await asyncio.gather(*tasks, return_exceptions=True)
            # Filter out exceptions
            return [result for result in results if not isinstance(result, Exception)]
        
        return []
    
    def _merge_state_updates(self, current_state: DocumentState, updates: Dict[str, Any]) -> DocumentState:
        """Merge state updates into current state."""
        
        if isinstance(current_state, dict):
            merged = current_state.copy()
            merged.update(updates)
            return merged
        
        # If DocumentState is a proper class, handle appropriately
        return current_state
    
    def _track_execution_performance(self, 
                                   operation_type: OperationType,
                                   execution_time: float,
                                   success: bool,
                                   agents_used: int):
        """Track execution performance for optimization."""
        
        self.performance_stats["total_executions"] += 1
        
        # Update average execution time
        total = self.performance_stats["total_executions"]
        current_avg = self.performance_stats["avg_execution_time"]
        self.performance_stats["avg_execution_time"] = (
            (current_avg * (total - 1) + execution_time) / total
        )
        
        # Update success rate
        if success:
            success_count = int(self.performance_stats["success_rate"] * (total - 1)) + 1
        else:
            success_count = int(self.performance_stats["success_rate"] * (total - 1))
        
        self.performance_stats["success_rate"] = success_count / total
        
        # Track agent usage
        op_type_str = operation_type.value
        if op_type_str not in self.performance_stats["agent_usage_stats"]:
            self.performance_stats["agent_usage_stats"][op_type_str] = {
                "total_executions": 0,
                "avg_agents_used": 0.0,
                "avg_execution_time": 0.0
            }
        
        stats = self.performance_stats["agent_usage_stats"][op_type_str]
        stats["total_executions"] += 1
        
        # Update averages
        n = stats["total_executions"]
        stats["avg_agents_used"] = ((stats["avg_agents_used"] * (n - 1)) + agents_used) / n
        stats["avg_execution_time"] = ((stats["avg_execution_time"] * (n - 1)) + execution_time) / n
        
        # Store execution record
        record = {
            "timestamp": datetime.now().isoformat(),
            "operation_type": operation_type.value,
            "execution_time": execution_time,
            "success": success,
            "agents_used": agents_used,
            "within_target": execution_time <= self.target_execution_time
        }
        
        self.execution_history.append(record)
        
        # Keep only recent history
        if len(self.execution_history) > 500:
            self.execution_history = self.execution_history[-500:]
    
    def get_performance_metrics(self) -> Dict[str, Any]:
        """Get performance metrics for monitoring."""
        
        recent_executions = self.execution_history[-100:] if self.execution_history else []
        
        metrics = {
            "overall_stats": self.performance_stats,
            "recent_performance": {
                "executions": len(recent_executions),
                "avg_time": sum(r["execution_time"] for r in recent_executions) / len(recent_executions) if recent_executions else 0,
                "success_rate": sum(1 for r in recent_executions if r["success"]) / len(recent_executions) if recent_executions else 1.0,
                "target_compliance": sum(1 for r in recent_executions if r["within_target"]) / len(recent_executions) if recent_executions else 1.0
            },
            "optimization_opportunities": self._identify_optimization_opportunities()
        }
        
        return metrics
    
    def _identify_optimization_opportunities(self) -> List[str]:
        """Identify opportunities for further optimization."""
        
        opportunities = []
        
        # Check if average execution time is above target
        if self.performance_stats["avg_execution_time"] > self.target_execution_time:
            opportunities.append("Overall execution time above target")
        
        # Check agent usage efficiency
        for op_type, stats in self.performance_stats["agent_usage_stats"].items():
            if stats["avg_agents_used"] > 2:
                opportunities.append(f"Operation {op_type} uses too many agents")
            
            if stats["avg_execution_time"] > self.target_execution_time:
                opportunities.append(f"Operation {op_type} execution time above target")
        
        # Check for frequently failing operations
        recent_failures = [r for r in self.execution_history[-100:] if not r["success"]]
        if len(recent_failures) > 5:
            opportunities.append("High failure rate detected")
        
        return opportunities
    
    def get_execution_history(self, limit: int = 100) -> List[Dict[str, Any]]:
        """Get recent execution history."""
        return self.execution_history[-limit:] if self.execution_history else []
    
    def optimize_workflow(self, operation_type: OperationType, performance_data: Dict[str, Any]):
        """Optimize workflow based on performance data."""
        
        if operation_type in self.workflows:
            workflow = self.workflows[operation_type]
            
            # Adjust based on performance
            avg_time = performance_data.get("avg_execution_time", 0)
            
            if avg_time > self.target_execution_time:
                # Try to reduce agents or enable more optimizations
                if len(workflow.required_agents) > 1:
                    workflow.max_parallel = min(workflow.max_parallel + 1, len(workflow.required_agents))
                    self.logger.info(f"Increased parallelization for {operation_type} to {workflow.max_parallel}")
            
            elif avg_time < self.target_execution_time * 0.5:
                # Consider adding quality improvements
                workflow.skip_validation = False
                self.logger.info(f"Enabled validation for {operation_type} due to good performance")
    
    def supports_operation_type(self, operation_type: OperationType) -> bool:
        """Check if this router supports the given operation type."""
        return operation_type in self.workflows
    
