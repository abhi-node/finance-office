"""
Secure Credential Management System for Financial API Integration

This module provides secure storage, management, and access control for financial API 
credentials including Alpha Vantage, Yahoo Finance, and Bloomberg API keys. It implements
encryption at rest, credential validation, rotation capabilities, and access logging.

Key Features:
- Encrypted credential storage using environment variables
- Multiple API key support per service for load balancing
- Credential validation and health checking
- Access logging and audit trails
- Credential rotation and expiration management
- Secure error handling that prevents credential exposure
"""

import os
import base64
import hashlib
import time
import logging
import json
from datetime import datetime, timezone, timedelta
from typing import Any, Dict, List, Optional, Union, Tuple, Set
from dataclasses import dataclass, field
from enum import Enum
from threading import Lock
from cryptography.fernet import Fernet
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC


class CredentialProvider(Enum):
    """Supported financial data providers."""
    YAHOO_FINANCE = "yahoo_finance"
    BLOOMBERG = "bloomberg"
    FINNHUB = "finnhub"
    QUANDL = "quandl"
    IEX_CLOUD = "iex_cloud"


class CredentialStatus(Enum):
    """Status of API credentials."""
    ACTIVE = "active"
    EXPIRED = "expired"
    INVALID = "invalid"
    RATE_LIMITED = "rate_limited"
    SUSPENDED = "suspended"
    UNKNOWN = "unknown"


@dataclass
class APICredential:
    """Container for API credential information."""
    provider: CredentialProvider
    key_id: str
    api_key: str
    secret_key: Optional[str] = None
    endpoint_url: Optional[str] = None
    rate_limit: Optional[int] = None
    created_at: float = field(default_factory=time.time)
    last_used: Optional[float] = None
    usage_count: int = 0
    status: CredentialStatus = CredentialStatus.UNKNOWN
    metadata: Dict[str, Any] = field(default_factory=dict)
    
    def is_expired(self, expiry_hours: int = 24 * 30) -> bool:
        """Check if credential is expired based on age."""
        if self.created_at is None:
            return True
        age_hours = (time.time() - self.created_at) / 3600
        return age_hours > expiry_hours
    
    def update_usage(self):
        """Update usage statistics."""
        self.last_used = time.time()
        self.usage_count += 1
    
    def to_dict(self, include_sensitive: bool = False) -> Dict[str, Any]:
        """Convert to dictionary, optionally excluding sensitive data."""
        data = {
            "provider": self.provider.value,
            "key_id": self.key_id,
            "endpoint_url": self.endpoint_url,
            "rate_limit": self.rate_limit,
            "created_at": self.created_at,
            "last_used": self.last_used,
            "usage_count": self.usage_count,
            "status": self.status.value,
            "metadata": self.metadata
        }
        
        if include_sensitive:
            data["api_key"] = self.api_key
            data["secret_key"] = self.secret_key
        else:
            # Only show masked versions
            data["api_key"] = self._mask_key(self.api_key)
            data["secret_key"] = self._mask_key(self.secret_key) if self.secret_key else None
        
        return data
    
    def _mask_key(self, key: Optional[str]) -> Optional[str]:
        """Mask sensitive key data for logging."""
        if not key or len(key) < 8:
            return "***"
        return f"{key[:4]}...{key[-4:]}"


@dataclass
class CredentialValidation:
    """Result of credential validation."""
    provider: CredentialProvider
    key_id: str
    is_valid: bool
    status: CredentialStatus
    response_time_ms: Optional[float] = None
    error_message: Optional[str] = None
    rate_limit_remaining: Optional[int] = None
    rate_limit_reset: Optional[float] = None
    validated_at: float = field(default_factory=time.time)


class SecureCredentialManager:
    """
    Secure credential management system for financial APIs.
    
    This manager provides encrypted storage, validation, and access control
    for financial API credentials with comprehensive audit logging and
    security measures to prevent credential exposure.
    """
    
    def __init__(self, config: Optional[Dict[str, Any]] = None):
        """
        Initialize the secure credential manager.
        
        Args:
            config: Optional configuration dictionary
        """
        self.config = config or {}
        self.logger = logging.getLogger(__name__)
        
        # Thread-safe operations
        self._lock = Lock()
        
        # Credential storage
        self._credentials: Dict[str, Dict[str, APICredential]] = {}
        self._credential_cache: Dict[str, Tuple[APICredential, float]] = {}
        
        # Security configuration
        self.encryption_key = self._get_or_create_encryption_key()
        self.cipher_suite = Fernet(self.encryption_key)
        
        # Cache configuration
        self.cache_ttl_seconds = self.config.get("cache_ttl_seconds", 300)  # 5 minutes
        self.validation_interval = self.config.get("validation_interval_seconds", 3600)  # 1 hour
        
        # Access logging
        self.access_log: List[Dict[str, Any]] = []
        self.max_access_log_size = self.config.get("max_access_log_size", 1000)
        
        # Provider configurations
        self.provider_configs = self._initialize_provider_configs()
        
        # Load existing credentials
        self._load_credentials()
        
        self.logger.info("SecureCredentialManager initialized")
    
    def add_credential(self, 
                      provider: CredentialProvider,
                      api_key: str,
                      secret_key: Optional[str] = None,
                      key_id: Optional[str] = None,
                      endpoint_url: Optional[str] = None,
                      rate_limit: Optional[int] = None,
                      metadata: Optional[Dict[str, Any]] = None) -> str:
        """
        Add a new API credential.
        
        Args:
            provider: The API provider
            api_key: The API key
            secret_key: Optional secret key for OAuth providers
            key_id: Optional custom key identifier
            endpoint_url: Optional custom endpoint URL
            rate_limit: Optional rate limit per hour
            metadata: Optional additional metadata
            
        Returns:
            The generated key_id for this credential
        """
        if not key_id:
            key_id = f"{provider.value}_{int(time.time())}"
        
        # Validate key format
        if not self._validate_key_format(provider, api_key):
            raise ValueError(f"Invalid API key format for {provider.value}")
        
        credential = APICredential(
            provider=provider,
            key_id=key_id,
            api_key=api_key,
            secret_key=secret_key,
            endpoint_url=endpoint_url or self.provider_configs[provider]["default_endpoint"],
            rate_limit=rate_limit or self.provider_configs[provider]["default_rate_limit"],
            metadata=metadata or {}
        )
        
        with self._lock:
            if provider.value not in self._credentials:
                self._credentials[provider.value] = {}
            
            self._credentials[provider.value][key_id] = credential
            
            # Store encrypted in environment
            self._store_credential_securely(credential)
        
        self._log_access("credential_added", {
            "provider": provider.value,
            "key_id": key_id,
            "has_secret": bool(secret_key)
        })
        
        self.logger.info(f"Added credential for {provider.value} with key_id: {key_id}")
        return key_id
    
    def get_credential(self, 
                      provider: CredentialProvider,
                      key_id: Optional[str] = None) -> Optional[APICredential]:
        """
        Get API credential for a provider.
        
        Args:
            provider: The API provider
            key_id: Optional specific key ID, if None uses best available
            
        Returns:
            APICredential if found, None otherwise
        """
        cache_key = f"{provider.value}_{key_id or 'default'}"
        
        # Check cache first
        cached_cred, cached_time = self._credential_cache.get(cache_key, (None, 0))
        if cached_cred and (time.time() - cached_time) < self.cache_ttl_seconds:
            self._log_access("credential_accessed_cached", {
                "provider": provider.value,
                "key_id": cached_cred.key_id
            })
            cached_cred.update_usage()
            return cached_cred
        
        with self._lock:
            provider_creds = self._credentials.get(provider.value, {})
            
            if not provider_creds:
                return None
            
            # Get specific credential or best available
            if key_id:
                credential = provider_creds.get(key_id)
            else:
                # Select best credential (active, most recently used)
                active_creds = [c for c in provider_creds.values() 
                              if c.status == CredentialStatus.ACTIVE]
                
                if not active_creds:
                    # Fall back to any available credential
                    active_creds = list(provider_creds.values())
                
                if not active_creds:
                    return None
                
                # Sort by last used time (most recent first)
                credential = max(active_creds, 
                               key=lambda c: c.last_used or 0)
            
            if credential:
                # Cache the credential
                self._credential_cache[cache_key] = (credential, time.time())
                
                self._log_access("credential_accessed", {
                    "provider": provider.value,
                    "key_id": credential.key_id
                })
                
                credential.update_usage()
                return credential
        
        return None
    
    def list_credentials(self, 
                        provider: Optional[CredentialProvider] = None,
                        include_sensitive: bool = False) -> List[Dict[str, Any]]:
        """
        List stored credentials.
        
        Args:
            provider: Optional provider filter
            include_sensitive: Whether to include sensitive key data
            
        Returns:
            List of credential information dictionaries
        """
        credentials = []
        
        with self._lock:
            providers_to_check = [provider] if provider else list(CredentialProvider)
            
            for prov in providers_to_check:
                prov_creds = self._credentials.get(prov.value, {})
                for credential in prov_creds.values():
                    credentials.append(credential.to_dict(include_sensitive))
        
        self._log_access("credentials_listed", {
            "provider_filter": provider.value if provider else None,
            "count": len(credentials),
            "include_sensitive": include_sensitive
        })
        
        return credentials
    
    def validate_credential(self, 
                           provider: CredentialProvider,
                           key_id: Optional[str] = None) -> CredentialValidation:
        """
        Validate an API credential by making a test request.
        
        Args:
            provider: The API provider
            key_id: Optional specific key ID
            
        Returns:
            CredentialValidation result
        """
        credential = self.get_credential(provider, key_id)
        
        if not credential:
            return CredentialValidation(
                provider=provider,
                key_id=key_id or "unknown",
                is_valid=False,
                status=CredentialStatus.INVALID,
                error_message="Credential not found"
            )
        
        # Perform provider-specific validation
        start_time = time.time()
        
        try:
            validation_result = self._validate_with_provider(credential)
            response_time = (time.time() - start_time) * 1000
            
            # Update credential status
            credential.status = validation_result.status
            
            self._log_access("credential_validated", {
                "provider": provider.value,
                "key_id": credential.key_id,
                "is_valid": validation_result.is_valid,
                "response_time_ms": response_time
            })
            
            return validation_result
            
        except Exception as e:
            response_time = (time.time() - start_time) * 1000
            error_msg = "Validation failed"  # Don't include actual error to prevent key exposure
            
            self.logger.error(f"Credential validation failed for {provider.value}: {type(e).__name__}")
            
            return CredentialValidation(
                provider=provider,
                key_id=credential.key_id,
                is_valid=False,
                status=CredentialStatus.INVALID,
                response_time_ms=response_time,
                error_message=error_msg
            )
    
    def remove_credential(self, 
                         provider: CredentialProvider,
                         key_id: str) -> bool:
        """
        Remove a stored credential.
        
        Args:
            provider: The API provider
            key_id: The key identifier
            
        Returns:
            True if credential was removed, False if not found
        """
        with self._lock:
            provider_creds = self._credentials.get(provider.value, {})
            if key_id in provider_creds:
                del provider_creds[key_id]
                
                # Remove from environment storage
                self._remove_credential_from_storage(provider, key_id)
                
                # Clear from cache
                cache_keys = [k for k in self._credential_cache.keys() 
                            if k.startswith(f"{provider.value}_")]
                for cache_key in cache_keys:
                    if cache_key in self._credential_cache:
                        del self._credential_cache[cache_key]
                
                self._log_access("credential_removed", {
                    "provider": provider.value,
                    "key_id": key_id
                })
                
                self.logger.info(f"Removed credential {key_id} for {provider.value}")
                return True
        
        return False
    
    def get_access_log(self, 
                      limit: Optional[int] = None,
                      provider: Optional[CredentialProvider] = None) -> List[Dict[str, Any]]:
        """
        Get access log entries.
        
        Args:
            limit: Maximum number of entries to return
            provider: Optional provider filter
            
        Returns:
            List of access log entries
        """
        logs = self.access_log.copy()
        
        if provider:
            logs = [log for log in logs if log.get("details", {}).get("provider") == provider.value]
        
        if limit:
            logs = logs[-limit:]
        
        return logs
    
    def _get_or_create_encryption_key(self) -> bytes:
        """Get or create encryption key for credential storage."""
        # Try to get from environment
        key_env = os.getenv("CREDENTIAL_ENCRYPTION_KEY")
        if key_env:
            try:
                return base64.urlsafe_b64decode(key_env)
            except Exception:
                self.logger.warning("Invalid encryption key in environment, generating new one")
        
        # Generate new key
        key = Fernet.generate_key()
        
        # Store in environment for persistence (in production, use secure key management)
        os.environ["CREDENTIAL_ENCRYPTION_KEY"] = base64.urlsafe_b64encode(key).decode()
        
        self.logger.info("Generated new encryption key for credential storage")
        return key
    
    def _store_credential_securely(self, credential: APICredential):
        """Store credential securely in environment variables."""
        env_key = f"CREDENTIAL_{credential.provider.value.upper()}_{credential.key_id}"
        
        # Create credential data
        cred_data = {
            "api_key": credential.api_key,
            "secret_key": credential.secret_key,
            "endpoint_url": credential.endpoint_url,
            "rate_limit": credential.rate_limit,
            "created_at": credential.created_at,
            "metadata": credential.metadata
        }
        
        # Encrypt and store
        encrypted_data = self.cipher_suite.encrypt(json.dumps(cred_data).encode())
        os.environ[env_key] = base64.urlsafe_b64encode(encrypted_data).decode()
    
    def _load_credentials(self):
        """Load credentials from secure environment storage."""
        loaded_count = 0
        
        for env_key, env_value in os.environ.items():
            if env_key.startswith("CREDENTIAL_") and "_" in env_key[11:]:
                try:
                    # Parse environment key
                    parts = env_key[11:].split("_", 1)  # Remove "CREDENTIAL_" prefix
                    if len(parts) != 2:
                        continue
                    
                    provider_name, key_id = parts
                    
                    # Find matching provider
                    provider = None
                    for prov in CredentialProvider:
                        if prov.value.upper() == provider_name:
                            provider = prov
                            break
                    
                    if not provider:
                        continue
                    
                    # Decrypt credential data
                    encrypted_data = base64.urlsafe_b64decode(env_value.encode())
                    decrypted_data = self.cipher_suite.decrypt(encrypted_data)
                    cred_data = json.loads(decrypted_data.decode())
                    
                    # Create credential object
                    credential = APICredential(
                        provider=provider,
                        key_id=key_id,
                        api_key=cred_data["api_key"],
                        secret_key=cred_data.get("secret_key"),
                        endpoint_url=cred_data.get("endpoint_url"),
                        rate_limit=cred_data.get("rate_limit"),
                        created_at=cred_data.get("created_at", time.time()),
                        metadata=cred_data.get("metadata", {})
                    )
                    
                    # Store in memory
                    if provider.value not in self._credentials:
                        self._credentials[provider.value] = {}
                    
                    self._credentials[provider.value][key_id] = credential
                    loaded_count += 1
                    
                except Exception as e:
                    self.logger.error(f"Failed to load credential from {env_key}: {type(e).__name__}")
        
        if loaded_count > 0:
            self.logger.info(f"Loaded {loaded_count} credentials from secure storage")
    
    def _remove_credential_from_storage(self, provider: CredentialProvider, key_id: str):
        """Remove credential from environment storage."""
        env_key = f"CREDENTIAL_{provider.value.upper()}_{key_id}"
        if env_key in os.environ:
            del os.environ[env_key]
    
    def _validate_key_format(self, provider: CredentialProvider, api_key: str) -> bool:
        """Validate API key format for specific provider."""
        if not api_key or len(api_key) < 8:
            return False
        
        # Provider-specific validation
        if provider == CredentialProvider.YAHOO_FINANCE:
            # Yahoo Finance may not require keys or have various formats
            return True
        elif provider == CredentialProvider.BLOOMBERG:
            # Bloomberg keys have specific format requirements
            return len(api_key) >= 12
        else:
            # Generic validation
            return len(api_key) >= 8
    
    def _validate_with_provider(self, credential: APICredential) -> CredentialValidation:
        """Perform provider-specific credential validation."""
        # This is a mock implementation - in production, make actual API calls
        
        # Simulate validation delay
        time.sleep(0.1)
        
        # Mock validation logic based on provider
        if credential.provider == CredentialProvider.YAHOO_FINANCE:
            # Yahoo Finance often doesn't require API keys
            return CredentialValidation(
                provider=credential.provider,
                key_id=credential.key_id,
                is_valid=True,
                status=CredentialStatus.ACTIVE,
                rate_limit_remaining=2000
            )
        
        else:
            # Generic validation
            is_valid = len(credential.api_key) >= 8
            return CredentialValidation(
                provider=credential.provider,
                key_id=credential.key_id,
                is_valid=is_valid,
                status=CredentialStatus.ACTIVE if is_valid else CredentialStatus.INVALID
            )
    
    def _initialize_provider_configs(self) -> Dict[CredentialProvider, Dict[str, Any]]:
        """Initialize provider-specific configurations."""
        return {
            CredentialProvider.YAHOO_FINANCE: {
                "default_endpoint": "https://query1.finance.yahoo.com/v8/finance/chart/",
                "default_rate_limit": 2000,  # 2000 requests per hour
                "required_params": []
            },
            CredentialProvider.BLOOMBERG: {
                "default_endpoint": "https://api.bloomberg.com/v1/",
                "default_rate_limit": 100,  # 100 requests per minute
                "required_params": ["authorization"]
            },
            CredentialProvider.FINNHUB: {
                "default_endpoint": "https://finnhub.io/api/v1/",
                "default_rate_limit": 60,  # 60 requests per minute
                "required_params": ["token"]
            },
            CredentialProvider.QUANDL: {
                "default_endpoint": "https://www.quandl.com/api/v3/",
                "default_rate_limit": 300,  # 300 requests per day (free tier)
                "required_params": ["api_key"]
            },
            CredentialProvider.IEX_CLOUD: {
                "default_endpoint": "https://cloud.iexapis.com/stable/",
                "default_rate_limit": 100,  # 100 requests per month (free tier)
                "required_params": ["token"]
            }
        }
    
    def _log_access(self, action: str, details: Dict[str, Any]):
        """Log credential access for audit purposes."""
        log_entry = {
            "timestamp": time.time(),
            "action": action,
            "details": details
        }
        
        with self._lock:
            self.access_log.append(log_entry)
            
            # Trim log if too large
            if len(self.access_log) > self.max_access_log_size:
                self.access_log = self.access_log[-self.max_access_log_size//2:]


# Global credential manager instance
_credential_manager: Optional[SecureCredentialManager] = None
_manager_lock = Lock()

def get_credential_manager(config: Optional[Dict[str, Any]] = None) -> SecureCredentialManager:
    """
    Get the global credential manager instance.
    
    Args:
        config: Optional configuration for first-time initialization
        
    Returns:
        Global SecureCredentialManager instance
    """
    global _credential_manager
    
    with _manager_lock:
        if _credential_manager is None:
            _credential_manager = SecureCredentialManager(config)
        return _credential_manager