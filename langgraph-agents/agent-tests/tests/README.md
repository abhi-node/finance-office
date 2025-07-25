# LangGraph Agents Test Suite

This directory contains comprehensive tests for the LibreOffice AI Writing Assistant LangGraph multi-agent system.

## Test Organization

### 📁 `agents/`
Tests for individual agent implementations:
- `test_content_generation.py` - Basic ContentGenerationAgent tests
- `test_enhanced_content_generation.py` - Advanced content generation capabilities
- `test_context_analysis.py` - Basic ContextAnalysisAgent tests  
- `test_enhanced_context_analysis.py` - Advanced context analysis features
- `test_context_analysis_cache.py` - Context analysis caching tests
- `test_context_analysis_uno.py` - UNO integration tests
- `test_context_uno_simple.py` - Simple UNO integration validation
- `test_document_master.py` - DocumentMasterAgent orchestration tests
- `test_base_functionality.py` - Base agent functionality tests

### 📁 `cache/`
Caching system tests:
- `test_cache_basic.py` - Basic caching functionality
- `test_cache_quick.py` - Quick cache validation tests

### 📁 `llm/`
LLM integration tests:
- `test_llm_basic.py` - Basic LLM client tests
- `test_llm_integration.py` - Advanced LLM integration tests
- `test_context_llm.py` - Context-aware LLM operations

### 📁 `integration/`
System integration tests:
- `test_integration_workflow.py` - Multi-agent integration tests

### 📁 `workflow/`
Complete workflow tests:
- `test_complete_workflow.py` - End-to-end workflow validation

### 📄 `test_config.py`
Configuration and setup tests

## Running Tests

```bash
# Run all tests
cd langgraph-agents/tests
python -m pytest

# Run specific test category
python -m pytest agents/
python -m pytest cache/
python -m pytest llm/
python -m pytest integration/
python -m pytest workflow/

# Run specific test file
python -m pytest agents/test_content_generation.py

# Run with verbose output
python -m pytest -v

# Run individual test methods
python3 agents/test_enhanced_content_generation.py
```

## Test Coverage

The test suite covers:
- ✅ Agent core functionality and capabilities
- ✅ LLM integration and prompt engineering
- ✅ Content generation, rewriting, and enhancement
- ✅ Context analysis and document understanding
- ✅ Caching systems and performance optimization
- ✅ UNO service integration patterns
- ✅ Multi-agent orchestration and coordination
- ✅ Error handling and recovery mechanisms
- ✅ Quality validation and scoring systems

## Test Status

All critical paths have been validated. The test suite ensures:
- Agent initialization and lifecycle management
- LLM client integration and fallback mechanisms
- Content generation quality and appropriateness
- Context analysis accuracy and performance
- Caching effectiveness and TTL management
- Document state management and coordination
- Error recovery and graceful degradation