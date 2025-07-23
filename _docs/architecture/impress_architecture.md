# LibreOffice Impress/Draw Architecture

## Overview

LibreOffice Impress and Draw (sd module) represent a unified codebase that powers both presentation software (Impress) and vector graphics editing (Draw). As noted in the README, **"Impress is essentially a hack on top of Draw"**, demonstrating how a sophisticated shared architecture can support two distinct applications. The module implements comprehensive presentation and drawing capabilities including advanced animations, slide management, multi-format support, and professional-grade rendering.

## High-Level Architecture

The sd module follows a **multi-layered architectural pattern** with clear separation of concerns across six primary layers:

```
┌─────────────────────────────────────┐
│          UNO API Layer              │  ← Service interfaces (external access)
├─────────────────────────────────────┤
│          UI Framework Layer         │  ← ViewShells, dialogs, sidebar panels
├─────────────────────────────────────┤
│          Application Layer          │  ← Function objects, tools, commands
├─────────────────────────────────────┤
│          Document Model Layer       │  ← SdDrawDocument, SdPage, animations
├─────────────────────────────────────┤
│          Rendering Engine Layer     │  ← Drawing views, layer management
├─────────────────────────────────────┤
│          Import/Export Layer        │  ← Format filters (PPT, PPTX, ODP, etc.)
└─────────────────────────────────────┘
```

## Core Architecture Components

### 1. Document Model Architecture

**Location**: `sd/inc/drawdoc.hxx`, `sd/source/core/`

#### SdDrawDocument - Central Document Controller
The `SdDrawDocument` class serves as the foundation, inheriting from `FmFormModel` and providing:

**Key Responsibilities**:
- Document type management (`DocumentType::Impress` vs `DocumentType::Draw`)
- Page hierarchy management (standard, notes, handout pages)
- Master page system with inheritance relationships
- Style sheet pool management through `SdStyleSheetPool`
- Animation and transition coordination
- Presentation configuration management

**Document Structure**:
```cpp
class SdDrawDocument final : public FmFormModel {
    DocumentType meDocType;                           // Application mode
    std::vector<std::unique_ptr<sd::FrameView>> maFrameViewList;  // View states
    sd::PresentationSettings maPresentationSettings; // Presentation config
    SdStyleSheetPool* mpStyleSheetPool;              // Style management
    SdCustomShowList* mpCustomShowList;              // Custom presentations
};
```

#### SdPage - Enhanced Page Representation
The `SdPage` class extends `FmFormPage` with presentation-specific capabilities:

**Core Features**:
- **Page Classification**: Standard slides, notes pages, and handout layouts
- **AutoLayout System**: Template-based content arrangement (`AutoLayout` enum)
- **Presentation Objects**: Specialized content placeholders (`PresObjKind` system)
- **Animation Integration**: Per-page animation sequences and transitions
- **Master Page Relationships**: Inheritance-based styling and templates

**Page Architecture**:
```cpp
class SdPage final : public FmFormPage {
    PageKind    mePageKind;               // Standard/Notes/Handout
    AutoLayout  meAutoLayout;             // Layout template
    sd::ShapeList maPresentationShapeList; // Presentation objects
    css::uno::Reference<css::animations::XAnimationNode> mxAnimationNode; // Animation tree
    std::shared_ptr<sd::MainSequence> mpMainSequence; // Animation controller
};
```

### 2. Presentation Object System

#### PresObjKind Classification
The presentation object system categorizes slide content through a comprehensive type system:

```cpp
enum class PresObjKind {
    Title,        // Title placeholders
    Outline,      // Bullet point content  
    Text,         // Text boxes
    Graphic,      // Images and graphics
    Object,       // Embedded objects
    Chart,        // Charts and graphs
    Table,        // Data tables
    Media,        // Audio/video content
    Header,       // Header text fields
    Footer,       // Footer text fields
    DateTime,     // Date/time fields
    SlideNumber   // Slide numbering
};
```

#### Layout Template System
The AutoLayout system provides predefined slide arrangements:
- **Template-based design**: Over 20 standard layouts from title slides to complex multi-content layouts
- **Automatic object placement**: Responsive positioning based on content type
- **Master page integration**: Template inheritance from master slides
- **Custom layout support**: User-defined layout creation and management

### 3. Animation and Effects Architecture

#### Core Animation Framework
**Location**: `sd/inc/CustomAnimationEffect.hxx`, `sd/source/core/`

The animation system implements a sophisticated effect management architecture:

**Key Components**:
- **`CustomAnimationEffect`**: Individual animation effects with timing, targets, and parameters
- **`MainSequence`**: Primary animation timeline coordinator
- **`InteractiveSequence`**: Click-triggered and event-driven animations
- **`EffectSequenceHelper`**: Base class for sequence management

**Animation Hierarchy**:
```cpp
class CustomAnimationEffect {
    sal_Int16 mnNodeType;           // Animation node type
    sal_Int16 mnPresetClass;        // Effect category (entrance/emphasis/exit)
    double mfDuration;              // Effect duration
    double mfBegin;                 // Start time
    css::uno::Any maTarget;         // Target object reference
    OUString maPresetId;            // Effect preset identifier
};
```

#### SMIL Integration
The system integrates with W3C SMIL (Synchronized Multimedia Integration Language):
- Standard SMIL timing model support (`smil:begin`, `smil:dur`, `smil:fill`)
- Animation node hierarchy following SMIL specification
- Cross-format animation preservation
- Extensible preset system with XML configuration

#### Transition System
**`TransitionPreset`** manages slide-to-slide transitions:
```cpp
class TransitionPreset {
    sal_Int16 mnTransition;      // Transition type
    sal_Int16 mnSubtype;         // Transition variant
    bool mbDirection;            // Transition direction
    sal_Int32 mnFadeColor;       // Optional fade color
    double mfDuration;           // Transition duration
};
```

### 4. View Architecture and UI Framework

#### View Shell Hierarchy
**Location**: `sd/source/ui/inc/`

The view system implements a sophisticated shell stacking architecture:

**Core View Classes**:
- **`ViewShellBase`**: Foundation class inheriting from `SfxViewShell`
- **`DrawViewShell`**: Primary editing interface for both Draw and Impress
- **`OutlineViewShell`**: Text-based outline editing for presentations
- **`SlideSorterViewShell`**: Thumbnail-based slide management
- **`PresentationViewShell`**: Full-screen presentation mode

**View Shell Coordination**:
```
ViewShellBase (SfxViewShell)
├── DrawViewShell (main editing)
├── SlideSorterViewShell (slide management)
├── OutlineViewShell (text editing)
├── PresentationViewShell (slideshow)
└── NotesPanelViewShell (speaker notes)
```

#### Function Object System
The application logic uses a command pattern implementation with 40+ function objects:

**Function Categories**:
- **`FuPoor`**: Abstract base class for all function objects
- **Drawing Functions**: `FuConstruct`, `FuDraw`, `FuSelection`
- **Editing Functions**: `FuText`, `FuFormatPaintBrush`, `FuEditGluePoints`
- **Navigation Functions**: `FuNavigation`, `FuZoom`, `FuPage`

**Function Object Pattern**:
```cpp
class FuPoor {
    virtual void DoExecute(SfxRequest& rReq);
    virtual bool MouseMove(const MouseEvent& rMEvt);
    virtual bool KeyInput(const KeyEvent& rKEvt);
    virtual void Activate();
    virtual void Deactivate();
};
```

#### Slide Sorter Architecture
**Location**: `sd/source/ui/slidesorter/`

The slide sorter implements a comprehensive MVC architecture:

**Model-View-Controller Structure**:
- **`SlideSorterModel`**: Data management and slide descriptors
- **`SlideSorterView`**: Visual rendering and layout
- **`SlideSorterController`**: User interaction and operations

**Advanced Features**:
- **Cache System**: `SlsBitmapCache`, `SlsPageCache` for performance
- **Animation System**: `SlsAnimator` for smooth transitions
- **Layout System**: `SlsLayouter` for responsive thumbnail arrangement
- **Selection System**: `SlsSelectionManager` for multi-slide operations

### 5. Rendering and Graphics Engine

#### Multi-Stage Rendering Pipeline
**Location**: `sd/source/ui/inc/SlideshowLayerRenderer.hxx`

The rendering system implements a sophisticated multi-stage approach:

```cpp
enum class RenderStage {
    Background = 0,    // Base slide background
    Master = 1,        // Master page objects
    Slide = 2,         // Individual slide content
    TextFields = 3     // Dynamic text fields
};
```

#### Layer Management System
**`LayerManager`** provides comprehensive drawing order control:
- **Z-order Management**: Shape organization by depth
- **Animation Layers**: Sprite-based layers for animated content
- **Background/Foreground Separation**: Optimal rendering performance
- **Update Area Tracking**: Minimal repaint regions

#### Graphics Optimization
**Preview Caching System**:
- **`PageCache`**: Slide thumbnail caching with LRU eviction
- **`PageCacheManager`**: Global cache coordination
- **Bitmap Scaling**: Quality-preserving size adjustments
- **Memory Management**: Configurable cache limits with precious flag protection

#### Hardware Acceleration
**OpenGL Integration**:
- **Sprite Rendering**: GPU-accelerated animation playback
- **Gradient Shaders**: Hardware-optimized gradient rendering
- **Texture Cache**: GPU memory management for graphics
- **Canvas Integration**: Hardware-accelerated drawing operations

### 6. Import/Export Filter Architecture

#### Filter Framework
**Location**: `sd/source/filter/`

The filter system supports comprehensive format compatibility:

**Filter Organization**:
```
sd/source/filter/
├── eppt/          // PowerPoint export (PPT/PPTX)
├── ppt/           // PowerPoint import (PPT)
├── xml/           // ODF XML processing
├── html/          // HTML export
├── pdf/           // PDF import
├── grf/           // Graphics filters
└── cgm/           // CGM vector format
```

#### Format Support Matrix

| Format | Import | Export | Implementation |
|--------|--------|--------|---------------|
| **ODP (ODF)** | ✓ | ✓ | Native format via XML filters |
| **PPT (Binary)** | ✓ | ✓ | Complete binary format support |
| **PPTX (OOXML)** | ✓ | ✓ | Full OOXML compliance |
| **PPTM (Macro)** | ✓ | ✓ | Macro-enabled format support |
| **HTML** | ✗ | ✓ | Web presentation export |
| **PDF** | ✓ | ✗ | PDF import with annotations |
| **Graphics** | ✓ | ✓ | All VCL-supported formats |

#### PowerPoint Compatibility
**Advanced PPT/PPTX Support**:
- **Animation Preservation**: Complete effect translation between formats
- **Theme Support**: Color schemes and design templates
- **Master Slide Mapping**: Template relationship preservation
- **Custom Shows**: Subset presentation support
- **Embedded Objects**: OLE and multimedia content
- **DRM Decryption**: Encrypted presentation support

### 7. Presenter Console System

#### Dual-Screen Architecture
**Location**: `sd/source/console/`

The presenter console provides professional presentation features:

**Core Components**:
- **`PresenterController`**: Main presenter interface coordination
- **`PresenterScreen`**: Dual-screen presentation management
- **`PresenterCanvas`**: Specialized rendering for presenter displays
- **`PresenterPreviewCache`**: Optimized slide thumbnail generation

**Presenter Features**:
- **Current/Next Slide Preview**: Multi-slide display support
- **Speaker Notes**: Full-featured note editing and display
- **Presentation Timer**: Time management and alerts
- **Slide Navigation**: Quick access to any slide
- **Annotation Support**: Real-time presentation markup

### 8. Framework Integration

#### LibreOffice Framework Dependencies
**Core Framework Integration**:
- **SFX2**: Document shell architecture and view management
- **VCL**: Graphics rendering and windowing system
- **SVX**: Drawing objects and shape manipulation
- **UNO**: Component interface and service architecture

#### UNO Service Architecture
**Service Registration** (from `sd.component`):
- `com.sun.star.drawing.DrawingDocument` - Draw document service
- `com.sun.star.presentation.PresentationDocument` - Impress document service
- `com.sun.star.drawing.framework.PanelFactory` - Sidebar panel factory
- Toolbar controllers for layout and display management

#### External Dependencies
**Key External Libraries**:
- **libxml2**: XML processing for ODF and configuration
- **boost**: Template libraries for modern C++ features
- **dbus**: System integration (Linux)
- **avahi**: Network service discovery (optional)
- **OpenGL**: Hardware-accelerated graphics rendering

### 9. Draw vs Impress Differentiation

#### Shared Codebase Architecture
The unified architecture supports both applications through:

**Document Type Management**:
```cpp
enum class DocumentType {
    Impress,  // Presentation mode
    Draw      // Graphics mode
};
```

**Conditional Feature Logic**:
- Mode-specific UI elements based on `meDocType` checks
- Application-specific toolbars and menu configurations
- Feature availability based on document type

**Specialized Components**:
- **Impress-Specific**: Slide sorter, presentation mode, animation panes
- **Draw-Specific**: Enhanced layer management, technical drawing tools
- **Shared**: Core drawing engine, object manipulation, file formats

### 10. Performance and Optimization

#### Memory Management
- **Smart Pointer Architecture**: Reference counting for automatic resource cleanup
- **Cache Management**: Configurable limits with LRU eviction strategies
- **Resource Pooling**: View-switching optimization through cache reuse

#### Rendering Optimization
- **Incremental Updates**: Region-based invalidation and repainting
- **Background Rendering**: Idle-time preview generation
- **Sprite Animation**: Hardware-accelerated animation playback
- **Layer-Based Rendering**: Optimized drawing order and compositing

#### Scalability Features
- **Asynchronous Operations**: Non-blocking thumbnail generation
- **Progressive Loading**: Lazy initialization of complex features
- **Memory Monitoring**: Adaptive cache sizing based on available resources

## Design Patterns and Principles

### Key Design Patterns

1. **Model-View-Controller**: Clear separation in slide sorter and animation systems
2. **Command Pattern**: Function objects for all user operations
3. **Factory Pattern**: Filter creation and UNO service instantiation
4. **Strategy Pattern**: Multiple import/export strategies
5. **Observer Pattern**: Animation event handling and document change notifications
6. **Visitor Pattern**: Object traversal for rendering and export operations

### Architectural Principles

- **Unified Codebase**: Single implementation supporting both Draw and Impress
- **Format Compatibility**: Comprehensive support for industry-standard formats
- **Performance Optimization**: Hardware acceleration and efficient caching
- **Extensibility**: Plugin architecture for filters and UI components
- **Accessibility**: Complete screen reader and keyboard navigation support

## Testing and Quality Assurance

### Testing Framework
**Location**: `sd/qa/`

- **Unit Tests**: Core functionality testing (`sd/qa/unit/`)
- **Import/Export Tests**: Format compatibility validation across 20+ test suites
- **UI Tests**: Automated user interface testing (`sd/qa/uitest/`)
- **Performance Tests**: Animation and rendering benchmarks
- **Accessibility Tests**: Screen reader and keyboard navigation validation

## Future Enhancement Opportunities

### Potential Improvements

1. **Enhanced Animation Editor**: Visual timeline editing with drag-and-drop
2. **Advanced Collaboration**: Real-time collaborative editing features
3. **AI Integration**: Smart layout suggestions and content optimization
4. **Web-Based Presentations**: Enhanced HTML5 export with interactivity
5. **Cloud Integration**: Native cloud storage and sharing capabilities

### Extension Points

- **Custom Animation Presets**: User-defined effect libraries
- **Filter Plugin System**: External format support modules
- **Template Marketplace**: Downloadable presentation templates
- **Advanced Export Options**: Additional output formats and quality settings

## Conclusion

LibreOffice Impress/Draw architecture demonstrates exceptional software engineering that successfully unifies presentation and drawing applications within a single, sophisticated codebase. The architecture provides:

- **Comprehensive Functionality**: Professional-grade presentation and drawing capabilities
- **Format Compatibility**: Industry-leading support for Microsoft Office and open standards
- **Performance Excellence**: Hardware acceleration and intelligent caching systems
- **Extensible Design**: Plugin architecture supporting future enhancements
- **Accessibility Compliance**: Full support for assistive technologies

The modular design with clear separation of concerns enables both applications to leverage shared infrastructure while providing specialized features appropriate to their respective use cases. The animation system rivals commercial presentation software, while the drawing capabilities support professional vector graphics creation. This architecture serves as an exemplar of how complex, feature-rich applications can be built with maintainable, extensible designs that support multiple related use cases efficiently.