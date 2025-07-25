"""
Configuration Management for LangGraph Multi-Agent System

This module provides centralized configuration management for the 
LibreOffice AI Writing Assistant agent system, including performance
settings, API credentials, and operational parameters.
"""

import os
from typing import Dict, Any, Optional
from dataclasses import dataclass
from pathlib import Path

# Load environment variables from .env file
try:
    from dotenv import load_dotenv
    load_dotenv()
except ImportError:
    # python-dotenv not installed, just use system environment variables
    pass

@dataclass
class PerformanceConfig:
    """Performance optimization settings for different operation types."""
    
    # Response time targets (seconds)
    simple_operation_target: float = 2.0
    moderate_operation_target: float = 4.0
    complex_operation_target: float = 5.0
    
    # Resource limits
    max_memory_mb: int = 200
    max_cpu_percent: float = 10.0
    
    # Concurrency settings
    max_parallel_agents: int = 3
    agent_timeout_seconds: int = 30
    
    # Caching configuration
    enable_analysis_cache: bool = True
    cache_ttl_seconds: int = 300
    max_cache_size_mb: int = 50

@dataclass 
class APIConfig:
    """External API integration configuration."""
    
    # Financial data APIs
    yahoo_finance_enabled: bool = True
    
    # AI model configuration  
    openai_api_key: Optional[str] = None
    anthropic_api_key: Optional[str] = None
    default_model: str = "gpt-3.5-turbo"
    
    # Rate limiting
    api_requests_per_minute: int = 60
    max_concurrent_requests: int = 5

@dataclass
class LibreOfficeConfig:
    """LibreOffice integration configuration."""
    
    # UNO service settings
    uno_connection_timeout: int = 10
    document_operation_timeout: int = 30
    
    # Document processing
    max_document_size_mb: int = 50
    enable_undo_tracking: bool = True
    
    # UI integration
    progress_update_interval_ms: int = 250
    enable_cancellation: bool = True

@dataclass
class SystemConfig:
    """Main system configuration container."""
    
    performance: PerformanceConfig
    api: APIConfig
    libreoffice: LibreOfficeConfig
    
    # System paths
    agent_data_dir: Path
    log_dir: Path
    cache_dir: Path
    
    # Logging configuration
    log_level: str = "INFO"
    enable_performance_logging: bool = True

class ConfigManager:
    """Manages configuration loading, validation, and environment integration."""
    
    def __init__(self, config_path: Optional[Path] = None):
        self.config_path = config_path or Path(__file__).parent / "agent_config.json"
        self._config: Optional[SystemConfig] = None
    
    def load_config(self) -> SystemConfig:
        """Load configuration from environment variables and config files."""
        if self._config is not None:
            return self._config
            
        # Create base directories
        base_dir = Path(__file__).parent
        data_dir = base_dir / "data"
        log_dir = base_dir / "logs" 
        cache_dir = base_dir / "cache"
        
        for directory in [data_dir, log_dir, cache_dir]:
            directory.mkdir(exist_ok=True)
        
        # Load configuration with environment variable overrides
        performance_config = PerformanceConfig(
            simple_operation_target=float(os.getenv("AGENT_SIMPLE_TARGET", "2.0")),
            moderate_operation_target=float(os.getenv("AGENT_MODERATE_TARGET", "4.0")),
            complex_operation_target=float(os.getenv("AGENT_COMPLEX_TARGET", "5.0")),
            max_memory_mb=int(os.getenv("AGENT_MAX_MEMORY_MB", "200")),
            max_cpu_percent=float(os.getenv("AGENT_MAX_CPU_PERCENT", "10.0"))
        )
        
        api_config = APIConfig(
            openai_api_key=os.getenv("OPENAI_API_KEY"),
            anthropic_api_key=os.getenv("ANTHROPIC_API_KEY"),
            default_model=os.getenv("AGENT_DEFAULT_MODEL", "gpt-3.5-turbo")
        )
        
        libreoffice_config = LibreOfficeConfig(
            uno_connection_timeout=int(os.getenv("UNO_CONNECTION_TIMEOUT", "10")),
            document_operation_timeout=int(os.getenv("DOC_OPERATION_TIMEOUT", "30")),
            max_document_size_mb=int(os.getenv("MAX_DOCUMENT_SIZE_MB", "50"))
        )
        
        self._config = SystemConfig(
            performance=performance_config,
            api=api_config,
            libreoffice=libreoffice_config,
            agent_data_dir=data_dir,
            log_dir=log_dir,
            cache_dir=cache_dir,
            log_level=os.getenv("AGENT_LOG_LEVEL", "INFO")
        )
        
        return self._config
    
    def validate_config(self, config: SystemConfig) -> bool:
        """Validate configuration settings and dependencies."""
        # Check required API keys for enabled features
        if not config.api.openai_api_key and not config.api.anthropic_api_key:
            raise ValueError("At least one AI API key must be configured")
        
        # Validate performance targets
        if config.performance.simple_operation_target >= config.performance.moderate_operation_target:
            raise ValueError("Simple operation target must be less than moderate target")
            
        if config.performance.moderate_operation_target >= config.performance.complex_operation_target:
            raise ValueError("Moderate operation target must be less than complex target")
        
        # Validate resource limits
        if config.performance.max_memory_mb < 50:
            raise ValueError("Maximum memory must be at least 50MB")
            
        if config.performance.max_cpu_percent > 50:
            raise ValueError("Maximum CPU usage should not exceed 50%")
        
        return True

# Global configuration instance
_config_manager = ConfigManager()

def get_config() -> SystemConfig:
    """Get the current system configuration."""
    config = _config_manager.load_config()
    _config_manager.validate_config(config)
    return config