# Wheatear ComfyUI Asset Workflow Pack

This folder contains reusable ComfyUI workflows and prompt manifests for generating
game assets for the Wheatear vertical slice.

The pack is intentionally built on core ComfyUI nodes only:

- `CheckpointLoaderSimple`
- `CLIPTextEncode`
- `EmptyLatentImage`
- `KSampler`
- `VAEDecode`
- `SaveImage`

That keeps the first setup reliable. Add IPAdapter, ControlNet, LoRA, or background
removal later when you want stronger character consistency or true alpha masks.

## Quick Start

1. Start ComfyUI locally, normally at:

   ```text
   http://127.0.0.1:8188
   ```

2. Put an SDXL checkpoint in ComfyUI:

   ```text
   ComfyUI/models/checkpoints/
   ```

3. Edit the checkpoint name in the workflow JSON, or pass it on the command line.

4. Submit a batch:

   ```powershell
   python tools/comfy/run_comfy_batch.py `
     --workflow tools/comfy/workflows/api/sdxl_game_asset_basic.json `
     --manifest tools/comfy/prompts/wheatear_vertical_slice_prompts.json `
     --group vn_backgrounds `
     --ckpt "sd_xl_base_1.0.safetensors"
   ```

5. ComfyUI writes images into its own `output/` directory using the prefixes from
   the prompt manifest, such as:

   ```text
   wheatear/vn/backgrounds/bg_modern_schoolroad_morning
   ```

## Workflows

- `workflows/api/sdxl_game_asset_basic.json`
  General SDXL text-to-image workflow for VN backgrounds, portraits, UI icons,
  props, and concept sheets.

- `workflows/api/sdxl_sprite_strip.json`
  Wider canvas workflow for horizontal sprite strips. The prompt manifest sets
  width, height, frame count text, and filename prefixes per asset.

- `workflows/api/sdxl_pixel_asset.json`
  Pixel-art flavored workflow for TurnCombat and ArcadeCombat placeholder/final
  pixel strips.

These files are ComfyUI API-format prompt JSON. They are best used through
`run_comfy_batch.py` or ComfyUI's `/prompt` API.

## Transparency Note

Prompts may say `transparent background`, but plain SDXL does not produce a real
alpha channel. Treat those results as source art. For production PNGs, remove the
background with a post-process step, or later add a rembg/background-removal node
to the workflow.

## Reusing For Future Sandboxes

Keep the workflow JSON generic and make a new manifest:

```text
tools/comfy/prompts/<new_game>_prompts.json
```

Use the same groups:

- `vn_backgrounds`
- `vn_portraits`
- `side_combat_strips`
- `ui_icons`
- `turn_combat_pixel`
- `arcade_combat_pixel`

Only the prompt manifest should be game-specific.

## References

- ComfyUI workflow concept: https://docs.comfy.org/development/core-concepts/workflow
- ComfyUI API-format workflow submission: https://docs.comfy.org/development/cloud/overview
- ComfyUI prompt endpoint reference: https://docs.comfy.org/api-reference/cloud/workflow/submit-a-workflow-for-execution
