"""
Test suite for ValidationAgent

This module provides comprehensive tests for the ValidationAgent including
rule engine functionality, validation levels, multi-dimensional validation,
and integration with the BaseAgent framework.
"""

import pytest
import asyncio
import json
import time
from datetime import datetime, timezone
from unittest.mock import Mock, patch

# Import validation agent components
from agents.validation import (
    ValidationAgent, 
    ValidationLevel, 
    ValidationCategory, 
    ValidationSeverity,
    ValidationIssue,
    ValidationRequest,
    ValidationResponse,
    ValidationRule,
    ValidationRuleResult,
    ContentAccuracyRule,
    FormattingConsistencyRule,
    FinancialComplianceRule,
    AccessibilityRule
)

from agents.base import AgentResult, AgentCapability


class TestValidationRule(ValidationRule):
    """Test validation rule for testing purposes"""
    
    def __init__(self, rule_id: str = "test_rule", should_pass: bool = True, execution_time: float = 0.1):
        super().__init__(
            rule_id=rule_id,
            category=ValidationCategory.CONTENT_ACCURACY,
            severity=ValidationSeverity.WARNING,
            description="Test validation rule"
        )
        self.should_pass = should_pass
        self.execution_time = execution_time
    
    async def validate(self, content: dict, context: dict = None) -> ValidationRuleResult:
        # Simulate execution time
        await asyncio.sleep(self.execution_time / 1000.0)
        
        issues = []
        if not self.should_pass:
            issues.append(ValidationIssue(
                category=self.category,
                severity=self.severity,
                message="Test validation failed",
                suggestion="Fix test issue"
            ))
        
        return ValidationRuleResult(
            rule_id=self.rule_id,
            passed=self.should_pass,
            issues=issues,
            execution_time_ms=self.execution_time
        )


class TestValidationAgent:
    """Test suite for ValidationAgent"""
    
    @pytest.fixture
    def agent(self):
        """Create ValidationAgent instance for testing"""
        config = {
            'performance_targets': {
                ValidationLevel.FAST: 500,
                ValidationLevel.STANDARD: 2000,
                ValidationLevel.COMPREHENSIVE: 5000
            }
        }
        return ValidationAgent("test_validation", config)
    
    @pytest.fixture
    def sample_content(self):
        """Sample document content for testing"""
        return {
            'text': 'This is a sample document with some teh content for testing.',
            'document_structure': {
                'paragraphs': 3,
                'tables': 1,
                'charts': 0
            },
            'formatting_state': {
                'font_info': {
                    'font_family': 'Arial',
                    'font_size': 12
                }
            },
            'external_data': {
                'financial_data': {
                    'source': 'Yahoo Finance',
                    'timestamp': '2024-01-01T00:00:00Z',
                    'stock_price': 150.00
                }
            },
            'generated_content': [
                {'content': 'Generated summary text', 'timestamp': '2024-01-01T00:00:00Z'}
            ],
            'pending_operations': [
                {'type': 'create_chart', 'data': {}}
            ]
        }
    
    def test_agent_initialization(self, agent):
        """Test ValidationAgent initialization"""
        assert agent.agent_id == "test_validation"
        assert len(agent.validation_rules) >= 4  # Default rules
        assert ValidationLevel.FAST in agent.performance_targets
        assert agent.cache_ttl == 300
    
    def test_register_rule(self, agent):
        """Test rule registration"""
        initial_count = len(agent.validation_rules)
        test_rule = TestValidationRule("custom_test_rule")
        
        agent.register_rule(test_rule)
        
        assert len(agent.validation_rules) == initial_count + 1
        assert "custom_test_rule" in agent.validation_rules
        assert "custom_test_rule" in agent.rule_execution_order
    
    def test_unregister_rule(self, agent):
        """Test rule unregistration"""
        test_rule = TestValidationRule("temp_rule")
        agent.register_rule(test_rule)
        initial_count = len(agent.validation_rules)
        
        agent.unregister_rule("temp_rule")
        
        assert len(agent.validation_rules) == initial_count - 1
        assert "temp_rule" not in agent.validation_rules
        assert "temp_rule" not in agent.rule_execution_order
    
    @pytest.mark.asyncio
    async def test_fast_validation(self, agent, sample_content):
        """Test fast validation level"""
        request = ValidationRequest(
            document_id="test_doc",
            content=sample_content,
            validation_level=ValidationLevel.FAST
        )
        
        start_time = time.time()
        response = await agent.validate_document(request)
        execution_time = (time.time() - start_time) * 1000
        
        assert isinstance(response, ValidationResponse)
        assert response.request_id.startswith("val_")
        assert execution_time < agent.performance_targets[ValidationLevel.FAST]
        assert len(response.rule_results) > 0
    
    @pytest.mark.asyncio
    async def test_standard_validation(self, agent, sample_content):
        """Test standard validation level"""
        request = ValidationRequest(
            document_id="test_doc",
            content=sample_content,
            validation_level=ValidationLevel.STANDARD
        )
        
        response = await agent.validate_document(request)
        
        assert isinstance(response, ValidationResponse)
        assert response.execution_time_ms < agent.performance_targets[ValidationLevel.STANDARD]
        assert len(response.issues) >= 0
        assert len(response.recommendations) >= 0
    
    @pytest.mark.asyncio
    async def test_comprehensive_validation(self, agent, sample_content):
        """Test comprehensive validation level"""
        request = ValidationRequest(
            document_id="test_doc",
            content=sample_content,
            validation_level=ValidationLevel.COMPREHENSIVE,
            categories={ValidationCategory.CONTENT_ACCURACY, ValidationCategory.ACCESSIBILITY}
        )
        
        response = await agent.validate_document(request)
        
        assert isinstance(response, ValidationResponse)
        assert response.execution_time_ms < agent.performance_targets[ValidationLevel.COMPREHENSIVE]
        assert any(rule.rule_id.startswith("accessibility") for rule in response.rule_results)
    
    @pytest.mark.asyncio
    async def test_validation_with_issues(self, agent, sample_content):
        """Test validation that finds issues"""
        # Add failing rule
        failing_rule = TestValidationRule("failing_rule", should_pass=False)
        failing_rule.severity = ValidationSeverity.ERROR
        agent.register_rule(failing_rule)
        
        request = ValidationRequest(
            document_id="test_doc",
            content=sample_content,
            validation_level=ValidationLevel.STANDARD
        )
        
        response = await agent.validate_document(request)
        
        assert not response.passed  # Should fail due to error-level issue
        assert len(response.issues) > 0
        assert any(issue.severity == ValidationSeverity.ERROR for issue in response.issues)
    
    @pytest.mark.asyncio
    async def test_approval_required_logic(self, agent, sample_content):
        """Test approval requirement logic"""
        # Add critical issue rule
        critical_rule = TestValidationRule("critical_rule", should_pass=False)
        critical_rule.severity = ValidationSeverity.CRITICAL
        agent.register_rule(critical_rule)
        
        request = ValidationRequest(
            document_id="test_doc",
            content=sample_content,
            validation_level=ValidationLevel.STANDARD
        )
        
        response = await agent.validate_document(request)
        
        assert response.approval_required
        assert any(issue.severity == ValidationSeverity.CRITICAL for issue in response.issues)
    
    @pytest.mark.asyncio
    async def test_validation_caching(self, agent, sample_content):
        """Test validation result caching"""
        request = ValidationRequest(
            document_id="test_doc",
            content=sample_content,
            validation_level=ValidationLevel.STANDARD
        )
        
        # First validation
        response1 = await agent.validate_document(request)
        cache_key = agent._generate_cache_key(request)
        
        # Second validation (should be cached)
        start_time = time.time()
        response2 = await agent.validate_document(request)
        cache_time = (time.time() - start_time) * 1000
        
        assert cache_key in agent.validation_cache
        assert cache_time < 50  # Cached response should be very fast
        assert response1.request_id != response2.request_id  # Different request IDs
        assert len(response1.issues) == len(response2.issues)  # Same validation results
    
    @pytest.mark.asyncio
    async def test_custom_rules(self, agent, sample_content):
        """Test custom rule execution"""
        custom_rule = TestValidationRule("custom_validation")
        agent.register_rule(custom_rule)
        
        request = ValidationRequest(
            document_id="test_doc",
            content=sample_content,
            validation_level=ValidationLevel.STANDARD,
            custom_rules=["custom_validation"]
        )
        
        response = await agent.validate_document(request)
        
        assert any(rule.rule_id == "custom_validation" for rule in response.rule_results)
    
    @pytest.mark.asyncio
    async def test_skip_rules(self, agent, sample_content):
        """Test rule skipping functionality"""
        request = ValidationRequest(
            document_id="test_doc",
            content=sample_content,
            validation_level=ValidationLevel.STANDARD,
            skip_rules=["content_accuracy_basic"]
        )
        
        response = await agent.validate_document(request)
        
        assert not any(rule.rule_id == "content_accuracy_basic" for rule in response.rule_results)
    
    @pytest.mark.asyncio
    async def test_agent_capabilities(self, agent):
        """Test agent capabilities"""
        capabilities = agent.get_capabilities()
        
        assert AgentCapability.VALIDATION in capabilities
        assert AgentCapability.QUALITY_ASSURANCE in capabilities
        assert AgentCapability.COMPLIANCE_CHECKING in capabilities
    
    @pytest.mark.asyncio
    async def test_process_request_interface(self, agent, sample_content):
        """Test BaseAgent process_request interface"""
        request_data = {
            'document_id': 'test_doc',
            'content': sample_content,
            'level': 'standard',
            'categories': ['content_accuracy', 'formatting_consistency']
        }
        
        result = await agent.process_request(json.dumps(request_data))
        
        assert isinstance(result, AgentResult)
        assert result.agent_id == agent.agent_id
        assert 'validation_response' in result.result
        assert 'passed' in result.result
        assert 'issues_count' in result.result
    
    @pytest.mark.asyncio
    async def test_error_handling(self, agent):
        """Test error handling in validation"""
        # Test with invalid content
        request = ValidationRequest(
            document_id="test_doc",
            content=None,  # Invalid content
            validation_level=ValidationLevel.STANDARD
        )
        
        response = await agent.validate_document(request)
        
        assert not response.passed
        assert len(response.issues) > 0
        assert any(issue.severity == ValidationSeverity.CRITICAL for issue in response.issues)


class TestValidationRules:
    """Test suite for individual validation rules"""
    
    @pytest.mark.asyncio
    async def test_content_accuracy_rule(self):
        """Test ContentAccuracyRule"""
        rule = ContentAccuracyRule()
        content = {
            'text': 'This has teh word with spelling error.',
            'financial_data': {
                'totals': {'revenue': 1000},
                'items': [{'amount': 500}, {'amount': 500}]
            }
        }
        
        result = await rule.validate(content)
        
        assert isinstance(result, ValidationRuleResult)
        assert result.rule_id == "content_accuracy_basic"
        assert result.execution_time_ms > 0
    
    @pytest.mark.asyncio
    async def test_formatting_consistency_rule(self):
        """Test FormattingConsistencyRule"""
        rule = FormattingConsistencyRule()
        content = {
            'text': 'Text with   inconsistent    spacing.',
            'formatting_state': {
                'font_info': {'font_family': 'Arial'}
            }
        }
        
        result = await rule.validate(content)
        
        assert isinstance(result, ValidationRuleResult)
        assert result.rule_id == "formatting_consistency_basic"
        assert len(result.issues) >= 0
    
    @pytest.mark.asyncio
    async def test_financial_compliance_rule(self):
        """Test FinancialComplianceRule"""
        rule = FinancialComplianceRule()
        content = {
            'external_data': {
                'financial_data': {
                    'stock_price': 150.00
                    # Missing 'source' and 'timestamp'
                }
            },
            'text': 'This is investment advice without disclaimer.'
        }
        
        result = await rule.validate(content)
        
        assert isinstance(result, ValidationRuleResult)
        assert result.rule_id == "financial_compliance_basic"
        assert len(result.issues) > 0  # Should find missing attribution and disclaimer
        assert any(issue.severity == ValidationSeverity.ERROR for issue in result.issues)
    
    @pytest.mark.asyncio
    async def test_accessibility_rule(self):
        """Test AccessibilityRule"""
        rule = AccessibilityRule()
        content = {
            'formatting_state': {'color_scheme': 'dark'},
            'pending_operations': [
                {'type': 'create_chart', 'data': {}}  # Missing alt_text
            ]
        }
        
        result = await rule.validate(content)
        
        assert isinstance(result, ValidationRuleResult)
        assert result.rule_id == "accessibility_basic"
        assert len(result.issues) >= 0
    
    def test_rule_level_support(self):
        """Test rule level support logic"""
        rule = FinancialComplianceRule()
        
        # Should run for standard, comprehensive, and compliance
        assert rule.should_run(ValidationLevel.STANDARD, set())
        assert rule.should_run(ValidationLevel.COMPREHENSIVE, set())
        assert rule.should_run(ValidationLevel.COMPLIANCE, set())
        
        # Should not run for fast
        assert not rule.should_run(ValidationLevel.FAST, set())
    
    def test_rule_category_filtering(self):
        """Test rule category filtering"""
        rule = ContentAccuracyRule()
        
        # Should run when category matches
        assert rule.should_run(
            ValidationLevel.STANDARD, 
            {ValidationCategory.CONTENT_ACCURACY}
        )
        
        # Should not run when category doesn't match
        assert not rule.should_run(
            ValidationLevel.STANDARD,
            {ValidationCategory.FORMATTING_CONSISTENCY}
        )
        
        # Should run when no categories specified (run all)
        assert rule.should_run(ValidationLevel.STANDARD, set())


class TestValidationDataStructures:
    """Test validation data structures"""
    
    def test_validation_issue_creation(self):
        """Test ValidationIssue creation"""
        issue = ValidationIssue(
            category=ValidationCategory.CONTENT_ACCURACY,
            severity=ValidationSeverity.WARNING,
            message="Test issue",
            location={'line': 1, 'column': 5},
            suggestion="Fix the issue"
        )
        
        assert issue.category == ValidationCategory.CONTENT_ACCURACY
        assert issue.severity == ValidationSeverity.WARNING
        assert issue.message == "Test issue"
        assert issue.location == {'line': 1, 'column': 5}
        assert issue.suggestion == "Fix the issue"
        assert isinstance(issue.timestamp, datetime)
    
    def test_validation_request_creation(self):
        """Test ValidationRequest creation"""
        request = ValidationRequest(
            document_id="test_doc",
            content={'text': 'test'},
            validation_level=ValidationLevel.COMPREHENSIVE,
            categories={ValidationCategory.CONTENT_ACCURACY},
            custom_rules=['custom_rule'],
            skip_rules=['skip_rule']
        )
        
        assert request.document_id == "test_doc"
        assert request.validation_level == ValidationLevel.COMPREHENSIVE
        assert ValidationCategory.CONTENT_ACCURACY in request.categories
        assert 'custom_rule' in request.custom_rules
        assert 'skip_rule' in request.skip_rules
    
    def test_validation_response_creation(self):
        """Test ValidationResponse creation"""
        issue = ValidationIssue(
            category=ValidationCategory.CONTENT_ACCURACY,
            severity=ValidationSeverity.WARNING,
            message="Test issue"
        )
        
        response = ValidationResponse(
            request_id="test_request",
            passed=False,
            issues=[issue],
            approval_required=True,
            recommendations=["Fix the issue"]
        )
        
        assert response.request_id == "test_request"
        assert not response.passed
        assert len(response.issues) == 1
        assert response.approval_required
        assert "Fix the issue" in response.recommendations


if __name__ == "__main__":
    pytest.main([__file__, "-v"])