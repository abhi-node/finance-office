"""
Task 18.5: Dynamic Performance Monitoring and Optimization

Creates monitoring system that tracks routing performance and implements dynamic
optimization based on request patterns and system performance metrics. Features
real-time performance tracking, request pattern analysis, adaptive routing 
optimization, caching strategies for frequent operations, learning algorithms
for routing decisions, and performance feedback loops.

Features:
- Real-time performance tracking and metrics collection
- Request pattern analysis and trend detection
- Adaptive routing optimization and machine learning
- Intelligent caching strategies for frequent operations
- Learning algorithms for dynamic routing decisions
- Performance feedback loops and continuous improvement
- Dashboard for monitoring routing effectiveness
- Resource usage optimization and capacity planning
"""

import asyncio
import logging
from typing import Dict, List, Any, Optional, Set, Tuple, Callable
from dataclasses import dataclass, field
from datetime import datetime, timedelta
import time
import json
import statistics
from enum import Enum
from collections import defaultdict, deque
import threading

try:
    from state.document_state import DocumentState
    STATE_AVAILABLE = True
except ImportError:
    DocumentState = Dict[str, Any]
    STATE_AVAILABLE = False

from .complexity_analyzer import OperationType, OperationComplexity

class MetricType(Enum):
    """Types of performance metrics."""
    EXECUTION_TIME = "execution_time"
    SUCCESS_RATE = "success_rate"
    THROUGHPUT = "throughput"
    RESOURCE_USAGE = "resource_usage"
    QUALITY_SCORE = "quality_score"
    USER_SATISFACTION = "user_satisfaction"

class OptimizationStrategy(Enum):
    """Optimization strategies."""
    AGGRESSIVE = "aggressive"     # Fast optimizations, higher risk
    CONSERVATIVE = "conservative" # Slow optimizations, safer
    BALANCED = "balanced"        # Balanced approach
    ADAPTIVE = "adaptive"        # Self-adjusting strategy

class AlertLevel(Enum):
    """Alert severity levels."""
    INFO = "info"
    WARNING = "warning"
    ERROR = "error"
    CRITICAL = "critical"

@dataclass
class PerformanceMetric:
    """Single performance metric data point."""
    metric_type: MetricType
    value: float
    timestamp: datetime
    operation_type: Optional[OperationType] = None
    router_type: Optional[str] = None
    metadata: Dict[str, Any] = field(default_factory=dict)

@dataclass
class RequestPattern:
    """Detected request pattern."""
    pattern_id: str
    pattern_type: str
    frequency: int
    success_rate: float
    avg_execution_time: float
    last_seen: datetime
    optimization_potential: float
    suggested_router: Optional[str] = None

@dataclass
class OptimizationRecommendation:
    """Performance optimization recommendation."""
    recommendation_id: str
    priority: str  # high, medium, low
    category: str  # routing, caching, resource, workflow
    description: str
    expected_improvement: float
    implementation_effort: str  # low, medium, high
    risk_level: str  # low, medium, high
    action_items: List[str]
    metadata: Dict[str, Any] = field(default_factory=dict)

@dataclass
class SystemAlert:
    """System performance alert."""
    alert_id: str
    level: AlertLevel
    title: str
    description: str
    timestamp: datetime
    affected_components: List[str]
    recommended_actions: List[str]
    auto_resolved: bool = False

class PerformanceMonitor:
    """
    Task 18.5 Implementation: Dynamic Performance Monitoring and Optimization
    
    Provides comprehensive performance monitoring, pattern analysis, and
    adaptive optimization for the routing system.
    """
    
    def __init__(self, config: Optional[Dict[str, Any]] = None):
        """Initialize the performance monitor."""
        self.config = config or {}
        self.logger = logging.getLogger("performance_monitor")
        
        # Configuration
        self.monitoring_enabled = self.config.get("monitoring_enabled", True)
        self.optimization_strategy = OptimizationStrategy(
            self.config.get("optimization_strategy", "balanced")
        )
        self.alert_thresholds = self.config.get("alert_thresholds", {
            "execution_time": 5.0,
            "success_rate": 0.8,
            "memory_usage": 150,  # MB
            "cpu_usage": 80       # %
        })
        
        # Data storage
        self.metrics_history: deque = deque(maxlen=10000)
        self.request_patterns: Dict[str, RequestPattern] = {}
        self.active_alerts: Dict[str, SystemAlert] = {}
        self.optimization_history: List[OptimizationRecommendation] = []
        
        # Real-time tracking
        self.current_metrics: Dict[str, Any] = {}
        self.performance_trends: Dict[str, List[float]] = defaultdict(list)
        self.router_performance: Dict[str, Dict[str, Any]] = {}
        
        # Learning and optimization
        self.pattern_detection_window = timedelta(hours=1)
        self.optimization_cooldown = timedelta(minutes=15)
        self.last_optimization = datetime.min
        
        # Threading for background processing
        self.monitoring_active = True
        self.background_thread = None
        
        # Callback system for optimization actions
        self.optimization_callbacks: Dict[str, Callable] = {}
        
        self.logger.info("PerformanceMonitor initialized successfully")
        
        if self.monitoring_enabled:
            self._start_background_monitoring()
    
    def _start_background_monitoring(self):
        """Start background monitoring thread."""
        self.background_thread = threading.Thread(
            target=self._background_monitoring_loop,
            daemon=True
        )
        self.background_thread.start()
    
    def _background_monitoring_loop(self):
        """Background monitoring loop."""
        while self.monitoring_active:
            try:
                self._analyze_performance_trends()
                self._detect_request_patterns()
                self._generate_optimization_recommendations()
                self._check_alert_conditions()
                self._cleanup_old_data()
                
                time.sleep(30)  # Run every 30 seconds
                
            except Exception as e:
                self.logger.error(f"Background monitoring error: {e}")
                time.sleep(60)  # Wait longer on error
    
    def record_metric(self, 
                     metric_type: MetricType,
                     value: float,
                     operation_type: Optional[OperationType] = None,
                     router_type: Optional[str] = None,
                     metadata: Optional[Dict[str, Any]] = None):
        """Record a performance metric."""
        
        if not self.monitoring_enabled:
            return
        
        metric = PerformanceMetric(
            metric_type=metric_type,
            value=value,
            timestamp=datetime.now(),
            operation_type=operation_type,
            router_type=router_type,
            metadata=metadata or {}
        )
        
        self.metrics_history.append(metric)
        
        # Update current metrics
        metric_key = f"{metric_type.value}_{router_type or 'all'}"
        self.current_metrics[metric_key] = value
        
        # Update trends
        trend_key = f"{metric_type.value}_{operation_type.value if operation_type else 'all'}"
        self.performance_trends[trend_key].append(value)
        
        # Keep trend history manageable
        if len(self.performance_trends[trend_key]) > 100:
            self.performance_trends[trend_key] = self.performance_trends[trend_key][-100:]
        
        # Update router performance
        if router_type:
            if router_type not in self.router_performance:
                self.router_performance[router_type] = {
                    "metrics": {},
                    "last_updated": datetime.now()
                }
            
            self.router_performance[router_type]["metrics"][metric_type.value] = value
            self.router_performance[router_type]["last_updated"] = datetime.now()
    
    def record_execution_result(self, 
                               operation_type: OperationType,
                               router_type: str,
                               execution_time: float,
                               success: bool,
                               quality_score: Optional[float] = None,
                               resource_usage: Optional[Dict[str, Any]] = None,
                               user_request: Optional[str] = None):
        """Record complete execution result with all metrics."""
        
        # Record individual metrics
        self.record_metric(MetricType.EXECUTION_TIME, execution_time, operation_type, router_type)
        self.record_metric(MetricType.SUCCESS_RATE, 1.0 if success else 0.0, operation_type, router_type)
        
        if quality_score is not None:
            self.record_metric(MetricType.QUALITY_SCORE, quality_score, operation_type, router_type)
        
        if resource_usage:
            memory_mb = resource_usage.get("memory_mb", 0)
            cpu_percent = resource_usage.get("cpu_percent", 0)
            
            self.record_metric(MetricType.RESOURCE_USAGE, memory_mb, operation_type, router_type, 
                             {"resource_type": "memory"})
            self.record_metric(MetricType.RESOURCE_USAGE, cpu_percent, operation_type, router_type,
                             {"resource_type": "cpu"})
        
        # Update request patterns
        if user_request:
            self._update_request_patterns(user_request, operation_type, router_type, 
                                        execution_time, success)
    
    def _update_request_patterns(self, 
                                user_request: str,
                                operation_type: OperationType,
                                router_type: str,
                                execution_time: float,
                                success: bool):
        """Update request pattern analysis."""
        
        # Create pattern key based on request characteristics
        pattern_key = self._generate_pattern_key(user_request, operation_type)
        
        if pattern_key in self.request_patterns:
            pattern = self.request_patterns[pattern_key]
            pattern.frequency += 1
            pattern.last_seen = datetime.now()
            
            # Update running averages
            old_success_rate = pattern.success_rate
            old_avg_time = pattern.avg_execution_time
            freq = pattern.frequency
            
            pattern.success_rate = ((old_success_rate * (freq - 1)) + (1.0 if success else 0.0)) / freq
            pattern.avg_execution_time = ((old_avg_time * (freq - 1)) + execution_time) / freq
            
            # Update optimization potential
            pattern.optimization_potential = self._calculate_optimization_potential(pattern)
            
        else:
            # Create new pattern
            pattern = RequestPattern(
                pattern_id=pattern_key,
                pattern_type=self._classify_pattern_type(user_request, operation_type),
                frequency=1,
                success_rate=1.0 if success else 0.0,
                avg_execution_time=execution_time,
                last_seen=datetime.now(),
                optimization_potential=0.0,
                suggested_router=router_type
            )
            
            self.request_patterns[pattern_key] = pattern
    
    def _generate_pattern_key(self, user_request: str, operation_type: OperationType) -> str:
        """Generate pattern key from request characteristics."""
        
        # Normalize request for pattern matching
        normalized = user_request.lower().strip()
        
        # Extract key terms
        key_terms = []
        
        # Common operation keywords
        operation_keywords = {
            "format": ["format", "style", "styling"],
            "create": ["create", "generate", "make"],
            "analyze": ["analyze", "analysis", "review"],
            "data": ["data", "chart", "table", "graph"],
            "financial": ["financial", "stock", "market", "investment"]
        }
        
        for category, keywords in operation_keywords.items():
            if any(keyword in normalized for keyword in keywords):
                key_terms.append(category)
        
        # Create pattern key
        terms_str = "_".join(sorted(key_terms)) if key_terms else "general"
        return f"{operation_type.value}_{terms_str}"
    
    def _classify_pattern_type(self, user_request: str, operation_type: OperationType) -> str:
        """Classify the type of request pattern."""
        
        request_lower = user_request.lower()
        
        if "financial" in request_lower or "stock" in request_lower:
            return "financial_operation"
        elif "format" in request_lower or "style" in request_lower:
            return "formatting_operation"
        elif "create" in request_lower or "generate" in request_lower:
            return "content_creation"
        elif "analyze" in request_lower or "review" in request_lower:
            return "analysis_operation"
        else:
            return "general_operation"
    
    def _calculate_optimization_potential(self, pattern: RequestPattern) -> float:
        """Calculate optimization potential for a request pattern."""
        
        # Factors that increase optimization potential
        potential = 0.0
        
        # High frequency patterns have more potential
        if pattern.frequency > 10:
            potential += 0.3
        elif pattern.frequency > 5:
            potential += 0.2
        elif pattern.frequency > 2:
            potential += 0.1
        
        # Poor performance indicates optimization opportunity
        if pattern.avg_execution_time > 4.0:
            potential += 0.3
        elif pattern.avg_execution_time > 2.0:
            potential += 0.2
        
        # Low success rate indicates need for optimization
        if pattern.success_rate < 0.8:
            potential += 0.3
        elif pattern.success_rate < 0.9:
            potential += 0.2
        
        # Recent patterns have higher priority
        time_since_seen = datetime.now() - pattern.last_seen
        if time_since_seen < timedelta(hours=1):
            potential += 0.1
        
        return min(1.0, potential)
    
    def _analyze_performance_trends(self):
        """Analyze performance trends for optimization opportunities."""
        
        for trend_key, values in self.performance_trends.items():
            if len(values) < 10:  # Need sufficient data
                continue
            
            # Calculate trend statistics
            recent_values = values[-10:]
            older_values = values[-20:-10] if len(values) >= 20 else values[:-10]
            
            if older_values:
                recent_avg = statistics.mean(recent_values)
                older_avg = statistics.mean(older_values)
                
                # Detect performance degradation
                if "execution_time" in trend_key and recent_avg > older_avg * 1.2:
                    self._create_alert(
                        AlertLevel.WARNING,
                        f"Performance degradation detected in {trend_key}",
                        f"Execution time increased from {older_avg:.2f}s to {recent_avg:.2f}s",
                        [trend_key.split('_')[1]]  # Operation type
                    )
                
                # Detect success rate drops
                elif "success_rate" in trend_key and recent_avg < older_avg * 0.9:
                    self._create_alert(
                        AlertLevel.ERROR,
                        f"Success rate decline in {trend_key}",
                        f"Success rate dropped from {older_avg:.2%} to {recent_avg:.2%}",
                        [trend_key.split('_')[1]]
                    )
    
    def _detect_request_patterns(self):
        """Detect and analyze request patterns."""
        
        current_time = datetime.now()
        patterns_to_optimize = []
        
        for pattern_id, pattern in self.request_patterns.items():
            # Skip old patterns
            if current_time - pattern.last_seen > self.pattern_detection_window:
                continue
            
            # Identify high-potential patterns
            if pattern.optimization_potential > 0.5:
                patterns_to_optimize.append(pattern)
        
        # Sort by potential and frequency
        patterns_to_optimize.sort(
            key=lambda p: (p.optimization_potential, p.frequency),
            reverse=True
        )
        
        # Process top patterns for optimization
        for pattern in patterns_to_optimize[:5]:  # Top 5 patterns
            self._generate_pattern_optimization(pattern)
    
    def _generate_pattern_optimization(self, pattern: RequestPattern):
        """Generate optimization recommendation for a pattern."""
        
        recommendations = []
        
        # High execution time optimization
        if pattern.avg_execution_time > 3.0:
            if pattern.pattern_type == "financial_operation":
                recommendations.append({
                    "type": "caching",
                    "description": f"Implement financial data caching for pattern {pattern.pattern_id}",
                    "expected_improvement": 0.4,
                    "effort": "medium"
                })
            
            recommendations.append({
                "type": "routing",
                "description": f"Optimize routing for pattern {pattern.pattern_id}",
                "expected_improvement": 0.3,
                "effort": "low"
            })
        
        # Low success rate optimization
        if pattern.success_rate < 0.9:
            recommendations.append({
                "type": "workflow",
                "description": f"Add validation steps for pattern {pattern.pattern_id}",
                "expected_improvement": 0.2,
                "effort": "medium"
            })
        
        # High frequency optimization
        if pattern.frequency > 10:
            recommendations.append({
                "type": "caching",
                "description": f"Create specialized cache for frequent pattern {pattern.pattern_id}",
                "expected_improvement": 0.5,
                "effort": "high"
            })
        
        # Store recommendations
        for rec in recommendations:
            optimization = OptimizationRecommendation(
                recommendation_id=f"{pattern.pattern_id}_{rec['type']}_{int(time.time())}",
                priority="high" if pattern.optimization_potential > 0.7 else "medium",
                category=rec["type"],
                description=rec["description"],
                expected_improvement=rec["expected_improvement"],
                implementation_effort=rec["effort"],
                risk_level="low",
                action_items=self._generate_action_items(rec),
                metadata={"pattern_id": pattern.pattern_id, "frequency": pattern.frequency}
            )
            
            self.optimization_history.append(optimization)
    
    def _generate_action_items(self, recommendation: Dict[str, Any]) -> List[str]:
        """Generate action items for optimization recommendation."""
        
        rec_type = recommendation["type"]
        
        if rec_type == "caching":
            return [
                "Identify cacheable data points",
                "Implement cache storage layer",
                "Add cache invalidation logic",
                "Monitor cache hit rates"
            ]
        elif rec_type == "routing":
            return [
                "Analyze current routing decisions",
                "Implement optimized routing logic",
                "Test routing performance",
                "Deploy routing optimization"
            ]
        elif rec_type == "workflow":
            return [
                "Identify workflow bottlenecks",
                "Design improved workflow",
                "Implement workflow changes",
                "Validate workflow improvements"
            ]
        else:
            return ["Analyze requirement", "Design solution", "Implement changes", "Test results"]
    
    def _generate_optimization_recommendations(self):
        """Generate system-wide optimization recommendations."""
        
        current_time = datetime.now()
        
        # Check if enough time has passed since last optimization
        if current_time - self.last_optimization < self.optimization_cooldown:
            return
        
        # Analyze router performance
        router_recommendations = self._analyze_router_performance()
        
        # Analyze resource usage
        resource_recommendations = self._analyze_resource_usage()
        
        # Combine recommendations
        all_recommendations = router_recommendations + resource_recommendations
        
        # Filter and prioritize
        prioritized = self._prioritize_recommendations(all_recommendations)
        
        # Apply automatic optimizations if strategy allows
        if self.optimization_strategy in [OptimizationStrategy.AGGRESSIVE, OptimizationStrategy.ADAPTIVE]:
            self._apply_automatic_optimizations(prioritized)
        
        self.last_optimization = current_time
    
    def _analyze_router_performance(self) -> List[OptimizationRecommendation]:
        """Analyze router performance for optimization opportunities."""
        
        recommendations = []
        
        for router_type, perf_data in self.router_performance.items():
            metrics = perf_data["metrics"]
            
            # Check execution time
            exec_time = metrics.get("execution_time", 0)
            if exec_time > 4.0:
                recommendations.append(OptimizationRecommendation(
                    recommendation_id=f"router_speed_{router_type}_{int(time.time())}",
                    priority="high",
                    category="routing",
                    description=f"Optimize {router_type} for better execution time",
                    expected_improvement=0.3,
                    implementation_effort="medium",
                    risk_level="low",
                    action_items=[
                        f"Profile {router_type} execution",
                        "Identify bottlenecks",
                        "Implement optimizations",
                        "Test performance improvements"
                    ],
                    metadata={"router_type": router_type, "current_time": exec_time}
                ))
            
            # Check success rate
            success_rate = metrics.get("success_rate", 1.0)
            if success_rate < 0.9:
                recommendations.append(OptimizationRecommendation(
                    recommendation_id=f"router_reliability_{router_type}_{int(time.time())}",
                    priority="critical",
                    category="workflow",
                    description=f"Improve {router_type} reliability",
                    expected_improvement=0.2,
                    implementation_effort="high",
                    risk_level="medium",
                    action_items=[
                        f"Analyze {router_type} failures",
                        "Implement error handling",
                        "Add validation steps",
                        "Test reliability improvements"
                    ],
                    metadata={"router_type": router_type, "current_success_rate": success_rate}
                ))
        
        return recommendations
    
    def _analyze_resource_usage(self) -> List[OptimizationRecommendation]:
        """Analyze resource usage for optimization opportunities."""
        
        recommendations = []
        
        # Get recent resource metrics
        recent_memory = [m.value for m in self.metrics_history 
                        if m.metric_type == MetricType.RESOURCE_USAGE 
                        and m.metadata.get("resource_type") == "memory"
                        and datetime.now() - m.timestamp < timedelta(minutes=30)]
        
        recent_cpu = [m.value for m in self.metrics_history 
                     if m.metric_type == MetricType.RESOURCE_USAGE 
                     and m.metadata.get("resource_type") == "cpu"
                     and datetime.now() - m.timestamp < timedelta(minutes=30)]
        
        # Check memory usage
        if recent_memory:
            avg_memory = statistics.mean(recent_memory)
            if avg_memory > self.alert_thresholds["memory_usage"]:
                recommendations.append(OptimizationRecommendation(
                    recommendation_id=f"memory_optimization_{int(time.time())}",
                    priority="high",
                    category="resource",
                    description="Optimize memory usage",
                    expected_improvement=0.25,
                    implementation_effort="medium",
                    risk_level="low",
                    action_items=[
                        "Profile memory usage",
                        "Implement memory pooling",
                        "Add garbage collection optimization",
                        "Monitor memory efficiency"
                    ],
                    metadata={"current_memory_mb": avg_memory}
                ))
        
        # Check CPU usage
        if recent_cpu:
            avg_cpu = statistics.mean(recent_cpu)
            if avg_cpu > self.alert_thresholds["cpu_usage"]:
                recommendations.append(OptimizationRecommendation(
                    recommendation_id=f"cpu_optimization_{int(time.time())}",
                    priority="medium",
                    category="resource",
                    description="Optimize CPU usage",
                    expected_improvement=0.2,
                    implementation_effort="high",
                    risk_level="medium",
                    action_items=[
                        "Profile CPU usage",
                        "Optimize algorithms",
                        "Implement parallelization",
                        "Monitor CPU efficiency"
                    ],
                    metadata={"current_cpu_percent": avg_cpu}
                ))
        
        return recommendations
    
    def _prioritize_recommendations(self, recommendations: List[OptimizationRecommendation]) -> List[OptimizationRecommendation]:
        """Prioritize optimization recommendations."""
        
        # Define priority scores
        priority_scores = {"critical": 4, "high": 3, "medium": 2, "low": 1}
        effort_scores = {"low": 3, "medium": 2, "high": 1}
        risk_scores = {"low": 3, "medium": 2, "high": 1}
        
        # Calculate composite scores
        for rec in recommendations:
            priority_score = priority_scores.get(rec.priority, 1)
            effort_score = effort_scores.get(rec.implementation_effort, 1)
            risk_score = risk_scores.get(rec.risk_level, 1)
            
            # Composite score: (priority * improvement) / (effort * risk)
            rec.composite_score = (priority_score * rec.expected_improvement) / max(1, effort_score * risk_score)
        
        # Sort by composite score
        return sorted(recommendations, key=lambda r: getattr(r, 'composite_score', 0), reverse=True)
    
    def _apply_automatic_optimizations(self, recommendations: List[OptimizationRecommendation]):
        """Apply automatic optimizations based on strategy."""
        
        auto_applicable = [
            rec for rec in recommendations[:3]  # Top 3 recommendations
            if rec.implementation_effort == "low" and rec.risk_level == "low"
        ]
        
        for rec in auto_applicable:
            if rec.category in self.optimization_callbacks:
                try:
                    callback = self.optimization_callbacks[rec.category]
                    callback(rec)
                    self.logger.info(f"Applied automatic optimization: {rec.description}")
                except Exception as e:
                    self.logger.error(f"Failed to apply optimization {rec.recommendation_id}: {e}")
    
    def _check_alert_conditions(self):
        """Check for alert conditions."""
        
        # Check execution time alerts
        recent_exec_times = [m.value for m in self.metrics_history 
                           if m.metric_type == MetricType.EXECUTION_TIME 
                           and datetime.now() - m.timestamp < timedelta(minutes=15)]
        
        if recent_exec_times and statistics.mean(recent_exec_times) > self.alert_thresholds["execution_time"]:
            self._create_alert(
                AlertLevel.WARNING,
                "High execution times detected",
                f"Average execution time is {statistics.mean(recent_exec_times):.2f}s",
                ["performance"]
            )
        
        # Check success rate alerts
        recent_success = [m.value for m in self.metrics_history 
                         if m.metric_type == MetricType.SUCCESS_RATE 
                         and datetime.now() - m.timestamp < timedelta(minutes=15)]
        
        if recent_success and statistics.mean(recent_success) < self.alert_thresholds["success_rate"]:
            self._create_alert(
                AlertLevel.ERROR,
                "Low success rate detected",
                f"Success rate is {statistics.mean(recent_success):.2%}",
                ["reliability"]
            )
    
    def _create_alert(self, 
                     level: AlertLevel,
                     title: str,
                     description: str,
                     affected_components: List[str]):
        """Create system alert."""
        
        alert_id = f"alert_{int(time.time() * 1000)}"
        
        alert = SystemAlert(
            alert_id=alert_id,
            level=level,
            title=title,
            description=description,
            timestamp=datetime.now(),
            affected_components=affected_components,
            recommended_actions=self._generate_alert_actions(level, affected_components)
        )
        
        self.active_alerts[alert_id] = alert
        self.logger.warning(f"Alert created: {title} - {description}")
    
    def _generate_alert_actions(self, level: AlertLevel, components: List[str]) -> List[str]:
        """Generate recommended actions for alerts."""
        
        actions = []
        
        if level in [AlertLevel.ERROR, AlertLevel.CRITICAL]:
            actions.append("Investigate root cause immediately")
            actions.append("Check system resources")
            
        if "performance" in components:
            actions.append("Review routing decisions")
            actions.append("Check for resource bottlenecks")
            
        if "reliability" in components:
            actions.append("Analyze error patterns")
            actions.append("Review validation logic")
        
        return actions
    
    def _cleanup_old_data(self):
        """Clean up old data to prevent memory leaks."""
        
        cutoff_time = datetime.now() - timedelta(hours=24)
        
        # Clean old request patterns
        old_patterns = [
            pattern_id for pattern_id, pattern in self.request_patterns.items()
            if pattern.last_seen < cutoff_time and pattern.frequency < 3
        ]
        
        for pattern_id in old_patterns:
            del self.request_patterns[pattern_id]
        
        # Clean old alerts
        old_alerts = [
            alert_id for alert_id, alert in self.active_alerts.items()
            if alert.timestamp < cutoff_time
        ]
        
        for alert_id in old_alerts:
            del self.active_alerts[alert_id]
        
        # Limit optimization history
        if len(self.optimization_history) > 100:
            self.optimization_history = self.optimization_history[-100:]
    
    def register_optimization_callback(self, category: str, callback: Callable):
        """Register callback for automatic optimizations."""
        self.optimization_callbacks[category] = callback
        self.logger.info(f"Registered optimization callback for category: {category}")
    
    def get_performance_dashboard(self) -> Dict[str, Any]:
        """Get comprehensive performance dashboard data."""
        
        current_time = datetime.now()
        
        # Recent metrics summary
        recent_metrics = [m for m in self.metrics_history 
                         if current_time - m.timestamp < timedelta(hours=1)]
        
        # Calculate summary statistics
        summary = {}
        for metric_type in MetricType:
            type_metrics = [m.value for m in recent_metrics if m.metric_type == metric_type]
            if type_metrics:
                summary[metric_type.value] = {
                    "avg": statistics.mean(type_metrics),
                    "min": min(type_metrics),
                    "max": max(type_metrics),
                    "count": len(type_metrics)
                }
        
        # Top patterns by optimization potential
        top_patterns = sorted(
            self.request_patterns.values(),
            key=lambda p: p.optimization_potential,
            reverse=True
        )[:10]
        
        # Recent recommendations
        recent_recommendations = [
            rec for rec in self.optimization_history
            if len(self.optimization_history) - self.optimization_history.index(rec) <= 10
        ]
        
        return {
            "timestamp": current_time.isoformat(),
            "monitoring_status": "active" if self.monitoring_enabled else "disabled",
            "summary_metrics": summary,
            "performance_trends": {
                key: values[-20:] for key, values in self.performance_trends.items()
            },
            "router_performance": self.router_performance,
            "top_optimization_patterns": [
                {
                    "pattern_id": p.pattern_id,
                    "type": p.pattern_type,
                    "frequency": p.frequency,
                    "optimization_potential": p.optimization_potential,
                    "avg_execution_time": p.avg_execution_time,
                    "success_rate": p.success_rate
                }
                for p in top_patterns
            ],
            "recent_recommendations": [
                {
                    "id": rec.recommendation_id,
                    "priority": rec.priority,
                    "category": rec.category,
                    "description": rec.description,
                    "expected_improvement": rec.expected_improvement,
                    "effort": rec.implementation_effort
                }
                for rec in recent_recommendations
            ],
            "active_alerts": [
                {
                    "id": alert.alert_id,
                    "level": alert.level.value,
                    "title": alert.title,
                    "description": alert.description,
                    "timestamp": alert.timestamp.isoformat(),
                    "components": alert.affected_components
                }
                for alert in self.active_alerts.values()
            ],
            "system_health": self._calculate_system_health()
        }
    
    def _calculate_system_health(self) -> Dict[str, Any]:
        """Calculate overall system health score."""
        
        health_score = 1.0
        health_factors = {}
        
        # Check recent success rate
        recent_success = [m.value for m in self.metrics_history 
                         if m.metric_type == MetricType.SUCCESS_RATE 
                         and datetime.now() - m.timestamp < timedelta(hours=1)]
        
        if recent_success:
            success_rate = statistics.mean(recent_success)
            health_factors["success_rate"] = success_rate
            if success_rate < 0.9:
                health_score *= 0.8
        
        # Check execution times
        recent_times = [m.value for m in self.metrics_history 
                       if m.metric_type == MetricType.EXECUTION_TIME 
                       and datetime.now() - m.timestamp < timedelta(hours=1)]
        
        if recent_times:
            avg_time = statistics.mean(recent_times)
            health_factors["avg_execution_time"] = avg_time
            if avg_time > 4.0:
                health_score *= 0.9
        
        # Check active alerts
        critical_alerts = sum(1 for alert in self.active_alerts.values() 
                            if alert.level == AlertLevel.CRITICAL)
        error_alerts = sum(1 for alert in self.active_alerts.values() 
                          if alert.level == AlertLevel.ERROR)
        
        health_factors["critical_alerts"] = critical_alerts
        health_factors["error_alerts"] = error_alerts
        
        if critical_alerts > 0:
            health_score *= 0.5
        elif error_alerts > 0:
            health_score *= 0.7
        
        # Determine health status
        if health_score >= 0.9:
            status = "excellent"
        elif health_score >= 0.8:
            status = "good"
        elif health_score >= 0.6:
            status = "fair"
        else:
            status = "poor"
        
        return {
            "score": health_score,
            "status": status,
            "factors": health_factors
        }
    
    def shutdown(self):
        """Shutdown the performance monitor."""
        self.monitoring_active = False
        if self.background_thread and self.background_thread.is_alive():
            self.background_thread.join(timeout=5)
        self.logger.info("PerformanceMonitor shutdown completed")