"""
Pytest configuration and shared fixtures for LibreOffice AI Agent System tests
"""

import pytest
import asyncio
import tempfile
import json
from pathlib import Path
from typing import Dict, Any, Generator
from unittest.mock import Mock, MagicMock

# Test configuration
pytest_plugins = ["pytest_asyncio"]

@pytest.fixture(scope="session")
def event_loop():
    """Create an instance of the default event loop for the test session."""
    loop = asyncio.get_event_loop_policy().new_event_loop()
    yield loop
    loop.close()

@pytest.fixture
def temp_directory() -> Generator[Path, None, None]:
    """Provide a temporary directory for test files."""
    with tempfile.TemporaryDirectory() as temp_dir:
        yield Path(temp_dir)

@pytest.fixture
def mock_document_context() -> Dict[str, Any]:
    """Mock LibreOffice document context for testing."""
    return {
        "current_document": {
            "title": "Test Document",
            "path": "/tmp/test_document.odt",
            "type": "text",
            "size": 1024,
            "modified": "2024-01-01T12:00:00Z"
        },
        "cursor_position": {
            "paragraph": 3,
            "character": 15,
            "page": 1
        },
        "selected_text": "Sample selected text",
        "document_structure": {
            "paragraphs": 10,
            "tables": 2, 
            "images": 1,
            "pages": 3
        },
        "user_preferences": {
            "language": "en-US",
            "theme": "default",
            "auto_save": True
        }
    }

@pytest.fixture
def mock_libreoffice_service():
    """Mock LibreOffice UNO service for testing."""
    service = Mock()
    service.getDocument.return_value = Mock()
    service.getCursor.return_value = Mock() 
    service.insertText.return_value = True
    service.formatText.return_value = True
    service.createTable.return_value = Mock()
    service.insertChart.return_value = Mock()
    return service

@pytest.fixture 
def mock_agent_response():
    """Mock agent response for testing."""
    return {
        "agent_id": "test_agent",
        "operation_id": "test_op_123",
        "status": "completed",
        "result": {
            "content": "Generated test content",
            "formatting": {"bold": True, "font_size": 12},
            "metadata": {"word_count": 10, "processing_time": 0.5}
        },
        "errors": [],
        "warnings": []
    }

@pytest.fixture
def mock_external_api_responses():
    """Mock external API responses for testing."""
    return {
        "alpha_vantage": {
            "Global Quote": {
                "01. symbol": "AAPL",
                "02. open": "150.00",
                "03. high": "155.00", 
                "04. low": "148.00",
                "05. price": "152.50",
                "06. volume": "1000000",
                "07. latest trading day": "2024-01-01",
                "08. previous close": "149.00",
                "09. change": "3.50",
                "10. change percent": "2.35%"
            }
        },
        "yahoo_finance": {
            "symbol": "AAPL",
            "price": 152.50,
            "change": 3.50,
            "change_percent": 2.35,
            "volume": 1000000
        }
    }

@pytest.fixture
def performance_test_config():
    """Configuration for performance testing."""
    return {
        "simple_operation_max_time": 2.0,
        "moderate_operation_max_time": 4.0, 
        "complex_operation_max_time": 5.0,
        "max_memory_mb": 200,
        "max_cpu_percent": 10.0,
        "benchmark_iterations": 100
    }

@pytest.fixture
def test_config_file(temp_directory: Path) -> Path:
    """Create a test configuration file."""
    config_data = {
        "performance": {
            "simple_operation_target": 2.0,
            "moderate_operation_target": 4.0,
            "complex_operation_target": 5.0,
            "max_memory_mb": 200,
            "max_cpu_percent": 10.0
        },
        "api": {
            "openai_api_key": "test_key",
            "default_model": "gpt-3.5-turbo"
        },
        "libreoffice": {
            "uno_connection_timeout": 10,
            "document_operation_timeout": 30
        }
    }
    
    config_file = temp_directory / "test_config.json"
    with open(config_file, 'w') as f:
        json.dump(config_data, f, indent=2)
    
    return config_file

# Performance test utilities
def assert_performance_target(execution_time: float, target_time: float, operation_type: str):
    """Assert that execution time meets performance targets."""
    assert execution_time <= target_time, (
        f"{operation_type} operation took {execution_time:.2f}s, "
        f"which exceeds target of {target_time:.2f}s"
    )

def assert_memory_usage(memory_mb: float, max_memory_mb: float):
    """Assert that memory usage is within limits."""
    assert memory_mb <= max_memory_mb, (
        f"Memory usage {memory_mb:.1f}MB exceeds limit of {max_memory_mb}MB"
    )

# Test markers and categories
pytestmark = [
    pytest.mark.filterwarnings("ignore:.*:DeprecationWarning"),
    pytest.mark.filterwarnings("ignore:.*:PendingDeprecationWarning")
]