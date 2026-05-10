#!/usr/bin/env python3
"""
Setup Mac repository and push to GitHub
"""

import os
import sys
import subprocess
from pathlib import Path

def create_github_repo():
    """Create GitHub repository for Mac version"""
    
    print("Setting up Mac GitHub repository...")
    
    # Check if we're in Mac directory
    mac_dir = Path("../PatchCraft-Mac")
    if not mac_dir.exists():
        print("❌ Mac directory not found!")
        return False
    
    # Change to Mac directory
    os.chdir(mac_dir)
    
    # Initialize git if not already done
    if not (mac_dir / ".git").exists():
        print("Initializing git repository...")
        subprocess.run(["git", "init"], check=True)
    
    # Add all files
    subprocess.run(["git", "add", "."], check=True)
    
    # Create initial commit if needed
    try:
        subprocess.run(["git", "commit", "-m", "Initial Mac version setup"], check=True)
        print("✅ Git repository ready")
    except subprocess.CalledProcessError:
        print("📝 Git repository already committed")
    
    # Create repository on GitHub using GitHub CLI
    try:
        # Check if gh CLI is available
        subprocess.run(["gh", "--version"], check=True, capture_output=True)
        
        # Create repository
        result = subprocess.run([
            "gh", "repo", "create", 
            "PatchCraft-Mac", 
            "--public", 
            "--description", "PatchCraft Studio for macOS - Professional sample instrument designer",
            "--source", "."
        ], check=True, capture_output=True)
        
        print("✅ GitHub repository created successfully!")
        print("Repository: https://github.com/plugmakr/PatchCraft-Mac")
        return True
        
    except subprocess.CalledProcessError as e:
        print(f"❌ GitHub CLI error: {e}")
        print("Please create repository manually at: https://github.com/plugmakr/PatchCraft-Mac")
        return False
    except FileNotFoundError:
        print("❌ GitHub CLI not found")
        print("Please install GitHub CLI: https://cli.github.com/")
        print("Or create repository manually at: https://github.com/plugmakr/PatchCraft-Mac")
        return False

def setup_github_with_token():
    """Setup using personal access token"""
    
    print("Setting up GitHub repository with token...")
    
    mac_dir = Path("../PatchCraft-Mac")
    if not mac_dir.exists():
        print("❌ Mac directory not found!")
        return False
    
    os.chdir(mac_dir)
    
    # Instructions for manual setup
    print("\n📋 Manual GitHub Setup Instructions:")
    print("1. Go to https://github.com/plugmakr/PatchCraft-Mac")
    print("2. Create new repository (if doesn't exist)")
    print("3. Run these commands in PatchCraft-Mac directory:")
    print("   git remote add origin https://github.com/plugmakr/PatchCraft-Mac.git")
    print("   git branch -M main")
    print("   git push -u origin main")
    print("\n📁 Repository contents:")
    
    # Show repository contents
    for item in mac_dir.rglob("*"):
        if item.is_file():
            rel_path = item.relative_to(mac_dir)
            size = item.stat().st_size
            print(f"   {rel_path} ({size} bytes)")
    
    return True

def main():
    print("PatchCraft Mac Repository Setup")
    print("=" * 40)
    
    if len(sys.argv) > 1 and sys.argv[1] == "--token":
        setup_github_with_token()
    else:
        create_github_repo()

if __name__ == "__main__":
    main()
