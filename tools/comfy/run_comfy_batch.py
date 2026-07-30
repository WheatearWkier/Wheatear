#!/usr/bin/env python3
"""
Submit Wheatear asset prompts to a local ComfyUI server.

The workflows in tools/comfy/workflows/api use stable node ids:
  1 CheckpointLoaderSimple
  2 Positive CLIPTextEncode
  3 Negative CLIPTextEncode
  4 EmptyLatentImage
  5 KSampler
  7 SaveImage
"""

from __future__ import annotations

import argparse
import copy
import json
import random
import sys
import urllib.error
import urllib.request
from pathlib import Path


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as file:
        return json.load(file)


def patch_workflow(base: dict, manifest: dict, item: dict, ckpt: str | None) -> dict:
    workflow = copy.deepcopy(base)

    global_style = manifest.get("global_style", "").strip()
    global_negative = manifest.get("global_negative", "").strip()
    positive = item.get("positive", "").strip()
    negative = item.get("negative", "").strip()

    workflow["2"]["inputs"]["text"] = ", ".join(part for part in (global_style, positive) if part)
    workflow["3"]["inputs"]["text"] = ", ".join(part for part in (global_negative, negative) if part)

    if ckpt:
        workflow["1"]["inputs"]["ckpt_name"] = ckpt

    latent = workflow["4"]["inputs"]
    latent["width"] = int(item.get("width", latent.get("width", 1024)))
    latent["height"] = int(item.get("height", latent.get("height", 1024)))
    latent["batch_size"] = int(item.get("batch_size", latent.get("batch_size", 1)))

    sampler = workflow["5"]["inputs"]
    seed = int(item.get("seed", -1))
    sampler["seed"] = random.randrange(1, 2**63 - 1) if seed < 0 else seed
    sampler["steps"] = int(item.get("steps", sampler.get("steps", 30)))
    sampler["cfg"] = float(item.get("cfg", sampler.get("cfg", 7.0)))
    sampler["sampler_name"] = item.get("sampler_name", sampler.get("sampler_name", "dpmpp_2m"))
    sampler["scheduler"] = item.get("scheduler", sampler.get("scheduler", "karras"))
    sampler["denoise"] = float(item.get("denoise", sampler.get("denoise", 1.0)))

    workflow["7"]["inputs"]["filename_prefix"] = item["filename_prefix"]
    return workflow


def submit_prompt(server: str, workflow: dict) -> dict:
    url = server.rstrip("/") + "/prompt"
    payload = json.dumps({"prompt": workflow}).encode("utf-8")
    request = urllib.request.Request(
        url,
        data=payload,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.loads(response.read().decode("utf-8"))


def write_export(export_dir: Path, item_id: str, workflow: dict) -> Path:
    export_dir.mkdir(parents=True, exist_ok=True)
    path = export_dir / f"{item_id}.json"
    with path.open("w", encoding="utf-8") as file:
        json.dump(workflow, file, ensure_ascii=False, indent=2)
        file.write("\n")
    return path


def iter_items(manifest: dict, group: str) -> list[dict]:
    groups = manifest.get("groups", {})
    if group == "all":
        items: list[dict] = []
        for group_items in groups.values():
            items.extend(group_items)
        return items
    if group not in groups:
        available = ", ".join(sorted(groups.keys()))
        raise SystemExit(f"Unknown group '{group}'. Available groups: {available}")
    return groups[group]


def main() -> int:
    parser = argparse.ArgumentParser(description="Submit game asset prompts to ComfyUI.")
    parser.add_argument("--server", default="http://127.0.0.1:8188")
    parser.add_argument("--workflow", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--group", required=True, help="Prompt group name, or 'all'.")
    parser.add_argument("--ckpt", help="Override CheckpointLoaderSimple ckpt_name.")
    parser.add_argument("--limit", type=int, default=0)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--export-dir", type=Path, help="Write patched API workflows instead of only submitting.")
    args = parser.parse_args()

    base_workflow = load_json(args.workflow)
    manifest = load_json(args.manifest)
    items = iter_items(manifest, args.group)
    if args.limit > 0:
        items = items[: args.limit]

    for item in items:
        workflow = patch_workflow(base_workflow, manifest, item, args.ckpt)
        if args.export_dir:
            path = write_export(args.export_dir, item["id"], workflow)
            print(f"exported {item['id']} -> {path}")
        if args.dry_run:
            print(f"dry-run {item['id']} -> {item['filename_prefix']}")
            continue
        try:
            result = submit_prompt(args.server, workflow)
        except urllib.error.URLError as exc:
            print(f"failed to submit {item['id']}: {exc}", file=sys.stderr)
            return 1
        prompt_id = result.get("prompt_id", "<unknown>")
        print(f"submitted {item['id']} -> prompt_id={prompt_id}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
