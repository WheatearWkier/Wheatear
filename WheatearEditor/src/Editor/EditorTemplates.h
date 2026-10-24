#pragma once

#include <string>
#include <vector>

namespace Wheatear {

    enum class EditorWorkspace
    {
        TwoD = 0,
        ThreeD
    };

    struct EditorTemplate
    {
        std::string Id;
        std::string Name;
        std::string Subtitle;
        std::string Description;
        EditorWorkspace Workspace = EditorWorkspace::TwoD;
        std::string IconPath;
        std::string StartupScene;
    };

    inline const char* WorkspaceLabel(EditorWorkspace workspace)
    {
        return workspace == EditorWorkspace::ThreeD ? "3D" : "2D";
    }

    inline const std::vector<EditorTemplate>& GetEditorTemplates()
    {
        static const std::vector<EditorTemplate> templates = {
            {
                "2d-blank", "Blank 2D",
                "Camera + UI canvas",
                "A clean 2D workspace with an orthographic camera and a canvas.",
                EditorWorkspace::TwoD,
                "Resources/Icons/Editor/template.png",
                R"(Scene: Start
SavePolicy:
  CanSave: true
  CanLoad: true
  SaveDirectory: assets/saves
  AutoLoadSlot: 0
Entities:
  - Entity: 1000000001
    TagComponent:
      Tag: Main Camera
    TransformComponent:
      Translation: [0, 0, 10]
      Rotation: [0, 0, 0]
      Scale: [1, 1, 1]
    CameraComponent:
      Camera:
        ProjectionType: 1
        OrthographicSize: 5
        OrthographicNear: -1
        OrthographicFar: 100
      Primary: true
      FixedAspectRatio: false
  - Entity: 1000000002
    TagComponent:
      Tag: Canvas
    TransformComponent:
      Translation: [0, 0, 0]
      Rotation: [0, 0, 0]
      Scale: [1, 1, 1]
    UICanvasComponent:
      Visible: true
      ReferenceWidth: 1920
      ReferenceHeight: 1080
    UIWidgetComponent:
      Visible: true
      Position: [0, 0]
      Size: [1, 1]
      Rotation: 0
      Anchor: 0
      SortOrder: 0
      ParentEntity: 0
)"
            },
            {
                "2d-ui", "UI / Menu",
                "Canvas-first layout",
                "Start with a UI canvas, ready for menus, HUD and interactive screens.",
                EditorWorkspace::TwoD,
                "Resources/Icons/Editor/panel.png",
                R"(Scene: Start
SavePolicy:
  CanSave: true
  CanLoad: true
  SaveDirectory: assets/saves
  AutoLoadSlot: 0
Entities:
  - Entity: 1000000101
    TagComponent:
      Tag: Main Camera
    TransformComponent:
      Translation: [0, 0, 10]
      Rotation: [0, 0, 0]
      Scale: [1, 1, 1]
    CameraComponent:
      Camera:
        ProjectionType: 1
        OrthographicSize: 5
        OrthographicNear: -1
        OrthographicFar: 100
      Primary: true
      FixedAspectRatio: false
  - Entity: 1000000102
    TagComponent:
      Tag: UI Canvas
    TransformComponent:
      Translation: [0, 0, 0]
      Rotation: [0, 0, 0]
      Scale: [1, 1, 1]
    UICanvasComponent:
      Visible: true
      ReferenceWidth: 1920
      ReferenceHeight: 1080
    UIWidgetComponent:
      Visible: true
      Position: [0, 0]
      Size: [1, 1]
      Rotation: 0
      Anchor: 0
      SortOrder: 0
      ParentEntity: 0
  - Entity: 1000000103
    TagComponent:
      Tag: Title
    TransformComponent:
      Translation: [0, 0, 0]
      Rotation: [0, 0, 0]
      Scale: [1, 1, 1]
    UIWidgetComponent:
      Visible: true
      Position: [0.5, 0.3]
      Size: [0.7, 0.14]
      Rotation: 0
      Anchor: 4
      SortOrder: 1
      ParentEntity: 1000000102
    UITextComponent:
      Text: "WHEATEAR STUDIO"
      Color: [1, 0.82, 0.48, 1]
      FontSize: 52
      FontPath: ""
      Padding: [0, 0, 0, 0]
      AutoFit: true
      HorizontalAlign: 1
      VerticalAlign: 1
      ShadowColor: [0, 0, 0, 0.55]
      ShadowOffset: [2, 2]
      OutlineColor: [0, 0, 0, 0]
      OutlineThickness: 0
)"
            },
            {
                "2d-side", "Side Combat",
                "Arena + two fighters",
                "An orthographic arena with colored stand-ins for player and enemy.",
                EditorWorkspace::TwoD,
                "Resources/Icons/Editor/gameplay.png",
                R"(Scene: Start
SavePolicy:
  CanSave: true
  CanLoad: true
  SaveDirectory: assets/saves
  AutoLoadSlot: 0
Entities:
  - Entity: 1000000201
    TagComponent:
      Tag: Main Camera
    TransformComponent:
      Translation: [0, 0, 10]
      Rotation: [0, 0, 0]
      Scale: [1, 1, 1]
    CameraComponent:
      Camera:
        ProjectionType: 1
        OrthographicSize: 7
        OrthographicNear: -1
        OrthographicFar: 100
      Primary: true
      FixedAspectRatio: false
  - Entity: 1000000202
    TagComponent:
      Tag: Ground
    TransformComponent:
      Translation: [0, -3, 0]
      Rotation: [0, 0, 0]
      Scale: [18, 1, 1]
    SpriteRendererComponent:
      Color: [0.12, 0.14, 0.18, 1]
      Texture: ""
      TilingFactor: 1
      SortingLayer: 0
      OrderInLayer: -10
      UVMin: [0, 0]
      UVMax: [1, 1]
      FlipX: false
      DrawOffset: [0, 0]
      DrawScale: [1, 1]
      PixelsPerUnit: 0
  - Entity: 1000000203
    TagComponent:
      Tag: Player
    TransformComponent:
      Translation: [-3, 0, 0]
      Rotation: [0, 0, 0]
      Scale: [1, 2, 1]
    SpriteRendererComponent:
      Color: [0.30, 0.55, 1.0, 1]
      Texture: ""
      TilingFactor: 1
      SortingLayer: 0
      OrderInLayer: 0
      UVMin: [0, 0]
      UVMax: [1, 1]
      FlipX: false
      DrawOffset: [0, 0]
      DrawScale: [1, 1]
      PixelsPerUnit: 0
  - Entity: 1000000204
    TagComponent:
      Tag: Enemy
    TransformComponent:
      Translation: [3, 0, 0]
      Rotation: [0, 0, 0]
      Scale: [1, 2, 1]
    SpriteRendererComponent:
      Color: [1.0, 0.34, 0.26, 1]
      Texture: ""
      TilingFactor: 1
      SortingLayer: 0
      OrderInLayer: 0
      UVMin: [0, 0]
      UVMax: [1, 1]
      FlipX: false
      DrawOffset: [0, 0]
      DrawScale: [1, 1]
      PixelsPerUnit: 0
)"
            },
            {
                "3d-blank", "Blank 3D",
                "Perspective + light rig",
                "An empty 3D stage with a camera, sun and a neutral ground plane.",
                EditorWorkspace::ThreeD,
                "Resources/Icons/Editor/box.png",
                R"(Scene: Start
SavePolicy:
  CanSave: true
  CanLoad: true
  SaveDirectory: assets/saves
  AutoLoadSlot: 0
Entities:
  - Entity: 1000000301
    TagComponent:
      Tag: Main Camera
    TransformComponent:
      Translation: [0, 3, 8]
      Rotation: [-0.25, 0, 0]
      Scale: [1, 1, 1]
    CameraComponent:
      Camera:
        ProjectionType: 0
        PerspectiveFOV: 0.785398
        PerspectiveNear: 0.1
        PerspectiveFar: 1000
        OrthographicSize: 10
        OrthographicNear: -1
        OrthographicFar: 1
      Primary: true
      FixedAspectRatio: false
  - Entity: 1000000302
    TagComponent:
      Tag: Sun
    TransformComponent:
      Translation: [0, 8, 0]
      Rotation: [-0.785398, 0.785398, 0]
      Scale: [1, 1, 1]
    DirectionalLightComponent:
      Color: [1, 0.96, 0.9]
      Intensity: 1.4
  - Entity: 1000000303
    TagComponent:
      Tag: Ground
    TransformComponent:
      Translation: [0, -0.05, 0]
      Rotation: [0, 0, 0]
      Scale: [20, 0.1, 20]
    MeshRendererComponent:
      MeshPath: ""
      MaterialPath: ""
  - Entity: 1000000304
    TagComponent:
      Tag: Cube
    TransformComponent:
      Translation: [0, 1, 0]
      Rotation: [0, 0.5, 0]
      Scale: [2, 2, 2]
    MeshRendererComponent:
      MeshPath: ""
      MaterialPath: ""
)"
            },
            {
                "3d-rts", "RTS Skirmish",
                "Terrain + staging",
                "A 60x60 heightfield, RTS camera framing and two staging blocks.",
                EditorWorkspace::ThreeD,
                "Resources/Icons/Editor/event_graph.png",
                R"(Scene: Start
SavePolicy:
  CanSave: true
  CanLoad: true
  SaveDirectory: assets/saves
  AutoLoadSlot: 0
Entities:
  - Entity: 1000000401
    TagComponent:
      Tag: Main Camera
    TransformComponent:
      Translation: [0, 20, 20]
      Rotation: [-0.55, 0, 0]
      Scale: [1, 1, 1]
    CameraComponent:
      Camera:
        ProjectionType: 0
        PerspectiveFOV: 0.785398
        PerspectiveNear: 0.1
        PerspectiveFar: 1000
        OrthographicSize: 10
        OrthographicNear: -1
        OrthographicFar: 1
      Primary: true
      FixedAspectRatio: false
  - Entity: 1000000402
    TagComponent:
      Tag: Sun
    TransformComponent:
      Translation: [0, 12, 0]
      Rotation: [-0.785398, 0.785398, 0]
      Scale: [1, 1, 1]
    DirectionalLightComponent:
      Color: [1, 0.96, 0.9]
      Intensity: 1.4
  - Entity: 1000000403
    TagComponent:
      Tag: Terrain
    TransformComponent:
      Translation: [0, 0, 0]
      Rotation: [0, 0, 0]
      Scale: [1, 1, 1]
    TerrainComponent:
      CellsX: 60
      CellsZ: 60
      CellSize: 1
      HeightScale: 1
      Seed: 1337
  - Entity: 1000000404
    TagComponent:
      Tag: Staging Block
    TransformComponent:
      Translation: [8, 1, 8]
      Rotation: [0, 0, 0]
      Scale: [4, 2, 4]
    MeshRendererComponent:
      MeshPath: ""
      MaterialPath: ""
  - Entity: 1000000405
    TagComponent:
      Tag: Player Base
    TransformComponent:
      Translation: [-14, 1.5, 14]
      Rotation: [0, 0, 0]
      Scale: [6, 3, 5]
    MeshRendererComponent:
      MeshPath: ""
      MaterialPath: ""
  - Entity: 1000000406
    TagComponent:
      Tag: Enemy Base
    TransformComponent:
      Translation: [14, 1.5, -14]
      Rotation: [0, 0, 0]
      Scale: [6, 3, 5]
    MeshRendererComponent:
      MeshPath: ""
      MaterialPath: ""
)"
            },
            {
                "3d-model", "Model Viewer",
                "PBR / material study",
                "A lit turntable-style stage for inspecting imported FBX or glTF assets.",
                EditorWorkspace::ThreeD,
                "Resources/Icons/Editor/film.png",
                R"(Scene: Start
SavePolicy:
  CanSave: true
  CanLoad: true
  SaveDirectory: assets/saves
  AutoLoadSlot: 0
Entities:
  - Entity: 1000000501
    TagComponent:
      Tag: Main Camera
    TransformComponent:
      Translation: [0, 2.2, 6]
      Rotation: [-0.16, 0, 0]
      Scale: [1, 1, 1]
    CameraComponent:
      Camera:
        ProjectionType: 0
        PerspectiveFOV: 0.785398
        PerspectiveNear: 0.1
        PerspectiveFar: 1000
        OrthographicSize: 10
        OrthographicNear: -1
        OrthographicFar: 1
      Primary: true
      FixedAspectRatio: false
  - Entity: 1000000502
    TagComponent:
      Tag: Sun
    TransformComponent:
      Translation: [0, 8, 0]
      Rotation: [-0.785398, 0.785398, 0]
      Scale: [1, 1, 1]
    DirectionalLightComponent:
      Color: [1, 0.96, 0.9]
      Intensity: 1.5
  - Entity: 1000000503
    TagComponent:
      Tag: Ground
    TransformComponent:
      Translation: [0, -0.05, 0]
      Rotation: [0, 0, 0]
      Scale: [12, 0.1, 12]
    MeshRendererComponent:
      MeshPath: ""
      MaterialPath: ""
  - Entity: 1000000504
    TagComponent:
      Tag: Preview Mesh
    TransformComponent:
      Translation: [0, 1, 0]
      Rotation: [0, 0.6, 0]
      Scale: [2, 2, 2]
    MeshRendererComponent:
      MeshPath: ""
      MaterialPath: ""
  - Entity: 1000000505
    TagComponent:
      Tag: Fill Light
    TransformComponent:
      Translation: [2.5, 3, 3]
      Rotation: [0, 0, 0]
      Scale: [1, 1, 1]
    PointLightComponent:
      Color: [1, 0.93, 0.85]
      Intensity: 3
      Constant: 1
      Linear: 0.09
      Quadratic: 0.032
)"
            }
        };
        return templates;
    }

} // namespace Wheatear
