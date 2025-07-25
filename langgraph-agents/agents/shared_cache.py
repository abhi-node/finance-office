"""
Shared State Coordination and Caching System for LangGraph Agents

This module provides a centralized caching and state coordination system for 
ContentGenerationAgent and FormattingAgent to enable efficient agent interaction,
cross-agent data sharing, and performance optimization through intelligent caching.

Key Features:
- Cross-agent content and formatting cache
- State coordination between agents
- Cache invalidation strategies
- Performance optimization for document workflows
- Shared context management
"""

import asyncio
import time
import hashlib
import json
import logging
from datetime import datetime, timezone
from typing import Any, Dict, List, Optional, Union, Tuple, Set
from dataclasses import dataclass, field
from enum import Enum
from threading import Lock


class CacheType(Enum):
    """Types of cached data in the shared system."""
    GENERATED_CONTENT = "generated_content"
    FORMATTING_OPERATION = "formatting_operation"
    DOCUMENT_ANALYSIS = "document_analysis"
    AGENT_RESULT = "agent_result"
    CROSS_AGENT_STATE = "cross_agent_state"


class InvalidationTrigger(Enum):
    """Events that trigger cache invalidation."""
    DOCUMENT_CHANGED = "document_changed"
    USER_PREFERENCE_UPDATED = "user_preference_updated"
    CONTENT_MODIFIED = "content_modified"
    FORMATTING_APPLIED = "formatting_applied"
    MANUAL_INVALIDATION = "manual_invalidation"
    TTL_EXPIRED = "ttl_expired"


@dataclass
class CacheEntry:
    """Container for cached data with metadata."""
    key: str
    cache_type: CacheType
    data: Any
    created_timestamp: float
    last_accessed: float
    access_count: int = 0
    ttl_seconds: Optional[int] = None
    metadata: Dict[str, Any] = field(default_factory=dict)
    dependencies: Set[str] = field(default_factory=set)  # Cache keys this entry depends on
    
    def is_expired(self) -> bool:
        """Check if cache entry has expired."""
        if self.ttl_seconds is None:
            return False
        return (time.time() - self.created_timestamp) > self.ttl_seconds
    
    def update_access(self):
        """Update access tracking."""
        self.last_accessed = time.time()
        self.access_count += 1


@dataclass 
class SharedContext:
    """Shared context information between agents."""
    document_id: str
    agent_states: Dict[str, Dict[str, Any]] = field(default_factory=dict)
    workflow_state: Dict[str, Any] = field(default_factory=dict)
    coordination_metadata: Dict[str, Any] = field(default_factory=dict)
    last_updated: float = field(default_factory=time.time)
    
    def update_agent_state(self, agent_id: str, state_updates: Dict[str, Any]):
        """Update state for a specific agent."""
        if agent_id not in self.agent_states:
            self.agent_states[agent_id] = {}
        self.agent_states[agent_id].update(state_updates)
        self.last_updated = time.time()
    
    def get_agent_state(self, agent_id: str) -> Dict[str, Any]:
        """Get current state for a specific agent."""
        return self.agent_states.get(agent_id, {})


class AgentCoordinationHub:
    """
    Central coordination system for managing agent interactions and shared state.
    
    This class provides the infrastructure for agents to coordinate their operations,
    share context, and maintain consistency across document manipulation workflows.
    """
    
    def __init__(self, config: Optional[Dict[str, Any]] = None):
        """
        Initialize the coordination hub.
        
        Args:
            config: Optional configuration dictionary
        """
        self.config = config or {}
        self.logger = logging.getLogger(__name__)
        
        # Thread-safe operations
        self._lock = Lock()
        
        # Cache storage
        self._cache: Dict[str, CacheEntry] = {}
        self._cache_stats = {
            "hits": 0,
            "misses": 0,
            "evictions": 0,
            "invalidations": 0
        }
        
        # Shared contexts by document
        self._contexts: Dict[str, SharedContext] = {}
        
        # Configuration
        self.max_cache_size = self.config.get("max_cache_size", 1000)
        self.default_ttl = self.config.get("default_ttl_seconds", 3600)  # 1 hour
        self.enable_cache_analytics = self.config.get("enable_cache_analytics", True)
        self.cleanup_interval = self.config.get("cleanup_interval_seconds", 300)  # 5 minutes
        
        # Cache invalidation mapping
        self._invalidation_triggers: Dict[InvalidationTrigger, Set[CacheType]] = {
            InvalidationTrigger.DOCUMENT_CHANGED: {
                CacheType.DOCUMENT_ANALYSIS,
                CacheType.CROSS_AGENT_STATE
            },
            InvalidationTrigger.CONTENT_MODIFIED: {
                CacheType.GENERATED_CONTENT,
                CacheType.AGENT_RESULT
            },
            InvalidationTrigger.FORMATTING_APPLIED: {
                CacheType.FORMATTING_OPERATION,
                CacheType.DOCUMENT_ANALYSIS
            },
            InvalidationTrigger.USER_PREFERENCE_UPDATED: {
                CacheType.GENERATED_CONTENT,
                CacheType.FORMATTING_OPERATION
            }
        }
        
        # Start cleanup task
        self._start_cleanup_task()
        
        self.logger.info("AgentCoordinationHub initialized")
    
    def get_shared_context(self, document_id: str) -> SharedContext:
        """
        Get or create shared context for a document.
        
        Args:
            document_id: Document identifier
            
        Returns:
            SharedContext for the document
        """
        with self._lock:
            if document_id not in self._contexts:
                self._contexts[document_id] = SharedContext(document_id=document_id)
            return self._contexts[document_id]
    
    def update_agent_context(self, 
                           document_id: str, 
                           agent_id: str, 
                           state_updates: Dict[str, Any]):
        """
        Update shared context with agent state changes.
        
        Args:
            document_id: Document identifier
            agent_id: Agent identifier
            state_updates: State changes to apply
        """
        context = self.get_shared_context(document_id)
        context.update_agent_state(agent_id, state_updates)
        
        # Cache the updated context
        self.cache_data(
            key=f"context_{document_id}",
            data=context,
            cache_type=CacheType.CROSS_AGENT_STATE,
            ttl_seconds=self.default_ttl
        )
    
    def get_agent_context(self, document_id: str, agent_id: str) -> Dict[str, Any]:
        """
        Get current context for a specific agent and document.
        
        Args:
            document_id: Document identifier
            agent_id: Agent identifier
            
        Returns:
            Agent's current context state
        """
        context = self.get_shared_context(document_id)
        return context.get_agent_state(agent_id)
    
    def cache_data(self, 
                   key: str,
                   data: Any,
                   cache_type: CacheType,
                   ttl_seconds: Optional[int] = None,
                   metadata: Optional[Dict[str, Any]] = None,
                   dependencies: Optional[Set[str]] = None) -> bool:
        """
        Cache data in the shared cache system.
        
        Args:
            key: Cache key
            data: Data to cache
            cache_type: Type of cached data
            ttl_seconds: Time to live in seconds
            metadata: Optional metadata
            dependencies: Optional cache key dependencies
            
        Returns:
            True if cached successfully
        """
        with self._lock:
            # Check cache size and evict if needed
            if len(self._cache) >= self.max_cache_size:
                self._evict_least_recently_used()
            
            # Create cache entry
            entry = CacheEntry(
                key=key,
                cache_type=cache_type,
                data=data,
                created_timestamp=time.time(),
                last_accessed=time.time() ,
                ttl_seconds=ttl_seconds or self.default_ttl,
                metadata=metadata or {},
                dependencies=dependencies or set()
            )
            
            self._cache[key] = entry
            
            self.logger.debug(f"Cached data: {key} (type: {cache_type.value})")
            return True
    
    def get_cached_data(self, key: str) -> Optional[Any]:
        """
        Retrieve data from cache.
        
        Args:
            key: Cache key
            
        Returns:
            Cached data or None if not found/expired
        """
        with self._lock:
            entry = self._cache.get(key)
            
            if entry is None:
                self._cache_stats["misses"] += 1
                return None
            
            if entry.is_expired():
                del self._cache[key]
                self._cache_stats["misses"] += 1
                return None
            
            entry.update_access()
            self._cache_stats["hits"] += 1
            
            self.logger.debug(f"Cache hit: {key}")
            return entry.data
    
    def invalidate_cache(self, 
                        trigger: InvalidationTrigger,
                        document_id: Optional[str] = None,
                        specific_keys: Optional[List[str]] = None):
        """
        Invalidate cache entries based on trigger events.
        
        Args:
            trigger: Event that triggered invalidation
            document_id: Optional document ID to limit scope
            specific_keys: Optional specific keys to invalidate
        """
        with self._lock:
            keys_to_remove = set()
            
            if specific_keys:
                # Invalidate specific keys
                keys_to_remove.update(specific_keys)
            else:
                # Invalidate based on trigger type
                affected_types = self._invalidation_triggers.get(trigger, set())
                
                for key, entry in self._cache.items():
                    if entry.cache_type in affected_types:
                        # Check document scope if specified
                        if document_id is None or key.startswith(f"{document_id}_") or key.endswith(f"_{document_id}"):
                            keys_to_remove.add(key)
            
            # Remove keys and handle dependencies
            for key in keys_to_remove:
                self._remove_with_dependencies(key)
            
            self._cache_stats["invalidations"] += len(keys_to_remove)
            
            self.logger.info(f"Invalidated {len(keys_to_remove)} cache entries (trigger: {trigger.value})")
    
    def _remove_with_dependencies(self, key: str):
        """Remove cache entry and all entries that depend on it."""
        if key not in self._cache:
            return
        
        # Find all entries that depend on this key
        dependent_keys = []
        for other_key, entry in self._cache.items():
            if key in entry.dependencies:
                dependent_keys.append(other_key)
        
        # Remove this entry
        del self._cache[key]
        
        # Recursively remove dependent entries
        for dependent_key in dependent_keys:
            self._remove_with_dependencies(dependent_key)
    
    def _evict_least_recently_used(self):
        """Evict the least recently used cache entry."""
        if not self._cache:
            return
        
        lru_key = min(self._cache.keys(), 
                     key=lambda k: self._cache[k].last_accessed)
        
        del self._cache[lru_key]
        self._cache_stats["evictions"] += 1
        
        self.logger.debug(f"Evicted LRU cache entry: {lru_key}")
    
    def _start_cleanup_task(self):
        """Start background cleanup task for expired entries."""
        def cleanup():
            while True:
                try:
                    time.sleep(self.cleanup_interval)
                    self._cleanup_expired_entries()
                except Exception as e:
                    self.logger.error(f"Cache cleanup error: {e}")
        
        import threading
        cleanup_thread = threading.Thread(target=cleanup, daemon=True)
        cleanup_thread.start()
    
    def _cleanup_expired_entries(self):
        """Remove expired cache entries."""
        with self._lock:
            expired_keys = []
            for key, entry in self._cache.items():
                if entry.is_expired():
                    expired_keys.append(key)
            
            for key in expired_keys:
                del self._cache[key]
            
            if expired_keys:
                self.logger.debug(f"Cleaned up {len(expired_keys)} expired cache entries")
    
    def get_cache_stats(self) -> Dict[str, Any]:
        """Get cache performance statistics."""
        with self._lock:
            total_requests = self._cache_stats["hits"] + self._cache_stats["misses"]
            hit_rate = (self._cache_stats["hits"] / total_requests) if total_requests > 0 else 0
            
            return {
                "total_entries": len(self._cache),
                "hits": self._cache_stats["hits"],
                "misses": self._cache_stats["misses"],
                "hit_rate": hit_rate,
                "evictions": self._cache_stats["evictions"],
                "invalidations": self._cache_stats["invalidations"],
                "cache_types": self._get_cache_type_distribution()
            }
    
    def _get_cache_type_distribution(self) -> Dict[str, int]:
        """Get distribution of cache types."""
        distribution = {}
        for entry in self._cache.values():
            cache_type = entry.cache_type.value
            distribution[cache_type] = distribution.get(cache_type, 0) + 1
        return distribution
    
    def clear_cache(self, document_id: Optional[str] = None):
        """
        Clear cache entries.
        
        Args:
            document_id: Optional document ID to limit scope
        """
        with self._lock:
            if document_id is None:
                # Clear all cache
                self._cache.clear()
                self._contexts.clear()
            else:
                # Clear document-specific cache
                keys_to_remove = []
                for key in self._cache.keys():
                    if key.startswith(f"{document_id}_") or key.endswith(f"_{document_id}"):
                        keys_to_remove.append(key)
                
                for key in keys_to_remove:
                    del self._cache[key]
                
                if document_id in self._contexts:
                    del self._contexts[document_id]
        
        self.logger.info(f"Cleared cache for document: {document_id or 'all'}")


# Global shared coordination hub instance
_coordination_hub: Optional[AgentCoordinationHub] = None
_hub_lock = Lock()

def get_coordination_hub(config: Optional[Dict[str, Any]] = None) -> AgentCoordinationHub:
    """
    Get the global agent coordination hub instance.
    
    Args:
        config: Optional configuration for first-time initialization
        
    Returns:
        Global AgentCoordinationHub instance
    """
    global _coordination_hub
    
    with _hub_lock:
        if _coordination_hub is None:
            _coordination_hub = AgentCoordinationHub(config)
        return _coordination_hub


class SharedCacheMixin:
    """
    Mixin class for agents to add shared caching capabilities.
    
    This mixin provides methods for agents to interact with the shared
    coordination hub and caching system.
    """
    
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.coordination_hub = get_coordination_hub()
        self.agent_id = getattr(self, 'agent_id', 'unknown_agent')
    
    def cache_agent_result(self, 
                          document_id: str,
                          operation_id: str, 
                          result: Any,
                          ttl_seconds: Optional[int] = None) -> bool:
        """
        Cache agent operation result.
        
        Args:
            document_id: Document identifier
            operation_id: Operation identifier
            result: Result to cache
            ttl_seconds: Optional TTL override
            
        Returns:
            True if cached successfully
        """
        cache_key = f"{document_id}_{self.agent_id}_{operation_id}"
        return self.coordination_hub.cache_data(
            key=cache_key,
            data=result,
            cache_type=CacheType.AGENT_RESULT,
            ttl_seconds=ttl_seconds,
            metadata={
                "agent_id": self.agent_id,
                "document_id": document_id,
                "operation_id": operation_id
            }
        )
    
    def get_cached_agent_result(self, document_id: str, operation_id: str) -> Optional[Any]:
        """
        Get cached agent operation result.
        
        Args:
            document_id: Document identifier
            operation_id: Operation identifier
            
        Returns:
            Cached result or None
        """
        cache_key = f"{document_id}_{self.agent_id}_{operation_id}"
        return self.coordination_hub.get_cached_data(cache_key)
    
    def update_shared_context(self, document_id: str, state_updates: Dict[str, Any]):
        """
        Update shared context with agent state changes.
        
        Args:
            document_id: Document identifier  
            state_updates: State changes to apply
        """
        self.coordination_hub.update_agent_context(document_id, self.agent_id, state_updates)
    
    def get_other_agent_context(self, document_id: str, other_agent_id: str) -> Dict[str, Any]:
        """
        Get context from another agent.
        
        Args:
            document_id: Document identifier
            other_agent_id: Other agent's identifier
            
        Returns:
            Other agent's context state
        """
        return self.coordination_hub.get_agent_context(document_id, other_agent_id)
    
    def invalidate_related_cache(self, 
                                document_id: str,
                                trigger: InvalidationTrigger):
        """
        Invalidate cache based on agent operations.
        
        Args:
            document_id: Document identifier
            trigger: Invalidation trigger event
        """
        self.coordination_hub.invalidate_cache(trigger=trigger, document_id=document_id)
    
    def generate_cache_key(self, document_id: str, *components: str) -> str:
        """
        Generate consistent cache key for agent operations.
        
        Args:
            document_id: Document identifier
            components: Additional key components
            
        Returns:
            Generated cache key
        """
        key_parts = [document_id, self.agent_id] + list(components)
        key_string = "_".join(str(part) for part in key_parts)
        return hashlib.md5(key_string.encode()).hexdigest()[:16]