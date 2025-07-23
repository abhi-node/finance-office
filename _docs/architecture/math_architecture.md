# LibreOffice Math Architecture

## Overview

LibreOffice Math (starmath module) is a mathematical formula editor that enables creation, editing, and rendering of mathematical expressions and equations. The module implements a sophisticated architecture encompassing formula parsing, Abstract Syntax Tree (AST) construction, layout calculation, and high-quality rendering. Math integrates seamlessly with LibreOffice's document framework while providing specialized mathematical notation capabilities.

## High-Level Architecture

Math follows a **multi-layered architectural pattern** with clear separation of concerns:

```
┌─────────────────────────────────────┐
│          UNO API Layer              │  ← Service interfaces (external access)
├─────────────────────────────────────┤
│          UI Framework Layer         │  ← ViewShell, dialogs, sidebar panels
├─────────────────────────────────────┤
│          Document Model Layer       │  ← SmDocShell, document lifecycle
├─────────────────────────────────────┤
│          Formula Processing Layer   │  ← Parser, AST, visitor pattern
├─────────────────────────────────────┤
│          Rendering Engine Layer     │  ← Layout calculation, drawing visitors
├─────────────────────────────────────┤
│          Import/Export Layer        │  ← MathML, OOXML, RTF, MathType filters
└─────────────────────────────────────┘
```

## Core Architecture Components

### 1. Document Model (SmDocShell and Formula Management)

**Location**: `starmath/inc/document.hxx`, `starmath/source/document.cxx`

The document model serves as the central hub for formula data and lifecycle management.

#### SmDocShell - Document Controller
- **Role**: Main document class inheriting from `SfxObjectShell`
- **Architecture**: Integrates with LibreOffice's document framework
- **Key Responsibilities**:
  - Formula text storage and management
  - Parse tree lifecycle (`mpTree` - SmTableNode root)
  - Format settings and configuration (`maFormat` - SmFormat)
  - Import/export coordination
  - Printing and rendering orchestration
  - Undo/redo functionality

#### Formula Storage Architecture
```cpp
class SmDocShell : public SfxObjectShell {
    OUString            maText;         // Source formula text
    std::unique_ptr<SmTableNode> mpTree; // Parsed AST root
    SmFormat            maFormat;       // Rendering configuration
    SmParser5*          mpParser;       // Formula parser instance
    // ...
};
```

### 2. Formula Processing Architecture

#### 2.1 Tokenization System

**Location**: `starmath/inc/token.hxx`

The tokenization system recognizes over 150 mathematical token types:
- **Operations**: `TPLUS`, `TMINUS`, `TMULTIPLY`, `TDIVIDEBY`
- **Relations**: `TGT`, `TLT`, `TGE`, `TLE`, `TASSIGN`, `TNEQ`
- **Functions**: `TSIN`, `TCOS`, `TLN`, `TLOG`, `TEXP`
- **Structures**: `TMATRIX`, `TFRAC`, `TSTACK`, `TBINOM`
- **Brackets**: `TLPARENT`, `TRPARENT`, `TLBRACE`, `TRBRACE`

**Token Structure**:
```cpp
struct SmToken {
    SmTokenType eType;      // Token identifier
    OUString    aText;      // Source text
    OUString    cMathChar;  // Mathematical character
    TG          nGroup;     // Token group (operators, relations, etc.)
    sal_uInt16  nLevel;     // Precedence level
};
```

#### 2.2 Parser Architecture

**Location**: `starmath/inc/parsebase.hxx`, `starmath/source/parse5.cxx`

Math implements a **recursive descent parser** with formal grammar rules:

**Grammar Hierarchy**:
```cpp
DoTable()           // Root: multiple lines/expressions
  DoLine()          // Single line of mathematics
    DoExpression()  // Expression with alignment
      DoRelation()  // Relational operations (=, <, >, etc.)
        DoSum()     // Addition/subtraction operations
          DoProduct() // Multiplication/division operations
            DoPower() // Exponentiation and sub/superscripts
              DoTerm() // Basic terms, functions, brackets
```

**Abstract Parser Interface**:
```cpp
class AbstractSmParser {
    virtual std::unique_ptr<SmTableNode> Parse(const OUString& rBuffer) = 0;
    virtual std::unique_ptr<SmNode> ParseExpression(const OUString& rBuffer) = 0;
    virtual const SmErrorDesc* NextError() = 0;
    virtual const std::set<OUString>& GetUsedSymbols() const = 0;
};
```

#### 2.3 Abstract Syntax Tree (AST) Architecture

**Location**: `starmath/inc/node.hxx`

The AST uses a comprehensive node hierarchy representing mathematical structures:

```
SmNode (base class)
├── SmStructureNode (containers)
│   ├── SmTableNode (root tables)
│   ├── SmLineNode (single lines)
│   ├── SmBinHorNode (horizontal binary operators)
│   ├── SmBinVerNode (vertical fractions)
│   ├── SmSubSupNode (subscripts/superscripts)
│   ├── SmBraceNode (bracketed expressions)
│   ├── SmMatrixNode (matrices)
│   └── SmOperNode (operators with limits)
└── SmVisibleNode (renderable nodes)
    ├── SmTextNode (text and identifiers)
    ├── SmMathSymbolNode (mathematical symbols)
    ├── SmBlankNode (spacing)
    └── SmSpecialNode (special characters)
```

**Key Node Features**:
- **Parent-Child Relationships**: Structure nodes manage child collections
- **Font and Style Management**: Each node maintains formatting properties
- **Selection Support**: Interactive editing capabilities
- **Token Preservation**: Original source information retained

### 3. Rendering and Layout Engine

#### 3.1 Visitor Pattern Architecture

**Location**: `starmath/inc/visitors.hxx`

Math employs the **visitor pattern** for tree operations:

```cpp
class SmVisitor {
    virtual void Visit(SmTableNode* pNode) = 0;
    virtual void Visit(SmTextNode* pNode) = 0;
    virtual void Visit(SmBraceNode* pNode) = 0;
    // ... visits for all node types
};
```

**Key Visitor Implementations**:
- **SmDrawingVisitor**: Formula rendering to output devices
- **SmCaretPosGraphBuildingVisitor**: Cursor navigation graph construction
- **SmSelectionDrawingVisitor**: Selection highlighting
- **SmNodeToTextVisitor**: AST to text reconstruction
- **SmCloningVisitor**: Deep tree copying

#### 3.2 Layout Calculation System

**Two-Phase Layout Process**:

1. **Prepare Phase**: Font and attribute inheritance
   ```cpp
   void SmNode::Prepare(const SmFormat &rFormat, const SmDocShell &rDocShell, int nDepth);
   ```
   - Font face selection based on node type
   - Size calculation with relative scaling
   - Color and style attribute propagation

2. **Arrange Phase**: Position and dimension calculation
   ```cpp
   virtual void SmNode::Arrange(OutputDevice &rDev, const SmFormat &rFormat) = 0;
   ```
   - Recursive arrangement of child nodes
   - Baseline alignment computation
   - Bounding rectangle calculation
   - Spacing application based on format rules

#### 3.3 Positioning Algorithms

**Horizontal Alignment**: 
- Left, center, right alignment within containers
- Operator spacing based on mathematical conventions
- Variable-width spacing for different operator types

**Vertical Alignment**:
- Baseline alignment for horizontal sequences
- Fraction line positioning for vertical divisions
- Sub/superscript positioning with precise offsets
- Matrix row/column alignment

### 4. User Interface Architecture

#### 4.1 View Management System

**Location**: `starmath/inc/view.hxx`

**Window Hierarchy**:
```
SmViewShell (SfxViewShell)
├── SmGraphicWindow (InterimItemWindow)
│   └── SmGraphicWidget (weld::CustomWidgetController) 
│       └── SmGraphicAccessible (accessibility layer)
└── SmCmdBoxWindow (SfxDockingWindow)
    └── SmEditWindow
        └── SmEditTextWindow (WeldEditView)
```

**Key UI Components**:

- **SmViewShell**: Main view controller integrating with SFX2 framework
- **SmGraphicWindow**: Formula display with zoom support (25% - 800%)
- **SmGraphicWidget**: Custom widget handling rendering and interaction
- **SmEditWindow**: Formula text input with syntax highlighting
- **SmCmdBoxWindow**: Docking window for text editing interface

#### 4.2 Dialog System

**Location**: `starmath/inc/dialog.hxx`

**Configuration Dialogs**:
- **SmFontDialog**: Font selection with preview
- **SmFontSizeDialog**: Size configuration for different element types  
- **SmFontTypeDialog**: Font type assignments per element category
- **SmDistanceDialog**: Spacing and distance settings
- **SmAlignDialog**: Formula alignment options
- **SmSymbolDialog**: Symbol browser and insertion

#### 4.3 Sidebar Panels

- **SmElementsPanel**: Categorized element insertion interface
- **SmPropertiesPanel**: Quick access to formatting dialogs

### 5. Import/Export Filter Architecture

**Location**: `starmath/source/mathml/`, `starmath/source/`

#### Format Support Matrix

| Format | Import | Export | Implementation |
|--------|--------|--------|---------------|
| **MathML** | ✓ | ✓ | Dedicated subsystem (`mathml/` directory) |
| **OOXML Math** | ✓ | ✓ | `ooxmlexport.cxx`, `ooxmlimport.cxx` |
| **RTF Math** | ✗ | ✓ | `rtfexport.cxx` |
| **MathType** | ✓ | ✗ | `mathtype.cxx` (legacy support) |
| **ODF Math** | ✓ | ✓ | Native format support |

#### MathML Architecture

The MathML subsystem implements comprehensive W3C MathML support:

```
starmath/source/mathml/
├── attribute.hxx/cxx       // MathML attribute handling
├── element.hxx/cxx         // MathML element processing
├── export.hxx/cxx          // MathML export functionality
├── import.hxx/cxx          // MathML import functionality
└── iterator.hxx/cxx        // MathML tree iteration
```

**Key Features**:
- Element-by-element processing with proper nesting
- Attribute preservation and mapping
- Semantic vs. presentational MathML support
- Error handling for malformed markup

### 6. External Module Dependencies

#### Framework Integration Dependencies

**SFX2 Framework**:
- Document model integration (`SfxObjectShell`, `SfxViewShell`)
- Request/dispatch system for command handling
- Printer integration and print support
- Application framework coordination

**VCL Graphics Framework**:
- Core rendering through `OutputDevice`
- Font handling and text metrics (`vcl/font.hxx`)
- Event handling (`vcl/event.hxx`)
- Widget toolkit integration (Welded UI)

**EditEngine Integration**:
- Text editing functionality (`editeng/editeng.hxx`)
- Specialized math edit engine (`smediteng.hxx/cxx`)
- Undo support and edit operations

#### External Library Dependencies

- **ICU**: Unicode and internationalization support
- **Boost**: Header-only utilities for modern C++ features
- **SAX**: XML parsing for MathML and OOXML processing

### 7. UNO Service Architecture

**Service Registrations** (from `sm.component`):
- `com.sun.star.formula.FormulaProperties` - Core formula document
- `com.sun.star.xml.XMLExportFilter` - Multiple XML export services
- `com.sun.star.xml.XMLImportFilter` - Multiple XML import services
- `com.sun.star.document.ImportFilter` - MathType filter
- `com.sun.star.ui.UIElementFactory` - Sidebar panel factory

**UNO Interface Implementation**:
- `XServiceInfo` for service identification
- `XRenderable` for printing support
- `XAccessible` for accessibility compliance
- `XUIElementFactory` for UI component creation

### 8. Accessibility Architecture

**Location**: `starmath/source/accessibility.hxx`

**SmGraphicAccessible Implementation**:
- **XAccessibleText Interface**: Text reading and navigation for screen readers
- **Focus Management**: Proper focus traversal between components
- **Event Broadcasting**: Accessibility state change notifications
- **Character-Level Navigation**: Fine-grained cursor movement support

**Key Accessibility Features**:
- Formula content exposed as readable text
- Full keyboard navigation support
- Screen reader compatibility
- Proper focus management between edit and graphics components

### 9. Error Handling and Robustness

#### Parse Error Management

**Error Types**: Comprehensive error classification in `SmParseError` enum:
- `UnexpectedChar`, `UnexpectedToken`
- `PoundExpected`, `ColorExpected`
- `ParentMismatch`, `DoubleSubsupscript`

**Recovery Mechanisms**:
- **Graceful Degradation**: Continue parsing after errors when possible
- **Error Node Insertion**: Replace invalid constructs with visible error markers
- **Token Synchronization**: Skip tokens to find valid continuation points
- **Position Tracking**: Precise error location for user feedback

### 10. Performance and Optimization

#### Memory Management
- Smart pointer usage (`std::unique_ptr`) for automatic cleanup
- Visitor pattern eliminates virtual function call overhead
- Node pooling potential for frequent allocations

#### Parsing Efficiency  
- Single-pass parsing with one-token lookahead
- Token table lookup using hash maps
- Recursive descent avoids backtracking overhead

#### Layout Optimization
- Two-phase layout minimizes recomputation
- Caching of arranged formulas until content changes
- Font metrics caching in temporary device

## Design Patterns and Principles

### Key Design Patterns

1. **Model-View-Controller**: Clear separation between document (SmDocShell), views (SmViewShell), and UI controllers
2. **Visitor Pattern**: Extensible tree operations without modifying node classes
3. **Factory Pattern**: UNO service creation and parser instantiation
4. **Strategy Pattern**: Multiple import/export filter implementations
5. **Observer Pattern**: Document change notifications and UI updates
6. **Command Pattern**: SFX2 integration for user actions

### Architectural Principles

- **Separation of Concerns**: Clear boundaries between parsing, layout, rendering, and UI
- **Extensibility**: Plugin architecture for new formats and mathematical notations
- **Standards Compliance**: MathML and accessibility standard adherence
- **Performance**: Efficient parsing and rendering for complex formulas
- **Integration**: Seamless LibreOffice framework participation

## Testing and Quality Assurance

### Testing Framework
**Location**: `starmath/qa/`

- **Unit Tests**: Core functionality testing (`starmath/qa/unit/`)
- **Import/Export Tests**: Format compatibility validation
- **UI Tests**: Automated user interface testing (`starmath/qa/uitest/`)
- **Regression Tests**: Mathematical expression parsing and rendering verification

## Future Enhancement Opportunities

### Potential Improvements

1. **Enhanced Visual Editor**: More intuitive graphical formula construction
2. **Additional Mathematical Notations**: Extended mathematical symbol support
3. **Performance Optimization**: Node pooling and layout caching improvements
4. **MathML 3.0 Support**: Complete W3C MathML 3.0 specification compliance
5. **Collaborative Editing**: Real-time collaborative formula editing

### Extension Points

- **Parser Plugin System**: Support for custom mathematical notations
- **Rendering Backends**: Alternative rendering engines (e.g., LaTeX output)
- **Import Filter Extensions**: Additional mathematical format support
- **Custom Symbol Management**: Enhanced user-defined symbol capabilities

## Conclusion

LibreOffice Math demonstrates sophisticated software architecture for mathematical formula processing. The modular design with clear separation of concerns enables powerful formula editing capabilities while maintaining excellent performance and accessibility. The architecture successfully balances complexity with maintainability through consistent design patterns, comprehensive error handling, and extensive testing.

The system's integration with LibreOffice's framework showcases how specialized mathematical functionality can be seamlessly embedded within a larger office suite while maintaining the flexibility for standalone mathematical authoring workflows. The visitor pattern architecture and extensible filter system provide a solid foundation for future enhancements and mathematical notation extensions.