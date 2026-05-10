import os
from PIL import Image, ImageDraw
import numpy as np

def create_knob(width=64, height=64):
    """Create a modern knob image"""
    img = Image.new('RGBA', (width, height), (255, 255, 255, 0))
    
    # Outer ring
    for i in range(height):
        for j in range(width):
            dx, dy = j - width//2, i - height//2
            dist = np.sqrt(dx*dx + dy*dy)
            
            if 20 <= dist <= 24:  # Outer ring
                img.putpixel((j, i), (100, 100, 110, 255))
            elif 16 <= dist <= 20:  # Middle ring
                img.putpixel((j, i), (80, 80, 90, 255))
            elif dist <= 16:  # Inner circle
                img.putpixel((j, i), (60, 60, 70, 255))
    
    # Indicator line
    img.putpixel((width//2, height//2 - 8), (200, 200, 220, 255))
    
    return img

def create_slider(width=52, height=220):
    """Create a vertical slider image"""
    img = Image.new('RGBA', (width, height), (255, 255, 255, 0))
    
    # Track background
    track_x = width // 2 - 2
    track_width = 4
    track_y = 10
    track_height = height - 20
    
    for i in range(track_height):
        y = track_y + i
        # Track gradient
        gray = int(60 + (i / track_height) * 40)
        img.putpixel((track_x, int(y)), (gray, gray, gray, 255))
        img.putpixel((track_x + 1, int(y)), (gray, gray, gray, 255))
        img.putpixel((track_x + 2, int(y)), (gray, gray, gray, 255))
        img.putpixel((track_x + 3, int(y)), (gray, gray, gray, 255))
    
    # Slider handle
    handle_y = height // 2
    handle_height = 12
    for i in range(handle_height):
        y = handle_y - handle_height//2 + i
        for j in range(width):
            if j >= track_x - 4 and j <= track_x + track_width + 4:
                img.putpixel((int(j), int(y)), (180, 180, 200, 255))
    
    return img

def create_meter(width=48, height=48):
    """Create a VU meter image"""
    img = Image.new('RGBA', (width, height), (255, 255, 255, 0))
    
    # Background
    for i in range(height):
        for j in range(width):
            img.putpixel((int(j), int(i)), (40, 40, 50, 255))
    
    # Meter segments
    segment_height = 6
    segment_width = 8
    spacing = 2
    
    colors = [
        (0, 255, 0, 255),    # Green
        (255, 255, 0, 255),  # Yellow  
        (255, 0, 0, 255),    # Red
    ]
    
    for i in range(4):  # 4 segments
        y = height - 10 - i * (segment_height + spacing)
        color = colors[min(i, 2)]
        
        for py in range(segment_height):
            for px in range(segment_width):
                img.putpixel((4 + px, int(y + py)), color)
    
    return img

# Create output directory
output_dir = "Examples/CinematicPad.patchcraft/assets"
os.makedirs(f"{output_dir}/knobs", exist_ok=True)
os.makedirs(f"{output_dir}/sliders", exist_ok=True)
os.makedirs(f"{output_dir}/meters", exist_ok=True)

# Create assets
knob = create_knob(64, 64)
knob.save(f"{output_dir}/knobs/modern-knob.png")

slider = create_slider(52, 220)
slider.save(f"{output_dir}/sliders/modern-slider.png")

meter = create_meter(48, 48)
meter.save(f"{output_dir}/meters/vu-meter.png")

print("Created component library assets!")
