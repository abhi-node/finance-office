"""
Rate Limiting and Caching System for Financial APIs

This module provides intelligent rate limiting and multi-tier caching mechanisms
to optimize API usage, ensure compliance with service limits, and improve
performance through efficient data caching strategies.

Key Features:
- Per-API rate limiting with configurable thresholds
- Adaptive backoff strategies for rate-limited requests
- Multi-tier caching (memory + persistent)
- Cache invalidation policies and TTL management
- Cache warming strategies for common queries
- Performance monitoring and analytics
"""

import asyncio
import time
import json
import hashlib
import pickle
import logging
from datetime import datetime, timezone, timedelta
from typing import Any, Dict, List, Optional, Union, Tuple, Callable
from dataclasses import dataclass, field
from enum import Enum
from threading import Lock
from collections import defaultdict, deque
import sqlite3
from pathlib import Path

from .credential_manager import CredentialProvider


class RateLimitStrategy(Enum):
    """Rate limiting strategies."""
    FIXED_WINDOW = "fixed_window"
    SLIDING_WINDOW = "sliding_window"
    TOKEN_BUCKET = "token_bucket"
    ADAPTIVE = "adaptive"


class CacheLevel(Enum):
    """Cache storage levels."""
    MEMORY = "memory"
    DISK = "disk"
    BOTH = "both"


@dataclass
class RateLimitConfig:
    """Configuration for rate limiting."""
    provider: CredentialProvider
    requests_per_minute: int = 60
    requests_per_hour: int = 1000
    requests_per_day: int = 10000
    strategy: RateLimitStrategy = RateLimitStrategy.SLIDING_WINDOW
    burst_allowance: int = 10
    backoff_multiplier: float = 2.0
    max_backoff_seconds: float = 300.0


@dataclass
class CacheConfig:
    """Configuration for caching."""
    default_ttl_seconds: int = 3600
    max_memory_items: int = 10000
    max_disk_size_mb: int = 1000
    cache_levels: CacheLevel = CacheLevel.BOTH
    enable_compression: bool = True
    enable_cache_warming: bool = True


@dataclass
class RateLimitStatus:
    """Current rate limit status for a provider."""
    provider: CredentialProvider
    requests_remaining_minute: int
    requests_remaining_hour: int
    requests_remaining_day: int
    reset_time_minute: float
    reset_time_hour: float
    reset_time_day: float
    backoff_until: Optional[float] = None
    consecutive_failures: int = 0


@dataclass
class CacheEntry:
    """Entry in the cache system."""
    key: str
    data: Any
    created_at: float
    ttl_seconds: int
    access_count: int = 0
    last_accessed: float = field(default_factory=time.time)
    size_bytes: int = 0
    level: CacheLevel = CacheLevel.MEMORY
    
    def is_expired(self) -> bool:
        """Check if cache entry is expired."""
        return (time.time() - self.created_at) > self.ttl_seconds
    
    def update_access(self):
        """Update access statistics."""
        self.last_accessed = time.time()
        self.access_count += 1


class RateLimiter:
    """
    Intelligent rate limiter with adaptive backoff strategies.
    
    Implements multiple rate limiting algorithms and automatically
    adapts to API provider limits and failures.
    """
    
    def __init__(self, config: Optional[Dict[CredentialProvider, RateLimitConfig]] = None):
        """
        Initialize rate limiter.
        
        Args:
            config: Optional rate limit configurations per provider
        """
        self.config = config or {}
        self.logger = logging.getLogger(__name__)
        
        # Thread-safe operations
        self._lock = Lock()
        
        # Rate limit tracking
        self._request_history: Dict[CredentialProvider, deque] = defaultdict(deque)
        self._status: Dict[CredentialProvider, RateLimitStatus] = {}
        self._token_buckets: Dict[CredentialProvider, Dict[str, float]] = defaultdict(dict)
        
        # Default configurations
        self._default_configs = self._initialize_default_configs()
        
        self.logger.info("RateLimiter initialized")
    
    async def acquire(self, provider: CredentialProvider) -> bool:
        """
        Acquire permission to make a request.
        
        Args:
            provider: The API provider
            
        Returns:
            True if request is allowed, False if rate limited
        """
        config = self._get_provider_config(provider)
        
        with self._lock:
            status = self._get_or_create_status(provider)
            
            # Check if we're in backoff period
            if status.backoff_until and time.time() < status.backoff_until:
                return False
            
            # Check rate limits based on strategy
            if config.strategy == RateLimitStrategy.SLIDING_WINDOW:
                return self._check_sliding_window(provider, config)
            elif config.strategy == RateLimitStrategy.TOKEN_BUCKET:
                return self._check_token_bucket(provider, config)
            elif config.strategy == RateLimitStrategy.ADAPTIVE:
                return self._check_adaptive(provider, config)
            else:  # FIXED_WINDOW
                return self._check_fixed_window(provider, config)
    
    def record_request(self, 
                      provider: CredentialProvider,
                      success: bool,
                      response_time_ms: Optional[float] = None):
        """
        Record the result of an API request.
        
        Args:
            provider: The API provider
            success: Whether the request was successful
            response_time_ms: Response time in milliseconds
        """
        with self._lock:
            status = self._get_or_create_status(provider)
            current_time = time.time()
            
            # Record request in history
            self._request_history[provider].append((current_time, success, response_time_ms))
            
            # Clean old history (keep last hour)
            cutoff_time = current_time - 3600
            while (self._request_history[provider] and 
                   self._request_history[provider][0][0] < cutoff_time):
                self._request_history[provider].popleft()
            
            if success:
                # Reset consecutive failures on success
                status.consecutive_failures = 0
                status.backoff_until = None
            else:
                # Handle failure
                status.consecutive_failures += 1
                config = self._get_provider_config(provider)
                
                # Apply exponential backoff
                backoff_delay = min(
                    config.backoff_multiplier ** status.consecutive_failures,
                    config.max_backoff_seconds
                )
                status.backoff_until = current_time + backoff_delay
                
                self.logger.warning(
                    f"Rate limiting {provider.value} for {backoff_delay:.1f}s after "
                    f"{status.consecutive_failures} consecutive failures"
                )
    
    def get_status(self, provider: CredentialProvider) -> RateLimitStatus:
        """
        Get current rate limit status for a provider.
        
        Args:
            provider: The API provider
            
        Returns:
            Current rate limit status
        """
        with self._lock:
            return self._get_or_create_status(provider)
    
    def _get_provider_config(self, provider: CredentialProvider) -> RateLimitConfig:
        """Get configuration for a provider."""
        return self.config.get(provider, self._default_configs[provider])
    
    def _get_or_create_status(self, provider: CredentialProvider) -> RateLimitStatus:
        """Get or create rate limit status for a provider."""
        if provider not in self._status:
            config = self._get_provider_config(provider)
            current_time = time.time()
            
            self._status[provider] = RateLimitStatus(
                provider=provider,
                requests_remaining_minute=config.requests_per_minute,
                requests_remaining_hour=config.requests_per_hour,
                requests_remaining_day=config.requests_per_day,
                reset_time_minute=current_time + 60,
                reset_time_hour=current_time + 3600,
                reset_time_day=current_time + 86400
            )
        
        return self._status[provider]
    
    def _check_sliding_window(self, provider: CredentialProvider, config: RateLimitConfig) -> bool:
        """Check sliding window rate limit."""
        current_time = time.time()
        history = self._request_history[provider]
        
        # Count requests in different time windows
        requests_last_minute = sum(1 for t, _, _ in history if t > current_time - 60)
        requests_last_hour = sum(1 for t, _, _ in history if t > current_time - 3600)
        requests_last_day = sum(1 for t, _, _ in history if t > current_time - 86400)
        
        # Check limits
        if requests_last_minute >= config.requests_per_minute:
            return False
        if requests_last_hour >= config.requests_per_hour:
            return False
        if requests_last_day >= config.requests_per_day:
            return False
        
        return True
    
    def _check_token_bucket(self, provider: CredentialProvider, config: RateLimitConfig) -> bool:
        """Check token bucket rate limit."""
        current_time = time.time()
        bucket = self._token_buckets[provider]
        
        # Initialize bucket if needed
        if "tokens" not in bucket:
            bucket["tokens"] = float(config.requests_per_minute)
            bucket["last_refill"] = current_time
        
        # Refill tokens
        time_elapsed = current_time - bucket["last_refill"]
        tokens_to_add = time_elapsed * (config.requests_per_minute / 60.0)
        bucket["tokens"] = min(config.requests_per_minute, bucket["tokens"] + tokens_to_add)
        bucket["last_refill"] = current_time
        
        # Check if token available
        if bucket["tokens"] >= 1.0:
            bucket["tokens"] -= 1.0
            return True
        
        return False
    
    def _check_adaptive(self, provider: CredentialProvider, config: RateLimitConfig) -> bool:
        """Check adaptive rate limit based on recent performance."""
        current_time = time.time()
        history = self._request_history[provider]
        
        # Analyze recent performance
        recent_requests = [req for req in history if req[0] > current_time - 300]  # Last 5 minutes
        
        if not recent_requests:
            return self._check_sliding_window(provider, config)
        
        # Calculate success rate and average response time
        success_rate = sum(1 for _, success, _ in recent_requests if success) / len(recent_requests)
        avg_response_time = sum(rt for _, _, rt in recent_requests if rt) / len(recent_requests)
        
        # Adjust limits based on performance
        adaptive_multiplier = success_rate * min(1.0, 1000.0 / max(avg_response_time, 100.0))
        
        adjusted_config = RateLimitConfig(
            provider=provider,
            requests_per_minute=int(config.requests_per_minute * adaptive_multiplier),
            requests_per_hour=int(config.requests_per_hour * adaptive_multiplier),
            requests_per_day=config.requests_per_day,
            strategy=RateLimitStrategy.SLIDING_WINDOW
        )
        
        return self._check_sliding_window(provider, adjusted_config)
    
    def _check_fixed_window(self, provider: CredentialProvider, config: RateLimitConfig) -> bool:
        """Check fixed window rate limit."""
        current_time = time.time()
        status = self._status[provider]
        
        # Reset counters if windows have passed
        if current_time >= status.reset_time_minute:
            status.requests_remaining_minute = config.requests_per_minute
            status.reset_time_minute = current_time + 60
        
        if current_time >= status.reset_time_hour:
            status.requests_remaining_hour = config.requests_per_hour
            status.reset_time_hour = current_time + 3600
        
        if current_time >= status.reset_time_day:
            status.requests_remaining_day = config.requests_per_day
            status.reset_time_day = current_time + 86400
        
        # Check limits
        if (status.requests_remaining_minute <= 0 or 
            status.requests_remaining_hour <= 0 or 
            status.requests_remaining_day <= 0):
            return False
        
        # Consume request
        status.requests_remaining_minute -= 1
        status.requests_remaining_hour -= 1
        status.requests_remaining_day -= 1
        
        return True
    
    def _initialize_default_configs(self) -> Dict[CredentialProvider, RateLimitConfig]:
        """Initialize default rate limit configurations."""
        return {
            CredentialProvider.YAHOO_FINANCE: RateLimitConfig(
                provider=CredentialProvider.YAHOO_FINANCE,
                requests_per_minute=100,
                requests_per_hour=2000,
                requests_per_day=10000,
                strategy=RateLimitStrategy.TOKEN_BUCKET
            ),
            CredentialProvider.BLOOMBERG: RateLimitConfig(
                provider=CredentialProvider.BLOOMBERG,
                requests_per_minute=100,
                requests_per_hour=5000,
                requests_per_day=50000,
                strategy=RateLimitStrategy.ADAPTIVE
            )
        }


class MultiTierCache:
    """
    Multi-tier caching system with memory and persistent storage.
    
    Provides intelligent caching with automatic cache warming,
    TTL management, and performance optimization.
    """
    
    def __init__(self, config: Optional[CacheConfig] = None):
        """
        Initialize multi-tier cache.
        
        Args:
            config: Optional cache configuration
        """
        self.config = config or CacheConfig()
        self.logger = logging.getLogger(__name__)
        
        # Thread-safe operations
        self._lock = Lock()
        
        # Memory cache
        self._memory_cache: Dict[str, CacheEntry] = {}
        self._memory_usage_bytes = 0
        
        # Disk cache
        self.cache_dir = Path("cache/financial_data")
        self.cache_dir.mkdir(parents=True, exist_ok=True)
        self.db_path = self.cache_dir / "cache.db"
        self._init_disk_cache()
        
        # Cache statistics
        self._stats = {
            "hits": 0,
            "misses": 0,
            "evictions": 0,
            "disk_hits": 0,
            "memory_hits": 0
        }
        
        # Cache warming
        self._warming_tasks: Set[str] = set()
        
        self.logger.info("MultiTierCache initialized")
    
    async def get(self, key: str) -> Optional[Any]:
        """
        Get item from cache.
        
        Args:
            key: Cache key
            
        Returns:
            Cached data if found and not expired, None otherwise
        """
        cache_key = self._normalize_key(key)
        
        # Check memory cache first
        with self._lock:
            if cache_key in self._memory_cache:
                entry = self._memory_cache[cache_key]
                if not entry.is_expired():
                    entry.update_access()
                    self._stats["hits"] += 1
                    self._stats["memory_hits"] += 1
                    return entry.data
                else:
                    # Remove expired entry
                    self._evict_memory_entry(cache_key)
        
        # Check disk cache
        if self.config.cache_levels in [CacheLevel.DISK, CacheLevel.BOTH]:
            disk_data = await self._get_from_disk(cache_key)
            if disk_data is not None:
                # Move to memory cache if configured
                if self.config.cache_levels == CacheLevel.BOTH:
                    await self.set(key, disk_data, self.config.default_ttl_seconds)
                
                self._stats["hits"] += 1
                self._stats["disk_hits"] += 1
                return disk_data
        
        self._stats["misses"] += 1
        return None
    
    async def set(self, 
                 key: str, 
                 data: Any, 
                 ttl_seconds: Optional[int] = None) -> bool:
        """
        Set item in cache.
        
        Args:
            key: Cache key
            data: Data to cache
            ttl_seconds: Time to live in seconds
            
        Returns:
            True if cached successfully
        """
        cache_key = self._normalize_key(key)
        ttl = ttl_seconds or self.config.default_ttl_seconds
        
        # Estimate size
        try:
            data_size = len(pickle.dumps(data))
        except:
            data_size = 1024  # Default estimate
        
        # Create cache entry
        entry = CacheEntry(
            key=cache_key,
            data=data,
            created_at=time.time(),
            ttl_seconds=ttl,
            size_bytes=data_size,
            level=self.config.cache_levels
        )
        
        # Store in memory cache
        if self.config.cache_levels in [CacheLevel.MEMORY, CacheLevel.BOTH]:
            with self._lock:
                # Check if we need to evict
                while (len(self._memory_cache) >= self.config.max_memory_items or
                       self._memory_usage_bytes + data_size > self.config.max_memory_items * 10240):
                    self._evict_least_recently_used()
                
                self._memory_cache[cache_key] = entry
                self._memory_usage_bytes += data_size
        
        # Store in disk cache
        if self.config.cache_levels in [CacheLevel.DISK, CacheLevel.BOTH]:
            await self._set_to_disk(cache_key, data, ttl)
        
        return True
    
    async def delete(self, key: str) -> bool:
        """
        Delete item from cache.
        
        Args:
            key: Cache key
            
        Returns:
            True if item was deleted
        """
        cache_key = self._normalize_key(key)
        deleted = False
        
        # Remove from memory
        with self._lock:
            if cache_key in self._memory_cache:
                self._evict_memory_entry(cache_key)
                deleted = True
        
        # Remove from disk
        if self.config.cache_levels in [CacheLevel.DISK, CacheLevel.BOTH]:
            disk_deleted = await self._delete_from_disk(cache_key)
            deleted = deleted or disk_deleted
        
        return deleted
    
    async def clear(self) -> bool:
        """
        Clear all cache entries.
        
        Returns:
            True if cache was cleared
        """
        # Clear memory cache
        with self._lock:
            self._memory_cache.clear()
            self._memory_usage_bytes = 0
        
        # Clear disk cache
        if self.config.cache_levels in [CacheLevel.DISK, CacheLevel.BOTH]:
            await self._clear_disk_cache()
        
        self.logger.info("Cache cleared")
        return True
    
    def get_stats(self) -> Dict[str, Any]:
        """Get cache statistics."""
        with self._lock:
            total_requests = self._stats["hits"] + self._stats["misses"]
            hit_rate = self._stats["hits"] / total_requests if total_requests > 0 else 0
            
            return {
                **self._stats,
                "hit_rate": hit_rate,
                "memory_entries": len(self._memory_cache),
                "memory_usage_bytes": self._memory_usage_bytes,
                "memory_usage_mb": self._memory_usage_bytes / (1024 * 1024)
            }
    
    async def warm_cache(self, keys: List[str], data_fetcher: Callable[[str], Any]):
        """
        Warm cache with commonly accessed data.
        
        Args:
            keys: List of cache keys to warm
            data_fetcher: Function to fetch data for a key
        """
        if not self.config.enable_cache_warming:
            return
        
        warming_tasks = []
        
        for key in keys:
            if key not in self._warming_tasks:
                self._warming_tasks.add(key)
                task = asyncio.create_task(self._warm_key(key, data_fetcher))
                warming_tasks.append(task)
        
        if warming_tasks:
            await asyncio.gather(*warming_tasks, return_exceptions=True)
    
    async def _warm_key(self, key: str, data_fetcher: Callable[[str], Any]):
        """Warm a specific cache key."""
        try:
            # Check if already cached
            if await self.get(key) is not None:
                return
            
            # Fetch and cache data
            data = await data_fetcher(key)
            if data is not None:
                await self.set(key, data)
                self.logger.debug(f"Warmed cache key: {key}")
        
        except Exception as e:
            self.logger.warning(f"Failed to warm cache key {key}: {type(e).__name__}")
        
        finally:
            self._warming_tasks.discard(key)
    
    def _normalize_key(self, key: str) -> str:
        """Normalize cache key."""
        return hashlib.md5(key.encode()).hexdigest()
    
    def _evict_memory_entry(self, key: str):
        """Evict entry from memory cache."""
        if key in self._memory_cache:
            entry = self._memory_cache[key]
            self._memory_usage_bytes -= entry.size_bytes
            del self._memory_cache[key]
            self._stats["evictions"] += 1
    
    def _evict_least_recently_used(self):
        """Evict least recently used entry from memory cache."""
        if not self._memory_cache:
            return
        
        lru_key = min(self._memory_cache.keys(), 
                     key=lambda k: self._memory_cache[k].last_accessed)
        self._evict_memory_entry(lru_key)
    
    def _init_disk_cache(self):
        """Initialize disk cache database."""
        conn = sqlite3.connect(self.db_path)
        conn.execute("""
            CREATE TABLE IF NOT EXISTS cache_entries (
                key TEXT PRIMARY KEY,
                data BLOB,
                created_at REAL,
                ttl_seconds INTEGER,
                access_count INTEGER DEFAULT 0,
                last_accessed REAL
            )
        """)
        conn.execute("""
            CREATE INDEX IF NOT EXISTS idx_created_at ON cache_entries(created_at)
        """)
        conn.commit()
        conn.close()
    
    async def _get_from_disk(self, key: str) -> Optional[Any]:
        """Get item from disk cache."""
        try:
            conn = sqlite3.connect(self.db_path)
            cursor = conn.execute(
                "SELECT data, created_at, ttl_seconds FROM cache_entries WHERE key = ?",
                (key,)
            )
            row = cursor.fetchone()
            conn.close()
            
            if row:
                data_blob, created_at, ttl_seconds = row
                
                # Check if expired
                if (time.time() - created_at) > ttl_seconds:
                    await self._delete_from_disk(key)
                    return None
                
                # Update access time
                conn = sqlite3.connect(self.db_path)
                conn.execute(
                    "UPDATE cache_entries SET access_count = access_count + 1, "
                    "last_accessed = ? WHERE key = ?",
                    (time.time(), key)
                )
                conn.commit()
                conn.close()
                
                # Deserialize data
                if self.config.enable_compression:
                    import gzip
                    data_blob = gzip.decompress(data_blob)
                
                return pickle.loads(data_blob)
            
            return None
            
        except Exception as e:
            self.logger.error(f"Failed to get from disk cache: {type(e).__name__}")
            return None
    
    async def _set_to_disk(self, key: str, data: Any, ttl_seconds: int):
        """Set item to disk cache."""
        try:
            # Serialize data
            data_blob = pickle.dumps(data)
            
            if self.config.enable_compression:
                import gzip
                data_blob = gzip.compress(data_blob)
            
            conn = sqlite3.connect(self.db_path)
            conn.execute(
                "INSERT OR REPLACE INTO cache_entries "
                "(key, data, created_at, ttl_seconds, last_accessed) "
                "VALUES (?, ?, ?, ?, ?)",
                (key, data_blob, time.time(), ttl_seconds, time.time())
            )
            conn.commit()
            conn.close()
            
        except Exception as e:
            self.logger.error(f"Failed to set to disk cache: {type(e).__name__}")
    
    async def _delete_from_disk(self, key: str) -> bool:
        """Delete item from disk cache."""
        try:
            conn = sqlite3.connect(self.db_path)
            cursor = conn.execute("DELETE FROM cache_entries WHERE key = ?", (key,))
            deleted = cursor.rowcount > 0
            conn.commit()
            conn.close()
            return deleted
            
        except Exception as e:
            self.logger.error(f"Failed to delete from disk cache: {type(e).__name__}")
            return False
    
    async def _clear_disk_cache(self):
        """Clear disk cache."""
        try:
            conn = sqlite3.connect(self.db_path)
            conn.execute("DELETE FROM cache_entries")
            conn.commit()
            conn.close()
            
        except Exception as e:
            self.logger.error(f"Failed to clear disk cache: {type(e).__name__}")


# Global instances
_rate_limiter: Optional[RateLimiter] = None
_cache: Optional[MultiTierCache] = None
_instances_lock = Lock()

def get_rate_limiter(config: Optional[Dict[CredentialProvider, RateLimitConfig]] = None) -> RateLimiter:
    """Get global rate limiter instance."""
    global _rate_limiter
    
    with _instances_lock:
        if _rate_limiter is None:
            _rate_limiter = RateLimiter(config)
        return _rate_limiter

def get_cache(config: Optional[CacheConfig] = None) -> MultiTierCache:
    """Get global cache instance."""
    global _cache
    
    with _instances_lock:
        if _cache is None:
            _cache = MultiTierCache(config)
        return _cache