# LibreOffice Draw Architecture

## Overview

LibreOffice Draw (sd module) is a vector graphics editor and presentation software built on a sophisticated, unified architecture. The module serves both LibreOffice Draw (graphics editing) and LibreOffice Impress (presentations) through a shared codebase, where **"Impress is essentially a hack on top of Draw"**. This architecture demonstrates mature software engineering with clear separation of concerns, comprehensive graphics capabilities, and extensive animation/transition support.

## High-Level Architecture

Draw/Impress follows a **layered architectural pattern** with six primary layers:

```
┌─────────────────────────────────────┐
│          UNO API Layer              │  ← SdX* classes (external interfaces)
├─────────────────────────────────────┤
│          UI Framework Layer         │  ← ViewShellBase, framework management
├─────────────────────────────────────┤
│          View Layer                 │  ← DrawViewShell, OutlineView, SlideSorter
├─────────────────────────────────────┤
│          Function Layer             │  ← Command objects (fu*.cxx)
├─────────────────────────────────────┤
│          Filter Layer               │  ← Import/Export filters
├─────────────────────────────────────┤
│          Document Model Layer       │  ← SdDrawDocument, SdPage
└─────────────────────────────────────┘
```

## Core Architecture Components

### 1. Document Model (SdDrawDocument and SdPage)

**Location**: `sd/source/core/`, `sd/inc/drawdoc.hxx`, `sd/inc/sdpage.hxx`

The document model forms the foundation supporting both Draw and Impress functionality.

#### SdDrawDocument - Central Document Class
- **Role**: Main document model for both Draw and Impress
- **Architecture**: Inherits from `FmFormModel` (SVX drawing model)
- **Key Components**:
  - Page management for slides/drawing pages
  - Master page handling
  - Animation and transition management
  - Style sheet coordination
  - Document type differentiation (`DocumentType::Draw` vs `DocumentType::Impress`)

#### SdPage - Enhanced Page Representation
**Key Features**:
- **Presentation Objects**: Specialized handling for title, content, and layout elements
- **Animation Support**: Complex animation sequences and timing
- **Transitions**: Slide transition effects and properties
- **Layout Management**: Automatic layout templates and manual positioning
- **Master Page Relationship**: Inheritance from master slides

#### Document Type Handling
```cpp
enum class DocumentType {
    Impress,
    Draw
};
```
The same codebase supports both applications through document type switching and mode-specific behavior.

### 2. View Management System

#### ViewShellBase - Foundation Architecture
**Location**: `sd/source/ui/inc/ViewShellBase.hxx`

- **Role**: Base class for all view implementations
- **Architecture**: Inherits from `SfxViewShell` (LibreOffice framework)
- **Key Features**:
  - Framework integration and resource management
  - View shell stacking and switching
  - Event distribution and handling
  - Controller coordination

#### DrawViewShell - Primary Editing Interface
**Location**: `sd/source/ui/inc/DrawViewShell.hxx`

- **Role**: Main editing view for both Draw and Impress
- **Key Responsibilities**:
  - Drawing tool management
  - Page navigation and selection
  - Object manipulation and selection
  - Property panel integration
  - Layer and tab management

#### Specialized View Shells
```cpp
DrawViewShell          // Primary editing interface
├── OutlineViewShell   // Outline/text editing mode
├── SlideSorterViewShell  // Thumbnail-based slide management
├── PresentationViewShell // Full-screen presentation
└── NotesPanelViewShell   // Speaker notes editing
```

### 3. UI Framework System

**Location**: `sd/source/ui/framework/`

#### Dynamic UI Configuration
- **Resource-Based Architecture**: URL-based resource identification
- **Factory Pattern**: Dynamic creation of views, panes, and toolbars
- **Configuration Management**: XML-based UI layout definitions

#### Framework URLs
```
private:resource/view/ImpressView    // Main Impress view
private:resource/view/DrawView       // Main Draw view
private:resource/view/OutlineView    // Outline editing
private:resource/view/SlideSorterView // Slide management
```

### 4. Command System (Function Objects)

**Location**: `sd/source/ui/func/`

#### Command Pattern Implementation
Each `fu*.cxx` file implements specific user operations:
- **fusel.cxx**: Selection and manipulation
- **fudraw.cxx**: Drawing operations
- **futext.cxx**: Text editing
- **fuinsert.cxx**: Object insertion
- **fumorph.cxx**: Shape transformations

#### Function Object Architecture
```cpp
class FuPoor {  // Base function class
    virtual void DoExecute(SfxRequest& rReq);
    virtual bool MouseMove(const MouseEvent& rMEvt);
    virtual bool KeyInput(const KeyEvent& rKEvt);
};
```

### 5. Animation and Effects System

#### CustomAnimationEffect - Animation Engine
**Location**: `sd/inc/CustomAnimationEffect.hxx`

- **Animation Presets**: Predefined animation templates
- **Custom Effects**: User-defined animation sequences
- **Timing Control**: Precise animation timing and synchronization
- **Property Animation**: Color, position, size, transparency changes

#### MainSequence - Animation Controller
- **Effect Management**: Adding, removing, reordering effects
- **Animation Tree**: Hierarchical animation structure
- **Timeline Coordination**: Synchronization of multiple effects

#### Transition System
**Location**: `sd/inc/TransitionPreset.hxx`

- **Slide Transitions**: 3D and 2D transition effects
- **OpenGL Integration**: Hardware-accelerated transitions
- **Timing Control**: Duration and trigger management

### 6. Import/Export Filter Architecture

**Location**: `sd/source/filter/`

#### Filter Organization
```
filter/
├── eppt/        // PowerPoint export (complex format handling)
├── ppt/         // PowerPoint import with animation preservation
├── html/        // HTML export for web presentations
├── pdf/         // PDF export functionality
├── xml/         // ODF XML processing
└── cgm/         // Computer Graphics Metafile support
```

#### PowerPoint Format Support
- **Binary PPT**: Complete import/export with animation preservation
- **PPTX (OOXML)**: Modern PowerPoint format support
- **Animation Mapping**: Complex animation translation between formats

#### Filter Integration Points
```cpp
// From xmloff module (ODF)
xmloff/source/draw/    // ODP import/export

// From oox module (OOXML) 
oox/source/ppt/        // PPTX import
sd/source/filter/eppt/ // PPTX export
```

### 7. Presentation Console System

**Location**: `sd/source/console/`

#### Dual-Screen Presentation
- **Presenter Controller**: Main presentation logic coordination
- **Notes Display**: Speaker notes on secondary screen
- **Slide Preview**: Current and next slide previews
- **Timer Integration**: Presentation timing and alerts
- **Accessibility Support**: Screen reader compatibility

## External Module Dependencies

Draw/Impress integrates deeply with LibreOffice's modular architecture:

### Core Graphics Dependencies
- **SVX**: Fundamental drawing objects and editing tools
- **Drawinglayer**: Modern 2D graphics rendering engine
- **Basegfx**: Mathematical primitives and transformations
- **VCL**: Windowing system and graphics output

### Framework Dependencies
- **SFX2**: Document framework and view management
- **EditEng**: Text editing and formatting engine
- **CompHelper**: Component utilities and helpers

### Integration Examples

#### SVX Drawing Foundation
```cpp
// SdDrawDocument inherits from SVX's form model
class SdDrawDocument : public FmFormModel
{
    // Extends SVX with presentation-specific functionality
    // Uses SdrObject hierarchy for all graphic elements
};
```

#### VCL UI Integration
```cpp
// DrawViewShell uses VCL for windowing
VclPtr<sd::Window> mpContentWindow;
VclPtr<ScrollAdaptor> mpHorizontalScrollBar;
VclPtr<SvxRuler> mpHorizontalRuler;
```

## Design Patterns and Principles

### Key Design Patterns
1. **Model-View-Controller**: Clear separation between document (SdDrawDocument), views (ViewShellBase derivatives), and controllers
2. **Command Pattern**: Function objects for all user operations
3. **Factory Pattern**: Dynamic view and resource creation
4. **Strategy Pattern**: Different filter implementations for various formats
5. **Observer Pattern**: Animation and transition event handling
6. **Bridge Pattern**: UNO API integration for external access

### Architectural Principles
- **Unified Codebase**: Single codebase supports both Draw and Impress
- **Extensibility**: Plugin architecture for filters, effects, and views
- **Performance**: Hardware acceleration and optimized rendering
- **Accessibility**: Comprehensive screen reader and keyboard support
- **Internationalization**: Multi-language support throughout

## Advanced Features

### Slide Sorter
**Location**: `sd/source/ui/slidesorter/`

- **MVC Architecture**: Clean model-view-controller separation
- **Caching System**: Performance optimization for large presentations
- **Drag and Drop**: Advanced slide reordering and copying
- **Selection Management**: Multi-slide operations

### Master Page System
- **Template Management**: Reusable slide templates
- **Inheritance Model**: Automatic formatting inheritance
- **Custom Masters**: User-created master slides
- **Layout Variants**: Different content layouts per master

### Shape and Object Management
- **Drawing Tools**: Comprehensive shape creation and editing
- **Custom Shapes**: User-defined geometric shapes
- **3D Objects**: Three-dimensional shape support
- **Grouping**: Hierarchical object organization

### Animation Timeline
- **Visual Editor**: Drag-and-drop animation sequencing
- **Property Animation**: Granular control over object properties
- **Motion Paths**: Custom animation trajectories
- **Effect Library**: Extensive preset animation collection

## Performance Optimizations

### Rendering Pipeline
- **Drawinglayer Integration**: Modern primitive-based rendering
- **Hardware Acceleration**: OpenGL for transitions and effects
- **Caching Strategies**: Smart preview and thumbnail caching
- **Incremental Updates**: Selective redrawing for performance

### Memory Management
- **Object Pooling**: Efficient reuse of drawing objects
- **Lazy Loading**: On-demand resource loading
- **Reference Counting**: Smart memory management for shared resources

## Testing and Quality Assurance

### Testing Framework
**Location**: `sd/qa/`

- **Unit Tests**: Core functionality testing (`sd/qa/unit/`)
- **Import/Export Tests**: Format compatibility validation
- **UI Tests**: Automated user interface testing (`sd/qa/uitest/`)
- **Performance Tests**: Animation and rendering benchmarks

## Conclusion

LibreOffice Draw architecture demonstrates sophisticated software engineering with:

- **Unified Application Support**: Single codebase elegantly supporting both drawing and presentation needs
- **Advanced Graphics Engine**: Modern 2D rendering with hardware acceleration
- **Comprehensive Animation System**: Professional-grade animation and transition capabilities
- **Extensible Filter Architecture**: Support for numerous import/export formats
- **Framework Integration**: Seamless integration with LibreOffice ecosystem
- **Performance Optimization**: Hardware acceleration and smart caching strategies

This architecture enables both Draw and Impress to provide professional-grade functionality while maintaining code efficiency, extensibility, and excellent performance. The clear separation of concerns and modular design facilitate ongoing development and enhancement of both applications.