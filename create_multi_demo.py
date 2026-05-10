#!/usr/bin/env python3
"""
Create a demo multi-instrument pack to test the new functionality
"""

import json
import os
from pathlib import Path

def create_demo_pack():
    """Create a demo pack with multiple instruments"""
    
    pack_dir = Path("Examples/MultiInstrumentDemo.patchcraft")
    pack_dir.mkdir(exist_ok=True)
    
    # Create manifest with multi-instrument support
    manifest = {
        "formatVersion": 1,
        "instrumentName": "Multi-Instrument Demo",
        "creator": "PatchCraft Demo",
        "description": "Demonstration of multi-instrument layering capabilities",
        "category": "Sample Instrument",
        "engine": "multi",
        "backgroundImage": "assets/background.png",
        "defaultPreset": "Main",
        "createdWith": "PatchCraft Studio",
        
        # Multi-instrument configuration
        "multiInstrumentMode": True,
        "instrumentIds": ["guitar", "piano"],
        "instrumentNames": ["Guitar Layer", "Piano Layer"],
        
        # Library metadata
        "libraryThumbnail": "assets/thumbnail.png",
        "tags": ["demo", "multi-instrument", "layering"],
        "version": "1.0.0",
        "website": "https://patchcraft.com"
    }
    
    # Create directories
    (pack_dir / "assets").mkdir(exist_ok=True)
    (pack_dir / "samples").mkdir(exist_ok=True)
    (pack_dir / "instruments").mkdir(exist_ok=True)
    
    # Write manifest
    with open(pack_dir / "manifest.json", "w") as f:
        json.dump(manifest, f, indent=2)
    
    # Create sample maps for each instrument
    guitar_map = {
        "zones": [
            {
                "samplePath": "samples/guitar_C3.wav",
                "rootNote": 48,
                "lowNote": 48,
                "highNote": 52,
                "lowVelocity": 1,
                "highVelocity": 127,
                "gainDb": 0.0,
                "pan": -0.5,
                "loopEnabled": False
            },
            {
                "samplePath": "samples/guitar_D3.wav",
                "rootNote": 50,
                "lowNote": 50,
                "highNote": 52,
                "lowVelocity": 1,
                "highVelocity": 127,
                "gainDb": 0.0,
                "pan": 0.0,
                "loopEnabled": False
            },
            {
                "samplePath": "samples/guitar_E3.wav",
                "rootNote": 52,
                "lowNote": 52,
                "highNote": 54,
                "lowVelocity": 1,
                "highVelocity": 127,
                "gainDb": 0.0,
                "pan": 0.5,
                "loopEnabled": False
            },
            {
                "samplePath": "samples/guitar_G3.wav",
                "rootNote": 55,
                "lowNote": 55,
                "highNote": 57,
                "lowVelocity": 1,
                "highVelocity": 127,
                "gainDb": 0.0,
                "pan": 0.5,
                "loopEnabled": False
            }
        ]
    }
    
    piano_map = {
        "zones": [
            {
                "samplePath": "samples/piano_C4.wav",
                "rootNote": 60,
                "lowNote": 60,
                "highNote": 72,
                "lowVelocity": 1,
                "highVelocity": 127,
                "gainDb": 0.0,
                "pan": -0.8,
                "loopEnabled": True
            },
            {
                "samplePath": "samples/piano_D4.wav",
                "rootNote": 62,
                "lowNote": 62,
                "highNote": 74,
                "lowVelocity": 1,
                "highVelocity": 127,
                "gainDb": -3.0,
                "pan": 0.0,
                "loopEnabled": True
            },
            {
                "samplePath": "samples/piano_E4.wav",
                "rootNote": 64,
                "lowNote": 64,
                "highNote": 76,
                "lowVelocity": 1,
                "highVelocity": 127,
                "gainDb": -3.0,
                "pan": 0.8,
                "loopEnabled": True
            }
        ]
    }
    
    # Write instrument definitions
    with open(pack_dir / "instruments" / "guitar.json", "w") as f:
        json.dump(guitar_map, f, indent=2)
    
    with open(pack_dir / "instruments" / "piano.json", "w") as f:
        json.dump(piano_map, f, indent=2)
    
    # Create placeholder sample files (just empty WAVs for demo)
    samples_dir = pack_dir / "samples"
    
    # Create guitar samples
    guitar_notes = ["C3", "D3", "E3", "G3"]
    for note in guitar_notes:
        sample_path = samples_dir / f"guitar_{note}.wav"
        # Create a simple sine wave sample (placeholder)
        # In a real scenario, these would be actual guitar recordings
        with open(sample_path, "wb") as f:
            f.write(b"PLACEHOLDER")  # Minimal WAV header would go here
    
    # Create piano samples
    piano_notes = ["C4", "D4", "E4"]
    for note in piano_notes:
        sample_path = samples_dir / f"piano_{note}.wav"
        with open(sample_path, "wb") as f:
            f.write(b"PLACEHOLDER")  # Minimal WAV header would go here
    
    print(f"Created multi-instrument demo pack: {pack_dir}")

if __name__ == "__main__":
    create_demo_pack()
