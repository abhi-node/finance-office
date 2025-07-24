# LibreOffice AI Agent Architecture Skeleton

## Project Structure Overview

This document defines the complete folder structure and component organization for integrating the LangGraph AI agent system into LibreOffice Writer. The architecture includes both the LangGraph agents system (in root directory) and the LibreOffice integration components, prioritizing simplicity and leveraging existing patterns.

## Root Directory Structure

### LangGraph Agents System - Intelligent Routing Architecture
```
langgraph-agents/
├── agents/
│   ├── __init__.py
│   ├── document_master.py             # Intelligent orchestrator with adaptive routing
│   │   ├── analyze_request_complexity()      # Determines simple/moderate/complex
│   │   ├── route_simple_workflow()           # Direct paths for 1-2s responses
│   │   ├── route_moderate_workflow()         # Focused paths for 2-4s responses
│   │   └── route_complex_workflow()          # Full orchestration for 3-5s responses
│   ├── context_analysis.py           # Document understanding agent
│   │   ├── lightweight_context()             # Quick analysis for simple operations
│   │   ├── focused_analysis()               # Targeted analysis for moderate ops
│   │   └── comprehensive_analysis()         # Full analysis for complex operations
│   ├── content_generation.py         # Writing and content creation agent
│   │   ├── template_content()              # Pre-built responses for simple requests
│   │   ├── adaptive_content()              # Dynamic generation for moderate requests
│   │   └── comprehensive_content()         # Full AI writing for complex requests
│   ├── formatting.py                 # Document styling and layout agent
│   │   ├── default_formatting()            # Template-based formatting (simple)
│   │   ├── adaptive_formatting()           # Context-aware formatting (moderate)
│   │   └── professional_formatting()       # Full styling suite (complex)
│   ├── data_integration.py           # External API integration agent
│   │   ├── cached_data_lookup()            # Fast cached responses
│   │   ├── targeted_api_calls()            # Focused data retrieval
│   │   └── comprehensive_research()        # Multi-source data integration
│   ├── validation.py                 # Quality assurance agent
│   │   ├── fast_validation()               # Basic checks for simple operations
│   │   ├── focused_validation()            # Targeted quality checks
│   │   └── comprehensive_validation()      # Full compliance and quality review
│   └── execution.py                  # LibreOffice UNO services operations agent
│       ├── direct_uno_calls()              # Simple operations bypass
│       ├── coordinated_execution()         # Moderate operation coordination
│       └── complex_operation_manager()     # Full transaction management
├── tools/
│   ├── __init__.py
│   ├── document_tools.py             # Document manipulation utilities
│   ├── formatting_tools.py           # Text and style formatting tools
│   ├── api_tools.py                  # External API connection tools
│   └── validation_tools.py           # Content validation utilities
├── graph.py                          # LangGraph workflow definition with conditional routing
├── state.py                          # Shared document state schema
├── bridge.py                         # LibreOffice UNO service bridge
├── config.py                         # Configuration management with performance tuning
├── routing/                          # Intelligent routing system
│   ├── __init__.py
│   ├── complexity_analyzer.py        # Request complexity assessment
│   ├── workflow_router.py            # Dynamic workflow path selection
│   └── performance_optimizer.py      # Response time optimization
├── requirements.txt                  # Dependencies
└── README.md                         # Documentation
```

## LibreOffice Integration Components (Current Implementation)

### 1. UI Components (✅ IMPLEMENTED)
```
sw/source/ui/sidebar/ai/
├── AIPanel.cxx                    # ✅ Main sidebar panel implementation
├── AIPanel.hxx                    # ✅ Panel header and interface
├── AIPanelFactory.cxx             # ✅ Panel factory for UNO registration
├── AIPanelFactory.hxx             # ✅ Factory header
├── AITextInput.cxx                # ✅ Auto-expanding text input component  
├── AITextInput.hxx                # ✅ Text input header
├── ChatHistory.cxx                # ✅ Chat history display component
└── ChatHistory.hxx                # ✅ Chat history header

sw/uiconfig/swriter/ui/
└── aipanel.ui                     # ✅ GTK UI layout definition
```

**Status**: COMPLETE - Fully functional chat UI with proper LibreOffice integration
**Features**: 
- ✅ Chat history display (500px height, scrollable, word-wrap)
- ✅ Multi-line text input (auto-expanding, 80px base height)
- ✅ Send button with event handling
- ✅ Proper GTK Box layout with spacing and margins
- ✅ Full sidebar integration through SwPanelFactory

### 2. Build System Integration (✅ IMPLEMENTED)
```
sw/UIConfig_swriter.mk             # ✅ UI file registration for aipanel.ui
sw/Library_sw.mk                   # ✅ AI source files added to main library
sw/source/uibase/sidebar/SwPanelFactory.cxx  # ✅ AI panel factory integration
```

**Status**: COMPLETE - All AI components properly integrated into LibreOffice build
**Integration**: 
- ✅ aipanel.ui properly packaged in soffice.cfg/modules/swriter/ui/
- ✅ AI components built into main sw library (resolved linking issues)
- ✅ Panel factory creates AI panels through established patterns

### 3. Configuration and Registry (✅ IMPLEMENTED)
```
officecfg/registry/data/org/openoffice/Office/UI/Sidebar.xcu  # ✅ AI panel registration
```

**Status**: COMPLETE - AI panel appears in Writer sidebar
**Features**:
- ✅ AIDeck defined with "AI Assistant" title
- ✅ AIPanel configured with proper resource URL
- ✅ Context rules ensure panel only appears in Writer
- ✅ Proper panel ordering and display properties

### 4. AI Agent Core Services (🔄 PENDING - PHASE 2)
```
sw/source/core/ai/                 # 🔄 To be implemented
├── AgentCoordinator.cxx           # 🔄 Main coordinator for LangGraph agents
├── AgentCoordinator.hxx           # 🔄 Coordinator interface
├── DocumentContext.cxx            # 🔄 Document state management
├── DocumentContext.hxx            # 🔄 Context interface
└── AgentTypes.hxx                 # 🔄 Agent type definitions and enums
```

**Purpose**: Core AI functionality isolated from UI and document model
**Integration**: Interfaces with existing `SwDoc` and `SwWrtShell` through established patterns

### 5. Document Operation Bridge (🔄 PENDING - PHASE 2)
```
sw/source/core/ai/operations/      # 🔄 To be implemented
├── DocumentOperations.cxx         # 🔄 UNO service bridge for document manipulation
├── DocumentOperations.hxx         # 🔄 Operations interface
├── ContentGenerator.cxx           # 🔄 Content generation operations
├── ContentGenerator.hxx           # 🔄 Content generation interface
├── DataIntegrator.cxx             # 🔄 External API integration operations
└── DataIntegrator.hxx             # 🔄 Data integration interface
```

**Purpose**: Bridge between AI agents and LibreOffice document operations
**Integration**: Uses existing `SwEditShell` and `SwTextNode` manipulation interfaces

### 6. Component Registration (🔄 PENDING - PHASE 2)
```
sw/util/ai.component               # 🔄 UNO component registration for AI services
```

**Purpose**: Registers AI services with LibreOffice service manager
**Integration**: Follows existing component registration patterns in `sw/util/sw.component`

## Component Dependencies

### Existing LibreOffice Components
- **SwDoc**: Document model access through established interfaces
- **SwWrtShell**: Document editing operations through existing shell layer
- **SwPanelFactory**: Sidebar panel creation using existing factory pattern
- **VCL Framework**: UI components and event handling
- **ConfigMgr**: Configuration management through existing system

### External Dependencies
- **LangGraph**: AI agent orchestration framework (integrated as external library)
- **HTTP Client**: For external API integration (using existing LibreOffice HTTP infrastructure)
- **JSON Parser**: For API responses (using existing LibreOffice JSON support)

## Service Architecture

### UNO Service Definitions
```cpp
// In AgentCoordinator.hxx
namespace com::sun::star::ai {
    interface XAIAgentCoordinator : XInterface {
        string processUserRequest([in] string request, [in] any documentContext);
        void cancelOperation([in] long operationId);
        sequence<string> getAvailableAgents();
    };
}

// In DocumentOperations.hxx  
namespace com::sun::star::writer::ai {
    interface XDocumentOperations : XInterface {
        void insertText([in] string text, [in] any position);
        void formatRange([in] any range, [in] any formatting);
        any getDocumentContext();
    };
}
```

### Service Registration Pattern
```xml
<!-- In ai.component -->
<component xmlns="http://openoffice.org/2010/uno-components">
  <implementation name="com.sun.star.comp.Writer.AIAgentCoordinator">
    <service name="com.sun.star.ai.AIAgentCoordinator"/>
  </implementation>
  <implementation name="com.sun.star.comp.Writer.AIPanel">
    <service name="com.sun.star.ui.UIElementFactory"/>
  </implementation>
</component>
```

## Integration Strategy

### Phase 1: Foundation (✅ COMPLETED)
```
sw/source/ui/sidebar/ai/
├── AIPanel.cxx                    # ✅ Complete chat UI implementation
├── AIPanel.hxx                    # ✅ Panel interface
├── AIPanelFactory.cxx             # ✅ UNO factory registration  
├── AIPanelFactory.hxx             # ✅ Factory interface
├── AITextInput.cxx                # ✅ Auto-expanding text input
├── AITextInput.hxx                # ✅ Input interface
├── ChatHistory.cxx                # ✅ Chat history display
└── ChatHistory.hxx                # ✅ History interface

sw/uiconfig/swriter/ui/aipanel.ui  # ✅ GTK UI layout
officecfg/registry/data/org/openoffice/Office/UI/Sidebar.xcu  # ✅ Panel registration
```

**Goal**: ✅ ACHIEVED - Working chat interface that can receive user input and display responses
**Status**: Fully functional chat UI with proper LibreOffice integration

### Phase 2: Document Integration with Intelligent Routing
```
sw/source/core/ai/operations/
├── DocumentOperations.cxx         # Multi-complexity document operations
│   ├── executeSimpleOperation()        # Direct UNO calls (1-2s)
│   ├── executeModerateOperation()      # Coordinated operations (2-4s)
│   └── executeComplexOperation()       # Full transaction management (3-5s)
└── DocumentOperations.hxx

sw/source/core/ai/
├── DocumentContext.cxx            # Adaptive document state analysis
│   ├── getLightweightContext()         # Quick context for simple operations
│   ├── getFocusedContext()             # Targeted analysis for moderate operations
│   └── getComprehensiveContext()       # Full analysis for complex operations
├── DocumentContext.hxx
└── PerformanceOptimizer.cxx       # Response time optimization and caching
```

**Goal**: AI provides optimal performance for all operation types through intelligent routing
**Performance Targets**: 
- Simple operations (chart creation, formatting): 1-2 seconds
- Moderate operations (content generation, document styling): 2-4 seconds  
- Complex operations (financial reports, research integration): 3-5 seconds

### Phase 3: Agent Expansion
```
sw/source/core/ai/agents/
├── ContentAgent.cxx               # Content generation agent
├── ContentAgent.hxx
├── FormattingAgent.cxx            # Formatting operations agent
├── FormattingAgent.hxx
├── DataAgent.cxx                  # External data integration agent
└── DataAgent.hxx
```

**Goal**: Specialized agents for different types of document operations

## Key Design Principles

### 1. Leverage Existing Patterns
- Use `SwPanelFactory` for sidebar integration (established in existing panels)
- Follow UNO service registration patterns from existing components
- Utilize `SwEditShell` for document operations (used by all existing editing features)
- Employ existing configuration management through `officecfg`

### 2. Minimal Interface Changes
- Single sidebar panel addition, no menu/toolbar modifications
- Reuse existing VCL components for chat interface
- No changes to core document model classes
- Integration through established event and notification systems

### 3. Clean Separation of Concerns
- AI logic isolated in `sw/source/core/ai/` 
- UI components isolated in `sw/source/ui/sidebar/ai/`
- Document operations go through established UNO service interfaces
- Configuration managed through existing LibreOffice configuration system

### 4. Incremental Development
- Each phase builds on previous without requiring refactoring
- Components can be developed and tested independently
- Fallback mechanisms for when AI services are unavailable
- Optional feature activation through configuration

## File Integration Points

### Existing Files to Modify (Minimal Changes)
```
sw/source/uibase/sidebar/SwPanelFactory.cxx
  + Add AI panel creation case in factory method
  + Register AI panel resource URL

sw/util/sw.component  
  + Add reference to ai.component for service registration

officecfg/registry/data/org/openoffice/Office/UI/Sidebar.xcu
  + Add AI panel deck and panel definitions
```

### Build System Integration
```
sw/Library_sw.mk
  + Add ai/ source directory to build
  + Include LangGraph library dependencies

sw/Library_swui.mk  
  + Add ai sidebar components to UI library
```

## Success Metrics

### Technical Integration
- AI panel appears in Writer sidebar without disrupting existing panels
- AI services register properly with LibreOffice service manager
- Document operations execute through established UNO interfaces
- Configuration integrates seamlessly with existing LibreOffice settings

### Development Efficiency  
- New AI features can be added without modifying existing LibreOffice code
- Components can be unit tested independently using existing test frameworks
- Build system integrates AI components without special handling
- Deployment follows standard LibreOffice extension patterns

### Maintainability
- AI code remains isolated and doesn't create dependencies in core LibreOffice
- Integration points are well-defined and documented
- Removal of AI features doesn't break existing functionality
- Code follows established LibreOffice coding standards and patterns

This skeleton provides the minimal structure needed to integrate sophisticated AI capabilities while maintaining LibreOffice's architectural integrity and development practices.