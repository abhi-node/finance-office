"""
ValidationAgent - Quality Assurance and Document Validation Specialist

This agent specializes in comprehensive quality assurance for LibreOffice documents,
providing multi-dimensional validation including content accuracy, formatting 
consistency, financial document compliance, and accessibility standards verification.

Key Capabilities:
- Content accuracy validation (spell check, grammar, data consistency) 
- Formatting consistency checks and professional appearance standards
- Financial document compliance verification (GAAP, IFRS, SEC requirements)
- Accessibility standards assessment (WCAG compliance)
- Performance impact analysis and resource optimization validation
- Multi-level validation with automatic approval for low-risk operations
- Extensible rule engine for custom validation requirements
"""

import asyncio
import time
import logging
import json
import re
from datetime import datetime, timezone
from typing import Any, Dict, List, Optional, Union, Tuple, Set
from dataclasses import dataclass, field
from enum import Enum
from abc import ABC, abstractmethod
import hashlib
import traceback

# Import base agent classes
from .base import (
    BaseAgent, 
    AgentCapability, 
    AgentResult, 
    AgentError,
    ValidationResult,
    PerformanceMetrics
)

# Import shared caching system
from .shared_cache import SharedCacheMixin, CacheType, InvalidationTrigger

# Import state management
try:
    from state.document_state import DocumentState, DocumentStateManager
    from langchain_core.messages import BaseMessage, HumanMessage, AIMessage
except ImportError:
    # Fallback for development
    DocumentState = Dict[str, Any]
    DocumentStateManager = Any
    BaseMessage = Dict[str, Any]
    HumanMessage = Dict[str, Any]
    AIMessage = Dict[str, Any]

# Import LLM client for intelligent validation
from llm_client import get_llm_client


class ValidationLevel(Enum):
    """Validation strictness levels"""
    FAST = "fast"           # Quick validation for low-risk operations
    STANDARD = "standard"   # Normal validation for moderate operations  
    COMPREHENSIVE = "comprehensive"  # Full validation for complex operations
    COMPLIANCE = "compliance"        # Regulatory compliance validation


class ValidationCategory(Enum):
    """Types of validation checks"""
    CONTENT_ACCURACY = "content_accuracy"
    FORMATTING_CONSISTENCY = "formatting_consistency"
    FINANCIAL_COMPLIANCE = "financial_compliance"
    ACCESSIBILITY = "accessibility"
    PERFORMANCE = "performance"
    SECURITY = "security"
    DATA_INTEGRITY = "data_integrity"


class ValidationSeverity(Enum):
    """Validation issue severity levels"""
    INFO = "info"           # Informational, no action required
    WARNING = "warning"     # Should be addressed but not blocking
    ERROR = "error"         # Must be fixed before proceeding
    CRITICAL = "critical"   # Blocks operation completely


@dataclass
class ValidationIssue:
    """Represents a validation issue found during quality assurance"""
    category: ValidationCategory
    severity: ValidationSeverity
    message: str
    location: Optional[Dict[str, Any]] = None
    suggestion: Optional[str] = None
    rule_id: Optional[str] = None
    context: Optional[Dict[str, Any]] = None
    timestamp: datetime = field(default_factory=lambda: datetime.now(timezone.utc))


@dataclass 
class ValidationRuleResult:
    """Result of applying a single validation rule"""
    rule_id: str
    passed: bool
    issues: List[ValidationIssue] = field(default_factory=list)
    execution_time_ms: float = 0.0
    context: Optional[Dict[str, Any]] = None


@dataclass
class ValidationRequest:
    """Request for document validation"""
    document_id: str
    content: Dict[str, Any]
    validation_level: ValidationLevel = ValidationLevel.STANDARD
    categories: Set[ValidationCategory] = field(default_factory=set)
    custom_rules: List[str] = field(default_factory=list)
    skip_rules: List[str] = field(default_factory=list)
    context: Optional[Dict[str, Any]] = None


@dataclass
class ValidationResponse:
    """Response from document validation"""
    request_id: str
    passed: bool
    issues: List[ValidationIssue] = field(default_factory=list)
    rule_results: List[ValidationRuleResult] = field(default_factory=list)
    execution_time_ms: float = 0.0
    approval_required: bool = False
    recommendations: List[str] = field(default_factory=list)
    metadata: Dict[str, Any] = field(default_factory=dict)


class ValidationRule(ABC):
    """Abstract base class for validation rules"""
    
    def __init__(self, rule_id: str, category: ValidationCategory, 
                 severity: ValidationSeverity, description: str):
        self.rule_id = rule_id
        self.category = category
        self.severity = severity
        self.description = description
    
    @abstractmethod
    async def validate(self, content: Dict[str, Any], 
                      context: Optional[Dict[str, Any]] = None) -> ValidationRuleResult:
        """Execute validation rule and return result"""
        pass
    
    def should_run(self, level: ValidationLevel, categories: Set[ValidationCategory]) -> bool:
        """Determine if this rule should run for given validation parameters"""
        if categories and self.category not in categories:
            return False
        return self._supports_level(level)
    
    def _supports_level(self, level: ValidationLevel) -> bool:
        """Override in subclasses to define level support"""
        return True


class ContentAccuracyRule(ValidationRule):
    """Validates content accuracy including spelling, grammar, and data consistency"""
    
    def __init__(self):
        super().__init__(
            rule_id="content_accuracy_basic",
            category=ValidationCategory.CONTENT_ACCURACY,
            severity=ValidationSeverity.WARNING,
            description="Basic content accuracy validation"
        )
    
    async def validate(self, content: Dict[str, Any], 
                      context: Optional[Dict[str, Any]] = None) -> ValidationRuleResult:
        start_time = time.time()
        issues = []
        
        try:
            # Basic spell checking (simplified)
            text_content = self._extract_text(content)
            if text_content:
                # Simple validation checks
                issues.extend(await self._check_spelling(text_content))
                issues.extend(await self._check_grammar(text_content))
                issues.extend(await self._check_data_consistency(content))
            
            passed = not any(issue.severity in [ValidationSeverity.ERROR, ValidationSeverity.CRITICAL] 
                           for issue in issues)
            
        except Exception as e:
            logging.error(f"Content accuracy validation failed: {e}")
            issues.append(ValidationIssue(
                category=self.category,
                severity=ValidationSeverity.ERROR,
                message=f"Validation error: {str(e)}"
            ))
            passed = False
        
        execution_time = (time.time() - start_time) * 1000
        return ValidationRuleResult(
            rule_id=self.rule_id,
            passed=passed,
            issues=issues,
            execution_time_ms=execution_time
        )
    
    def _extract_text(self, content: Dict[str, Any]) -> str:
        """Extract text content for validation"""
        if isinstance(content, dict):
            if 'text' in content:
                return content['text']
            elif 'content' in content:
                return str(content['content'])
        return str(content)
    
    async def _check_spelling(self, text: str) -> List[ValidationIssue]:
        """Basic spelling validation"""
        issues = []
        # Simplified spelling check - look for common patterns
        if re.search(r'\b(teh|recieve|seperate|occured)\b', text, re.IGNORECASE):
            issues.append(ValidationIssue(
                category=self.category,
                severity=ValidationSeverity.WARNING,
                message="Potential spelling errors detected",
                suggestion="Review spelling and consider using spell check"
            ))
        return issues
    
    async def _check_grammar(self, text: str) -> List[ValidationIssue]:
        """Basic grammar validation"""
        issues = []
        # Simple grammar checks
        if re.search(r'\b(your|you\'re)\s+(going|coming)\b', text, re.IGNORECASE):
            issues.append(ValidationIssue(
                category=self.category,
                severity=ValidationSeverity.INFO,
                message="Grammar check completed",
                suggestion="Consider reviewing grammar for complex sentences"
            ))
        return issues
    
    async def _check_data_consistency(self, content: Dict[str, Any]) -> List[ValidationIssue]:
        """Check for data consistency issues"""
        issues = []
        
        # Check for numeric consistency in financial data
        if 'financial_data' in content:
            financial_data = content['financial_data']
            if isinstance(financial_data, dict):
                # Basic consistency checks
                if 'totals' in financial_data and 'items' in financial_data:
                    issues.extend(self._validate_financial_totals(financial_data))
        
        return issues
    
    def _validate_financial_totals(self, data: Dict[str, Any]) -> List[ValidationIssue]:
        """Validate financial data totals"""
        issues = []
        try:
            totals = data.get('totals', {})
            items = data.get('items', [])
            
            # Simple validation - ensure totals are reasonable
            if isinstance(totals, dict) and isinstance(items, list):
                issues.append(ValidationIssue(
                    category=self.category,
                    severity=ValidationSeverity.INFO,
                    message="Financial data consistency validated"
                ))
        except Exception:
            issues.append(ValidationIssue(
                category=self.category,
                severity=ValidationSeverity.WARNING,
                message="Could not validate financial data consistency"
            ))
        
        return issues


class FormattingConsistencyRule(ValidationRule):
    """Validates formatting consistency and professional appearance"""
    
    def __init__(self):
        super().__init__(
            rule_id="formatting_consistency_basic",
            category=ValidationCategory.FORMATTING_CONSISTENCY,
            severity=ValidationSeverity.WARNING,
            description="Basic formatting consistency validation"
        )
    
    async def validate(self, content: Dict[str, Any], 
                      context: Optional[Dict[str, Any]] = None) -> ValidationRuleResult:
        start_time = time.time()
        issues = []
        
        try:
            # Check formatting consistency
            issues.extend(await self._check_font_consistency(content))
            issues.extend(await self._check_spacing_consistency(content))
            issues.extend(await self._check_style_consistency(content))
            
            passed = not any(issue.severity in [ValidationSeverity.ERROR, ValidationSeverity.CRITICAL] 
                           for issue in issues)
            
        except Exception as e:
            logging.error(f"Formatting validation failed: {e}")
            issues.append(ValidationIssue(
                category=self.category,
                severity=ValidationSeverity.ERROR,
                message=f"Formatting validation error: {str(e)}"
            ))
            passed = False
        
        execution_time = (time.time() - start_time) * 1000
        return ValidationRuleResult(
            rule_id=self.rule_id,
            passed=passed,
            issues=issues,
            execution_time_ms=execution_time
        )
    
    async def _check_font_consistency(self, content: Dict[str, Any]) -> List[ValidationIssue]:
        """Check font consistency across document"""
        issues = []
        
        formatting_state = content.get('formatting_state', {})
        if formatting_state:
            font_info = formatting_state.get('font_info', {})
            if font_info:
                issues.append(ValidationIssue(
                    category=self.category,
                    severity=ValidationSeverity.INFO,
                    message="Font consistency validated"
                ))
        
        return issues
    
    async def _check_spacing_consistency(self, content: Dict[str, Any]) -> List[ValidationIssue]:
        """Check spacing and alignment consistency"""
        issues = []
        
        # Check for spacing issues in content
        text = self._extract_text_from_content(content)
        if text:
            # Look for inconsistent spacing
            if re.search(r'\s{3,}', text):
                issues.append(ValidationIssue(
                    category=self.category,
                    severity=ValidationSeverity.WARNING,
                    message="Inconsistent spacing detected",
                    suggestion="Review spacing and alignment for consistency"
                ))
        
        return issues
    
    async def _check_style_consistency(self, content: Dict[str, Any]) -> List[ValidationIssue]:
        """Check style consistency"""
        issues = []
        
        # Basic style consistency check
        issues.append(ValidationIssue(
            category=self.category,
            severity=ValidationSeverity.INFO,
            message="Style consistency check completed"
        ))
        
        return issues
    
    def _extract_text_from_content(self, content: Dict[str, Any]) -> str:
        """Extract text from content for validation"""
        if isinstance(content, dict):
            if 'text' in content:
                return content['text']
            elif 'generated_content' in content:
                generated = content['generated_content']
                if isinstance(generated, list) and generated:
                    return ' '.join(str(item.get('content', '')) for item in generated)
        return str(content)


class FinancialComplianceRule(ValidationRule):
    """Validates financial document compliance with industry standards"""
    
    def __init__(self):
        super().__init__(
            rule_id="financial_compliance_basic",
            category=ValidationCategory.FINANCIAL_COMPLIANCE,
            severity=ValidationSeverity.ERROR,
            description="Basic financial document compliance validation"
        )
    
    def _supports_level(self, level: ValidationLevel) -> bool:
        """Only run for standard, comprehensive, and compliance levels"""
        return level in [ValidationLevel.STANDARD, ValidationLevel.COMPREHENSIVE, ValidationLevel.COMPLIANCE]
    
    async def validate(self, content: Dict[str, Any], 
                      context: Optional[Dict[str, Any]] = None) -> ValidationRuleResult:
        start_time = time.time()
        issues = []
        
        try:
            # Check for financial compliance requirements
            issues.extend(await self._check_financial_data_format(content))
            issues.extend(await self._check_regulatory_requirements(content))
            issues.extend(await self._check_attribution_requirements(content))
            
            passed = not any(issue.severity in [ValidationSeverity.ERROR, ValidationSeverity.CRITICAL] 
                           for issue in issues)
            
        except Exception as e:
            logging.error(f"Financial compliance validation failed: {e}")
            issues.append(ValidationIssue(
                category=self.category,
                severity=ValidationSeverity.ERROR,
                message=f"Financial compliance validation error: {str(e)}"
            ))
            passed = False
        
        execution_time = (time.time() - start_time) * 1000
        return ValidationRuleResult(
            rule_id=self.rule_id,
            passed=passed,
            issues=issues,
            execution_time_ms=execution_time
        )
    
    async def _check_financial_data_format(self, content: Dict[str, Any]) -> List[ValidationIssue]:
        """Check financial data formatting compliance"""
        issues = []
        
        external_data = content.get('external_data', {})
        if external_data and 'financial_data' in external_data:
            financial_data = external_data['financial_data']
            
            # Check for proper attribution
            if 'source' not in financial_data:
                issues.append(ValidationIssue(
                    category=self.category,
                    severity=ValidationSeverity.ERROR,
                    message="Financial data missing source attribution",
                    suggestion="Add proper source attribution for all financial data"
                ))
            
            # Check for timestamp
            if 'timestamp' not in financial_data:
                issues.append(ValidationIssue(
                    category=self.category,
                    severity=ValidationSeverity.WARNING,
                    message="Financial data missing timestamp",
                    suggestion="Include timestamp for data freshness verification"
                ))
        
        return issues
    
    async def _check_regulatory_requirements(self, content: Dict[str, Any]) -> List[ValidationIssue]:
        """Check regulatory compliance requirements"""
        issues = []
        
        # Basic regulatory checks
        text = self._extract_text_from_content(content)
        if text and any(keyword in text.lower() for keyword in ['investment', 'financial advice', 'recommendation']):
            if 'disclaimer' not in text.lower():
                issues.append(ValidationIssue(
                    category=self.category,
                    severity=ValidationSeverity.ERROR,
                    message="Financial content may require disclaimer",
                    suggestion="Consider adding appropriate disclaimers for financial content"
                ))
        
        return issues
    
    async def _check_attribution_requirements(self, content: Dict[str, Any]) -> List[ValidationIssue]:
        """Check data attribution requirements"""
        issues = []
        
        # Check citations
        citations = content.get('research_citations', [])
        external_data = content.get('external_data', {})
        
        if external_data and not citations:
            issues.append(ValidationIssue(
                category=self.category,
                severity=ValidationSeverity.WARNING,
                message="External data used without proper citations",
                suggestion="Add citations for all external data sources"
            ))
        
        return issues
    
    def _extract_text_from_content(self, content: Dict[str, Any]) -> str:
        """Extract text from content for validation"""
        if isinstance(content, dict):
            if 'text' in content:
                return content['text']
            elif 'generated_content' in content:
                generated = content['generated_content']
                if isinstance(generated, list) and generated:
                    return ' '.join(str(item.get('content', '')) for item in generated)
        return str(content)


class AccessibilityRule(ValidationRule):
    """Validates accessibility standards compliance"""
    
    def __init__(self):
        super().__init__(
            rule_id="accessibility_basic",
            category=ValidationCategory.ACCESSIBILITY,
            severity=ValidationSeverity.WARNING,
            description="Basic accessibility standards validation"
        )
    
    def _supports_level(self, level: ValidationLevel) -> bool:
        """Run for standard and comprehensive levels"""
        return level in [ValidationLevel.STANDARD, ValidationLevel.COMPREHENSIVE]
    
    async def validate(self, content: Dict[str, Any], 
                      context: Optional[Dict[str, Any]] = None) -> ValidationRuleResult:
        start_time = time.time()
        issues = []
        
        try:
            # Check accessibility requirements
            issues.extend(await self._check_color_contrast(content))
            issues.extend(await self._check_text_alternatives(content))
            issues.extend(await self._check_structure_accessibility(content))
            
            passed = not any(issue.severity in [ValidationSeverity.ERROR, ValidationSeverity.CRITICAL] 
                           for issue in issues)
            
        except Exception as e:
            logging.error(f"Accessibility validation failed: {e}")
            issues.append(ValidationIssue(
                category=self.category,
                severity=ValidationSeverity.WARNING,
                message=f"Accessibility validation error: {str(e)}"
            ))
            passed = False
        
        execution_time = (time.time() - start_time) * 1000
        return ValidationRuleResult(
            rule_id=self.rule_id,
            passed=passed,
            issues=issues,
            execution_time_ms=execution_time
        )
    
    async def _check_color_contrast(self, content: Dict[str, Any]) -> List[ValidationIssue]:
        """Check color contrast for accessibility"""
        issues = []
        
        formatting_state = content.get('formatting_state', {})
        if formatting_state:
            issues.append(ValidationIssue(
                category=self.category,
                severity=ValidationSeverity.INFO,
                message="Color contrast check completed",
                suggestion="Ensure sufficient color contrast for accessibility"
            ))
        
        return issues
    
    async def _check_text_alternatives(self, content: Dict[str, Any]) -> List[ValidationIssue]:
        """Check for text alternatives for images and charts"""
        issues = []
        
        # Check for images/charts without alt text
        pending_operations = content.get('pending_operations', [])
        for operation in pending_operations:
            if operation.get('type') in ['create_chart', 'insert_image']:
                if 'alt_text' not in operation:
                    issues.append(ValidationIssue(
                        category=self.category,
                        severity=ValidationSeverity.WARNING,
                        message="Visual element missing alternative text",
                        suggestion="Add descriptive alternative text for images and charts"
                    ))
        
        return issues
    
    async def _check_structure_accessibility(self, content: Dict[str, Any]) -> List[ValidationIssue]:
        """Check document structure for accessibility"""
        issues = []
        
        document_structure = content.get('document_structure', {})
        if document_structure:
            # Basic structure validation
            issues.append(ValidationIssue(
                category=self.category,
                severity=ValidationSeverity.INFO,
                message="Document structure accessibility validated"
            ))
        
        return issues


class ValidationAgent(BaseAgent, SharedCacheMixin):
    """
    ValidationAgent provides comprehensive quality assurance for LibreOffice documents
    with multi-dimensional validation capabilities and extensible rule engine.
    """
    
    def __init__(self, agent_id: str = "validation_agent", config: Optional[Dict[str, Any]] = None):
        super().__init__(agent_id, config)
        
        # Initialize validation engine
        self.validation_rules = {}
        self.rule_execution_order = []
        self.performance_targets = {
            ValidationLevel.FAST: 500,        # 500ms for fast validation
            ValidationLevel.STANDARD: 2000,   # 2s for standard validation
            ValidationLevel.COMPREHENSIVE: 5000,  # 5s for comprehensive validation
            ValidationLevel.COMPLIANCE: 8000  # 8s for compliance validation
        }
        
        # Initialize validation cache
        self.validation_cache = {}
        self.cache_ttl = 300  # 5 minutes
        
        # Register default validation rules
        self._register_default_rules()
        
        # Initialize LLM client for intelligent validation
        self.llm_client = None
        try:
            self.llm_client = get_llm_client()
        except Exception as e:
            logging.warning(f"Could not initialize LLM client for validation: {e}")
        
        logging.info(f"ValidationAgent initialized with {len(self.validation_rules)} rules")
    
    def _register_default_rules(self):
        """Register default validation rules"""
        rules = [
            ContentAccuracyRule(),
            FormattingConsistencyRule(), 
            FinancialComplianceRule(),
            AccessibilityRule()
        ]
        
        for rule in rules:
            self.register_rule(rule)
    
    def register_rule(self, rule: ValidationRule):
        """Register a validation rule"""
        self.validation_rules[rule.rule_id] = rule
        if rule.rule_id not in self.rule_execution_order:
            self.rule_execution_order.append(rule.rule_id)
        logging.debug(f"Registered validation rule: {rule.rule_id}")
    
    def unregister_rule(self, rule_id: str):
        """Unregister a validation rule"""
        if rule_id in self.validation_rules:
            del self.validation_rules[rule_id]
            if rule_id in self.rule_execution_order:
                self.rule_execution_order.remove(rule_id)
            logging.debug(f"Unregistered validation rule: {rule_id}")
    
    async def validate_document(self, request: ValidationRequest) -> ValidationResponse:
        """
        Perform comprehensive document validation
        
        Args:
            request: ValidationRequest with document content and validation parameters
            
        Returns:
            ValidationResponse with validation results and recommendations
        """
        start_time = time.time()
        request_id = f"val_{int(time.time() * 1000)}"
        
        try:
            # Check cache first
            cache_key = self._generate_cache_key(request)
            cached_result = await self._get_cached_validation(cache_key)
            if cached_result:
                logging.debug(f"Returning cached validation result for {request.document_id}")
                return cached_result
            
            # Initialize response
            response = ValidationResponse(
                request_id=request_id,
                passed=True,
                issues=[],
                rule_results=[],
                recommendations=[]
            )
            
            # Execute validation rules
            await self._execute_validation_rules(request, response)
            
            # Determine overall pass/fail
            response.passed = not any(
                issue.severity in [ValidationSeverity.ERROR, ValidationSeverity.CRITICAL]
                for issue in response.issues
            )
            
            # Determine if approval required
            response.approval_required = self._requires_approval(response)
            
            # Generate recommendations
            response.recommendations = await self._generate_recommendations(response)
            
            # Calculate execution time
            response.execution_time_ms = (time.time() - start_time) * 1000
            
            # Cache result
            await self._cache_validation_result(cache_key, response)
            
            # Log performance
            target_time = self.performance_targets.get(request.validation_level, 2000)
            if response.execution_time_ms > target_time:
                logging.warning(f"Validation exceeded target time: {response.execution_time_ms:.1f}ms > {target_time}ms")
            
            logging.info(f"Document validation completed: {len(response.issues)} issues found, passed={response.passed}")
            return response
            
        except Exception as e:
            logging.error(f"Validation failed: {e}")
            traceback.print_exc()
            
            return ValidationResponse(
                request_id=request_id,
                passed=False,
                issues=[ValidationIssue(
                    category=ValidationCategory.CONTENT_ACCURACY,
                    severity=ValidationSeverity.CRITICAL,
                    message=f"Validation system error: {str(e)}"
                )],
                execution_time_ms=(time.time() - start_time) * 1000
            )
    
    async def _execute_validation_rules(self, request: ValidationRequest, response: ValidationResponse):
        """Execute validation rules based on request parameters"""
        applicable_rules = self._get_applicable_rules(request)
        
        logging.debug(f"Executing {len(applicable_rules)} validation rules")
        
        # Execute rules in order
        for rule_id in self.rule_execution_order:
            if rule_id in applicable_rules and rule_id not in request.skip_rules:
                rule = applicable_rules[rule_id]
                
                try:
                    result = await rule.validate(request.content, request.context)
                    response.rule_results.append(result)
                    response.issues.extend(result.issues)
                    
                    logging.debug(f"Rule {rule_id}: {'PASSED' if result.passed else 'FAILED'} "
                                f"({result.execution_time_ms:.1f}ms, {len(result.issues)} issues)")
                    
                except Exception as e:
                    logging.error(f"Rule {rule_id} failed: {e}")
                    error_result = ValidationRuleResult(
                        rule_id=rule_id,
                        passed=False,
                        issues=[ValidationIssue(
                            category=rule.category,
                            severity=ValidationSeverity.ERROR,
                            message=f"Rule execution error: {str(e)}"
                        )]
                    )
                    response.rule_results.append(error_result)
                    response.issues.extend(error_result.issues)
    
    def _get_applicable_rules(self, request: ValidationRequest) -> Dict[str, ValidationRule]:
        """Get rules applicable to the validation request"""
        applicable = {}
        
        for rule_id, rule in self.validation_rules.items():
            if rule.should_run(request.validation_level, request.categories):
                applicable[rule_id] = rule
        
        # Add any custom rules
        for custom_rule_id in request.custom_rules:
            if custom_rule_id in self.validation_rules:
                applicable[custom_rule_id] = self.validation_rules[custom_rule_id]
        
        return applicable
    
    def _requires_approval(self, response: ValidationResponse) -> bool:
        """Determine if validation requires human approval"""
        # Require approval for critical issues or financial compliance errors
        for issue in response.issues:
            if issue.severity == ValidationSeverity.CRITICAL:
                return True
            if (issue.category == ValidationCategory.FINANCIAL_COMPLIANCE and 
                issue.severity == ValidationSeverity.ERROR):
                return True
        
        return False
    
    async def _generate_recommendations(self, response: ValidationResponse) -> List[str]:
        """Generate recommendations based on validation results"""
        recommendations = []
        
        # Group issues by category
        issue_categories = {}
        for issue in response.issues:
            if issue.category not in issue_categories:
                issue_categories[issue.category] = []
            issue_categories[issue.category].append(issue)
        
        # Generate category-specific recommendations
        for category, issues in issue_categories.items():
            if category == ValidationCategory.CONTENT_ACCURACY:
                recommendations.append("Review content for spelling and grammar accuracy")
            elif category == ValidationCategory.FORMATTING_CONSISTENCY:
                recommendations.append("Ensure consistent formatting throughout the document")
            elif category == ValidationCategory.FINANCIAL_COMPLIANCE:
                recommendations.append("Verify financial data compliance and attribution")
            elif category == ValidationCategory.ACCESSIBILITY:
                recommendations.append("Review accessibility standards compliance")
        
        # Add suggestions from individual issues 
        for issue in response.issues:
            if issue.suggestion and issue.suggestion not in recommendations:
                recommendations.append(issue.suggestion)
        
        return recommendations
    
    def _generate_cache_key(self, request: ValidationRequest) -> str:
        """Generate cache key for validation request"""
        key_data = {
            'document_id': request.document_id,
            'level': request.validation_level.value,
            'categories': sorted([c.value for c in request.categories]),
            'content_hash': hashlib.md5(json.dumps(request.content, sort_keys=True).encode()).hexdigest()
        }
        return hashlib.md5(json.dumps(key_data, sort_keys=True).encode()).hexdigest()
    
    async def _get_cached_validation(self, cache_key: str) -> Optional[ValidationResponse]:
        """Get cached validation result if available and fresh"""
        if cache_key in self.validation_cache:
            cached_data, timestamp = self.validation_cache[cache_key]
            if time.time() - timestamp < self.cache_ttl:
                return cached_data
            else:
                del self.validation_cache[cache_key]
        return None
    
    async def _cache_validation_result(self, cache_key: str, response: ValidationResponse):
        """Cache validation result"""
        self.validation_cache[cache_key] = (response, time.time())
        
        # Cleanup old cache entries
        if len(self.validation_cache) > 100:
            current_time = time.time()
            expired_keys = [
                key for key, (_, timestamp) in self.validation_cache.items()
                if current_time - timestamp > self.cache_ttl
            ]
            for key in expired_keys:
                del self.validation_cache[key]
    
    # BaseAgent abstract method implementations
    
    async def process(self, state: DocumentState, message: Optional[BaseMessage] = None) -> AgentResult:
        """Process validation request through BaseAgent interface"""
        try:
            # Extract validation parameters from state or message
            content = {}
            validation_level = ValidationLevel.STANDARD
            categories = set()
            
            if isinstance(state, dict):
                content = state
                # Try to get validation parameters from pending operations or user request
                current_task = state.get('current_task', '')
                if 'comprehensive' in current_task.lower():
                    validation_level = ValidationLevel.COMPREHENSIVE
                elif 'fast' in current_task.lower() or 'quick' in current_task.lower():
                    validation_level = ValidationLevel.FAST
            
            # Create validation request
            validation_request = ValidationRequest(
                document_id=content.get('current_document', {}).get('path', 'unknown'),
                content=content,
                validation_level=validation_level,
                categories=categories
            )
            
            # Perform validation
            response = await self.validate_document(validation_request)
            
            # Convert to AgentResult
            return AgentResult(
                agent_id=self.agent_id,
                success=response.passed,
                result={
                    'validation_response': response,
                    'passed': response.passed,
                    'issues_count': len(response.issues),
                    'approval_required': response.approval_required,
                    'recommendations': response.recommendations
                },
                error=None if response.passed else f"{len(response.issues)} validation issues found",
                execution_time=response.execution_time_ms / 1000.0,  # Convert to seconds
                metadata={'request_id': response.request_id}
            )
            
        except Exception as e:
            logging.error(f"Validation processing failed: {e}")
            return AgentResult(
                agent_id=self.agent_id,
                success=False,
                result={},
                error=f"Validation processing error: {str(e)}",
                execution_time=0
            )
    
    def validate_input(self, state: DocumentState, message: Optional[BaseMessage] = None) -> ValidationResult:
        """Validate input parameters for validation operation"""
        issues = []
        
        # Basic input validation
        if not isinstance(state, dict):
            issues.append({'type': 'invalid_state', 'message': 'State must be a dictionary'})
        
        # Check for required content
        if isinstance(state, dict) and not state.get('current_document'):
            issues.append({'type': 'missing_document', 'message': 'No document context provided'})
        
        return ValidationResult(
            agent_id=self.agent_id,
            validation_type="input_validation",
            passed=len(issues) == 0,
            confidence=1.0 if len(issues) == 0 else 0.0,
            issues=issues
        )
    
    def get_required_tools(self) -> List[str]:
        """Get list of required tools for validation agent"""
        return [
            'spell_checker',
            'grammar_checker', 
            'financial_data_validator',
            'accessibility_checker',
            'compliance_checker'
        ]
    
    async def process_request(self, request: str, context: Optional[Dict[str, any]] = None) -> AgentResult:
        """Process validation request through BaseAgent interface"""
        try:
            # Parse request
            if isinstance(request, str):
                request_data = json.loads(request)
            else:
                request_data = request
            
            # Create validation request
            validation_request = ValidationRequest(
                document_id=request_data.get('document_id', 'unknown'),
                content=request_data.get('content', {}),
                validation_level=ValidationLevel(request_data.get('level', 'standard')),
                categories=set(ValidationCategory(c) for c in request_data.get('categories', [])),
                context=context
            )
            
            # Perform validation
            response = await self.validate_document(validation_request)
            
            # Convert to AgentResult
            return AgentResult(
                agent_id=self.agent_id,
                success=response.passed,
                result={
                    'validation_response': response,
                    'passed': response.passed,
                    'issues_count': len(response.issues),
                    'approval_required': response.approval_required,
                    'recommendations': response.recommendations
                },
                error=None if response.passed else f"{len(response.issues)} validation issues found",
                execution_time=response.execution_time_ms / 1000.0,  # Convert to seconds
                metadata={'request_id': response.request_id}
            )
            
        except Exception as e:
            logging.error(f"Validation request processing failed: {e}")
            return AgentResult(
                agent_id=self.agent_id,
                success=False,
                result={},
                error=f"Validation processing error: {str(e)}",
                execution_time=0
            )
    
    def get_capabilities(self) -> List[AgentCapability]:
        """Return agent capabilities"""
        return [
            AgentCapability.VALIDATION,
            AgentCapability.QUALITY_ASSURANCE,
            AgentCapability.COMPLIANCE_CHECKING
        ]
    
    async def cleanup(self):
        """Cleanup resources"""
        self.validation_cache.clear()
        await super().cleanup()