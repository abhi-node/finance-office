"""
Test Suite Package

This package contains comprehensive tests for the LibreOffice AI Writing 
Assistant LangGraph multi-agent system.

Test Categories:
- Unit Tests: Individual agent logic and state management
- Integration Tests: Agent-to-agent communication and workflows
- Performance Tests: Response time and resource usage validation
- Mock Tests: LibreOffice UNO service interaction simulation
- End-to-End Tests: Complete user request processing flows

Test Structure:
- test_agents/: Tests for individual agent implementations
- test_state/: State management and synchronization tests  
- test_workflow/: LangGraph workflow and routing tests
- test_utils/: Utility function and helper tests
- test_integration/: Cross-component integration tests
- fixtures/: Mock data and test fixtures
- performance/: Performance benchmarking and load tests

The test suite ensures all success criteria are met including
performance targets and integration requirements.
"""

# Test configuration and shared utilities
# from .fixtures import mock_document_context, mock_agent_responses
# from .performance import PerformanceBenchmark
# from .integration_helpers import LibreOfficeMockServer

__all__ = [
    # Will be uncommented as test utilities are implemented
    # 'mock_document_context',
    # 'mock_agent_responses',
    # 'PerformanceBenchmark',
    # 'LibreOfficeMockServer'
]