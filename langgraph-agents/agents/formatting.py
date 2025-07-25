"""
FormattingAgent - Document Styling and Layout Specialist

This agent specializes in comprehensive document formatting for LibreOffice documents,
providing sophisticated text formatting, table/chart creation, and layout optimization
capabilities with a focus on professional and financial document standards.

Key Capabilities:
- Comprehensive text and paragraph formatting (fonts, sizes, colors, styles)
- Table creation and formatting with professional styling
- Chart generation and data visualization formatting
- Style sheet management and application
- Layout optimization for various document types and output formats
- Financial document formatting templates and compliance
- Integration with LibreOffice UNO services for direct formatting operations
"""

import asyncio
import time
import logging
import json
from datetime import datetime, timezone
from typing import Any, Dict, List, Optional, Union, Tuple
from dataclasses import dataclass, field
from enum import Enum

# Import base agent classes
from .base import (
    BaseAgent, 
    AgentCapability, 
    AgentResult, 
    AgentError,
    ValidationResult,
    PerformanceMetrics
)

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

# Import shared caching system
from .shared_cache import SharedCacheMixin, CacheType, InvalidationTrigger


class FormattingType(Enum):
    """Types of formatting operations that can be performed."""
    TEXT_FORMAT = "text_format"           # Font, size, color, style
    PARAGRAPH_FORMAT = "paragraph_format" # Alignment, spacing, indentation
    TABLE_FORMAT = "table_format"         # Table creation and styling
    CHART_FORMAT = "chart_format"         # Chart creation and formatting
    STYLE_APPLICATION = "style_application" # Style sheet application
    LAYOUT_OPTIMIZATION = "layout_optimization" # Document layout improvements
    FINANCIAL_TEMPLATE = "financial_template" # Financial document templates


class FormattingComplexity(Enum):
    """Formatting operation complexity levels."""
    SIMPLE = "simple"           # Basic formatting (≤0.5s)
    STANDARD = "standard"       # Professional formatting (≤1.5s)
    COMPLEX = "complex"         # Advanced layout optimization (≤3s)


@dataclass
class FormattingRequest:
    """Request for document formatting operations."""
    request_id: str
    formatting_type: FormattingType
    complexity: FormattingComplexity
    target_element: Optional[str] = None      # Element to format (text range, table, etc.)
    formatting_params: Dict[str, Any] = field(default_factory=dict)
    context: Dict[str, Any] = field(default_factory=dict)
    preserve_content: bool = True             # Whether to preserve existing content
    apply_immediately: bool = True            # Whether to apply formatting immediately


@dataclass
class FormattingOperation:
    """Container for formatting operation results."""
    request_id: str
    formatting_type: FormattingType
    operation_data: Dict[str, Any]            # UNO service operation parameters
    metadata: Dict[str, Any] = field(default_factory=dict)
    preview_data: Optional[str] = None        # Preview of formatting changes
    validation_status: str = "pending"        # pending, approved, rejected
    execution_time_ms: float = 0.0
    success: bool = True
    error_message: Optional[str] = None


class FormattingAgent(SharedCacheMixin, BaseAgent):
    """
    Specialized agent for document styling and layout optimization.
    
    This agent provides comprehensive formatting capabilities ranging from
    simple text styling to complex financial document layout optimization,
    with full integration with LibreOffice UNO services.
    
    Performance targets:
    - Simple formatting: ≤0.5 seconds
    - Standard formatting: ≤1.5 seconds  
    - Complex layout optimization: ≤3 seconds
    """
    
    def __init__(self, 
                 agent_id: str = "formatting_agent",
                 config: Optional[Dict[str, Any]] = None):
        """
        Initialize the FormattingAgent.
        
        Args:
            agent_id: Unique identifier for this agent instance
            config: Optional configuration dictionary
        """
        # Define agent capabilities
        capabilities = [
            AgentCapability.FORMATTING,
            AgentCapability.DOCUMENT_ANALYSIS
        ]
        
        # Default configuration
        default_config = {
            "max_retries": 2,
            "retry_delay": 0.3,
            "enable_caching": True,
            "cache_ttl_seconds": 300,  # 5 minutes for formatting operations
            "performance_targets": {
                FormattingComplexity.SIMPLE: 500,      # ms
                FormattingComplexity.STANDARD: 1500,   # ms
                FormattingComplexity.COMPLEX: 3000     # ms
            },
            "supported_formats": ["odt", "docx", "pdf"],
            "enable_preview": True,
            "financial_templates_enabled": True,
            "accessibility_compliance": True,
            "auto_style_optimization": True
        }
        
        # Merge with provided config
        merged_config = {**default_config, **(config or {})}
        
        # Initialize base agent
        super().__init__(
            agent_id=agent_id,
            capabilities=capabilities,
            config=merged_config
        )
        
        # Formatting configuration
        self.supported_formats = self.config.get("supported_formats", ["odt"])
        self.enable_preview = self.config.get("enable_preview", True)
        self.accessibility_compliance = self.config.get("accessibility_compliance", True)
        self.auto_style_optimization = self.config.get("auto_style_optimization", True)
        
        # Performance targets
        self.performance_targets = self.config.get("performance_targets", {})
        
        # Operation caching for performance
        self.formatting_cache: Dict[str, FormattingOperation] = {}
        self.cache_enabled = self.config.get("enable_caching", True)
        self.cache_ttl_seconds = self.config.get("cache_ttl_seconds", 300)
        
        # UNO service integration (mock for development)
        self.uno_bridge = self._initialize_uno_bridge()
        
        # Financial document templates
        self.financial_templates = self._initialize_financial_templates()
        
        # Style and formatting libraries
        self.style_library = self._initialize_style_library()
        
        # Formatting validation rules
        self.validation_rules = self._initialize_validation_rules()
        
        self.logger.info(f"FormattingAgent {agent_id} initialized with formatting capabilities")
    
    def get_required_tools(self) -> List[str]:
        """
        Get list of tools required by this agent.
        
        Returns:
            List of tool names required for formatting operations
        """
        return [
            "text_formatter",
            "paragraph_styler", 
            "table_creator",
            "chart_generator",
            "style_manager",
            "layout_optimizer",
            "uno_bridge",
            "template_engine"
        ]
    
    async def process(self, 
                     state: DocumentState, 
                     message: Optional[BaseMessage] = None) -> AgentResult:
        """
        Process a formatting request.
        
        Args:
            state: Current document state
            message: Optional message containing formatting request
            
        Returns:
            AgentResult with formatting operations and state updates
        """
        operation_id = f"format_{int(time.time() * 1000)}"
        start_time = time.time()
        
        try:
            # Extract document ID for shared state coordination
            document_id = state.get("current_document", {}).get("path", "unknown_doc")
            
            # Extract formatting request
            formatting_request = self._extract_formatting_request(state, message)
            
            # Validate request
            validation = self._validate_formatting_request(formatting_request)
            if not validation.passed:
                error_messages = [issue.get("message", "") for issue in validation.issues if issue.get("level") == "error"]
                return AgentResult(
                    agent_id=self.agent_id,
                    operation_id=operation_id,
                    success=False,
                    error=f"Invalid formatting request: {'; '.join(error_messages)}",
                    execution_time=time.time() - start_time
                )
            
            # Check shared cache first
            cache_key = self.generate_cache_key(document_id, formatting_request.request_id)
            cached_result = self.get_cached_agent_result(document_id, cache_key)
            if cached_result and self.cache_enabled:
                self.logger.debug(f"Shared cache hit for formatting request {formatting_request.request_id}")
                return cached_result
            
            # Get context from other agents (like ContentGenerationAgent)
            content_context = self.get_other_agent_context(document_id, "content_generation_agent")
            
            # Process formatting based on request type and cross-agent context
            formatting_operation = await self._process_formatting(formatting_request, state, content_context)
            
            # Create agent result with state updates
            agent_result = self._create_agent_result(operation_id, formatting_operation, start_time)
            
            # Cache the result in shared cache
            if self.cache_enabled and formatting_operation.success:
                self.cache_agent_result(document_id, cache_key, agent_result)
            
            # Update shared context with formatting results
            self.update_shared_context(document_id, {
                "last_formatting_operation": {
                    "request_id": formatting_request.request_id,
                    "formatting_type": formatting_request.formatting_type.value,
                    "applied_at": time.time(),
                    "operation_summary": f"{formatting_operation.operation_data.get('operation_type', 'unknown')} applied"
                },
                "formatting_state": "completed",
                "document_styled": True
            })
            
            # Invalidate related cache when formatting is applied
            self.invalidate_related_cache(document_id, InvalidationTrigger.FORMATTING_APPLIED)
            
            return agent_result
            
        except Exception as e:
            execution_time = time.time() - start_time
            error_msg = f"Formatting processing failed: {str(e)}"
            self.logger.error(error_msg, exc_info=True)
            
            return AgentResult(
                agent_id=self.agent_id,
                operation_id=operation_id,
                success=False,
                error=error_msg,
                execution_time=execution_time,
                metadata={"error_type": type(e).__name__}
            )
    
    def validate_input(self, 
                      state: DocumentState, 
                      message: Optional[BaseMessage] = None) -> ValidationResult:
        """
        Validate input for formatting operations.
        
        Args:
            state: Document state to validate
            message: Optional message to validate
            
        Returns:
            ValidationResult indicating if input is valid
        """
        errors = []
        warnings = []
        
        # Validate document state structure
        if not isinstance(state, dict):
            errors.append("Document state must be a dictionary")
        else:
            # Check required fields for formatting
            required_fields = ["current_document"]
            for field in required_fields:
                if field not in state:
                    errors.append(f"Missing required field: {field}")
        
        # Validate message format if provided
        if message:
            if not isinstance(message, dict):
                errors.append("Message must be a dictionary")
            elif "content" not in message:
                warnings.append("Message missing content field")
        
        return ValidationResult(
            agent_id=self.agent_id,
            validation_type="input_validation",
            passed=len(errors) == 0,
            confidence=1.0 if len(errors) == 0 else 0.0,
            issues=[{"level": "error", "message": error} for error in errors] + 
                   [{"level": "warning", "message": warning} for warning in warnings],
            recommendations=warnings,
            metadata={
                "agent": self.agent_id,
                "validation_time": datetime.now(timezone.utc).isoformat()
            }
        )
    
    def _extract_formatting_request(self, 
                                  state: DocumentState, 
                                  message: Optional[BaseMessage]) -> FormattingRequest:
        """Extract formatting request from state and message."""
        request_id = f"fmt_req_{int(time.time() * 1000)}"
        
        # Default request parameters
        formatting_type = FormattingType.TEXT_FORMAT
        complexity = FormattingComplexity.STANDARD
        formatting_params = {}
        
        # Extract request details from message
        if message and isinstance(message, dict):
            content = message.get("content", "")
            
            # Analyze message to determine formatting type and complexity
            formatting_type = self._analyze_formatting_type(content)
            complexity = self._analyze_formatting_complexity(content)
            formatting_params = self._extract_formatting_parameters(content)
        
        # Build context from document state
        context = {
            "document_title": state.get("current_document", {}).get("title", ""),
            "document_path": state.get("current_document", {}).get("path", ""),
            "cursor_position": state.get("cursor_position", {}),
            "selected_text": state.get("selected_text", ""),
            "document_structure": state.get("document_structure", {}),
            "current_section": self._get_current_section(state),
            "document_type": self._infer_document_type(state),
            "existing_formatting": self._analyze_existing_formatting(state)
        }
        
        return FormattingRequest(
            request_id=request_id,
            formatting_type=formatting_type,
            complexity=complexity,
            formatting_params=formatting_params,
            context=context
        )
    
    def _validate_formatting_request(self, request: FormattingRequest) -> ValidationResult:
        """Validate formatting request parameters."""
        errors = []
        warnings = []
        
        # Validate formatting type
        if not isinstance(request.formatting_type, FormattingType):
            errors.append("Invalid formatting type")
        
        # Validate complexity level
        if not isinstance(request.complexity, FormattingComplexity):
            errors.append("Invalid complexity level")
        
        # Validate formatting parameters based on type
        if request.formatting_type == FormattingType.TABLE_FORMAT:
            if "rows" not in request.formatting_params or "cols" not in request.formatting_params:
                warnings.append("Table formatting missing row/column specifications")
        
        elif request.formatting_type == FormattingType.CHART_FORMAT:
            if "chart_type" not in request.formatting_params:
                warnings.append("Chart formatting missing chart type specification")
        
        # Check for conflicting parameters
        if request.preserve_content and request.formatting_type == FormattingType.LAYOUT_OPTIMIZATION:
            warnings.append("Layout optimization may require content modifications")
        
        return ValidationResult(
            agent_id=self.agent_id,
            validation_type="formatting_request_validation",
            passed=len(errors) == 0,
            confidence=1.0 if len(errors) == 0 else 0.5,
            issues=[{"level": "error", "message": error} for error in errors] + 
                   [{"level": "warning", "message": warning} for warning in warnings],
            recommendations=warnings
        )
    
    async def _process_formatting(self, 
                                request: FormattingRequest, 
                                state: DocumentState,
                                content_context: Optional[Dict[str, Any]] = None) -> FormattingOperation:
        """Process formatting request based on type and complexity."""
        start_time = time.time()
        
        try:
            # Route to appropriate formatting method based on type
            if request.formatting_type == FormattingType.TEXT_FORMAT:
                operation_data = await self._process_text_formatting(request, state)
            elif request.formatting_type == FormattingType.PARAGRAPH_FORMAT:
                operation_data = await self._process_paragraph_formatting(request, state)
            elif request.formatting_type == FormattingType.TABLE_FORMAT:
                operation_data = await self._process_table_formatting(request, state)
            elif request.formatting_type == FormattingType.CHART_FORMAT:
                operation_data = await self._process_chart_formatting(request, state)
            elif request.formatting_type == FormattingType.STYLE_APPLICATION:
                operation_data = await self._process_style_application(request, state)
            elif request.formatting_type == FormattingType.LAYOUT_OPTIMIZATION:
                operation_data = await self._process_layout_optimization(request, state)
            elif request.formatting_type == FormattingType.FINANCIAL_TEMPLATE:
                operation_data = await self._process_financial_template(request, state)
            else:
                raise ValueError(f"Unsupported formatting type: {request.formatting_type}")
            
            # Generate preview if enabled
            preview_data = None
            if self.enable_preview:
                preview_data = self._generate_preview(operation_data, request)
            
            operation = FormattingOperation(
                request_id=request.request_id,
                formatting_type=request.formatting_type,
                operation_data=operation_data,
                metadata={
                    "complexity": request.complexity.value,
                    "parameters_used": request.formatting_params,
                    "context_type": request.context.get("document_type", "unknown"),
                    "performance_target": self.performance_targets.get(request.complexity, 1500),
                    "created_timestamp": time.time() * 1000  # Store creation timestamp for caching
                },
                preview_data=preview_data,
                validation_status="approved",
                execution_time_ms=(time.time() - start_time) * 1000,
                success=True
            )
            return operation
            
        except Exception as e:
            return FormattingOperation(
                request_id=request.request_id,
                formatting_type=request.formatting_type,
                operation_data={},
                metadata={"created_timestamp": time.time() * 1000},
                execution_time_ms=(time.time() - start_time) * 1000,
                success=False,
                error_message=str(e)
            )
    
    async def _process_text_formatting(self, 
                                     request: FormattingRequest, 
                                     state: DocumentState) -> Dict[str, Any]:
        """Process text formatting operations."""
        params = request.formatting_params
        
        # Extract formatting specifications
        font_family = params.get("font_family", "Liberation Serif")
        font_size = params.get("font_size", 12)
        font_color = params.get("font_color", "#000000")
        bold = params.get("bold", False)
        italic = params.get("italic", False)
        underline = params.get("underline", False)
        
        # Build UNO service operation
        operation_data = {
            "operation_type": "text_formatting",
            "target_range": request.target_element or "selection",
            "formatting_attributes": {
                "CharFontName": font_family,
                "CharHeight": font_size,
                "CharColor": int(font_color.replace("#", ""), 16),
                "CharWeight": 700 if bold else 400,  # Bold weight
                "CharPosture": 2 if italic else 0,   # Italic posture
                "CharUnderline": 1 if underline else 0
            },
            "uno_service": "text.TextRange",
            "method": "setPropertyValues"
        }
        
        return operation_data
    
    async def _process_paragraph_formatting(self, 
                                          request: FormattingRequest, 
                                          state: DocumentState) -> Dict[str, Any]:
        """Process paragraph formatting operations."""
        params = request.formatting_params
        
        # Extract paragraph specifications
        alignment = params.get("alignment", "left")  # left, center, right, justify
        line_spacing = params.get("line_spacing", 1.0)
        space_before = params.get("space_before", 0)
        space_after = params.get("space_after", 0)
        indent_left = params.get("indent_left", 0)
        indent_right = params.get("indent_right", 0)
        
        # Convert alignment to UNO constants
        alignment_map = {
            "left": 0,    # LEFT
            "center": 1,  # CENTER
            "right": 2,   # RIGHT
            "justify": 3  # BLOCK
        }
        
        operation_data = {
            "operation_type": "paragraph_formatting",
            "target_range": request.target_element or "current_paragraph",
            "formatting_attributes": {
                "ParaAdjust": alignment_map.get(alignment, 0),
                "ParaLineSpacing": {"Mode": 0, "Height": int(line_spacing * 100)},
                "ParaTopMargin": space_before,
                "ParaBottomMargin": space_after,
                "ParaLeftMargin": indent_left,
                "ParaRightMargin": indent_right
            },
            "uno_service": "text.Paragraph",
            "method": "setPropertyValues"
        }
        
        return operation_data
    
    async def _process_table_formatting(self, 
                                      request: FormattingRequest, 
                                      state: DocumentState) -> Dict[str, Any]:
        """Process table creation and formatting."""
        params = request.formatting_params
        
        # Extract table specifications
        rows = params.get("rows", 3)
        cols = params.get("cols", 3)
        data = params.get("data", [])
        header_row = params.get("header_row", True)
        table_style = params.get("table_style", "professional")
        
        # Apply financial document table styling if appropriate
        if state.get("document_type") == "financial_report":
            table_style = "financial_professional"
        
        operation_data = {
            "operation_type": "table_creation",
            "table_dimensions": {"rows": rows, "cols": cols},
            "table_data": data,
            "formatting_attributes": {
                "TableBorder": self._get_table_border_style(table_style),
                "HeaderRowFormat": header_row,
                "CellPadding": 200,  # 2mm padding
                "TableStyle": table_style
            },
            "uno_service": "text.TextTable",
            "method": "insertTable"
        }
        
        return operation_data
    
    async def _process_chart_formatting(self, 
                                      request: FormattingRequest, 
                                      state: DocumentState) -> Dict[str, Any]:
        """Process chart creation and formatting."""
        params = request.formatting_params
        
        # Extract chart specifications
        chart_type = params.get("chart_type", "column")
        chart_data = params.get("chart_data", {})
        chart_title = params.get("chart_title", "")
        width = params.get("width", 10000)  # 10cm
        height = params.get("height", 7000)   # 7cm
        
        operation_data = {
            "operation_type": "chart_creation",
            "chart_specifications": {
                "ChartType": chart_type,
                "Title": chart_title,
                "Width": width,
                "Height": height,
                "Data": chart_data
            },
            "formatting_attributes": {
                "HasLegend": True,
                "HasTitle": bool(chart_title),
                "3DLook": False,
                "ProfessionalStyling": True
            },
            "uno_service": "text.TextEmbeddedObject",
            "method": "insertChart"
        }
        
        return operation_data
    
    async def _process_style_application(self, 
                                       request: FormattingRequest, 
                                       state: DocumentState) -> Dict[str, Any]:
        """Process style sheet application."""
        params = request.formatting_params
        
        style_name = params.get("style_name", "Standard")
        apply_to = params.get("apply_to", "selection")  # selection, paragraph, document
        
        operation_data = {
            "operation_type": "style_application",
            "target_scope": apply_to,
            "style_specifications": {
                "StyleName": style_name,
                "StyleFamily": "ParagraphStyles"
            },
            "uno_service": "style.StyleManager",
            "method": "applyStyle"
        }
        
        return operation_data
    
    async def _process_layout_optimization(self, 
                                         request: FormattingRequest, 
                                         state: DocumentState) -> Dict[str, Any]:
        """Process document layout optimization."""
        params = request.formatting_params
        
        optimization_type = params.get("optimization_type", "general")
        target_format = params.get("target_format", "print")
        
        operation_data = {
            "operation_type": "layout_optimization",
            "optimization_parameters": {
                "OptimizationType": optimization_type,
                "TargetFormat": target_format,
                "PageBreakOptimization": True,
                "ImagePositioning": True,
                "TablePositioning": True
            },
            "uno_service": "document.LayoutManager",
            "method": "optimizeLayout"
        }
        
        return operation_data
    
    async def _process_financial_template(self, 
                                        request: FormattingRequest, 
                                        state: DocumentState) -> Dict[str, Any]:
        """Process financial document template application."""
        params = request.formatting_params
        
        template_type = params.get("template_type", "quarterly_report")
        template_data = self.financial_templates.get(template_type, {})
        
        operation_data = {
            "operation_type": "financial_template_application",
            "template_specifications": {
                "TemplateType": template_type,
                "TemplateData": template_data,
                "ComplianceLevel": "professional"
            },
            "formatting_attributes": template_data.get("formatting", {}),
            "uno_service": "document.TemplateManager",
            "method": "applyFinancialTemplate"
        }
        
        return operation_data
    
    def _analyze_formatting_type(self, content: str) -> FormattingType:
        """Analyze user message to determine formatting type."""
        content_lower = content.lower()
        
        # Formatting type detection patterns
        if any(word in content_lower for word in ["table", "create table", "insert table"]):
            return FormattingType.TABLE_FORMAT
        elif any(word in content_lower for word in ["chart", "graph", "visualization"]):
            return FormattingType.CHART_FORMAT
        elif any(word in content_lower for word in ["style", "apply style", "template"]):
            return FormattingType.STYLE_APPLICATION
        elif any(word in content_lower for word in ["layout", "optimize", "page break"]):
            return FormattingType.LAYOUT_OPTIMIZATION
        elif any(word in content_lower for word in ["financial", "quarterly", "annual report"]):
            return FormattingType.FINANCIAL_TEMPLATE
        elif any(word in content_lower for word in ["paragraph", "alignment", "spacing", "indent"]):
            return FormattingType.PARAGRAPH_FORMAT
        else:
            return FormattingType.TEXT_FORMAT
    
    def _analyze_formatting_complexity(self, content: str) -> FormattingComplexity:
        """Analyze user message to determine formatting complexity."""
        content_lower = content.lower()
        
        # Complexity detection patterns
        if any(word in content_lower for word in ["simple", "basic", "quick", "just"]):
            return FormattingComplexity.SIMPLE
        elif any(word in content_lower for word in ["complex", "advanced", "comprehensive", "optimize"]):
            return FormattingComplexity.COMPLEX
        else:
            return FormattingComplexity.STANDARD
    
    def _extract_formatting_parameters(self, content: str) -> Dict[str, Any]:
        """Extract formatting parameters from user message."""
        params = {}
        content_lower = content.lower()
        
        # Extract common formatting parameters
        if "bold" in content_lower:
            params["bold"] = True
        if "italic" in content_lower:
            params["italic"] = True
        if "underline" in content_lower:
            params["underline"] = True
        
        # Extract alignment
        if "center" in content_lower:
            params["alignment"] = "center"
        elif "right" in content_lower:
            params["alignment"] = "right"
        elif "justify" in content_lower:
            params["alignment"] = "justify"
        
        return params
    
    def _get_current_section(self, state: DocumentState) -> Optional[Dict[str, Any]]:
        """Get current document section based on cursor position."""
        cursor_pos = state.get("cursor_position", {})
        document_structure = state.get("document_structure", {})
        sections = document_structure.get("sections", [])
        
        if not cursor_pos or not sections:
            return None
        
        current_paragraph = cursor_pos.get("paragraph", 0)
        for section in sections:
            if isinstance(section, dict):
                start_para = section.get("start_paragraph", 0)
                end_para = section.get("end_paragraph", float('inf'))
                if start_para <= current_paragraph <= end_para:
                    return section
        
        return None
    
    def _infer_document_type(self, state: DocumentState) -> str:
        """Infer document type from title and content."""
        doc_title = state.get("current_document", {}).get("title", "").lower()
        
        if any(word in doc_title for word in ["financial", "report", "quarterly", "annual"]):
            return "financial_report"
        elif any(word in doc_title for word in ["business", "plan", "strategy"]):
            return "business_plan"
        elif any(word in doc_title for word in ["memo", "memorandum"]):
            return "memorandum"
        elif any(word in doc_title for word in ["proposal", "contract"]):
            return "proposal"
        else:
            return "general_document"
    
    def _analyze_existing_formatting(self, state: DocumentState) -> Dict[str, Any]:
        """Analyze existing document formatting."""
        # Mock implementation - would integrate with UNO services
        return {
            "default_font": "Liberation Serif",
            "default_size": 12,
            "paragraph_styles": ["Standard", "Heading 1", "Heading 2"],
            "character_styles": ["Default Style"],
            "page_styles": ["Standard"]
        }
    
    def _generate_preview(self, operation_data: Dict[str, Any], request: FormattingRequest) -> str:
        """Generate a preview of formatting changes."""
        operation_type = operation_data.get("operation_type", "unknown")
        
        if operation_type == "text_formatting":
            attrs = operation_data.get("formatting_attributes", {})
            return f"Text formatting: {attrs.get('CharFontName', 'default')} {attrs.get('CharHeight', 12)}pt"
        elif operation_type == "table_creation":
            dims = operation_data.get("table_dimensions", {})
            return f"Table: {dims.get('rows', 0)}x{dims.get('cols', 0)}"
        elif operation_type == "chart_creation":
            specs = operation_data.get("chart_specifications", {})
            return f"Chart: {specs.get('ChartType', 'unknown')} - {specs.get('Title', 'Untitled')}"
        else:
            return f"Formatting operation: {operation_type}"
    
    def _check_cache(self, request: FormattingRequest) -> Optional[FormattingOperation]:
        """Check if formatting operation is cached."""
        if not self.cache_enabled:
            return None
        
        cache_key = self._generate_cache_key(request)
        cached_operation = self.formatting_cache.get(cache_key)
        
        if cached_operation:
            # Check TTL using created timestamp
            created_timestamp = cached_operation.metadata.get("created_timestamp", 0)
            age_seconds = (time.time() * 1000 - created_timestamp) / 1000
            if age_seconds < self.cache_ttl_seconds:
                return cached_operation
            else:
                # Remove expired entry
                del self.formatting_cache[cache_key]
        
        return None
    
    def _cache_operation(self, request: FormattingRequest, operation: FormattingOperation):
        """Cache formatting operation."""
        if not self.cache_enabled:
            return
        
        cache_key = self._generate_cache_key(request)
        self.formatting_cache[cache_key] = operation
        
        # Simple cache size management
        if len(self.formatting_cache) > 50:  # Max 50 cached operations
            oldest_key = min(self.formatting_cache.keys(), 
                           key=lambda k: self.formatting_cache[k].metadata.get("created_timestamp", 0))
            del self.formatting_cache[oldest_key]
    
    def _generate_cache_key(self, request: FormattingRequest) -> str:
        """Generate cache key for formatting request."""
        import hashlib
        key_data = {
            "formatting_type": request.formatting_type.value,
            "complexity": request.complexity.value,
            "params": str(sorted(request.formatting_params.items())),
            "target": request.target_element
        }
        return hashlib.md5(str(key_data).encode()).hexdigest()
    
    def _create_agent_result(self, 
                           operation_id: str, 
                           operation: FormattingOperation, 
                           start_time: float,
                           from_cache: bool = False) -> AgentResult:
        """Create AgentResult from FormattingOperation."""
        execution_time = time.time() - start_time
        
        # Prepare state updates
        state_updates = {
            "formatting_operations": [operation.operation_data] if operation.success else [],
            "formatting_status": {
                "request_id": operation.request_id,
                "formatting_type": operation.formatting_type.value,
                "validation_status": operation.validation_status,
                "preview_available": bool(operation.preview_data),
                "success": operation.success,
                "from_cache": from_cache,
                "metadata": operation.metadata
            }
        }
        
        return AgentResult(
            agent_id=self.agent_id,
            operation_id=operation_id,
            success=operation.success,
            result=operation.operation_data if operation.success else None,
            error=operation.error_message,
            execution_time=execution_time,
            state_updates=state_updates,
            metadata={
                "formatting_type": operation.formatting_type.value,
                "performance": {
                    "target_ms": self.performance_targets.get(
                        FormattingComplexity.STANDARD, 1500
                    ),
                    "actual_ms": operation.execution_time_ms,
                    "performance_met": operation.execution_time_ms <= self.performance_targets.get(
                        FormattingComplexity.STANDARD, 1500
                    )
                },
                "cache_used": from_cache,
                "preview_data": operation.preview_data
            }
        )
    
    def _initialize_uno_bridge(self) -> Any:
        """Initialize UNO service bridge (mock for development)."""
        # Mock UNO bridge - would be replaced with actual UNO service integration
        class MockUnoBridge:
            def format_text(self, **kwargs):
                return {"success": True, "operation": "text_formatting"}
            
            def create_table(self, **kwargs):
                return {"success": True, "operation": "table_creation"}
            
            def create_chart(self, **kwargs):
                return {"success": True, "operation": "chart_creation"}
        
        return MockUnoBridge()
    
    def _initialize_financial_templates(self) -> Dict[str, Dict[str, Any]]:
        """Initialize financial document templates."""
        return {
            "quarterly_report": {
                "name": "Quarterly Financial Report",
                "formatting": {
                    "title_style": "Heading 1",
                    "section_style": "Heading 2",
                    "body_style": "Standard",
                    "table_style": "financial_professional"
                },
                "sections": [
                    "Executive Summary",
                    "Financial Highlights", 
                    "Revenue Analysis",
                    "Operational Metrics",
                    "Forward Guidance"
                ]
            },
            "annual_report": {
                "name": "Annual Financial Report",
                "formatting": {
                    "title_style": "Title",
                    "section_style": "Heading 1",
                    "subsection_style": "Heading 2",
                    "body_style": "Standard"
                },
                "sections": [
                    "Letter to Shareholders",
                    "Business Overview",
                    "Financial Performance",
                    "Risk Factors",
                    "Financial Statements"
                ]
            }
        }
    
    def _initialize_style_library(self) -> Dict[str, Dict[str, Any]]:
        """Initialize style library for formatting operations."""
        return {
            "professional": {
                "font_family": "Liberation Serif",
                "font_size": 12,
                "line_spacing": 1.15,
                "paragraph_spacing": 3
            },
            "modern": {
                "font_family": "Liberation Sans",
                "font_size": 11,
                "line_spacing": 1.2,
                "paragraph_spacing": 2
            },
            "financial": {
                "font_family": "Liberation Serif",
                "font_size": 11,
                "line_spacing": 1.0,
                "paragraph_spacing": 2,
                "table_border": True
            }
        }
    
    def _initialize_validation_rules(self) -> Dict[str, Any]:
        """Initialize formatting validation rules."""
        return {
            "accessibility": {
                "min_font_size": 9,
                "min_contrast_ratio": 4.5,
                "heading_hierarchy": True
            },
            "professional": {
                "consistent_fonts": True,
                "appropriate_spacing": True,
                "proper_alignment": True
            },
            "financial": {
                "table_formatting": True,
                "number_alignment": True,
                "professional_colors": True
            }
        }
    
    def _get_table_border_style(self, style_name: str) -> Dict[str, Any]:
        """Get table border style configuration."""
        styles = {
            "professional": {
                "TopBorder": {"LineStyle": 1, "LineWidth": 35},
                "BottomBorder": {"LineStyle": 1, "LineWidth": 35},
                "LeftBorder": {"LineStyle": 1, "LineWidth": 18},
                "RightBorder": {"LineStyle": 1, "LineWidth": 18}
            },
            "financial_professional": {
                "TopBorder": {"LineStyle": 1, "LineWidth": 53},
                "BottomBorder": {"LineStyle": 1, "LineWidth": 53},
                "LeftBorder": {"LineStyle": 1, "LineWidth": 18},
                "RightBorder": {"LineStyle": 1, "LineWidth": 18},
                "HeaderRowBorder": {"LineStyle": 1, "LineWidth": 35}
            }
        }
        
        return styles.get(style_name, styles["professional"])