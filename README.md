# LibreOffice AI Writing Assistant
[![Coverity Scan Build Status](https://scan.coverity.com/projects/211/badge.svg)](https://scan.coverity.com/projects/211) [![CII Best Practices](https://bestpractices.coreinfrastructure.org/projects/307/badge)](https://bestpractices.coreinfrastructure.org/projects/307) [![Translation status](https://weblate.documentfoundation.org/widgets/libo_ui-master/-/svg-badge.svg)](https://weblate.documentfoundation.org/engage/libo_ui-master/?utm_source=widget)

<img align="right" width="150" src="https://opensource.org/wp-content/uploads/2009/06/OSIApproved.svg">

**LibreOffice AI Writing Assistant** extends the LibreOffice office suite with sophisticated AI-powered document creation and editing capabilities through a LangGraph multi-agent architecture. This project integrates intelligent agents directly into LibreOffice Writer, providing users with contextual writing assistance, financial document generation, and advanced document manipulation through natural language conversation.

## 🚀 Project Status

**Phase 1: COMPLETED** ✅ - Fully functional chat interface integrated into Writer's sidebar
- Native LibreOffice sidebar panel with professional chat UI
- Multi-line auto-expanding text input with keyboard shortcuts
- Scrollable chat history with word wrapping and message status
- Complete UNO service integration and build system configuration

**Phase 2: IN DEVELOPMENT** 🔄 - LangGraph multi-agent backend integration
- Intelligent routing for simple vs. complex operations (1-2s vs 3-5s response times)
- Six specialized agents for document understanding, content generation, and execution
- Financial data integration with real-time market data APIs
- Professional document formatting and compliance validation

## 🏗️ AI Architecture Overview

The LibreOffice AI Writing Assistant implements a sophisticated multi-agent architecture that provides intelligent document assistance while maintaining LibreOffice's performance and integration standards.

### Core Architecture Components

**1. Native LibreOffice Integration**
- **AIPanel.cxx**: Main sidebar chat interface (500px chat area, 80px input area)
- **AIPanelFactory.cxx**: UNO service factory for panel creation and registration
- **AgentCoordinator.cxx**: Bridge between LibreOffice C++ and Python LangGraph agents *(Phase 2)*

**2. LangGraph Multi-Agent System** *(Phase 2)*
- **DocumentMasterAgent**: Intelligent request routing and workflow orchestration
- **ContextAnalysisAgent**: Document structure analysis and contextual intelligence
- **ContentGenerationAgent**: AI-powered writing assistance and content creation
- **FormattingAgent**: Professional document styling and layout optimization
- **DataIntegrationAgent**: Real-time financial data integration and external APIs
- **ValidationAgent**: Quality assurance and compliance checking
- **ExecutionAgent**: LibreOffice UNO service execution bridge

**3. Intelligent Routing System**
The system automatically routes user requests through optimized workflows:
- **Simple operations** (1-2s): "Create chart" → Context → Formatting → Execution
- **Moderate operations** (2-4s): "Write summary" → Context → Content → Formatting → Validation → Execution  
- **Complex operations** (3-5s): "Financial report" → All agents with parallel processing and external data

### Key Features

**🚀 Performance-Optimized**: Conditional routing ensures simple operations complete in 1-2 seconds while complex financial document generation leverages the full agent network

**📊 Financial Document Specialization**: Integrated with financial APIs (Alpha Vantage, Yahoo Finance) for real-time market data, professional chart generation, and regulatory compliance

**🔧 Native LibreOffice Integration**: Built using established UNO service patterns, ensuring compatibility with existing features like undo/redo, collaboration, and document versioning

**♿ Accessibility-First Design**: Full keyboard navigation, screen reader support, and high-contrast mode compatibility

## The Build Chain and Runtime Baselines

These are the current minimal operating system and compiler versions to
run and compile LibreOffice, also used by the TDF builds:

* Windows:
    * Runtime: Windows 10
    * Build: Cygwin + Visual Studio 2019 version 16.10
* macOS:
    * Runtime: 11
    * Build: 13 or later + Xcode 14.3 or later (using latest version available for a given version of macOS)
* Linux:
    * Runtime: RHEL 8 or CentOS 8 and comparable
    * Build: either GCC 12; or Clang 12 with libstdc++ 10
* iOS (only for LibreOfficeKit):
    * Runtime: 14.5 (only support for newer i devices == 64 bit)
    * Build: Xcode 12.5 and iPhone SDK 14.5
* Android:
    * Build: NDK 27 and SDK 30.0.3
* Emscripten / WASM:
    * Runtime: a browser with SharedMemory support (threads + atomics)
    * Build: Qt 5.15 with Qt supported Emscripten 1.39.8
    * See [README.wasm](static/README.wasm.md)

Java is required for building many parts of LibreOffice. In TDF Wiki article
[Development/Java](https://wiki.documentfoundation.org/Development/Java), the
exact modules that depend on Java are listed.

The baseline for Java is Java Development Kit (JDK) Version 17 or later.

The baseline for Python is version 3.11. It follows the version available
in SUSE Linux Enterprise Desktop and the Maintenance Support version of
Red Hat Enterprise Linux.

If you want to use Clang with the LibreOffice compiler plugins, the minimal
version of Clang is 12.0.1. Since Xcode doesn't provide the compiler plugin
headers, you have to compile your own Clang to use them on macOS.

You can find the TDF configure switches in the `distro-configs/` directory.

To setup your initial build environment on Windows and macOS, we provide
the LibreOffice Development Environment
([LODE](https://wiki.documentfoundation.org/Development/lode)) scripts.

For more information see the build instructions for your platform in the
[TDF wiki](https://wiki.documentfoundation.org/Development/How_to_build).

## 📂 AI Writing Assistant Code Structure

The AI Writing Assistant components are integrated into LibreOffice's existing module structure. Here are the key locations for AI-specific code:

### AI Integration Components (Phase 1 - COMPLETED ✅)

**LibreOffice Writer UI Components**
```
sw/source/ui/sidebar/ai/
├── AIPanel.cxx                    # Main sidebar chat interface
├── AIPanel.hxx                    # Panel header and interface
├── AIPanelFactory.cxx             # UNO service factory for panel creation
├── AIPanelFactory.hxx             # Factory header
├── AITextInput.cxx                # Auto-expanding text input component
├── AITextInput.hxx                # Text input interface
├── ChatHistory.cxx                # Chat history display component
└── ChatHistory.hxx                # Chat history interface

sw/uiconfig/swriter/ui/
└── aipanel.ui                     # GTK UI layout definition (500px history + 80px input)
```

**Configuration and Registration**
```
officecfg/registry/data/org/openoffice/Office/UI/Sidebar.xcu  # AI panel registration
sw/UIConfig_swriter.mk             # UI file build integration
sw/Library_sw.mk                   # AI source files in main library
sw/source/uibase/sidebar/SwPanelFactory.cxx  # Panel factory integration
```

### Agent Backend Components (Phase 2 - IN DEVELOPMENT 🔄)

**LibreOffice C++ Backend Bridge**
```
sw/source/core/ai/
├── AgentCoordinator.cxx           # Main coordinator for LangGraph agents
├── AgentCoordinator.hxx           # Coordinator interface (UNO service)
├── DocumentContext.cxx            # Document state management
├── DocumentContext.hxx            # Context interface
└── operations/
    ├── DocumentOperations.cxx     # UNO service bridge for document manipulation
    ├── DocumentOperations.hxx     # Operations interface
    ├── ContentGenerator.cxx       # Content generation operations
    ├── ContentGenerator.hxx       # Content generation interface
    ├── DataIntegrator.cxx         # External API integration operations
    └── DataIntegrator.hxx         # Data integration interface
```

**LangGraph Multi-Agent System**
```
langgraph-agents/
├── agents/
│   ├── document_master.py         # Intelligent orchestrator with adaptive routing
│   ├── context_analysis.py        # Document understanding agent
│   ├── content_generation.py      # Writing and content creation agent
│   ├── formatting.py              # Document styling and layout agent
│   ├── data_integration.py        # External API integration agent
│   ├── validation.py              # Quality assurance agent
│   └── execution.py               # LibreOffice UNO services operations agent
├── tools/                         # Document manipulation utilities
├── routing/                       # Intelligent routing system
│   ├── complexity_analyzer.py     # Request complexity assessment
│   ├── workflow_router.py         # Dynamic workflow path selection
│   └── performance_optimizer.py   # Response time optimization
├── graph.py                       # LangGraph workflow definition
├── state.py                       # Shared document state schema
├── bridge.py                      # LibreOffice UNO service bridge
└── config.py                      # Configuration management
```

### Key LibreOffice Modules for AI Integration

The AI Writing Assistant leverages these existing LibreOffice modules:

Module    | AI Integration Purpose
----------|--------------------------------------------------------
[sal/](sal)             | System abstraction for cross-platform AI component deployment
[tools/](tools)         | Basic types used in document context analysis and formatting
[vcl/](vcl)             | Widget toolkit for AI chat interface and theme integration
[framework/](framework) | UNO framework for AI panel registration and lifecycle management
[sfx2/](sfx2)           | Document model integration for AI document operations
[sw/](sw/)              | Writer-specific AI integration and document manipulation

### Documentation Structure

**Project Documentation**
```
_docs/feature/
├── overview.md                    # Project vision and implementation status
├── prd.md                         # Product requirements and success criteria
├── agent_architecture.md         # Detailed agent design and routing logic
├── architecture_skeleton.md      # File structure and integration patterns
├── agent_PRD.txt                 # LangGraph system requirements and communication flow
└── diagram.md                    # Complete architecture integration diagrams
```

**Build Integration**
```
sw/util/ai.component               # UNO component registration for AI services
.taskmaster/tasks/tasks.json       # Development task tracking and progress
```

## Rules for #include Directives (C/C++)

Use the `"..."` form if and only if the included file is found next to the
including file. Otherwise, use the `<...>` form. (For further details, see the
mail [Re: C[++]: Normalizing include syntax ("" vs
<>)](https://lists.freedesktop.org/archives/libreoffice/2017-November/078778.html).)

The UNO API include files should consistently use double quotes, for the
benefit of external users of this API.

`loplugin:includeform (compilerplugins/clang/includeform.cxx)` enforces these rules.


## 🚦 Getting Started with AI Writing Assistant Development

### Prerequisites
- LibreOffice development environment ([build instructions](https://wiki.documentfoundation.org/Development/How_to_build))
- Python 3.11+ for LangGraph agent development
- C++ development tools for LibreOffice integration

### Quick Start - Phase 1 (UI Testing)
1. **Build LibreOffice with AI components**:
   ```bash
   # Standard LibreOffice build with AI UI components included
   make clean && make -j$(nproc)
   ```

2. **Test the AI Assistant Panel**:
   - Launch LibreOffice Writer
   - Open the sidebar (View → Sidebar or F5)
   - Look for "AI Assistant" in the sidebar panel tabs
   - Test the chat interface (currently shows development responses)

3. **Verify UI Integration**:
   - Chat history area should be 500px height with scrolling
   - Text input should auto-expand from 80px base height
   - Send button and Enter key should trigger message sending

### Development Workflow - Phase 2 (Agent Integration)

**Current Task Priority**: Task 6 - Create Agent Coordinator Backend Interface

The next critical development step is implementing `AgentCoordinator.cxx` to bridge the existing UI with the future LangGraph agent system. See `.taskmaster/tasks/tasks.json` for detailed task breakdown.

### Documentation Resources

- **[Overview](_docs/feature/overview.md)**: Project vision and current implementation status
- **[PRD](_docs/feature/prd.md)**: Product requirements and success criteria  
- **[Agent Architecture](_docs/feature/agent_architecture.md)**: Detailed agent system design
- **[Architecture Skeleton](_docs/feature/architecture_skeleton.md)**: File structure and integration patterns
- **[Integration Diagram](_docs/feature/diagram.md)**: Complete system communication flow
- **[Agent PRD](_docs/feature/agent_PRD.txt)**: LangGraph system requirements with node connection explanations

### Performance Targets

**Simple Operations** (1-2 seconds): Basic formatting, chart creation, table insertion
**Moderate Operations** (2-4 seconds): Content generation, document styling, text improvement  
**Complex Operations** (3-5 seconds): Financial reports, research integration, multi-step analysis

### Contributing

The AI Writing Assistant follows LibreOffice's established development practices:
- Use existing UNO service patterns for all integrations
- Follow VCL widget conventions for UI components
- Maintain compatibility with LibreOffice's undo/redo, collaboration, and accessibility systems
- Leverage established build system integration patterns

### Testing and Validation

**UI Testing**: Verify chat interface functionality, theme integration, and accessibility compliance
**Integration Testing**: Validate UNO service registration and panel lifecycle management
**Performance Testing**: Ensure response times meet targets across operation complexity levels

## Finding Out More

For LibreOffice development guidance, read the module `README.md` files, consult the [LibreOffice documentation](https://docs.libreoffice.org/), or reach out on the mailing list libreoffice@lists.freedesktop.org (no subscription required) or IRC `#libreoffice-dev` on irc.libera.chat.

For AI Writing Assistant specific questions, refer to the comprehensive documentation in `_docs/feature/` or examine the task breakdown in `.taskmaster/tasks/tasks.json`.

## SAST Tools

[PVS-Studio](https://pvs-studio.com/en/pvs-studio/?utm_source=website&utm_medium=github&utm_campaign=open_source) - static analyzer for C, C++, C#, and Java code.
