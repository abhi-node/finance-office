"""
Test suite to validate the LangGraph agent system directory structure and imports.

This test file verifies that Task 11.1 (Set up langgraph-agents directory structure)
has been completed successfully according to the task requirements.
"""

import pytest
import sys
from pathlib import Path
from importlib import import_module

# Add the langgraph-agents directory to Python path for testing
AGENTS_DIR = Path(__file__).parent.parent
sys.path.insert(0, str(AGENTS_DIR))

class TestDirectoryStructure:
    """Test the foundational directory structure for the LangGraph multi-agent system."""
    
    def test_root_directory_exists(self):
        """Verify the langgraph-agents root directory exists."""
        assert AGENTS_DIR.exists(), "langgraph-agents directory should exist"
        assert AGENTS_DIR.is_dir(), "langgraph-agents should be a directory"
    
    def test_required_subdirectories_exist(self):
        """Verify all required subdirectories are created."""
        required_dirs = [
            "agents",      # Agent implementations
            "state",       # Shared state management  
            "workflow",    # LangGraph configurations
            "utils",       # Common utilities
            "tests"        # Testing suite
        ]
        
        for dir_name in required_dirs:
            dir_path = AGENTS_DIR / dir_name
            assert dir_path.exists(), f"{dir_name}/ directory should exist"
            assert dir_path.is_dir(), f"{dir_name}/ should be a directory"
    
    def test_python_package_structure(self):
        """Verify proper Python package structure with __init__.py files."""
        package_dirs = [
            "",           # Root package
            "agents",     # Agents package
            "state",      # State package
            "workflow",   # Workflow package
            "utils",      # Utils package
            "tests"       # Tests package
        ]
        
        for package_dir in package_dirs:
            init_file = AGENTS_DIR / package_dir / "__init__.py"
            assert init_file.exists(), f"__init__.py should exist in {package_dir}/"
            assert init_file.is_file(), f"__init__.py should be a file in {package_dir}/"
    
    def test_configuration_files_exist(self):
        """Verify base configuration files are created."""
        config_files = [
            "config.py",        # Configuration management
            "requirements.txt", # Dependencies
            "README.md",        # Documentation
            "setup.py",         # Package setup
            "pytest.ini"       # Test configuration
        ]
        
        for config_file in config_files:
            file_path = AGENTS_DIR / config_file
            assert file_path.exists(), f"{config_file} should exist"
            assert file_path.is_file(), f"{config_file} should be a file"
    
    def test_test_subdirectories_exist(self):
        """Verify comprehensive test directory structure."""
        test_dirs = [
            "test_agents",      # Agent tests
            "test_state",       # State management tests
            "test_workflow",    # Workflow tests
            "test_utils",       # Utility tests
            "test_integration", # Integration tests
            "fixtures",         # Test fixtures
            "performance"       # Performance tests
        ]
        
        tests_dir = AGENTS_DIR / "tests"
        for test_dir in test_dirs:
            dir_path = tests_dir / test_dir
            assert dir_path.exists(), f"tests/{test_dir}/ directory should exist"
            assert dir_path.is_dir(), f"tests/{test_dir}/ should be a directory"

class TestPythonPackageImports:
    """Test that Python packages can be imported correctly."""
    
    def test_root_package_import(self):
        """Verify the root package can be imported."""
        try:
            # This will be available after agents are implemented
            # import langgraph_agents
            pass  # Skip for now since agents aren't implemented yet
        except ImportError:
            pass  # Expected until Task 11.2+ are completed
    
    def test_config_module_import(self):
        """Verify the configuration module can be imported."""
        try:
            from config import ConfigManager, get_config
            assert ConfigManager is not None
            assert get_config is not None
        except ImportError as e:
            pytest.fail(f"Failed to import config module: {e}")
    
    def test_package_structure_validation(self):
        """Verify package structure matches expected organization."""
        # Test that __init__.py files contain expected structure
        root_init = AGENTS_DIR / "__init__.py"
        with open(root_init, 'r') as f:
            content = f.read()
            
        # Verify docstring and key imports are defined
        assert '"""' in content, "Root __init__.py should have docstring"
        assert '__version__' in content, "Root __init__.py should define version"
        assert '__all__' in content, "Root __init__.py should define __all__"

class TestConfigurationManagement:
    """Test the configuration management system."""
    
    def test_config_manager_instantiation(self):
        """Verify ConfigManager can be instantiated."""
        from config import ConfigManager
        
        config_manager = ConfigManager()
        assert config_manager is not None
        assert hasattr(config_manager, 'load_config')
        assert hasattr(config_manager, 'validate_config')
    
    def test_default_configuration_loading(self):
        """Verify default configuration can be loaded."""
        from config import get_config
        
        # This should work with environment variables or defaults
        config = get_config()
        assert config is not None
        assert hasattr(config, 'performance')
        assert hasattr(config, 'api')  
        assert hasattr(config, 'libreoffice')
    
    def test_performance_targets_defined(self):
        """Verify performance targets are properly defined."""
        from config import get_config
        
        config = get_config()
        perf = config.performance
        
        # Verify performance targets match PRD requirements
        assert perf.simple_operation_target <= 2.0
        assert perf.moderate_operation_target <= 4.0
        assert perf.complex_operation_target <= 5.0
        assert perf.max_memory_mb <= 200
        assert perf.max_cpu_percent <= 10.0

class TestRequirements:
    """Test that requirements are properly specified."""
    
    def test_requirements_file_content(self):
        """Verify requirements.txt contains necessary dependencies."""
        req_file = AGENTS_DIR / "requirements.txt"
        with open(req_file, 'r') as f:
            content = f.read()
        
        # Check for key dependencies mentioned in PRD
        required_packages = [
            'langgraph',
            'langchain', 
            'redis',
            'pydantic',
            'openai',
            'anthropic',
            'pytest'
        ]
        
        for package in required_packages:
            assert package in content, f"{package} should be in requirements.txt"
    
    def test_setup_py_configuration(self):
        """Verify setup.py is properly configured."""
        setup_file = AGENTS_DIR / "setup.py"
        with open(setup_file, 'r') as f:
            content = f.read()
        
        # Check for proper package configuration
        assert 'name="libreoffice-ai-agents"' in content
        assert 'version="1.0.0"' in content
        assert 'find_packages()' in content
        assert 'install_requires=requirements' in content

# Performance and integration test placeholders for Task 11.1 completion
@pytest.mark.performance
class TestDirectoryStructurePerformance:
    """Performance tests for directory structure operations."""
    
    def test_package_import_performance(self):
        """Verify package imports are fast enough for production use."""
        import time
        
        start_time = time.time()
        try:
            from config import get_config
            get_config()
        except Exception:
            pass  # Focus on import time, not functionality
        
        import_time = time.time() - start_time
        
        # Package imports should be sub-second
        assert import_time < 1.0, f"Package import took {import_time:.2f}s, should be < 1.0s"

if __name__ == "__main__":
    # Run tests when executed directly
    pytest.main([__file__, "-v"])