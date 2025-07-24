"""
Advanced State Management Utilities for LibreOffice AI Writing Assistant

This module provides advanced state management utilities including persistence,
synchronization, and monitoring capabilities for the DocumentState system.
These utilities support the multi-agent architecture with thread-safe operations
and performance optimization.
"""

import asyncio
import json
import threading
import time
from typing import Dict, List, Any, Optional, Callable, Union
from pathlib import Path
from datetime import datetime, timezone
import pickle
import hashlib
import logging
from concurrent.futures import ThreadPoolExecutor
from contextlib import contextmanager

from .document_state import DocumentState, DocumentStateManager, StateSnapshot

# Configure logging for state management
logger = logging.getLogger(__name__)

class StateChangeEvent:
    """Represents a state change event for monitoring and callbacks."""
    
    def __init__(self, event_type: str, changes: Dict[str, Any], 
                 timestamp: Optional[datetime] = None):
        self.event_id = hashlib.md5(f"{time.time()}_{event_type}".encode()).hexdigest()[:8]
        self.event_type = event_type
        self.changes = changes
        self.timestamp = timestamp or datetime.now(timezone.utc)
        self.agent_context = {}

class StatePersistenceManager:
    """
    Manages state persistence to disk with automatic saving and recovery capabilities.
    Supports both JSON for human readability and pickle for performance.
    """
    
    def __init__(self, storage_dir: Path, auto_save_interval: int = 30):
        """
        Initialize persistence manager.
        
        Args:
            storage_dir: Directory for storing state files
            auto_save_interval: Automatic save interval in seconds
        """
        self.storage_dir = Path(storage_dir)
        self.storage_dir.mkdir(exist_ok=True, parents=True)
        self.auto_save_interval = auto_save_interval
        self._auto_save_thread: Optional[threading.Thread] = None
        self._stop_auto_save = threading.Event()
        self._state_manager: Optional[DocumentStateManager] = None
        
    def attach_state_manager(self, state_manager: DocumentStateManager):
        """Attach a state manager for automatic persistence."""
        self._state_manager = state_manager
        self._start_auto_save()
    
    def _start_auto_save(self):
        """Start the automatic save thread."""
        if self._auto_save_thread is None or not self._auto_save_thread.is_alive():
            self._stop_auto_save.clear()
            self._auto_save_thread = threading.Thread(target=self._auto_save_loop, daemon=True)
            self._auto_save_thread.start()
    
    def _auto_save_loop(self):
        """Auto-save loop running in background thread."""
        while not self._stop_auto_save.wait(self.auto_save_interval):
            if self._state_manager:
                try:
                    self.save_state_async(self._state_manager)
                except Exception as e:
                    logger.error(f"Auto-save failed: {e}")
    
    def save_state_json(self, state_manager: DocumentStateManager, 
                       filename: Optional[str] = None) -> Path:
        """Save state as JSON file for human readability."""
        if filename is None:
            timestamp = datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%S")
            filename = f"document_state_{timestamp}.json"
        
        file_path = self.storage_dir / filename
        
        try:
            state_json = state_manager.serialize_state()
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(state_json)
            
            logger.info(f"State saved as JSON: {file_path}")
            return file_path
            
        except Exception as e:
            logger.error(f"Failed to save state as JSON: {e}")
            raise
    
    def save_state_pickle(self, state_manager: DocumentStateManager, 
                         filename: Optional[str] = None) -> Path:
        """Save state as pickle file for performance."""
        if filename is None:
            timestamp = datetime.now(timezone.utc).strftime("%Y%m%d_%H%M%S")
            filename = f"document_state_{timestamp}.pkl"
        
        file_path = self.storage_dir / filename
        
        try:
            state = state_manager.get_state()
            with open(file_path, 'wb') as f:
                pickle.dump(state, f, protocol=pickle.HIGHEST_PROTOCOL)
            
            logger.info(f"State saved as pickle: {file_path}")
            return file_path
            
        except Exception as e:
            logger.error(f"Failed to save state as pickle: {e}")
            raise
    
    def save_state_async(self, state_manager: DocumentStateManager):
        """Save state asynchronously to avoid blocking main thread."""
        def _save():
            # Save both formats for flexibility
            self.save_state_json(state_manager, "latest_state.json")
            self.save_state_pickle(state_manager, "latest_state.pkl")
        
        thread = threading.Thread(target=_save, daemon=True)
        thread.start()
    
    def load_state_json(self, filename: str) -> DocumentState:
        """Load state from JSON file."""
        file_path = self.storage_dir / filename
        
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                state_data = json.load(f)
            
            logger.info(f"State loaded from JSON: {file_path}")
            return state_data
            
        except Exception as e:
            logger.error(f"Failed to load state from JSON: {e}")
            raise
    
    def load_state_pickle(self, filename: str) -> DocumentState:
        """Load state from pickle file."""
        file_path = self.storage_dir / filename
        
        try:
            with open(file_path, 'rb') as f:
                state_data = pickle.load(f)
            
            logger.info(f"State loaded from pickle: {file_path}")
            return state_data
            
        except Exception as e:
            logger.error(f"Failed to load state from pickle: {e}")
            raise
    
    def load_latest_state(self) -> Optional[DocumentState]:
        """Load the most recent state, trying pickle first for performance."""
        try:
            return self.load_state_pickle("latest_state.pkl")
        except FileNotFoundError:
            pass
        
        try:
            return self.load_state_json("latest_state.json")
        except FileNotFoundError:
            logger.info("No previous state found")
            return None
    
    def list_saved_states(self) -> List[Dict[str, Any]]:
        """List all saved state files with metadata."""
        states = []
        
        for file_path in self.storage_dir.glob("document_state_*"):
            stat = file_path.stat()
            states.append({
                "filename": file_path.name,
                "size_bytes": stat.st_size,
                "modified": datetime.fromtimestamp(stat.st_mtime, timezone.utc).isoformat(),
                "format": "json" if file_path.suffix == ".json" else "pickle"
            })
        
        return sorted(states, key=lambda x: x["modified"], reverse=True)
    
    def cleanup_old_states(self, keep_count: int = 10):
        """Remove old state files, keeping only the most recent ones."""
        states = self.list_saved_states()
        
        if len(states) > keep_count:
            for state in states[keep_count:]:
                file_path = self.storage_dir / state["filename"]
                try:
                    file_path.unlink()
                    logger.info(f"Cleaned up old state: {file_path}")
                except Exception as e:
                    logger.error(f"Failed to clean up {file_path}: {e}")
    
    def stop_auto_save(self):
        """Stop the automatic save thread."""
        self._stop_auto_save.set()
        if self._auto_save_thread and self._auto_save_thread.is_alive():
            self._auto_save_thread.join(timeout=5.0)

class StateMonitor:
    """
    Monitors state changes and provides callbacks, metrics, and debugging capabilities.
    """
    
    def __init__(self):
        self._callbacks: List[Callable[[StateChangeEvent], None]] = []
        self._change_history: List[StateChangeEvent] = []
        self._max_history_size = 1000
        self._lock = threading.RLock()
        self._metrics = {
            "total_changes": 0,
            "changes_by_type": {},
            "last_change_time": None,
            "change_frequency": 0.0
        }
    
    def add_callback(self, callback: Callable[[StateChangeEvent], None]):
        """Add a callback function to be called on state changes."""
        with self._lock:
            if callback not in self._callbacks:
                self._callbacks.append(callback)
    
    def remove_callback(self, callback: Callable[[StateChangeEvent], None]):
        """Remove a callback function."""
        with self._lock:
            if callback in self._callbacks:
                self._callbacks.remove(callback)
    
    def notify_change(self, event_type: str, changes: Dict[str, Any]):
        """Notify all callbacks of a state change."""
        event = StateChangeEvent(event_type, changes)
        
        with self._lock:
            # Update metrics
            self._metrics["total_changes"] += 1
            self._metrics["changes_by_type"][event_type] = \
                self._metrics["changes_by_type"].get(event_type, 0) + 1
            self._metrics["last_change_time"] = event.timestamp.isoformat()
            
            # Calculate change frequency (changes per minute)
            if len(self._change_history) > 1:
                time_span = (event.timestamp - self._change_history[0].timestamp).total_seconds()
                if time_span > 0:
                    self._metrics["change_frequency"] = len(self._change_history) / (time_span / 60)
            
            # Add to history
            self._change_history.append(event)
            if len(self._change_history) > self._max_history_size:
                self._change_history.pop(0)
            
            # Notify callbacks
            for callback in self._callbacks:
                try:
                    callback(event)
                except Exception as e:
                    logger.error(f"State change callback failed: {e}")
    
    def get_change_history(self, limit: Optional[int] = None) -> List[Dict[str, Any]]:
        """Get recent state change history."""
        with self._lock:
            history = self._change_history[-limit:] if limit else self._change_history
            return [
                {
                    "event_id": event.event_id,
                    "event_type": event.event_type,
                    "timestamp": event.timestamp.isoformat(),
                    "changes": event.changes,
                    "agent_context": event.agent_context
                }
                for event in history
            ]
    
    def get_metrics(self) -> Dict[str, Any]:
        """Get state change metrics."""
        with self._lock:
            return self._metrics.copy()
    
    def clear_history(self):
        """Clear the change history."""
        with self._lock:
            self._change_history.clear()
            self._metrics = {
                "total_changes": 0,
                "changes_by_type": {},
                "last_change_time": None,
                "change_frequency": 0.0
            }

class EnhancedDocumentStateManager(DocumentStateManager):
    """
    Enhanced DocumentStateManager with persistence, monitoring, and advanced features.
    """
    
    def __init__(self, initial_state: Optional[DocumentState] = None,
                 storage_dir: Optional[Path] = None,
                 enable_persistence: bool = True,
                 enable_monitoring: bool = True):
        """
        Initialize enhanced state manager.
        
        Args:
            initial_state: Optional initial state
            storage_dir: Directory for state persistence
            enable_persistence: Enable automatic state persistence
            enable_monitoring: Enable state change monitoring
        """
        super().__init__(initial_state)
        
        # Initialize persistence if enabled
        self.persistence_manager = None
        if enable_persistence and storage_dir:
            self.persistence_manager = StatePersistenceManager(storage_dir)
            self.persistence_manager.attach_state_manager(self)
            
            # Try to load previous state
            previous_state = self.persistence_manager.load_latest_state()
            if previous_state:
                self._state = previous_state
                logger.info("Loaded previous state from disk")
        
        # Initialize monitoring if enabled
        self.monitor = StateMonitor() if enable_monitoring else None
        
        # Performance tracking
        self._operation_times: Dict[str, List[float]] = {}
        self._executor = ThreadPoolExecutor(max_workers=2, thread_name_prefix="StateManager")
    
    def update_state(self, updates: Dict[str, Any], create_snapshot: bool = True,
                    event_type: str = "update") -> bool:
        """Enhanced state update with monitoring and performance tracking."""
        start_time = time.time()
        
        # Call parent update method
        success = super().update_state(updates, create_snapshot)
        
        # Track performance
        execution_time = time.time() - start_time
        if event_type not in self._operation_times:
            self._operation_times[event_type] = []
        self._operation_times[event_type].append(execution_time)
        
        # Keep only recent times for memory efficiency
        if len(self._operation_times[event_type]) > 100:
            self._operation_times[event_type] = self._operation_times[event_type][-50:]
        
        # Notify monitor if enabled
        if success and self.monitor:
            self.monitor.notify_change(event_type, updates)
        
        return success
    
    @contextmanager
    def transaction(self, description: str = "Transaction"):
        """Context manager for atomic state transactions with automatic rollback on error."""
        snapshot = self._create_snapshot(f"Transaction start: {description}")
        try:
            yield
        except Exception as e:
            # Rollback on error
            self.rollback_to_snapshot(snapshot.snapshot_id)
            logger.error(f"Transaction rolled back due to error: {e}")
            raise
    
    def bulk_update(self, updates_list: List[Dict[str, Any]], 
                   description: str = "Bulk update") -> bool:
        """Perform multiple updates as a single atomic transaction."""
        with self.transaction(description):
            for updates in updates_list:
                if not self.update_state(updates, create_snapshot=False):
                    raise Exception(f"Bulk update failed at: {updates}")
            return True
    
    def get_performance_metrics(self) -> Dict[str, Any]:
        """Get detailed performance metrics including operation timing."""
        base_metrics = super().get_performance_summary()
        
        # Add operation timing statistics
        timing_stats = {}
        for operation, times in self._operation_times.items():
            if times:
                timing_stats[operation] = {
                    "count": len(times),
                    "avg_time": sum(times) / len(times),
                    "min_time": min(times),
                    "max_time": max(times),
                    "recent_avg": sum(times[-10:]) / min(len(times), 10)
                }
        
        base_metrics.update({
            "operation_timing": timing_stats,
            "monitor_metrics": self.monitor.get_metrics() if self.monitor else {},
            "persistence_enabled": self.persistence_manager is not None
        })
        
        return base_metrics
    
    def export_state_report(self) -> Dict[str, Any]:
        """Export a comprehensive state report for debugging and analysis."""
        return {
            "state_summary": self.get_performance_metrics(),
            "state_history": self.get_state_history(),
            "change_history": self.monitor.get_change_history(50) if self.monitor else [],
            "current_state_structure": {
                key: type(value).__name__ if not isinstance(value, (dict, list)) 
                     else f"{type(value).__name__}({len(value)})"
                for key, value in self.get_state().items()
            },
            "export_timestamp": datetime.now(timezone.utc).isoformat()
        }
    
    def cleanup(self):
        """Clean up resources when shutting down."""
        if self.persistence_manager:
            self.persistence_manager.stop_auto_save()
        
        if hasattr(self, '_executor'):
            self._executor.shutdown(wait=True)
        
        logger.info("DocumentStateManager cleanup completed")

# Factory function for creating state managers based on configuration
def create_state_manager(config: Dict[str, Any]) -> EnhancedDocumentStateManager:
    """
    Factory function to create a DocumentStateManager based on configuration.
    
    Args:
        config: Configuration dictionary with settings
        
    Returns:
        Configured EnhancedDocumentStateManager instance
    """
    storage_dir = None
    if config.get("enable_persistence", False):
        storage_dir = Path(config.get("storage_dir", "./state_storage"))
    
    initial_state = config.get("initial_state")
    enable_persistence = config.get("enable_persistence", True)
    enable_monitoring = config.get("enable_monitoring", True)
    
    manager = EnhancedDocumentStateManager(
        initial_state=initial_state,
        storage_dir=storage_dir,
        enable_persistence=enable_persistence,
        enable_monitoring=enable_monitoring
    )
    
    # Add any configured callbacks
    if "state_callbacks" in config and manager.monitor:
        for callback in config["state_callbacks"]:
            manager.monitor.add_callback(callback)
    
    return manager