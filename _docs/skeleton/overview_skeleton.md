# LibreOffice AI Writing Assistant: Project Skeleton Overview

## High-Level Project Structure

The LibreOffice AI Writing Assistant is organized as a new top-level module `/aiagent/` that integrates seamlessly with LibreOffice's existing architecture. The project maintains clear separation of concerns through well-defined component boundaries while following LibreOffice's established conventions.

## Major Component Categories

### Core Agent System (`/source/core/`)
The heart of the AI assistant, containing all LangGraph agent implementations and orchestration logic. This component handles multi-agent coordination, state management, and the bridge between LibreOffice's UNO services and the Python-based LangGraph framework.

**Key Responsibility**: Agent coordination, state management, and UNO-LangGraph bridge implementation

### User Interface Components (`/source/ui/`)
All user-facing interface elements including sidebar panels, configuration dialogs, toolbar controls, and menu integrations. These components provide intuitive access to AI capabilities while maintaining consistency with LibreOffice's design language.

**Key Responsibility**: User interaction, visual feedback, and seamless integration with LibreOffice's existing UI framework

### Tool Integration System (`/source/tools/`)
Comprehensive tool wrappers that enable agents to perform document operations, access external APIs, and analyze content. This layer abstracts complex operations into simple, agent-friendly interfaces.

**Key Responsibility**: Document manipulation, external API integration, and content analysis capabilities

### Python-LangGraph Integration (`/source/python/`)
The Python service layer that implements the actual LangGraph agents and handles the sophisticated multi-agent workflows. This component leverages LangGraph's powerful orchestration capabilities while providing clean interfaces to the C++ LibreOffice integration layer.

**Key Responsibility**: LangGraph agent implementation, Python service interfaces, and advanced AI workflow orchestration

### Configuration Management (`/source/config/`)
Handles all configuration, user preferences, API credentials, and resource management. This component ensures secure, flexible configuration while integrating with LibreOffice's existing settings system.

**Key Responsibility**: Configuration management, credential security, and user preference handling

### User Interface Definitions (`/uiconfig/`)
XML-based UI layout definitions that describe the visual structure of dialogs, panels, menus, and toolbars. These files define the appearance and behavior of all user interface elements.

**Key Responsibility**: UI layout definitions, menu structures, and toolbar configurations

### Build and Registration (`/util/`)
UNO component registration files and build utilities that integrate the AI assistant with LibreOffice's service discovery and build systems.

**Key Responsibility**: Service registration, component discovery, and build system integration

### Quality Assurance (`/qa/`)
Comprehensive testing infrastructure covering unit tests, integration tests, and end-to-end workflow validation to ensure reliability and maintainability.

**Key Responsibility**: Code quality, regression prevention, and system reliability validation

### Resources and Assets (`/res/`)
Static resources including icons, localization strings, document templates, and other assets required for the AI assistant's operation.

**Key Responsibility**: Static assets, internationalization, and template management

## Integration Strategy

The project integrates with LibreOffice through three primary mechanisms:

1. **UNO Service Registration**: All AI services register as standard UNO components, making them discoverable through LibreOffice's service manager
2. **UI Framework Extension**: User interface components extend existing LibreOffice UI frameworks (sidebar, menus, toolbars)
3. **Build System Integration**: The module follows LibreOffice's gbuild conventions for compilation, linking, and distribution

## Component Interaction Flow

```
User Input → UI Components → Core Agent System → Python-LangGraph Agents → Tool Integration → LibreOffice Operations
```

Each component has clearly defined interfaces and responsibilities, enabling independent development, testing, and maintenance while ensuring cohesive system behavior.

## Development and Deployment

The structure supports both integrated development (building alongside LibreOffice) and extension-based deployment (distributable package), providing flexibility for different use cases and deployment scenarios.

For detailed information about specific components, refer to the dedicated skeleton documentation files:
- `core_skeleton.md` - Core agent system implementation details
- `ui_skeleton.md` - User interface component specifications  
- `tools_skeleton.md` - Tool integration system documentation
- `python_skeleton.md` - Python-LangGraph integration details
- `config_skeleton.md` - Configuration management specifications
- `qa_skeleton.md` - Quality assurance and testing structure
- `build_skeleton.md` - Build system and integration details