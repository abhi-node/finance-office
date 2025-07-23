# LibreOffice Calc Architecture

## Overview

LibreOffice Calc (sc module) is a powerful spreadsheet application built on a sophisticated, multi-layered architecture. The design demonstrates mature software engineering principles with clear separation of concerns, comprehensive file format support, advanced formula processing capabilities, and extensive optimization for performance.

## High-Level Architecture

Calc follows a **layered architectural pattern** with five primary layers:

```
┌─────────────────────────────────────┐
│          UNO API Layer              │  ← ScX* classes (external interfaces)
├─────────────────────────────────────┤
│          UI Layer                   │  ← ScTabViewShell, dialogs, toolbars
├─────────────────────────────────────┤
│          Control Layer              │  ← ScDocShell, ScDocFunc
├─────────────────────────────────────┤
│          Filter Layer               │  ← Import/Export filters
├─────────────────────────────────────┤
│          Document Model Layer       │  ← ScDocument, ScTable, ScColumn
└─────────────────────────────────────┘
```

## Core Architecture Components

### 1. Document Model (ScDocument Hierarchy)

**Location**: `sc/source/core/data/`, `sc/inc/document.hxx`

The document model forms the foundation of Calc, representing spreadsheet data and metadata.

#### ScDocument - Central Document Class
- **Role**: Central repository for all spreadsheet data and operations
- **Architecture**: Multi-table container with optimized storage
- **Key Components**:
  - Multiple `ScTable` instances (one per worksheet)
  - Formula calculation engine integration
  - Reference tracking and dependency management
  - Document-wide settings and configuration

#### Data Hierarchy Structure
```cpp
ScDocument
└── ScTable (worksheets)
    └── ScColumn (columns within sheet)
        └── Cell Data (stored in multi-type vectors)
            ├── ScFormulaCell (formula cells)
            ├── ScCellValue (values)
            └── ScRefCellValue (references)
```

#### ScTable - Worksheet Representation
**Location**: `sc/inc/table.hxx`, `sc/source/core/data/table*.cxx`

- **Role**: Represents individual worksheets
- **Key Features**:
  - Contains ScColumn objects (one per column)
  - Manages sheet-specific settings and properties
  - Handles row/column operations
  - Manages print ranges and scenarios

#### ScColumn - Optimized Column Storage
**Location**: `sc/inc/column.hxx`, `sc/source/core/data/column*.cxx`

- **Role**: Manages all cells within a single column
- **Architecture**: Uses Multi-Type Vector (mdds library) for efficient sparse storage
- **Key Features**:
  - `sc::CellStoreType` for cell data storage
  - `ScAttrArray` for cell formatting attributes
  - Compressed arrays for memory efficiency
  - Support for different cell types (empty, numeric, string, formula)

### 2. Formula Processing System

#### ScFormulaCell - Formula Engine
**Location**: `sc/inc/formulacell.hxx`, `sc/source/core/data/formulacell.cxx`

- **Role**: Represents cells containing formulas
- **Key Features**:
  - Formula compilation and interpretation
  - Dependency tracking and listener management
  - Formula groups for vectorized calculations
  - Multi-threaded calculation support
  - OpenCL acceleration integration

#### Formula Compilation Chain
```
Input Formula → ScCompiler → ScTokenArray → ScInterpreter → Result
```

#### ScCompiler - Formula Parser
**Location**: `sc/inc/compiler.hxx`, `sc/source/core/tool/compiler.cxx`

- **Role**: Parses formula strings into executable token arrays
- **Key Features**:
  - Multiple syntax support (A1, R1C1, etc.)
  - Function name internationalization
  - Reference resolution and validation
  - External reference handling

#### Formula Interpretation
**Location**: `sc/source/core/tool/interpr*.cxx` (split by function categories)

- **ScInterpreter**: Executes compiled formulas
- **Function Categories**: Mathematical, statistical, database, text, information, array
- **Performance Features**: 
  - Vectorized operations
  - OpenCL GPU acceleration
  - Formula group optimization

### 3. User Interface Layer

#### ScTabViewShell - Main View Controller
**Location**: `sc/source/ui/inc/tabvwsh.hxx`, `sc/source/ui/view/tabvwsh*.cxx`

- **Role**: Primary application view controller
- **Architecture**: Inherits from `SfxViewShell` (LibreOffice framework)
- **Responsibilities**:
  - User interaction handling
  - Command dispatching
  - View state management
  - Integration with LibreOffice shell architecture

#### ScTabView - Display Management
**Location**: `sc/source/ui/inc/tabview.hxx`

- **Role**: Manages the actual spreadsheet display
- **Features**:
  - Multiple pane support (split windows)
  - Grid rendering and scrolling
  - Selection management
  - Zoom and display options

#### Shell Architecture
```cpp
ScTabViewShell (Controller)
├── ScCellShell        // Cell editing context
├── ScEditShell        // In-cell editing context
├── ScDrawShell        // Drawing object context
├── ScChartShell       // Chart editing context
└── ScPivotShell       // Pivot table context
```

### 4. Document Management (ScDocShell)

**Location**: `sc/source/ui/inc/docsh.hxx`, `sc/source/ui/docshell/docsh*.cxx`

#### ScDocShell - Document Controller
- **Role**: Document lifecycle management and coordination
- **Architecture**: Inherits from `SfxObjectShell`
- **Responsibilities**:
  - File I/O operations
  - Document modification tracking
  - Undo/redo coordination
  - Print management
  - Integration with LibreOffice document framework

#### ScDocFunc - Document Operations
- **Role**: High-level document operation interface
- **Features**:
  - Cell content modification
  - Range operations
  - Sheet management
  - Formula handling
  - Undo support for all operations

### 5. Import/Export Filter Architecture

**Location**: `sc/source/filter/`

Calc supports numerous file formats through a pluggable filter system:

#### Filter Organization
```
filter/
├── excel/          // Microsoft Excel formats (.xls, .xlsx)
│   ├── xe*.cxx     // Export engine
│   ├── xi*.cxx     // Import engine  
│   └── xl*.cxx     // Common infrastructure
├── oox/            // Office Open XML (.xlsx, .xlsm)
├── xml/            // OpenDocument Format (.ods)
├── html/           // HTML import/export
├── lotus/          // Lotus 1-2-3 support
├── dif/            // Data Interchange Format
└── rtf/            // Rich Text Format
```

#### Excel Filter Architecture (Example)
- **ExcelFilter**: Main coordination class
- **WorkbookHelper**: Central helper for OOXML processing
- **Buffer Classes**: Efficient intermediate data structures
- **Fragment Processing**: Handles different OOXML document parts

### 6. Performance Optimization Systems

#### OpenCL Acceleration
**Location**: `sc/source/core/opencl/`

- **GPU Acceleration**: Mathematical operations on graphics cards
- **Function Support**: Extensive function library optimized for parallel processing
- **Automatic Detection**: Runtime GPU capability detection and fallback

#### Formula Groups
- **Vectorized Calculation**: Groups of similar formulas calculated together
- **Performance Benefits**: Significant speedup for large datasets
- **Automatic Grouping**: System automatically detects groupable formulas

#### Memory Optimization
- **Compressed Arrays**: `ScCompressedArray` for sparse data storage
- **Multi-Type Vectors**: Efficient storage for different cell types
- **Attribute Pooling**: Shared storage for common formatting attributes

## External Module Dependencies

Calc integrates with LibreOffice's modular architecture through well-defined interfaces:

### Core Dependencies
- **VCL (Visual Component Library)**: UI toolkit, graphics, event handling
- **SFX2**: Application framework, document management
- **SVX**: Shared drawing and editing components
- **EditEng**: Text editing engine for in-cell editing
- **Tools**: Basic utilities and geometric types
- **CompHelper**: Component helper utilities

### Integration Examples

#### VCL Integration
```cpp
// Grid window rendering
class ScGridWindow : public vcl::Window
{
    // Uses VCL for:
    // - Canvas drawing operations  
    // - Mouse/keyboard event handling
    // - Cursor management
    // - Device context management
};
```

#### SFX2 Integration
```cpp
// Document shell integration
class ScDocShell : public SfxObjectShell
{
    // Uses SFX2 for:
    // - Document lifecycle management
    // - Menu/toolbar command binding
    // - Print framework integration
};
```

## Design Patterns and Principles

### Key Design Patterns
1. **Model-View-Controller**: Clear separation between data (ScDocument), view (ScTabView), and control (ScTabViewShell)
2. **Strategy Pattern**: Different filter implementations for various file formats
3. **Observer Pattern**: Dependency tracking and automatic recalculation
4. **Factory Pattern**: Cell creation and dialog instantiation
5. **Command Pattern**: Undo/redo system and menu commands
6. **Composite Pattern**: Hierarchical document structure

### Architectural Principles
- **Separation of Concerns**: Clear layer boundaries and responsibilities
- **Performance Optimization**: Multi-threading, GPU acceleration, memory efficiency
- **Extensibility**: Plugin architecture for functions and formats
- **Scalability**: Efficient handling of large spreadsheets
- **Internationalization**: Multi-language support throughout

## Advanced Features

### Pivot Tables (DataPilot)
**Location**: `sc/source/core/data/dp*.cxx`

- **Data Analysis**: Advanced data summarization and analysis
- **Cache System**: Efficient data processing and storage
- **Multiple Sources**: Support for database and spreadsheet sources
- **Interactive Interface**: Drag-and-drop field configuration

### Conditional Formatting
**Location**: `sc/source/core/data/colorscale.cxx`, `conditio.cxx`

- **Rule-Based Formatting**: Color scales, data bars, icon sets
- **Performance Optimization**: Efficient rule evaluation
- **Visual Analytics**: Data visualization through formatting

### Solver and Goal Seek
**Location**: `sc/source/ui/view/cellsh1.cxx`, solver functionality

- **Mathematical Optimization**: Non-linear problem solving
- **Plugin Architecture**: Support for external solver engines
- **Constraint Handling**: Complex constraint satisfaction

### Charts and Graphics
- **Embedded Charts**: Tight integration with LibreOffice Chart module
- **Drawing Layer**: SVX-based drawing object support
- **Data Visualization**: Automatic chart updates with data changes

## Testing and Quality Assurance

### Testing Framework
**Location**: `sc/qa/`

- **Unit Tests**: Direct class testing (`sc/qa/unit/`)
- **Integration Tests**: Import/export validation
- **UI Tests**: User interface automation (`sc/qa/uitest/`)
- **Performance Tests**: Calculation and memory benchmarks

### Quality Features
- **Error Handling**: Comprehensive error detection and reporting
- **Data Validation**: Input validation and constraint checking
- **Recovery Systems**: Document recovery and backup mechanisms

## Conclusion

LibreOffice Calc demonstrates sophisticated software architecture with:

- **Hierarchical Data Model**: Efficient document → table → column → cell structure
- **Advanced Formula Engine**: Multi-threaded calculation with GPU acceleration
- **Comprehensive Format Support**: Extensive import/export capabilities
- **Performance Optimization**: Memory efficiency and calculation speed optimization
- **Extensible Architecture**: Plugin systems and modular design
- **Robust Integration**: Seamless integration with LibreOffice ecosystem

This architecture enables Calc to handle complex spreadsheet requirements while maintaining excellent performance and extensibility for future development. The clear separation of concerns, efficient data structures, and comprehensive testing framework make it a mature and reliable spreadsheet application.