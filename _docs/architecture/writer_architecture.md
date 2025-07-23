# LibreOffice Writer Architecture

## Overview

LibreOffice Writer (sw module) is a sophisticated word processing application built on a layered, modular architecture. Dating back to November 1990, the current implementation demonstrates mature software engineering principles with clear separation of concerns, extensive format support, and robust testing infrastructure.

## High-Level Architecture

Writer follows a **layered architectural pattern** with five primary layers:

```
┌─────────────────────────────────────┐
│          UNO API Layer              │  ← SwX* classes (external interfaces)
├─────────────────────────────────────┤
│          UI Layer                   │  ← SwView, dialogs, toolbars
├─────────────────────────────────────┤
│          Shell Layer                │  ← SwWrtShell, SwEditShell, SwCursorShell
├─────────────────────────────────────┤
│          Layout Layer               │  ← SwFrame hierarchy (visual representation)
├─────────────────────────────────────┤
│          Document Model Layer       │  ← SwDoc, SwNode hierarchy (content)
└─────────────────────────────────────┘
```

## Core Components

### 1. Document Model (SwDoc and Nodes)

**Location**: `sw/source/core/doc/`, `sw/inc/doc.hxx`

The document model is the foundation of Writer, representing all document content and metadata.

#### SwDoc - Central Document Class
- **Role**: Central repository for all document data and operations
- **Architecture**: Uses the **Manager Pattern** to delegate functionality
- **Key Components**:
  - `std::unique_ptr<SwNodes> m_pNodes` - Document content structure
  - `rtl::Reference<SwAttrPool> mpAttrPool` - Attribute pool for formatting
  - `SwPageDescs m_PageDescs` - Page layout descriptors
  - Manager classes implementing `IDocument*` interfaces

#### SwNode Hierarchy - Content Structure
**Location**: `sw/inc/node.hxx`, `sw/source/core/docnode/`

```cpp
SwNode (base)
├── SwContentNode
│   ├── SwTextNode      // Text paragraphs
│   ├── SwGrfNode       // Images/graphics
│   └── SwOLENode       // Embedded objects
├── SwTableNode         // Tables
├── SwSectionNode       // Document sections
└── SwStartNode/SwEndNode // Structural boundaries
```

**Key Features**:
- **Tree Structure**: Hierarchical content organization
- **Type Safety**: Each node type has specific capabilities
- **Position Tracking**: Efficient navigation and editing operations

#### Manager Classes (IDocument* Implementations)
SwDoc delegates specialized operations to manager classes:

- **DocumentContentOperationsManager**: Content manipulation (insert, delete, move)
- **DocumentLayoutManager**: Layout operations and frame management
- **DocumentFieldsManager**: Field operations and updates
- **DocumentRedlineManager**: Track changes functionality
- **DocumentSettingManager**: Document settings and configuration
- **DocumentUndoManager**: Undo/redo operations

### 2. Layout System (SwFrame Hierarchy)

**Location**: `sw/source/core/layout/`, `sw/source/core/inc/frame.hxx`

The layout system manages the visual representation and positioning of document elements.

#### SwFrame Architecture
```cpp
SwFrame (base layout class)
├── SwLayoutFrame
│   ├── SwRootFrame     // Document root
│   ├── SwPageFrame     // Page layout
│   ├── SwColumnFrame   // Column layout
│   ├── SwBodyFrame     // Page body content
│   └── SwTabFrame      // Table layout
├── SwContentFrame
│   ├── SwTextFrame     // Text content layout
│   └── SwNoTextFrame   // Non-text content
└── SwFlyFrame          // Floating objects
```

**Key Responsibilities**:
- **Geometry Management**: Frame positioning and sizing
- **Layout Calculations**: Automatic reflow and positioning
- **Rendering Coordination**: Paint operations and display
- **Frame Tree Navigation**: Hierarchical layout relationships

### 3. Shell Layer - Editing Operations

**Location**: `sw/inc/viewsh.hxx`, `sw/inc/editsh.hxx`, `sw/source/uibase/inc/wrtsh.hxx`

The shell layer provides different levels of document interaction capabilities.

#### Shell Hierarchy
```cpp
SwViewShell              // Base view operations
└── SwCursorShell        // Cursor management
    └── SwEditShell      // Content editing
        └── SwWrtShell   // Writer-specific operations
```

**Key Responsibilities**:
- **SwViewShell**: Basic view operations, painting, display coordination
- **SwCursorShell**: Cursor positioning, text selection, navigation
- **SwEditShell**: High-level editing operations (insert, delete, format)
- **SwWrtShell**: Writer-specific features (auto-format, smart operations)

### 4. User Interface Layer

**Location**: `sw/source/ui/`, `sw/source/uibase/`, `sw/inc/view.hxx`

#### SwView - Main Application Controller
- **Role**: Primary application view controller
- **Responsibilities**:
  - UI event handling and coordination
  - Toolbar, menu, and dialog management
  - Integration with LibreOffice framework (SfxViewShell)
  - Contains and manages SwWrtShell for document operations

#### Dialog Framework
**Location**: `sw/source/ui/`

Organized by functional areas:
- `chrdlg/`: Character and paragraph formatting dialogs
- `dialog/`: General dialog implementations  
- `fldui/`: Field-related dialogs
- `table/`: Table editing dialogs
- `index/`: Table of contents and index dialogs

**Architecture Pattern**: Model-View-Controller with dialog controllers managing document changes

### 5. Import/Export Filters

**Location**: `sw/source/filter/`, `sw/source/writerfilter/`

Writer supports multiple file formats through a pluggable filter architecture.

#### Filter Organization
```
filter/
├── html/          // HTML import/export
├── xml/           // ODF XML processing
├── rtf/           // RTF format handling
├── ww8/           // Microsoft Word formats (.doc, .docx)
└── writerfilter/  // Advanced OOXML/RTF processing
    ├── dmapper/   // Domain mapping (OOXML → Writer model)
    ├── ooxml/     // OOXML format parsing
    └── rtftok/    // RTF tokenization
```

**Architecture Patterns**:
- **Strategy Pattern**: Different filters for different formats
- **Interpreter Pattern**: OOXML token interpretation
- **Builder Pattern**: Document construction during import

## External Module Dependencies

Writer integrates with other LibreOffice modules through well-defined interfaces:

### Core Dependencies
- **VCL (Visual Component Library)**: UI toolkit for windows, events, rendering
- **SFX2**: Application framework for document management
- **SVX**: Shared drawing and editing components
- **EditEng**: Text editing engine for formatting
- **Tools**: Basic utilities and geometric types
- **CompHelper**: Component helper utilities

### Integration Examples

#### VCL Integration
```cpp
// sw/source/uibase/docvw/edtwin.cxx
class SwEditWin : public vcl::Window  // Inherits VCL window
{
    // Uses VCL for:
    // - Mouse and keyboard event handling
    // - Drawing and painting operations
    // - Dialog management
};
```

#### SFX2 Integration
```cpp
// sw/source/uibase/app/docsh.cxx  
class SwDocShell : public SfxObjectShell  // Inherits SFX2 document shell
{
    // Uses SFX2 for:
    // - Document lifecycle management
    // - Menu and toolbar binding
    // - Print management
};
```

## Design Patterns and Principles

### Key Design Patterns
1. **Manager Pattern**: SwDoc delegates to specialized manager classes
2. **Composite Pattern**: Node and Frame hierarchical structures
3. **Command Pattern**: Shell operations and UI commands
4. **Strategy Pattern**: Filter architecture for different formats
5. **Observer Pattern**: Modification tracking and view updates
6. **Adapter Pattern**: UNO API wrappers (SwX* classes)

### Architectural Principles
- **Separation of Concerns**: Each layer has distinct responsibilities
- **Interface Segregation**: Multiple specialized interfaces rather than monolithic
- **Dependency Inversion**: UI depends on abstractions, not implementations
- **Single Responsibility**: Specialized classes for specific functionality

## Text Processing and Formatting

### Attribute System
**Location**: `sw/inc/swatrset.hxx`, `sw/source/core/attr/`

- **SwAttrPool**: Centralized attribute storage and sharing
- **SfxPoolItem** hierarchy: Individual formatting attributes
- **SwAttrSet**: Collections of attributes for formatting
- **SwpHintsArray**: Text-level attribute storage

### Field System
**Location**: `sw/inc/fldbas.hxx`, `sw/source/core/fields/`

Complex multi-class system for dynamic content:
- **SwFieldIds**: Enumeration of field types
- **SwFieldType**: Shared field type data  
- **SwField**: Core field logic and data
- **SwFormatField**: SfxPoolItem wrapper for formatting
- **SwTextField**: Text attribute containing field

## Testing Framework

**Location**: `sw/qa/`

Comprehensive testing structure:
- **Unit Tests** (`core/`): Direct class testing with private symbol access
- **Integration Tests** (`extras/`): Import/export filter tests with UNO model assertions
- **UI Tests** (`uitest/`): User interface automation tests
- **API Tests** (`unoapi/`): UNO API compliance testing

## Performance and Optimization

### Memory Management
- **Attribute Sharing**: SwAttrPool shares common formatting attributes
- **Node Array Optimization**: Efficient SwNodes array with gap management
- **Layout Caching**: Frame geometry caching for performance

### Incremental Operations  
- **Partial Reformat**: Only affected content reflows during editing
- **Lazy Loading**: UI components loaded on demand
- **Background Operations**: Spell checking, field updates in background threads

## Conclusion

LibreOffice Writer demonstrates sophisticated software architecture with:

- **Clear Layered Structure**: Document model, layout, shells, UI, and API layers
- **Extensible Design**: Plugin architecture for filters and extensions  
- **Robust Integration**: Well-defined interfaces with other LibreOffice modules
- **Mature Engineering**: Comprehensive testing and performance optimization

This architecture enables Writer to handle complex document processing requirements while maintaining code quality and extensibility for future development.