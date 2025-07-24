"""
Agent Utility Classes

This module provides utility classes that support agent operations including
logging, configuration management, performance monitoring, state validation,
and error recovery. These utilities ensure consistent behavior across all
agents and provide comprehensive monitoring and debugging capabilities.

Utility Classes:
- AgentLogger: Centralized logging system for agents
- AgentConfigManager: Configuration management and validation
- PerformanceMonitor: Real-time performance tracking and optimization
- StateValidator: State consistency and validation utilities
- ErrorRecoveryManager: Comprehensive error handling and recovery
"""

import json
import logging
import time
import threading
from dataclasses import dataclass, field
from datetime import datetime, timezone, timedelta
from typing import Any, Dict, List, Optional, Callable, Union
from pathlib import Path
from collections import defaultdict, deque
import traceback

# Optional dependencies with fallbacks
try:
    import psutil
except ImportError:
    psutil = None
from contextlib import contextmanager
from enum import Enum

# Import DocumentState for validation
try:
    from state.document_state import DocumentState, AgentStatus
except ImportError:
    # For testing, create mock types
    DocumentState = Dict[str, Any]
    AgentStatus = Any

class LogLevel(Enum):
    """Log level enumeration."""
    DEBUG = "DEBUG"
    INFO = "INFO"
    WARNING = "WARNING"
    ERROR = "ERROR"
    CRITICAL = "CRITICAL"

@dataclass
class LogEntry:
    """Structured log entry."""
    timestamp: str
    level: str
    agent_id: str
    message: str
    context: Dict[str, Any] = field(default_factory=dict)
    operation_id: Optional[str] = None
    performance_data: Optional[Dict[str, Any]] = None

@dataclass
class PerformanceSnapshot:
    """Performance metrics snapshot."""
    timestamp: str
    agent_id: str
    cpu_usage: float
    memory_usage_mb: float
    operation_count: int
    average_response_time: float
    error_rate: float
    active_operations: int

@dataclass
class ValidationRule:
    """State validation rule definition."""
    rule_id: str
    rule_name: str
    validation_function: Callable[[DocumentState], bool]
    error_message: str
    severity: str = "error"  # error, warning, info
    enabled: bool = True

class AgentLogger:
    """
    Centralized logging system for agents with structured logging,
    performance tracking, and debugging capabilities.
    """
    
    def __init__(self, 
                 agent_id: str,
                 log_level: LogLevel = LogLevel.INFO,
                 log_file: Optional[Path] = None,
                 max_log_entries: int = 1000):
        """
        Initialize agent logger.
        
        Args:
            agent_id: Unique identifier for the agent
            log_level: Minimum log level to record
            log_file: Optional file path for persistent logging
            max_log_entries: Maximum number of log entries to retain in memory
        """
        self.agent_id = agent_id
        self.log_level = log_level
        self.log_file = log_file
        self.max_log_entries = max_log_entries
        
        # In-memory log storage
        self.log_entries: deque = deque(maxlen=max_log_entries)
        self.lock = threading.RLock()
        
        # Configure Python logger
        self.logger = logging.getLogger(f"agent.{agent_id}")
        self.logger.setLevel(getattr(logging, log_level.value))
        
        # Setup handlers
        self._setup_handlers()
        
        # Performance tracking
        self.operation_times: Dict[str, List[float]] = defaultdict(list)
        self.performance_snapshots: deque = deque(maxlen=100)
    
    def _setup_handlers(self) -> None:
        """Setup logging handlers."""
        # Clear existing handlers
        self.logger.handlers.clear()
        
        # Console handler
        console_handler = logging.StreamHandler()
        console_formatter = logging.Formatter(
            f'%(asctime)s - Agent[{self.agent_id}] - %(levelname)s - %(message)s'
        )
        console_handler.setFormatter(console_formatter)
        self.logger.addHandler(console_handler)
        
        # File handler if specified
        if self.log_file:
            file_handler = logging.FileHandler(self.log_file)
            file_formatter = logging.Formatter(
                f'%(asctime)s - Agent[{self.agent_id}] - %(levelname)s - %(message)s - %(pathname)s:%(lineno)d'
            )
            file_handler.setFormatter(file_formatter)
            self.logger.addHandler(file_handler)
    
    def log(self, 
            level: LogLevel, 
            message: str, 
            context: Optional[Dict[str, Any]] = None,
            operation_id: Optional[str] = None,
            performance_data: Optional[Dict[str, Any]] = None) -> None:
        """
        Log a structured message.
        
        Args:
            level: Log level
            message: Log message
            context: Optional context data
            operation_id: Optional operation identifier
            performance_data: Optional performance metrics
        """
        if self._should_log(level):
            with self.lock:
                # Create structured log entry
                log_entry = LogEntry(
                    timestamp=datetime.now(timezone.utc).isoformat(),
                    level=level.value,
                    agent_id=self.agent_id,
                    message=message,
                    context=context or {},
                    operation_id=operation_id,
                    performance_data=performance_data
                )
                
                # Store in memory
                self.log_entries.append(log_entry)
                
                # Log through Python logger
                log_method = getattr(self.logger, level.value.lower())
                context_str = f" | Context: {context}" if context else ""
                log_method(f"{message}{context_str}")
    
    def _should_log(self, level: LogLevel) -> bool:
        """Check if message should be logged based on level."""
        level_values = {
            LogLevel.DEBUG: 10,
            LogLevel.INFO: 20,
            LogLevel.WARNING: 30,
            LogLevel.ERROR: 40,
            LogLevel.CRITICAL: 50
        }
        return level_values[level] >= level_values[self.log_level]
    
    def debug(self, message: str, **kwargs) -> None:
        """Log debug message."""
        self.log(LogLevel.DEBUG, message, **kwargs)
    
    def info(self, message: str, **kwargs) -> None:
        """Log info message."""
        self.log(LogLevel.INFO, message, **kwargs)
    
    def warning(self, message: str, **kwargs) -> None:
        """Log warning message."""
        self.log(LogLevel.WARNING, message, **kwargs)
    
    def error(self, message: str, **kwargs) -> None:
        """Log error message."""
        self.log(LogLevel.ERROR, message, **kwargs)
    
    def critical(self, message: str, **kwargs) -> None:
        """Log critical message."""
        self.log(LogLevel.CRITICAL, message, **kwargs)
    
    def log_operation_time(self, operation_name: str, execution_time: float) -> None:
        """Log operation execution time for performance tracking."""
        with self.lock:
            self.operation_times[operation_name].append(execution_time)
            
            # Keep only recent times (last 100)
            if len(self.operation_times[operation_name]) > 100:
                self.operation_times[operation_name] = self.operation_times[operation_name][-50:]
    
    def get_recent_logs(self, count: int = 50) -> List[LogEntry]:
        """Get recent log entries."""
        with self.lock:
            return list(self.log_entries)[-count:]
    
    def get_performance_summary(self) -> Dict[str, Any]:
        """Get performance summary from logged operations."""
        with self.lock:
            summary = {}
            for operation, times in self.operation_times.items():
                if times:
                    summary[operation] = {
                        "count": len(times),
                        "average_time": sum(times) / len(times),
                        "min_time": min(times),
                        "max_time": max(times),
                        "recent_average": sum(times[-10:]) / min(len(times), 10)
                    }
            return summary

class AgentConfigManager:
    """
    Configuration management system for agents with validation,
    hot reloading, and environment-specific settings.
    """
    
    def __init__(self, 
                 agent_id: str,
                 config_file: Optional[Path] = None,
                 default_config: Optional[Dict[str, Any]] = None):
        """
        Initialize configuration manager.
        
        Args:
            agent_id: Unique identifier for the agent
            config_file: Optional configuration file path
            default_config: Default configuration values
        """
        self.agent_id = agent_id
        self.config_file = config_file
        self.default_config = default_config or {}
        
        # Configuration storage
        self.config: Dict[str, Any] = {}
        self.config_history: List[Dict[str, Any]] = []
        self.lock = threading.RLock()
        
        # Validation rules
        self.validation_rules: Dict[str, Callable] = {}
        
        # Load initial configuration
        self._load_configuration()
        
        # Setup file watching if config file exists
        if self.config_file and self.config_file.exists():
            self._setup_file_watching()
    
    def _load_configuration(self) -> None:
        """Load configuration from file and defaults."""
        with self.lock:
            # Start with defaults
            self.config = self.default_config.copy()
            
            # Load from file if exists
            if self.config_file and self.config_file.exists():
                try:
                    with open(self.config_file, 'r') as f:
                        file_config = json.load(f)
                    
                    # Merge with defaults
                    self.config.update(file_config)
                    
                except Exception as e:
                    logging.error(f"Failed to load config file {self.config_file}: {e}")
            
            # Validate configuration
            if not self._validate_configuration():
                logging.warning(f"Configuration validation failed for agent {self.agent_id}")
            
            # Store in history
            self.config_history.append({
                "timestamp": datetime.now(timezone.utc).isoformat(),
                "config": self.config.copy(),
                "source": "file" if self.config_file else "default"
            })
    
    def _setup_file_watching(self) -> None:
        """Setup file watching for configuration changes."""
        # Implementation would use file system watchers
        # For now, just log that watching would be enabled
        logging.info(f"Configuration file watching enabled for {self.config_file}")
    
    def _validate_configuration(self) -> bool:
        """Validate current configuration against rules."""
        try:
            for rule_name, rule_func in self.validation_rules.items():
                if not rule_func(self.config):
                    logging.error(f"Configuration validation failed for rule: {rule_name}")
                    return False
            return True
        except Exception as e:
            logging.error(f"Configuration validation error: {e}")
            return False
    
    def get(self, key: str, default: Any = None) -> Any:
        """Get configuration value."""
        with self.lock:
            return self.config.get(key, default)
    
    def set(self, key: str, value: Any) -> bool:
        """Set configuration value."""
        with self.lock:
            old_value = self.config.get(key)
            self.config[key] = value
            
            if self._validate_configuration():
                # Save to file if configured
                if self.config_file:
                    self._save_configuration()
                return True
            else:
                # Revert on validation failure
                if old_value is not None:
                    self.config[key] = old_value
                else:
                    self.config.pop(key, None)
                return False
    
    def update(self, updates: Dict[str, Any]) -> bool:
        """Update multiple configuration values."""
        with self.lock:
            # Store backup
            backup = self.config.copy()
            
            # Apply updates
            self.config.update(updates)
            
            if self._validate_configuration():
                # Save to file if configured
                if self.config_file:
                    self._save_configuration()
                return True
            else:
                # Revert on validation failure
                self.config = backup
                return False
    
    def _save_configuration(self) -> None:
        """Save configuration to file."""
        if self.config_file:
            try:
                with open(self.config_file, 'w') as f:
                    json.dump(self.config, f, indent=2)
            except Exception as e:
                logging.error(f"Failed to save configuration: {e}")
    
    def add_validation_rule(self, rule_name: str, rule_func: Callable[[Dict[str, Any]], bool]) -> None:
        """Add configuration validation rule."""
        self.validation_rules[rule_name] = rule_func
    
    def get_config_history(self) -> List[Dict[str, Any]]:
        """Get configuration change history."""
        with self.lock:
            return self.config_history.copy()

class PerformanceMonitor:
    """
    Real-time performance monitoring system with alerting,
    optimization recommendations, and resource tracking.
    """
    
    def __init__(self, 
                 agent_id: str,
                 monitoring_interval: float = 5.0,
                 alert_thresholds: Optional[Dict[str, float]] = None):
        """
        Initialize performance monitor.
        
        Args:
            agent_id: Unique identifier for the agent
            monitoring_interval: Monitoring interval in seconds
            alert_thresholds: Alert threshold values
        """
        self.agent_id = agent_id
        self.monitoring_interval = monitoring_interval
        self.alert_thresholds = alert_thresholds or {
            "cpu_usage": 80.0,
            "memory_usage_mb": 500.0,
            "error_rate": 0.1,
            "response_time": 5.0
        }
        
        # Performance data
        self.snapshots: deque = deque(maxlen=1000)
        self.alerts: List[Dict[str, Any]] = []
        self.lock = threading.RLock()
        
        # Monitoring state
        self.monitoring_active = False
        self.monitoring_thread: Optional[threading.Thread] = None
        
        # Counters
        self.operation_count = 0
        self.error_count = 0
        self.total_response_time = 0.0
        self.active_operations = 0
    
    def start_monitoring(self) -> None:
        """Start performance monitoring."""
        if not self.monitoring_active:
            self.monitoring_active = True
            self.monitoring_thread = threading.Thread(
                target=self._monitoring_loop, 
                daemon=True,
                name=f"PerformanceMonitor-{self.agent_id}"
            )
            self.monitoring_thread.start()
    
    def stop_monitoring(self) -> None:
        """Stop performance monitoring."""
        self.monitoring_active = False
        if self.monitoring_thread:
            self.monitoring_thread.join(timeout=5.0)
    
    def _monitoring_loop(self) -> None:
        """Main monitoring loop."""
        while self.monitoring_active:
            try:
                snapshot = self._capture_snapshot()
                
                with self.lock:
                    self.snapshots.append(snapshot)
                    self._check_alerts(snapshot)
                
            except Exception as e:
                logging.error(f"Performance monitoring error: {e}")
            
            time.sleep(self.monitoring_interval)
    
    def _capture_snapshot(self) -> PerformanceSnapshot:
        """Capture current performance snapshot."""
        # Get system metrics if psutil is available
        if psutil:
            try:
                process = psutil.Process()
                cpu_usage = process.cpu_percent()
                memory_usage_mb = process.memory_info().rss / 1024 / 1024
            except Exception:
                cpu_usage = 0.0
                memory_usage_mb = 0.0
        else:
            cpu_usage = 0.0
            memory_usage_mb = 0.0
        
        # Calculate metrics
        avg_response_time = (
            self.total_response_time / max(self.operation_count, 1)
        )
        error_rate = self.error_count / max(self.operation_count, 1)
        
        return PerformanceSnapshot(
            timestamp=datetime.now(timezone.utc).isoformat(),
            agent_id=self.agent_id,
            cpu_usage=cpu_usage,
            memory_usage_mb=memory_usage_mb,
            operation_count=self.operation_count,
            average_response_time=avg_response_time,
            error_rate=error_rate,
            active_operations=self.active_operations
        )
    
    def _check_alerts(self, snapshot: PerformanceSnapshot) -> None:
        """Check for performance alerts."""
        alerts = []
        
        if snapshot.cpu_usage > self.alert_thresholds["cpu_usage"]:
            alerts.append({
                "type": "cpu_usage",
                "value": snapshot.cpu_usage,
                "threshold": self.alert_thresholds["cpu_usage"],
                "severity": "warning"
            })
        
        if snapshot.memory_usage_mb > self.alert_thresholds["memory_usage_mb"]:
            alerts.append({
                "type": "memory_usage",
                "value": snapshot.memory_usage_mb,
                "threshold": self.alert_thresholds["memory_usage_mb"],
                "severity": "warning"
            })
        
        if snapshot.error_rate > self.alert_thresholds["error_rate"]:
            alerts.append({
                "type": "error_rate",
                "value": snapshot.error_rate,
                "threshold": self.alert_thresholds["error_rate"],
                "severity": "error"
            })
        
        if snapshot.average_response_time > self.alert_thresholds["response_time"]:
            alerts.append({
                "type": "response_time",
                "value": snapshot.average_response_time,
                "threshold": self.alert_thresholds["response_time"],
                "severity": "warning"
            })
        
        # Store alerts
        for alert in alerts:
            alert["timestamp"] = snapshot.timestamp
            alert["agent_id"] = self.agent_id
            self.alerts.append(alert)
    
    def record_operation(self, execution_time: float, success: bool) -> None:
        """Record operation metrics."""
        with self.lock:
            self.operation_count += 1
            self.total_response_time += execution_time
            
            if not success:
                self.error_count += 1
    
    def start_operation(self) -> None:
        """Record start of operation."""
        with self.lock:
            self.active_operations += 1
    
    def end_operation(self) -> None:
        """Record end of operation."""
        with self.lock:
            self.active_operations = max(0, self.active_operations - 1)
    
    def get_recent_snapshots(self, count: int = 50) -> List[PerformanceSnapshot]:
        """Get recent performance snapshots."""
        with self.lock:
            return list(self.snapshots)[-count:]
    
    def get_recent_alerts(self, count: int = 20) -> List[Dict[str, Any]]:
        """Get recent performance alerts."""
        with self.lock:
            return self.alerts[-count:]
    
    def get_performance_summary(self) -> Dict[str, Any]:
        """Get comprehensive performance summary."""
        with self.lock:
            if not self.snapshots:
                return {"status": "no_data"}
            
            recent_snapshots = list(self.snapshots)[-10:]
            
            return {
                "agent_id": self.agent_id,
                "monitoring_active": self.monitoring_active,
                "total_operations": self.operation_count,
                "error_count": self.error_count,
                "success_rate": (self.operation_count - self.error_count) / max(self.operation_count, 1),
                "average_response_time": self.total_response_time / max(self.operation_count, 1),
                "active_operations": self.active_operations,
                "recent_cpu_avg": sum(s.cpu_usage for s in recent_snapshots) / len(recent_snapshots),
                "recent_memory_avg": sum(s.memory_usage_mb for s in recent_snapshots) / len(recent_snapshots),
                "alert_count": len(self.alerts),
                "last_snapshot": recent_snapshots[-1] if recent_snapshots else None
            }

class StateValidator:
    """
    State consistency and validation utilities for DocumentState.
    Ensures state integrity across agent operations.
    """
    
    def __init__(self, custom_rules: Optional[List[ValidationRule]] = None):
        """
        Initialize state validator.
        
        Args:
            custom_rules: Optional list of custom validation rules
        """
        self.validation_rules: Dict[str, ValidationRule] = {}
        self.validation_history: List[Dict[str, Any]] = []
        self.lock = threading.RLock()
        
        # Setup default validation rules
        self._setup_default_rules()
        
        # Add custom rules
        if custom_rules:
            for rule in custom_rules:
                self.add_validation_rule(rule)
    
    def _setup_default_rules(self) -> None:
        """Setup default state validation rules."""
        # Required fields validation
        required_fields_rule = ValidationRule(
            rule_id="required_fields",
            rule_name="Required Fields Present",
            validation_function=self._validate_required_fields,
            error_message="Required DocumentState fields are missing"
        )
        self.add_validation_rule(required_fields_rule)
        
        # Data type validation
        data_types_rule = ValidationRule(
            rule_id="data_types",
            rule_name="Data Type Consistency",
            validation_function=self._validate_data_types,
            error_message="DocumentState field data types are inconsistent"
        )
        self.add_validation_rule(data_types_rule)
        
        # List size limits
        list_limits_rule = ValidationRule(
            rule_id="list_limits",
            rule_name="List Size Limits",
            validation_function=self._validate_list_limits,
            error_message="DocumentState lists exceed size limits",
            severity="warning"
        )
        self.add_validation_rule(list_limits_rule)
    
    def _validate_required_fields(self, state: DocumentState) -> bool:
        """Validate that required fields are present."""
        required_fields = [
            "current_document", "cursor_position", "selected_text",
            "document_structure", "formatting_state", "messages",
            "current_task", "agent_status"
        ]
        
        for field in required_fields:
            if field not in state:
                return False
        return True
    
    def _validate_data_types(self, state: DocumentState) -> bool:
        """Validate data type consistency."""
        try:
            # Check basic type constraints
            if not isinstance(state.get("selected_text", ""), str):
                return False
            if not isinstance(state.get("current_task", ""), str):
                return False
            if not isinstance(state.get("messages", []), list):
                return False
            if not isinstance(state.get("agent_status", {}), dict):
                return False
            return True
        except Exception:
            return False
    
    def _validate_list_limits(self, state: DocumentState) -> bool:
        """Validate list size limits."""
        list_limits = {
            "messages": 1000,
            "task_history": 500,
            "generated_content": 200,
            "pending_operations": 100,
            "completed_operations": 1000
        }
        
        for field, limit in list_limits.items():
            field_value = state.get(field, [])
            if isinstance(field_value, list) and len(field_value) > limit:
                return False
        return True
    
    def add_validation_rule(self, rule: ValidationRule) -> None:
        """Add a validation rule."""
        with self.lock:
            self.validation_rules[rule.rule_id] = rule
    
    def remove_validation_rule(self, rule_id: str) -> bool:
        """Remove a validation rule."""
        with self.lock:
            if rule_id in self.validation_rules:
                del self.validation_rules[rule_id]
                return True
            return False
    
    def validate_state(self, state: DocumentState) -> Dict[str, Any]:
        """
        Validate DocumentState against all rules.
        
        Args:
            state: DocumentState to validate
            
        Returns:
            Validation results with issues and recommendations
        """
        validation_start = time.time()
        results = {
            "valid": True,
            "errors": [],
            "warnings": [],
            "info": [],
            "rules_checked": 0,
            "validation_time": 0.0,
            "timestamp": datetime.now(timezone.utc).isoformat()
        }
        
        with self.lock:
            for rule_id, rule in self.validation_rules.items():
                if not rule.enabled:
                    continue
                    
                try:
                    results["rules_checked"] += 1
                    
                    if not rule.validation_function(state):
                        issue = {
                            "rule_id": rule_id,
                            "rule_name": rule.rule_name,
                            "message": rule.error_message,
                            "severity": rule.severity
                        }
                        
                        if rule.severity == "error":
                            results["errors"].append(issue)
                            results["valid"] = False
                        elif rule.severity == "warning":
                            results["warnings"].append(issue)
                        else:
                            results["info"].append(issue)
                            
                except Exception as e:
                    error_issue = {
                        "rule_id": rule_id,
                        "rule_name": rule.rule_name,
                        "message": f"Validation rule failed: {str(e)}",
                        "severity": "error"
                    }
                    results["errors"].append(error_issue)
                    results["valid"] = False
        
        results["validation_time"] = time.time() - validation_start
        
        # Store in history
        self.validation_history.append(results)
        
        return results
    
    def get_validation_history(self, count: int = 50) -> List[Dict[str, Any]]:
        """Get recent validation history."""
        with self.lock:
            return self.validation_history[-count:]

class ErrorRecoveryManager:
    """
    Comprehensive error handling and recovery system for agents.
    Provides automatic retry, fallback strategies, and error analysis.
    """
    
    def __init__(self, 
                 agent_id: str,
                 max_retries: int = 3,
                 retry_delay: float = 1.0,
                 exponential_backoff: bool = True):
        """
        Initialize error recovery manager.
        
        Args:
            agent_id: Unique identifier for the agent
            max_retries: Maximum number of retry attempts
            retry_delay: Base retry delay in seconds
            exponential_backoff: Whether to use exponential backoff
        """
        self.agent_id = agent_id
        self.max_retries = max_retries
        self.retry_delay = retry_delay
        self.exponential_backoff = exponential_backoff
        
        # Error tracking
        self.error_history: List[Dict[str, Any]] = []
        self.recovery_strategies: Dict[str, Callable] = {}
        self.lock = threading.RLock()
        
        # Setup default recovery strategies
        self._setup_default_strategies()
    
    def _setup_default_strategies(self) -> None:
        """Setup default error recovery strategies."""
        self.recovery_strategies.update({
            "connection_error": self._handle_connection_error,
            "timeout_error": self._handle_timeout_error,
            "validation_error": self._handle_validation_error,
            "resource_error": self._handle_resource_error,
            "unknown_error": self._handle_unknown_error
        })
    
    def _handle_connection_error(self, error: Exception, context: Dict[str, Any]) -> Dict[str, Any]:
        """Handle connection-related errors."""
        return {
            "strategy": "retry_with_backoff",
            "max_retries": self.max_retries,
            "delay": self.retry_delay * 2,  # Longer delay for connection issues
            "fallback": "use_cached_data"
        }
    
    def _handle_timeout_error(self, error: Exception, context: Dict[str, Any]) -> Dict[str, Any]:
        """Handle timeout errors."""
        return {
            "strategy": "retry_with_timeout_increase",
            "max_retries": 2,  # Fewer retries for timeouts
            "delay": self.retry_delay,
            "timeout_multiplier": 1.5
        }
    
    def _handle_validation_error(self, error: Exception, context: Dict[str, Any]) -> Dict[str, Any]:
        """Handle validation errors."""
        return {
            "strategy": "fix_and_retry",
            "max_retries": 1,  # Only one retry for validation issues
            "delay": 0.1,  # Quick retry after fix
            "fix_function": "auto_fix_validation"
        }
    
    def _handle_resource_error(self, error: Exception, context: Dict[str, Any]) -> Dict[str, Any]:
        """Handle resource-related errors."""
        return {
            "strategy": "wait_and_retry",
            "max_retries": self.max_retries,
            "delay": self.retry_delay * 3,  # Longer wait for resources
            "cleanup_before_retry": True
        }
    
    def _handle_unknown_error(self, error: Exception, context: Dict[str, Any]) -> Dict[str, Any]:
        """Handle unknown errors."""
        return {
            "strategy": "cautious_retry",
            "max_retries": 1,  # Conservative retry for unknown errors
            "delay": self.retry_delay,
            "escalate_on_failure": True
        }
    
    @contextmanager
    def error_recovery_context(self, operation_name: str, context: Optional[Dict[str, Any]] = None):
        """
        Context manager for automatic error recovery.
        
        Args:
            operation_name: Name of the operation being performed
            context: Optional context information
        """
        operation_context = context or {}
        operation_context["operation_name"] = operation_name
        operation_context["agent_id"] = self.agent_id
        
        try:
            yield
        except Exception as e:
            self._handle_error(e, operation_context)
            raise
    
    def _handle_error(self, error: Exception, context: Dict[str, Any]) -> None:
        """Handle and log error with recovery information."""
        error_info = {
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "agent_id": self.agent_id,
            "error_type": type(error).__name__,
            "error_message": str(error),
            "context": context,
            "stack_trace": traceback.format_exc()
        }
        
        # Determine error category
        error_category = self._categorize_error(error)
        
        # Get recovery strategy
        recovery_strategy = self.recovery_strategies.get(
            error_category, 
            self.recovery_strategies["unknown_error"]
        )
        
        # Apply recovery strategy
        strategy_info = recovery_strategy(error, context)
        error_info["recovery_strategy"] = strategy_info
        
        with self.lock:
            self.error_history.append(error_info)
        
        logging.error(f"Error in agent {self.agent_id}: {error_info}")
    
    def _categorize_error(self, error: Exception) -> str:
        """Categorize error for recovery strategy selection."""
        error_type = type(error).__name__.lower()
        
        if "connection" in error_type or "network" in error_type:
            return "connection_error"
        elif "timeout" in error_type:
            return "timeout_error"
        elif "validation" in error_type or "invalid" in error_type:
            return "validation_error"
        elif "memory" in error_type or "resource" in error_type:
            return "resource_error"
        else:
            return "unknown_error"
    
    def add_recovery_strategy(self, error_type: str, strategy_func: Callable) -> None:
        """Add custom recovery strategy."""
        self.recovery_strategies[error_type] = strategy_func
    
    def get_error_statistics(self) -> Dict[str, Any]:
        """Get error statistics and patterns."""
        with self.lock:
            if not self.error_history:
                return {"total_errors": 0}
            
            error_types = defaultdict(int)
            recovery_strategies = defaultdict(int)
            
            for error_info in self.error_history:
                error_types[error_info["error_type"]] += 1
                strategy = error_info.get("recovery_strategy", {}).get("strategy", "unknown")
                recovery_strategies[strategy] += 1
            
            return {
                "total_errors": len(self.error_history),
                "error_types": dict(error_types),
                "recovery_strategies": dict(recovery_strategies),
                "recent_error_rate": len([e for e in self.error_history 
                                        if datetime.fromisoformat(e["timestamp"]) > 
                                        datetime.now(timezone.utc) - timedelta(hours=1)]),
                "last_error": self.error_history[-1] if self.error_history else None
            }
    
    def get_recent_errors(self, count: int = 20) -> List[Dict[str, Any]]:
        """Get recent error information."""
        with self.lock:
            return self.error_history[-count:]

# Utility functions for agent utilities
def create_agent_logger(agent_id: str, config: Optional[Dict[str, Any]] = None) -> AgentLogger:
    """Factory function to create agent logger."""
    config = config or {}
    return AgentLogger(
        agent_id=agent_id,
        log_level=LogLevel(config.get("log_level", "INFO")),
        log_file=Path(config["log_file"]) if config.get("log_file") else None,
        max_log_entries=config.get("max_log_entries", 1000)
    )

def create_performance_monitor(agent_id: str, config: Optional[Dict[str, Any]] = None) -> PerformanceMonitor:
    """Factory function to create performance monitor."""
    config = config or {}
    return PerformanceMonitor(
        agent_id=agent_id,
        monitoring_interval=config.get("monitoring_interval", 5.0),
        alert_thresholds=config.get("alert_thresholds")
    )

def create_error_recovery_manager(agent_id: str, config: Optional[Dict[str, Any]] = None) -> ErrorRecoveryManager:
    """Factory function to create error recovery manager."""
    config = config or {}
    return ErrorRecoveryManager(
        agent_id=agent_id,
        max_retries=config.get("max_retries", 3),
        retry_delay=config.get("retry_delay", 1.0),
        exponential_backoff=config.get("exponential_backoff", True)
    )