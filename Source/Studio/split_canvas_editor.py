import re
import os

with open("m:/AudiCode/PCraft/Source/Studio/CanvasEditor.cpp", "r", encoding="utf-8") as f:
    lines = f.readlines()

# Find the start of the first method which is the constructor
start_idx = 0
for i, line in enumerate(lines):
    if line.startswith("    CanvasEditor::CanvasEditor"):
        start_idx = i
        break

header_lines = lines[:start_idx]
methods = []

current_method = []
current_name = ""

for line in lines[start_idx:]:
    # Method definition start
    match = re.match(r'^    (?:[\w:<>]+ )?CanvasEditor::([~\w]+)\s*\(', line)
    # Check if it's actually a method definition (starts with 4 spaces, optionally a return type, then Class::Method)
    if match and (line.startswith("    void ") or line.startswith("    bool ") or line.startswith("    int ") or line.startswith("    CanvasEditor::")):
        if current_method:
            methods.append((current_name, current_method))
        current_name = match.group(1)
        current_method = [line]
    else:
        if current_method:
            current_method.append(line)

if current_method:
    methods.append((current_name, current_method))

# Classify methods
rendering = []
interaction = []
actions = []
main = []

for name, method_lines in methods:
    if name.startswith("draw") or name == "paint":
        rendering.extend(method_lines)
    elif name.startswith("mouse") or name.startswith("hit") or name.startswith("edit") or name.startswith("drum") or name.startswith("arp") or name == "timerCallback" or name == "keyPressed":
        interaction.extend(method_lines)
    elif name in ["showContextMenu", "collectCanvasActionEntries", "launchCanvasActionPicker"]:
        actions.extend(method_lines)
    else:
        main.extend(method_lines)

with open("m:/AudiCode/PCraft/Source/Studio/CanvasEditor_Rendering.cpp", "w", encoding="utf-8") as f:
    f.writelines(rendering)

with open("m:/AudiCode/PCraft/Source/Studio/CanvasEditor_Interaction.cpp", "w", encoding="utf-8") as f:
    f.writelines(interaction)

with open("m:/AudiCode/PCraft/Source/Studio/CanvasEditor_Actions.cpp", "w", encoding="utf-8") as f:
    f.writelines(actions)

with open("m:/AudiCode/PCraft/Source/Studio/CanvasEditor.cpp", "w", encoding="utf-8") as f:
    f.writelines(header_lines)
    f.writelines(main)
    f.write("\n")
    f.write("#include \"CanvasEditor_Rendering.cpp\"\n")
    f.write("#include \"CanvasEditor_Interaction.cpp\"\n")
    f.write("#include \"CanvasEditor_Actions.cpp\"\n")

print("Splitting complete.")
print(f"Header: {len(header_lines)} lines")
print(f"Main: {len(main)} lines")
print(f"Rendering: {len(rendering)} lines")
print(f"Interaction: {len(interaction)} lines")
print(f"Actions: {len(actions)} lines")
