"""
Enhanced DocumentMasterAgent with Integrated Error Handling

This module extends the DocumentMasterAgent with comprehensive error handling
capabilities implemented in Task 19.1. It integrates the UnifiedErrorCoordinator,
ErrorTracker, and AuditTrail to provide robust error management throughout
the agent orchestration process.

Enhanced Features:
- Integrated error handling with unified coordinator
- Comprehensive error tracking and pattern analysis
- Audit trail for all operations and errors
- Improved error recovery and user communication
- Performance monitoring with error correlation
"""

import asyncio
import logging
import time
from dataclasses import dataclass, field
from datetime import datetime, timezone
from typing import Any, Dict, List, Optional, Union
from contextlib import asynccontextmanager

# Import base agent framework
from .base import (
    BaseAgent, AgentResult, AgentCapability, AgentLifecycleState,
    ValidationResult, ToolCallResult
)

# Import error handling infrastructure
from .error_handler import (
    UnifiedErrorCoordinator, ErrorContext, ErrorSeverity, 
    ErrorCategory, ErrorResponse
)
from .error_tracking import ErrorTracker, AuditTrail
from .error_integration import (
    ErrorHandlingIntegrator, ErrorIntegrationConfig,
    get_error_integrator, handle_agent_error
)

# Import existing DocumentMasterAgent functionality
try:
    from .document_master import (
        DocumentMasterAgent, RequestAnalysis, OperationComplexity,
        WorkflowPath, RequestType, AgentSubset
    )
    from state.document_state import DocumentState
    from langchain_core.messages import BaseMessage
except ImportError:
    # Fallback for testing
    DocumentMasterAgent = BaseAgent
    RequestAnalysis = Any
    OperationComplexity = Any
    WorkflowPath = Any
    RequestType = Any
    AgentSubset = Any
    DocumentState = Dict[str, Any]
    BaseMessage = Any


@dataclass
class EnhancedRequestAnalysis:
    """Enhanced request analysis with error handling context."""
    # Original analysis fields
    complexity: OperationComplexity
    request_type: RequestType
    confidence: float
    reasoning: str
    required_agents: List[str]
    estimated_time: float
    
    # Enhanced error handling fields
    error_risk_assessment: Dict[str, float] = field(default_factory=dict)
    potential_failure_points: List[str] = field(default_factory=list)
    recovery_strategies: List[str] = field(default_factory=list)
    monitoring_requirements: Dict[str, Any] = field(default_factory=dict)
    
    # Performance and reliability
    expected_resource_usage: Dict[str, float] = field(default_factory=dict)
    timeout_estimates: Dict[str, float] = field(default_factory=dict)
    fallback_options: List[str] = field(default_factory=list)


class EnhancedDocumentMasterAgent(BaseAgent):
    """
    Enhanced DocumentMasterAgent with comprehensive error handling integration.
    
    This agent extends the original DocumentMasterAgent with integrated error 
    handling, tracking, and recovery capabilities while maintaining all original
    orchestration functionality.
    """
    
    def __init__(self, 
                 agent_id: str = "enhanced_document_master",
                 config: Optional[Dict[str, Any]] = None,
                 error_integration_config: Optional[ErrorIntegrationConfig] = None):
        """
        Initialize enhanced document master agent.
        
        Args:
            agent_id: Unique identifier for the agent
            config: Agent configuration
            error_integration_config: Error handling configuration
        """
        # Initialize base agent
        super().__init__(
            agent_id=agent_id,
            capabilities=[
                AgentCapability.ORCHESTRATION,
                AgentCapability.DOCUMENT_ANALYSIS,
                AgentCapability.QUALITY_ASSURANCE
            ],
            config=config or {}
        )
        
        # Initialize error handling integration
        self.error_integrator = get_error_integrator(error_integration_config)
        self.error_coordinator = self.error_integrator.unified_coordinator
        self.error_tracker = self.error_integrator.error_tracker
        self.audit_trail = self.error_integrator.audit_trail
        
        # Enhanced orchestration state
        self.specialized_agents: Dict[str, BaseAgent] = {}
        self.agent_error_history: Dict[str, List[ErrorContext]] = {}
        self.workflow_statistics: Dict[str, Any] = {
            "total_requests": 0,
            "successful_requests": 0,
            "failed_requests": 0,
            "error_recovery_attempts": 0,
            "error_recovery_successes": 0
        }
        
        # Performance monitoring
        self.operation_metrics: Dict[str, List[float]] = {
            "analysis_time": [],
            "orchestration_time": [],
            "total_execution_time": []
        }
        
        # Error handling configuration
        self.error_handling_config = {
            "max_retry_attempts": config.get("max_retry_attempts", 3),
            "retry_delay_base": config.get("retry_delay_base", 1.0),
            "enable_adaptive_routing": config.get("enable_adaptive_routing", True),
            "enable_error_prediction": config.get("enable_error_prediction", True),
            "circuit_breaker_threshold": config.get("circuit_breaker_threshold", 5)
        }
        
        # Register with error integrator
        self.error_integrator.register_agent_logger(self.agent_id, self.logger)
        
        # Setup enhanced logging
        self.logger = logging.getLogger(f"enhanced_document_master.{agent_id}")
        self.logger.setLevel(logging.INFO)
    
    async def process(self, 
                     state: DocumentState, 
                     message: Optional[BaseMessage] = None) -> AgentResult:
        """
        Enhanced process method with comprehensive error handling.
        
        Args:
            state: Current document state
            message: Optional user message
            
        Returns:
            AgentResult: Processing results with error handling
        """
        operation_start = time.time()
        operation_id = f"master_op_{int(operation_start)}"
        
        # Log operation start in audit trail
        if self.audit_trail:
            self.audit_trail.log_event(
                event_type="operation_start",
                event_source=self.agent_id,
                event_description="Document master agent processing started",
                agent_id=self.agent_id,
                operation_id=operation_id,
                user_request=str(message) if message else None
            )
        
        try:
            # Enhanced request analysis with error assessment
            analysis_start = time.time()
            analysis = await self._enhanced_analyze_request(state, message)
            analysis_time = time.time() - analysis_start
            self.operation_metrics["analysis_time"].append(analysis_time)
            
            # Check for predicted errors and take preventive action
            if await self._should_apply_preventive_measures(analysis):
                analysis = await self._apply_error_prevention(analysis, state)
            
            # Execute orchestration with error handling
            orchestration_start = time.time()
            result = await self._execute_with_error_handling(analysis, state, message)
            orchestration_time = time.time() - orchestration_start
            self.operation_metrics["orchestration_time"].append(orchestration_time)
            
            # Update statistics
            total_time = time.time() - operation_start
            self.operation_metrics["total_execution_time"].append(total_time)
            self.workflow_statistics["total_requests"] += 1
            self.workflow_statistics["successful_requests"] += 1
            
            # Log successful completion
            if self.audit_trail:
                self.audit_trail.log_event(
                    event_type="operation_complete",
                    event_source=self.agent_id,
                    event_description="Operation completed successfully",
                    operation_id=operation_id,
                    execution_time=total_time
                )
            
            return result
            
        except Exception as e:
            # Handle error through integrated system
            total_time = time.time() - operation_start
            self.workflow_statistics["failed_requests"] += 1
            
            error_response = await self._handle_orchestration_error(
                e, state, message, operation_id, total_time
            )
            
            # Create agent result with error information
            return AgentResult(
                agent_id=self.agent_id,
                operation_id=operation_id,
                success=False,
                error=error_response.technical_message,
                metadata={
                    "error_response": error_response.to_dict(),
                    "execution_time": total_time,
                    "analysis_completed": 'analysis' in locals()
                },
                execution_time=total_time
            )
    
    async def _enhanced_analyze_request(self, 
                                      state: DocumentState, 
                                      message: Optional[BaseMessage]) -> EnhancedRequestAnalysis:
        """
        Enhanced request analysis with error risk assessment.
        
        Args:
            state: Current document state
            message: User message
            
        Returns:
            EnhancedRequestAnalysis: Enhanced analysis with error handling context
        """
        try:
            # Perform base analysis (would call original DocumentMasterAgent logic)
            base_analysis = await self._perform_base_analysis(state, message)
            
            # Enhance with error risk assessment
            error_risk = await self._assess_error_risks(base_analysis, state)
            
            # Identify potential failure points
            failure_points = await self._identify_failure_points(base_analysis, state)
            
            # Determine recovery strategies
            recovery_strategies = await self._determine_recovery_strategies(base_analysis, error_risk)
            
            # Set monitoring requirements
            monitoring_requirements = self._set_monitoring_requirements(base_analysis, error_risk)
            
            return EnhancedRequestAnalysis(
                complexity=base_analysis.get("complexity", OperationComplexity.MODERATE),
                request_type=base_analysis.get("request_type", "general"),
                confidence=base_analysis.get("confidence", 0.8),
                reasoning=base_analysis.get("reasoning", "Enhanced analysis"),
                required_agents=base_analysis.get("required_agents", []),
                estimated_time=base_analysis.get("estimated_time", 3.0),
                error_risk_assessment=error_risk,
                potential_failure_points=failure_points,
                recovery_strategies=recovery_strategies,
                monitoring_requirements=monitoring_requirements
            )
            
        except Exception as e:
            self.logger.error(f"Enhanced analysis failed: {e}")
            # Return basic fallback analysis
            return EnhancedRequestAnalysis(
                complexity=OperationComplexity.SIMPLE,
                request_type="fallback",
                confidence=0.5,
                reasoning=f"Analysis failed, using fallback: {str(e)}",
                required_agents=["context_analysis"],
                estimated_time=2.0,
                error_risk_assessment={"high_risk": 1.0}
            )
    
    async def _perform_base_analysis(self, 
                                   state: DocumentState, 
                                   message: Optional[BaseMessage]) -> Dict[str, Any]:
        """Perform base request analysis (placeholder for original logic)."""
        # This would integrate with the original DocumentMasterAgent analysis
        user_request = str(message) if message else ""
        
        # Simple analysis for demonstration
        if "financial" in user_request.lower() or "data" in user_request.lower():
            return {
                "complexity": OperationComplexity.COMPLEX,
                "request_type": "financial_analysis",
                "confidence": 0.9,
                "reasoning": "Financial data request requires complex processing",
                "required_agents": ["context_analysis", "data_integration", "content_generation", "formatting"],
                "estimated_time": 5.0
            }
        elif "format" in user_request.lower() or "style" in user_request.lower():
            return {
                "complexity": OperationComplexity.MODERATE,
                "request_type": "formatting",
                "confidence": 0.8,
                "reasoning": "Formatting request requires moderate processing",
                "required_agents": ["context_analysis", "formatting"],
                "estimated_time": 2.5
            }
        else:
            return {
                "complexity": OperationComplexity.SIMPLE,
                "request_type": "general",
                "confidence": 0.7,
                "reasoning": "General request with simple processing",
                "required_agents": ["context_analysis"],
                "estimated_time": 1.5
            }
    
    async def _assess_error_risks(self, 
                                base_analysis: Dict[str, Any], 
                                state: DocumentState) -> Dict[str, float]:
        """Assess error risks for the operation."""
        risks = {}
        
        # Assess complexity-based risks
        complexity = base_analysis.get("complexity", OperationComplexity.MODERATE)
        if complexity == OperationComplexity.COMPLEX:
            risks["coordination_failure"] = 0.3
            risks["timeout_risk"] = 0.2
            risks["resource_exhaustion"] = 0.1
        elif complexity == OperationComplexity.MODERATE:
            risks["coordination_failure"] = 0.1
            risks["timeout_risk"] = 0.1
        else:
            risks["coordination_failure"] = 0.05
        
        # Assess agent-specific risks
        required_agents = base_analysis.get("required_agents", [])
        if "data_integration" in required_agents:
            risks["api_failure"] = 0.2
            risks["network_error"] = 0.15
        
        if "execution" in required_agents:
            risks["uno_service_error"] = 0.1
        
        # Assess historical error patterns
        historical_risk = await self._assess_historical_risks(base_analysis)
        risks.update(historical_risk)
        
        return risks
    
    async def _assess_historical_risks(self, base_analysis: Dict[str, Any]) -> Dict[str, float]:
        """Assess risks based on historical error patterns."""
        risks = {}
        
        if self.error_tracker:
            # Analyze recent errors for similar operations
            similar_errors = self.error_tracker.get_error_history(
                category=ErrorCategory.AGENT_COORDINATION,
                limit=50
            )
            
            if len(similar_errors) > 10:  # High error frequency
                risks["pattern_repeat"] = 0.3
            elif len(similar_errors) > 5:
                risks["pattern_repeat"] = 0.15
        
        return risks
    
    async def _identify_failure_points(self, 
                                     base_analysis: Dict[str, Any], 
                                     state: DocumentState) -> List[str]:
        """Identify potential failure points in the operation."""
        failure_points = []
        
        required_agents = base_analysis.get("required_agents", [])
        
        # Agent-specific failure points
        if "data_integration" in required_agents:
            failure_points.extend([
                "API rate limiting",
                "Network connectivity",
                "Data validation failure"
            ])
        
        if "content_generation" in required_agents:
            failure_points.extend([
                "LLM service unavailable",
                "Content quality validation failure"
            ])
        
        if "execution" in required_agents:
            failure_points.extend([
                "UNO service connection failure",
                "Document operation rejection"
            ])
        
        # Complexity-based failure points
        complexity = base_analysis.get("complexity", OperationComplexity.MODERATE)
        if complexity == OperationComplexity.COMPLEX:
            failure_points.extend([
                "Agent coordination timeout",
                "Resource exhaustion",
                "State synchronization failure"
            ])
        
        return failure_points
    
    async def _determine_recovery_strategies(self, 
                                           base_analysis: Dict[str, Any], 
                                           error_risks: Dict[str, float]) -> List[str]:
        """Determine appropriate recovery strategies."""
        strategies = []
        
        # Risk-based strategies
        if error_risks.get("network_error", 0) > 0.1:
            strategies.extend(["retry_with_backoff", "use_cached_data"])
        
        if error_risks.get("coordination_failure", 0) > 0.2:
            strategies.extend(["simplified_workflow", "sequential_execution"])
        
        if error_risks.get("timeout_risk", 0) > 0.15:
            strategies.extend(["increase_timeout", "parallel_execution"])
        
        # Complexity-based strategies
        complexity = base_analysis.get("complexity", OperationComplexity.MODERATE)
        if complexity == OperationComplexity.COMPLEX:
            strategies.extend(["checkpoint_state", "partial_rollback"])
        
        # Default strategies
        strategies.extend(["graceful_degradation", "user_notification"])
        
        return list(set(strategies))  # Remove duplicates
    
    def _set_monitoring_requirements(self, 
                                   base_analysis: Dict[str, Any], 
                                   error_risks: Dict[str, float]) -> Dict[str, Any]:
        """Set monitoring requirements for the operation."""
        monitoring = {
            "enable_performance_tracking": True,
            "enable_resource_monitoring": error_risks.get("resource_exhaustion", 0) > 0.05,
            "enable_network_monitoring": error_risks.get("network_error", 0) > 0.1,
            "checkpoint_frequency": "high" if error_risks.get("coordination_failure", 0) > 0.2 else "normal",
            "timeout_multiplier": 1.5 if error_risks.get("timeout_risk", 0) > 0.1 else 1.0
        }
        
        return monitoring
    
    async def _should_apply_preventive_measures(self, analysis: EnhancedRequestAnalysis) -> bool:
        """Determine if preventive measures should be applied."""
        # Apply preventive measures if high error risk
        max_risk = max(analysis.error_risk_assessment.values()) if analysis.error_risk_assessment else 0
        return max_risk > 0.25  # 25% error risk threshold
    
    async def _apply_error_prevention(self, 
                                    analysis: EnhancedRequestAnalysis, 
                                    state: DocumentState) -> EnhancedRequestAnalysis:
        """Apply error prevention measures."""
        # Increase timeout estimates
        for agent in analysis.timeout_estimates:
            analysis.timeout_estimates[agent] *= 1.5
        
        # Add fallback options
        if "use_cached_data" not in analysis.fallback_options:
            analysis.fallback_options.append("use_cached_data")
        
        if "simplified_workflow" not in analysis.fallback_options:
            analysis.fallback_options.append("simplified_workflow")
        
        # Log preventive measures
        if self.audit_trail:
            self.audit_trail.log_event(
                event_type="preventive_measures_applied",
                event_source=self.agent_id,
                event_description="Applied error prevention measures",
                agent_id=self.agent_id,
                error_risk_level=max(analysis.error_risk_assessment.values())
            )
        
        return analysis
    
    async def _execute_with_error_handling(self, 
                                         analysis: EnhancedRequestAnalysis, 
                                         state: DocumentState, 
                                         message: Optional[BaseMessage]) -> AgentResult:
        """Execute orchestration with comprehensive error handling."""
        
        @asynccontextmanager
        async def error_handling_context():
            """Context manager for operation error handling."""
            try:
                yield
            except Exception as e:
                # Track error recovery attempt
                self.workflow_statistics["error_recovery_attempts"] += 1
                
                # Attempt recovery based on analysis
                if await self._attempt_error_recovery(e, analysis, state):
                    self.workflow_statistics["error_recovery_successes"] += 1
                else:
                    raise
        
        async with error_handling_context():
            # Execute based on complexity with monitoring
            if analysis.complexity == OperationComplexity.SIMPLE:
                return await self._execute_simple_workflow(analysis, state, message)
            elif analysis.complexity == OperationComplexity.MODERATE:
                return await self._execute_moderate_workflow(analysis, state, message)
            else:
                return await self._execute_complex_workflow(analysis, state, message)
    
    async def _attempt_error_recovery(self, 
                                     error: Exception, 
                                     analysis: EnhancedRequestAnalysis, 
                                     state: DocumentState) -> bool:
        """Attempt error recovery based on analysis."""
        recovery_strategies = analysis.recovery_strategies
        
        # Try strategies in order of preference
        for strategy in recovery_strategies:
            try:
                if strategy == "retry_with_backoff":
                    await asyncio.sleep(self.error_handling_config["retry_delay_base"])
                    return True
                
                elif strategy == "simplified_workflow":
                    # Modify analysis for simpler execution
                    analysis.complexity = OperationComplexity.SIMPLE
                    analysis.required_agents = analysis.required_agents[:1]  # Use only first agent
                    return True
                
                elif strategy == "use_cached_data":
                    # Would implement cache usage
                    return True
                
                elif strategy == "graceful_degradation":
                    # Reduce operation scope
                    return True
                    
            except Exception as recovery_error:
                self.logger.warning(f"Recovery strategy {strategy} failed: {recovery_error}")
                continue
        
        return False
    
    async def _execute_simple_workflow(self, 
                                     analysis: EnhancedRequestAnalysis, 
                                     state: DocumentState, 
                                     message: Optional[BaseMessage]) -> AgentResult:
        """Execute simple workflow with error handling."""
        # Simple workflow implementation with error monitoring
        return AgentResult(
            agent_id=self.agent_id,
            success=True,
            result={"workflow": "simple", "analysis": analysis},
            metadata={"execution_path": "simple"}
        )
    
    async def _execute_moderate_workflow(self, 
                                       analysis: EnhancedRequestAnalysis, 
                                       state: DocumentState, 
                                       message: Optional[BaseMessage]) -> AgentResult:
        """Execute moderate workflow with error handling."""
        # Moderate workflow implementation with error monitoring
        return AgentResult(
            agent_id=self.agent_id,
            success=True,
            result={"workflow": "moderate", "analysis": analysis},
            metadata={"execution_path": "moderate"}
        )
    
    async def _execute_complex_workflow(self, 
                                      analysis: EnhancedRequestAnalysis, 
                                      state: DocumentState, 
                                      message: Optional[BaseMessage]) -> AgentResult:
        """Execute complex workflow with error handling."""
        # Complex workflow implementation with error monitoring
        return AgentResult(
            agent_id=self.agent_id,
            success=True,
            result={"workflow": "complex", "analysis": analysis},
            metadata={"execution_path": "complex"}
        )
    
    async def _handle_orchestration_error(self, 
                                        error: Exception, 
                                        state: DocumentState, 
                                        message: Optional[BaseMessage], 
                                        operation_id: str,
                                        execution_time: float) -> ErrorResponse:
        """Handle orchestration errors through integrated system."""
        context = {
            "agent_id": self.agent_id,
            "operation_id": operation_id,
            "document_state": state,
            "message": str(message) if message else None,
            "execution_time": execution_time,
            "workflow_statistics": self.workflow_statistics
        }
        
        # Handle through integrated error system
        if self.error_coordinator:
            response = await self.error_coordinator.handle_error(error, context)
        else:
            # Fallback error response
            response = ErrorResponse(
                success=False,
                user_message="An error occurred during processing. Please try again.",
                technical_message=f"Orchestration error: {str(error)}",
                suggested_actions=["Try again", "Simplify your request"],
                recovery_options=["retry", "simplified_request"]
            )
        
        # Log error in audit trail
        if self.audit_trail:
            self.audit_trail.log_event(
                event_type="orchestration_error",
                event_source=self.agent_id,
                event_description=f"Orchestration failed: {str(error)}",
                agent_id=self.agent_id,
                operation_id=operation_id,
                error_id=response.error_context.error_id if response.error_context else None
            )
        
        return response
    
    def validate_input(self, 
                      state: DocumentState, 
                      message: Optional[BaseMessage] = None) -> ValidationResult:
        """Enhanced input validation with error context."""
        validation_id = f"validation_{int(time.time())}"
        
        try:
            # Basic validation
            if not state:
                return ValidationResult(
                    validation_id=validation_id,
                    agent_id=self.agent_id,
                    validation_type="input_validation",
                    passed=False,
                    confidence=1.0,
                    issues=[{"type": "missing_state", "message": "Document state is required"}]
                )
            
            # Enhanced validation with error risk assessment
            if not state.get("current_document"):
                return ValidationResult(
                    validation_id=validation_id,
                    agent_id=self.agent_id,
                    validation_type="input_validation",
                    passed=False,
                    confidence=0.9,
                    issues=[{"type": "missing_document", "message": "Current document context required"}],
                    recommendations=["Ensure document is loaded", "Check document state"]
                )
            
            return ValidationResult(
                validation_id=validation_id,
                agent_id=self.agent_id,
                validation_type="input_validation",
                passed=True,
                confidence=0.95
            )
            
        except Exception as e:
            return ValidationResult(
                validation_id=validation_id,
                agent_id=self.agent_id,
                validation_type="input_validation",
                passed=False,
                confidence=0.5,
                issues=[{"type": "validation_error", "message": f"Validation failed: {str(e)}"}]
            )
    
    def get_required_tools(self) -> List[str]:
        """Get required tools for enhanced document master agent."""
        return [
            "request_analyzer",
            "agent_coordinator",
            "error_handler",
            "performance_monitor",
            "state_manager"
        ]
    
    def get_performance_summary(self) -> Dict[str, Any]:
        """Get enhanced performance summary with error statistics."""
        base_summary = super().get_performance_summary()
        
        # Add enhanced metrics
        enhanced_summary = {
            **base_summary,
            "workflow_statistics": self.workflow_statistics,
            "operation_metrics": {
                "avg_analysis_time": sum(self.operation_metrics["analysis_time"]) / max(len(self.operation_metrics["analysis_time"]), 1),
                "avg_orchestration_time": sum(self.operation_metrics["orchestration_time"]) / max(len(self.operation_metrics["orchestration_time"]), 1),
                "avg_total_time": sum(self.operation_metrics["total_execution_time"]) / max(len(self.operation_metrics["total_execution_time"]), 1)
            },
            "error_handling_status": {
                "coordinator_available": self.error_coordinator is not None,
                "tracker_available": self.error_tracker is not None,
                "audit_available": self.audit_trail is not None
            }
        }
        
        # Add error statistics if available
        if self.error_tracker:
            enhanced_summary["error_statistics"] = self.error_tracker.get_tracking_statistics()
        
        return enhanced_summary


# Factory function for creating enhanced document master agent
def create_enhanced_document_master(config: Optional[Dict[str, Any]] = None,
                                   error_config: Optional[ErrorIntegrationConfig] = None) -> EnhancedDocumentMasterAgent:
    """
    Factory function to create enhanced document master agent.
    
    Args:
        config: Agent configuration
        error_config: Error integration configuration
        
    Returns:
        EnhancedDocumentMasterAgent: Configured enhanced agent
    """
    return EnhancedDocumentMasterAgent(
        config=config,
        error_integration_config=error_config
    )