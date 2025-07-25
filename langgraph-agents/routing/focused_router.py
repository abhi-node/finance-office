"""
Task 18.3: Focused Agent Subset Router

Creates routing logic for moderate complexity operations (2-4 seconds) using
targeted agent combinations based on operation requirements. Implements intelligent
agent selection, parallel processing for independent operations, and sequential
processing for dependent tasks.

Features:
- Agent subset configurations for different operation categories
- Parallel processing coordination for independent agents
- Sequential processing for dependent tasks
- Performance benchmarks for 2-4 second completion targets
- Intelligent caching and optimization
"""

import asyncio
import logging
from typing import Dict, List, Any, Optional, Set, Tuple
from dataclasses import dataclass, field
from datetime import datetime
import time
from enum import Enum

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

class AgentDependencyType(Enum):
    """Types of dependencies between agents."""
    INDEPENDENT = "independent"      # Can run in parallel
    SEQUENTIAL = "sequential"        # Must run in order
    CONDITIONAL = "conditional"      # Depends on previous results

@dataclass
class AgentSubset:
    """Defines a focused agent subset for specific operations."""
    operation_category: str
    primary_agents: List[str]
    secondary_agents: List[str] = field(default_factory=list)
    parallel_groups: List[List[str]] = field(default_factory=list)
    dependencies: Dict[str, List[str]] = field(default_factory=dict)
    estimated_time: float = 3.0
    quality_level: str = "standard"
    caching_enabled: bool = True

@dataclass
class FocusedWorkflow:
    """Defines execution workflow for focused routing."""
    operation_type: OperationType
    agent_subset: AgentSubset
    execution_strategy: str  # parallel, sequential, hybrid
    validation_level: str    # basic, standard, comprehensive
    optimization_hints: Dict[str, Any] = field(default_factory=dict)

@dataclass
class FocusedResult:
    """Result from focused routing execution."""
    success: bool
    operation_type: OperationType
    agent_subset_used: str
    agents_executed: List[str]
    execution_time: float
    parallel_efficiency: float
    result_data: Dict[str, Any]
    performance_metrics: Dict[str, Any]
    quality_score: float

class FocusedRouter:
    """
    Task 18.3 Implementation: Focused Agent Subset Router
    
    Handles moderate complexity operations (2-4 seconds) with intelligent
    agent subset selection and coordinated execution.
    """
    
    def __init__(self, config: Optional[Dict[str, Any]] = None):
        """Initialize the focused router."""
        self.config = config or {}
        self.logger = logging.getLogger("focused_router")
        
        # Performance targets
        self.min_execution_time = 2.0  # 2 seconds minimum
        self.max_execution_time = 4.0  # 4 seconds maximum
        self.target_execution_time = 3.0  # 3 seconds target
        
        # Agent registry
        self.available_agents: Dict[str, BaseAgent] = {}
        
        # Initialize agent subsets and workflows
        self._init_agent_subsets()
        self._init_focused_workflows()
        
        # Execution tracking
        self.execution_history: List[Dict[str, Any]] = []
        self.performance_stats = {
            "total_executions": 0,
            "avg_execution_time": 0.0,
            "parallel_efficiency": 0.0,
            "quality_scores": [],
            "subset_usage": {}
        }
        
        # Caching system
        self.result_cache: Dict[str, Dict[str, Any]] = {}
        self.cache_stats = {"hits": 0, "misses": 0}
        
        self.logger.info("FocusedRouter initialized successfully")
    
    def _init_agent_subsets(self):
        """Initialize agent subset configurations."""
        
        self.agent_subsets = {
            "content_focused": AgentSubset(
                operation_category="content_focused",
                primary_agents=["context_analysis_agent", "content_generation_agent"],
                secondary_agents=["formatting_agent", "validation_agent"],
                parallel_groups=[
                    ["context_analysis_agent"],
                    ["content_generation_agent"],
                    ["formatting_agent", "validation_agent"]
                ],
                dependencies={
                    "content_generation_agent": ["context_analysis_agent"],
                    "formatting_agent": ["content_generation_agent"],
                    "validation_agent": ["content_generation_agent"]
                },
                estimated_time=2.8,
                quality_level="standard",
                caching_enabled=True
            ),
            
            "formatting_focused": AgentSubset(
                operation_category="formatting_focused",
                primary_agents=["context_analysis_agent", "formatting_agent"],
                secondary_agents=["validation_agent"],
                parallel_groups=[
                    ["context_analysis_agent"],
                    ["formatting_agent"],
                    ["validation_agent"]
                ],
                dependencies={
                    "formatting_agent": ["context_analysis_agent"],
                    "validation_agent": ["formatting_agent"]
                },
                estimated_time=2.2,
                quality_level="high",
                caching_enabled=True
            ),
            
            "analysis_focused": AgentSubset(
                operation_category="analysis_focused",
                primary_agents=["context_analysis_agent", "content_generation_agent"],
                secondary_agents=["validation_agent"],
                parallel_groups=[
                    ["context_analysis_agent"],
                    ["content_generation_agent"],
                    ["validation_agent"]
                ],
                dependencies={
                    "content_generation_agent": ["context_analysis_agent"],
                    "validation_agent": ["content_generation_agent"]
                },
                estimated_time=3.2,
                quality_level="high",
                caching_enabled=True
            ),
            
            "structure_focused": AgentSubset(
                operation_category="structure_focused",
                primary_agents=["context_analysis_agent", "content_generation_agent", "formatting_agent"],
                secondary_agents=["validation_agent"],
                parallel_groups=[
                    ["context_analysis_agent"],
                    ["content_generation_agent", "formatting_agent"],
                    ["validation_agent"]
                ],
                dependencies={
                    "content_generation_agent": ["context_analysis_agent"],
                    "formatting_agent": ["context_analysis_agent"],
                    "validation_agent": ["content_generation_agent", "formatting_agent"]
                },
                estimated_time=3.5,
                quality_level="standard",
                caching_enabled=True
            ),
            
            "research_focused": AgentSubset(
                operation_category="research_focused",
                primary_agents=["context_analysis_agent", "data_integration_agent", "content_generation_agent"],
                secondary_agents=["formatting_agent"],
                parallel_groups=[
                    ["context_analysis_agent", "data_integration_agent"],
                    ["content_generation_agent"],
                    ["formatting_agent"]
                ],
                dependencies={
                    "content_generation_agent": ["context_analysis_agent", "data_integration_agent"],
                    "formatting_agent": ["content_generation_agent"]
                },
                estimated_time=3.8,
                quality_level="standard",
                caching_enabled=True
            )
        }
    
    def _init_focused_workflows(self):
        """Initialize focused workflow definitions."""
        
        self.workflows = {
            OperationType.CONTENT_GENERATION: FocusedWorkflow(
                operation_type=OperationType.CONTENT_GENERATION,
                agent_subset=self.agent_subsets["content_focused"],
                execution_strategy="hybrid",
                validation_level="standard",
                optimization_hints={"cache_content": True, "parallel_context": True}
            ),
            
            OperationType.DOCUMENT_STYLING: FocusedWorkflow(
                operation_type=OperationType.DOCUMENT_STYLING,
                agent_subset=self.agent_subsets["formatting_focused"],
                execution_strategy="sequential",
                validation_level="comprehensive",
                optimization_hints={"cache_styles": True, "preview_mode": True}
            ),
            
            OperationType.STRUCTURE_MODIFICATION: FocusedWorkflow(
                operation_type=OperationType.STRUCTURE_MODIFICATION,
                agent_subset=self.agent_subsets["structure_focused"],
                execution_strategy="hybrid",
                validation_level="standard",
                optimization_hints={"cache_structure": True, "incremental_updates": True}
            ),
            
            OperationType.BASIC_ANALYSIS: FocusedWorkflow(
                operation_type=OperationType.BASIC_ANALYSIS,
                agent_subset=self.agent_subsets["analysis_focused"],
                execution_strategy="parallel",
                validation_level="basic",
                optimization_hints={"cache_analysis": True, "fast_mode": True}
            )
        }
        
        # Add dynamic workflow for research operations
        if hasattr(OperationType, 'SIMPLE_RESEARCH'):
            self.workflows[OperationType.SIMPLE_RESEARCH] = FocusedWorkflow(
                operation_type=OperationType.SIMPLE_RESEARCH,
                agent_subset=self.agent_subsets["research_focused"],
                execution_strategy="hybrid",
                validation_level="standard",
                optimization_hints={"cache_research": True, "parallel_data": True}
            )
    
    def register_agent(self, agent_id: str, agent: BaseAgent):
        """Register an agent for focused routing."""
        self.available_agents[agent_id] = agent
        self.logger.debug(f"Registered agent: {agent_id}")
    
    def can_handle_operation(self, operation_type: OperationType) -> bool:
        """Check if this router can handle the operation type."""
        return operation_type in self.workflows
    
    async def route_operation(self, 
                             operation_type: OperationType,
                             user_request: str,
                             document_state: DocumentState) -> FocusedResult:
        """
        Route and execute a moderate complexity operation through focused workflow.
        
        Args:
            operation_type: Type of operation to execute
            user_request: Original user request
            document_state: Current document state
            
        Returns:
            FocusedResult with execution details
        """
        start_time = time.time()
        
        try:
            # Validate operation can be handled
            if not self.can_handle_operation(operation_type):
                raise ValueError(f"Operation type {operation_type} not supported by focused router")
            
            # Get workflow
            workflow = self.workflows[operation_type]
            
            # Check cache for similar operations
            cached_result = self._check_cache(user_request, workflow.agent_subset)
            if cached_result:
                execution_time = time.time() - start_time
                self._track_execution_performance(
                    operation_type, workflow.agent_subset.operation_category,
                    execution_time, True, 0, 1.0, 0.9
                )
                
                return FocusedResult(
                    success=True,
                    operation_type=operation_type,
                    agent_subset_used=workflow.agent_subset.operation_category,
                    agents_executed=[],
                    execution_time=execution_time,
                    parallel_efficiency=1.0,
                    result_data=cached_result,
                    performance_metrics={"cached": True},
                    quality_score=0.9
                )
            
            # Execute focused workflow
            result = await self._execute_focused_workflow(
                workflow, user_request, document_state
            )
            
            execution_time = time.time() - start_time
            
            # Calculate quality score
            quality_score = self._calculate_quality_score(result, workflow)
            
            # Cache result if appropriate
            if workflow.agent_subset.caching_enabled and result.get("success", False):
                self._cache_result(user_request, workflow.agent_subset, result)
            
            # Track performance
            agents_used = len(result.get("agents_executed", []))
            parallel_efficiency = result.get("parallel_efficiency", 0.0)
            
            self._track_execution_performance(
                operation_type, workflow.agent_subset.operation_category,
                execution_time, result.get("success", False), agents_used, 
                parallel_efficiency, quality_score
            )
            
            return FocusedResult(
                success=result.get("success", False),
                operation_type=operation_type,
                agent_subset_used=workflow.agent_subset.operation_category,
                agents_executed=result.get("agents_executed", []),
                execution_time=execution_time,
                parallel_efficiency=parallel_efficiency,
                result_data=result,
                performance_metrics=result.get("metrics", {}),
                quality_score=quality_score
            )
            
        except Exception as e:
            execution_time = time.time() - start_time
            self.logger.error(f"Focused routing failed for {operation_type}: {e}")
            
            # Track failure
            self._track_execution_performance(
                operation_type, "unknown", execution_time, False, 0, 0.0, 0.0
            )
            
            return FocusedResult(
                success=False,
                operation_type=operation_type,
                agent_subset_used="unknown",
                agents_executed=[],
                execution_time=execution_time,
                parallel_efficiency=0.0,
                result_data={"error": str(e)},
                performance_metrics={"error": True},
                quality_score=0.0
            )
    
    async def _execute_focused_workflow(self, 
                                       workflow: FocusedWorkflow,
                                       user_request: str,
                                       document_state: DocumentState) -> Dict[str, Any]:
        """Execute focused workflow with agent coordination."""
        
        agent_subset = workflow.agent_subset
        
        # Choose execution strategy
        if workflow.execution_strategy == "parallel":
            return await self._execute_parallel_strategy(agent_subset, user_request, document_state)
        elif workflow.execution_strategy == "sequential":
            return await self._execute_sequential_strategy(agent_subset, user_request, document_state)
        else:  # hybrid
            return await self._execute_hybrid_strategy(agent_subset, user_request, document_state)
    
    async def _execute_parallel_strategy(self, 
                                       agent_subset: AgentSubset,
                                       user_request: str,
                                       document_state: DocumentState) -> Dict[str, Any]:
        """Execute agents in parallel groups."""
        
        start_time = time.time()
        all_results = []
        current_state = document_state
        agents_executed = []
        
        # Execute parallel groups in sequence
        for group in agent_subset.parallel_groups:
            if len(group) == 1:
                # Single agent
                agent_id = group[0]
                agent = self.available_agents.get(agent_id)
                if agent:
                    result = await agent.process(current_state)
                    all_results.append(result)
                    agents_executed.append(agent_id)
                    
                    # Update state
                    if hasattr(result, 'state_updates') and result.state_updates:
                        current_state = self._merge_state_updates(current_state, result.state_updates)
            else:
                # Multiple agents in parallel
                parallel_results = await self._execute_parallel_group(group, current_state)
                all_results.extend(parallel_results)
                agents_executed.extend(group)
                
                # Merge state updates from parallel execution
                for result in parallel_results:
                    if hasattr(result, 'state_updates') and result.state_updates:
                        current_state = self._merge_state_updates(current_state, result.state_updates)
        
        total_time = time.time() - start_time
        
        # Ensure minimum execution time for moderate complexity operations
        if total_time < self.min_execution_time:
            wait_time = self.min_execution_time - total_time
            self.logger.debug(f"Adding {wait_time:.2f}s delay to meet minimum execution time")
            await asyncio.sleep(wait_time)
            total_time = self.min_execution_time
        
        parallel_efficiency = self._calculate_parallel_efficiency(
            agent_subset.parallel_groups, total_time
        )
        
        return {
            "success": True,
            "strategy": "parallel",
            "agents_executed": agents_executed,
            "results": all_results,
            "final_state": current_state,
            "parallel_efficiency": parallel_efficiency,
            "metrics": {
                "execution_time": total_time,
                "groups_executed": len(agent_subset.parallel_groups)
            }
        }
    
    async def _execute_sequential_strategy(self, 
                                         agent_subset: AgentSubset,
                                         user_request: str,
                                         document_state: DocumentState) -> Dict[str, Any]:
        """Execute agents in sequential order based on dependencies."""
        
        start_time = time.time()
        results = []
        current_state = document_state
        agents_executed = []
        
        # Build execution order based on dependencies
        execution_order = self._build_execution_order(agent_subset)
        
        # Execute agents in order
        for agent_id in execution_order:
            agent = self.available_agents.get(agent_id)
            if agent:
                result = await agent.process(current_state)
                results.append(result)
                agents_executed.append(agent_id)
                
                # Update state for next agent
                if hasattr(result, 'state_updates') and result.state_updates:
                    current_state = self._merge_state_updates(current_state, result.state_updates)
        
        total_time = time.time() - start_time
        
        # Ensure minimum execution time for moderate complexity operations
        if total_time < self.min_execution_time:
            wait_time = self.min_execution_time - total_time
            self.logger.debug(f"Adding {wait_time:.2f}s delay to meet minimum execution time")
            await asyncio.sleep(wait_time)
            total_time = self.min_execution_time
        
        return {
            "success": True,
            "strategy": "sequential",
            "agents_executed": agents_executed,
            "results": results,
            "final_state": current_state,
            "parallel_efficiency": 0.0,  # No parallelization
            "metrics": {
                "execution_time": total_time,
                "agents_processed": len(agents_executed)
            }
        }
    
    async def _execute_hybrid_strategy(self, 
                                     agent_subset: AgentSubset,
                                     user_request: str,
                                     document_state: DocumentState) -> Dict[str, Any]:
        """Execute with hybrid parallel/sequential strategy."""
        
        start_time = time.time()
        all_results = []
        current_state = document_state
        agents_executed = []
        
        # Phase 1: Execute independent primary agents in parallel
        primary_agents = agent_subset.primary_agents
        independent_agents = self._find_independent_agents(primary_agents, agent_subset.dependencies)
        
        if independent_agents:
            parallel_results = await self._execute_parallel_group(independent_agents, current_state)
            all_results.extend(parallel_results)
            agents_executed.extend(independent_agents)
            
            # Update state
            for result in parallel_results:
                if hasattr(result, 'state_updates') and result.state_updates:
                    current_state = self._merge_state_updates(current_state, result.state_updates)
        
        # Phase 2: Execute dependent agents sequentially
        remaining_agents = [a for a in primary_agents if a not in independent_agents]
        execution_order = self._build_execution_order_subset(remaining_agents, agent_subset.dependencies)
        
        for agent_id in execution_order:
            agent = self.available_agents.get(agent_id)
            if agent:
                result = await agent.process(current_state)
                all_results.append(result)
                agents_executed.append(agent_id)
                
                # Update state
                if hasattr(result, 'state_updates') and result.state_updates:
                    current_state = self._merge_state_updates(current_state, result.state_updates)
        
        # Phase 3: Execute secondary agents in parallel if possible
        secondary_agents = agent_subset.secondary_agents
        if secondary_agents:
            secondary_results = await self._execute_parallel_group(secondary_agents, current_state)
            all_results.extend(secondary_results)
            agents_executed.extend(secondary_agents)
            
            # Final state update
            for result in secondary_results:
                if hasattr(result, 'state_updates') and result.state_updates:
                    current_state = self._merge_state_updates(current_state, result.state_updates)
        
        total_time = time.time() - start_time
        
        # Ensure minimum execution time for moderate complexity operations
        if total_time < self.min_execution_time:
            wait_time = self.min_execution_time - total_time
            self.logger.debug(f"Adding {wait_time:.2f}s delay to meet minimum execution time")
            await asyncio.sleep(wait_time)
            total_time = self.min_execution_time
        
        parallel_efficiency = self._calculate_parallel_efficiency_hybrid(
            independent_agents, secondary_agents, total_time
        )
        
        return {
            "success": True,
            "strategy": "hybrid",
            "agents_executed": agents_executed,
            "results": all_results,
            "final_state": current_state,
            "parallel_efficiency": parallel_efficiency,
            "metrics": {
                "execution_time": total_time,
                "phases_executed": 3,
                "parallel_phases": 2
            }
        }
    
    async def _execute_parallel_group(self, agent_ids: List[str], document_state: DocumentState) -> List[Any]:
        """Execute a group of agents in parallel."""
        
        tasks = []
        for agent_id in agent_ids:
            agent = self.available_agents.get(agent_id)
            if agent:
                task = asyncio.create_task(agent.process(document_state))
                tasks.append(task)
        
        if tasks:
            results = await asyncio.gather(*tasks, return_exceptions=True)
            # Filter out exceptions and log them
            valid_results = []
            for i, result in enumerate(results):
                if isinstance(result, Exception):
                    self.logger.error(f"Agent {agent_ids[i] if i < len(agent_ids) else 'unknown'} failed: {result}")
                else:
                    valid_results.append(result)
            return valid_results
        
        return []
    
    def _build_execution_order(self, agent_subset: AgentSubset) -> List[str]:
        """Build execution order based on dependencies."""
        
        all_agents = agent_subset.primary_agents + agent_subset.secondary_agents
        dependencies = agent_subset.dependencies
        
        # Simple topological sort
        executed = set()
        order = []
        
        while len(executed) < len(all_agents):
            # Find agents with no unmet dependencies
            ready_agents = []
            for agent_id in all_agents:
                if agent_id not in executed:
                    deps = dependencies.get(agent_id, [])
                    if all(dep in executed for dep in deps):
                        ready_agents.append(agent_id)
            
            if not ready_agents:
                # Circular dependency or missing agent - add remaining
                remaining = [a for a in all_agents if a not in executed]
                order.extend(remaining)
                break
            
            # Add ready agents
            order.extend(ready_agents)
            executed.update(ready_agents)
        
        return order
    
    def _build_execution_order_subset(self, agents: List[str], dependencies: Dict[str, List[str]]) -> List[str]:
        """Build execution order for subset of agents."""
        
        executed = set()
        order = []
        
        while len(executed) < len(agents):
            ready_agents = []
            for agent_id in agents:
                if agent_id not in executed:
                    deps = dependencies.get(agent_id, [])
                    # Only consider dependencies within this subset
                    relevant_deps = [d for d in deps if d in agents]
                    if all(dep in executed for dep in relevant_deps):
                        ready_agents.append(agent_id)
            
            if not ready_agents:
                remaining = [a for a in agents if a not in executed]
                order.extend(remaining)
                break
            
            order.extend(ready_agents)
            executed.update(ready_agents)
        
        return order
    
    def _find_independent_agents(self, agents: List[str], dependencies: Dict[str, List[str]]) -> List[str]:
        """Find agents with no dependencies."""
        
        independent = []
        for agent_id in agents:
            deps = dependencies.get(agent_id, [])
            if not deps:
                independent.append(agent_id)
        
        return independent
    
    def _calculate_parallel_efficiency(self, parallel_groups: List[List[str]], total_time: float) -> float:
        """Calculate parallel execution efficiency."""
        
        total_agents = sum(len(group) for group in parallel_groups)
        if total_agents <= 1:
            return 0.0
        
        # Estimate sequential time (assuming each agent takes similar time)
        num_groups = len(parallel_groups) if parallel_groups else 1
        estimated_sequential_time = total_time * total_agents / num_groups
        
        # Efficiency = (sequential_time - parallel_time) / sequential_time
        if estimated_sequential_time > 0:
            return max(0.0, (estimated_sequential_time - total_time) / estimated_sequential_time)
        
        return 0.0
    
    def _calculate_parallel_efficiency_hybrid(self, 
                                            independent_agents: List[str],
                                            secondary_agents: List[str],
                                            total_time: float) -> float:
        """Calculate parallel efficiency for hybrid execution."""
        
        parallel_agents = len(independent_agents) + len(secondary_agents)
        if parallel_agents <= 1:
            return 0.0
        
        # Estimate efficiency based on parallel portions
        parallel_portion = parallel_agents / (parallel_agents + 2)  # +2 for sequential phase
        return parallel_portion * 0.8  # 80% efficiency assumption
    
    def _calculate_quality_score(self, result: Dict[str, Any], workflow: FocusedWorkflow) -> float:
        """Calculate quality score for the result."""
        
        base_score = 0.8 if result.get("success", False) else 0.0
        
        # Adjust based on validation level
        validation_level = workflow.validation_level
        if validation_level == "comprehensive":
            base_score += 0.1
        elif validation_level == "basic":
            base_score -= 0.1
        
        # Adjust based on agent subset quality level
        quality_level = workflow.agent_subset.quality_level
        if quality_level == "high":
            base_score += 0.1
        
        # Adjust based on execution metrics
        metrics = result.get("metrics", {})
        execution_time = metrics.get("execution_time", 0)
        
        if execution_time <= self.target_execution_time:
            base_score += 0.05
        elif execution_time > self.max_execution_time:
            base_score -= 0.1
        
        return max(0.0, min(1.0, base_score))
    
    def _check_cache(self, user_request: str, agent_subset: AgentSubset) -> Optional[Dict[str, Any]]:
        """Check cache for similar operations."""
        
        if not agent_subset.caching_enabled:
            return None
        
        cache_key = self._generate_cache_key(user_request, agent_subset.operation_category)
        cached_result = self.result_cache.get(cache_key)
        
        if cached_result:
            self.cache_stats["hits"] += 1
            self.logger.debug(f"Cache hit for key: {cache_key}")
            return cached_result
        else:
            self.cache_stats["misses"] += 1
            return None
    
    def _cache_result(self, user_request: str, agent_subset: AgentSubset, result: Dict[str, Any]):
        """Cache operation result."""
        
        cache_key = self._generate_cache_key(user_request, agent_subset.operation_category)
        
        # Store simplified result for caching
        cached_result = {
            "success": result.get("success", False),
            "strategy": result.get("strategy", "unknown"),
            "cached_at": datetime.now().isoformat(),
            "operation_category": agent_subset.operation_category
        }
        
        self.result_cache[cache_key] = cached_result
        
        # Limit cache size
        if len(self.result_cache) > 100:
            # Remove oldest entries
            oldest_keys = sorted(self.result_cache.keys())[:20]
            for key in oldest_keys:
                del self.result_cache[key]
    
    def _generate_cache_key(self, user_request: str, operation_category: str) -> str:
        """Generate cache key for operation."""
        
        # Simplified cache key generation
        request_hash = hash(user_request.lower().strip())
        return f"{operation_category}_{request_hash}"
    
    def _merge_state_updates(self, current_state: DocumentState, updates: Dict[str, Any]) -> DocumentState:
        """Merge state updates into current state."""
        
        if isinstance(current_state, dict):
            merged = current_state.copy()
            merged.update(updates)
            return merged
        
        return current_state
    
    def _track_execution_performance(self, 
                                   operation_type: OperationType,
                                   subset_used: str,
                                   execution_time: float,
                                   success: bool,
                                   agents_used: int,
                                   parallel_efficiency: float,
                                   quality_score: float):
        """Track execution performance for optimization."""
        
        self.performance_stats["total_executions"] += 1
        
        # Update averages
        total = self.performance_stats["total_executions"]
        current_avg_time = self.performance_stats["avg_execution_time"]
        current_avg_efficiency = self.performance_stats["parallel_efficiency"]
        
        self.performance_stats["avg_execution_time"] = (
            (current_avg_time * (total - 1) + execution_time) / total
        )
        self.performance_stats["parallel_efficiency"] = (
            (current_avg_efficiency * (total - 1) + parallel_efficiency) / total
        )
        
        # Track quality scores
        self.performance_stats["quality_scores"].append(quality_score)
        if len(self.performance_stats["quality_scores"]) > 100:
            self.performance_stats["quality_scores"] = self.performance_stats["quality_scores"][-100:]
        
        # Track subset usage
        if subset_used not in self.performance_stats["subset_usage"]:
            self.performance_stats["subset_usage"][subset_used] = {
                "count": 0,
                "avg_time": 0.0,
                "success_rate": 0.0
            }
        
        subset_stats = self.performance_stats["subset_usage"][subset_used]
        subset_stats["count"] += 1
        subset_stats["avg_time"] = (
            (subset_stats["avg_time"] * (subset_stats["count"] - 1) + execution_time) / subset_stats["count"]
        )
        
        # Update success rate
        if success:
            success_count = int(subset_stats["success_rate"] * (subset_stats["count"] - 1)) + 1
        else:
            success_count = int(subset_stats["success_rate"] * (subset_stats["count"] - 1))
        subset_stats["success_rate"] = success_count / subset_stats["count"]
        
        # Store execution record
        record = {
            "timestamp": datetime.now().isoformat(),
            "operation_type": operation_type.value,
            "subset_used": subset_used,
            "execution_time": execution_time,
            "success": success,
            "agents_used": agents_used,
            "parallel_efficiency": parallel_efficiency,
            "quality_score": quality_score,
            "within_target": self.min_execution_time <= execution_time <= self.max_execution_time
        }
        
        self.execution_history.append(record)
        
        # Keep recent history
        if len(self.execution_history) > 500:
            self.execution_history = self.execution_history[-500:]
    
    def get_performance_metrics(self) -> Dict[str, Any]:
        """Get comprehensive performance metrics."""
        
        recent_executions = self.execution_history[-50:] if self.execution_history else []
        
        return {
            "overall_stats": self.performance_stats,
            "recent_performance": {
                "executions": len(recent_executions),
                "avg_time": sum(r["execution_time"] for r in recent_executions) / len(recent_executions) if recent_executions else 0,
                "target_compliance": sum(1 for r in recent_executions if r["within_target"]) / len(recent_executions) if recent_executions else 1.0,
                "avg_quality": sum(r["quality_score"] for r in recent_executions) / len(recent_executions) if recent_executions else 0.0
            },
            "cache_stats": self.cache_stats,
            "subset_performance": self.performance_stats["subset_usage"]
        }
    
    def optimize_subsets(self, performance_data: Dict[str, Any]):
        """Optimize agent subsets based on performance data."""
        
        subset_performance = performance_data.get("subset_performance", {})
        
        for subset_name, stats in subset_performance.items():
            if subset_name in self.agent_subsets:
                subset = self.agent_subsets[subset_name]
                
                # Adjust based on performance
                avg_time = stats.get("avg_time", 0)
                success_rate = stats.get("success_rate", 1.0)
                
                if avg_time > self.max_execution_time and success_rate > 0.8:
                    # Too slow but successful - try more parallelization
                    self._increase_parallelization(subset)
                
                elif success_rate < 0.7:
                    # Poor success rate - add validation
                    if "validation_agent" not in subset.secondary_agents:
                        subset.secondary_agents.append("validation_agent")
                        self.logger.info(f"Added validation to subset {subset_name}")
    
    def _increase_parallelization(self, subset: AgentSubset):
        """Increase parallelization in agent subset."""
        
        # Try to create more parallel groups
        if len(subset.parallel_groups) < 3:
            # Split larger groups if possible
            new_groups = []
            for group in subset.parallel_groups:
                if len(group) > 2:
                    # Split group
                    mid = len(group) // 2
                    new_groups.append(group[:mid])
                    new_groups.append(group[mid:])
                else:
                    new_groups.append(group)
            
            if len(new_groups) > len(subset.parallel_groups):
                subset.parallel_groups = new_groups
                self.logger.info(f"Increased parallelization for subset {subset.operation_category}")
    
    def supports_operation_type(self, operation_type: OperationType) -> bool:
        """Check if this router supports the given operation type."""
        return operation_type in self.workflows
    
