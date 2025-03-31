#!/usr/bin/env python3

import os
import sys
import shutil
import gzip
import glob
import subprocess
from pathlib import Path

# Make sure we can handle Import("env") for both command line and PlatformIO contexts
try:
    Import("env")
    project_dir = env.get("PROJECT_DIR")
except:
    print("Running script outside of PlatformIO environment - using current directory")
    project_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Configuration variables
FRONTEND_DIR = os.path.join(os.path.dirname(project_dir), "Modular_Macropad_Front_End_v2")
OUTPUT_DIR = os.path.join(project_dir, "data", "web")
BUILD_DIR = os.path.join(FRONTEND_DIR, "build")

def get_package_manager():
    """Detect which package manager is being used for the SvelteKit project"""
    if os.path.exists(os.path.join(FRONTEND_DIR, "pnpm-lock.yaml")):
        return "pnpm"
    if os.path.exists(os.path.join(FRONTEND_DIR, "yarn.lock")):
        return "yarn"
    if os.path.exists(os.path.join(FRONTEND_DIR, "package-lock.json")) or os.path.exists(os.path.join(FRONTEND_DIR, "package.json")):
        return "npm"
    return None

def build_frontend():
    """Build the SvelteKit frontend"""
    print("Building SvelteKit frontend...")
    
    if not os.path.exists(FRONTEND_DIR):
        print(f"ERROR: Frontend directory not found at {FRONTEND_DIR}")
        print("Please make sure your SvelteKit project is in the correct location")
        return False
    
    package_manager = get_package_manager()
    if not package_manager:
        print("No package manager detected. Please install dependencies in the frontend directory.")
        return False
    
    print(f"Detected package manager: {package_manager}")
    
    original_dir = os.getcwd()
    os.chdir(FRONTEND_DIR)
    
    try:
        # Install dependencies if needed
        if not os.path.exists(os.path.join(FRONTEND_DIR, "node_modules")):
            print("Installing dependencies...")
            subprocess.run([package_manager, "install"], check=True)
        
        # Build the project
        print("Building frontend...")
        subprocess.run([package_manager, "run", "build"], check=True)
        
        # Check if build was successful
        if not os.path.exists(BUILD_DIR):
            print(f"ERROR: Build directory {BUILD_DIR} not found after build")
            os.chdir(original_dir)
            return False
            
        os.chdir(original_dir)
        return True
    except subprocess.CalledProcessError as e:
        print(f"Error during build process: {e}")
        os.chdir(original_dir)
        return False
    except Exception as e:
        print(f"Unexpected error during build: {e}")
        os.chdir(original_dir)
        return False

def compress_file(file_path):
    """Compress a file with gzip"""
    try:
        with open(file_path, 'rb') as f_in:
            with gzip.open(f"{file_path}.gz", 'wb', compresslevel=9) as f_out:
                shutil.copyfileobj(f_in, f_out)
        
        # Remove the original file (we only need the gzipped version)
        os.remove(file_path)
        return True
    except Exception as e:
        print(f"Error compressing file {file_path}: {e}")
        return False

def process_frontend_files():
    """Process and compress frontend build files for LittleFS"""
    print(f"Processing frontend files from {BUILD_DIR}")
    
    # Verify build directory exists
    if not os.path.exists(BUILD_DIR):
        print(f"ERROR: Build directory {BUILD_DIR} not found")
        return False
    
    # Create required directories
    try:
        # Make sure output directory exists
        os.makedirs(OUTPUT_DIR, exist_ok=True)
        
        # Clean output directory
        for item in os.listdir(OUTPUT_DIR):
            item_path = os.path.join(OUTPUT_DIR, item)
            if os.path.isfile(item_path):
                os.remove(item_path)
            elif os.path.isdir(item_path):
                shutil.rmtree(item_path)
                
        # Ensure data directory structure exists
        data_dir = os.path.dirname(OUTPUT_DIR)
        os.makedirs(data_dir, exist_ok=True)
        
        # Copy files from build directory
        print(f"Copying files from {BUILD_DIR} to {OUTPUT_DIR}")
        shutil.copytree(BUILD_DIR, OUTPUT_DIR, dirs_exist_ok=True)
        
        # Compress all files
        print("Compressing files...")
        compressed_count = 0
        error_count = 0
        for root, dirs, files in os.walk(OUTPUT_DIR):
            for file in files:
                file_path = os.path.join(root, file)
                # Exclude already compressed files
                if not file.endswith('.gz'):
                    if compress_file(file_path):
                        compressed_count += 1
                    else:
                        error_count += 1
        
        print(f"Frontend files processed. Compressed {compressed_count} files with {error_count} errors.")
        return error_count == 0
    except Exception as e:
        print(f"Error processing frontend files: {e}")
        return False

def main():
    """Main function to build and process SvelteKit frontend"""
    print("Starting SvelteKit build process...")
    
    # Check if frontend directory exists
    if not os.path.exists(FRONTEND_DIR):
        print(f"Warning: Frontend directory {FRONTEND_DIR} not found!")
        print("SvelteKit build process will be skipped.")
        return
    
    # Build frontend
    if build_frontend():
        # Process the build files
        if process_frontend_files():
            print("SvelteKit build completed and prepared for LittleFS")
        else:
            print("Failed to process SvelteKit build files")
    else:
        print("Failed to build SvelteKit frontend")

if __name__ == "__main__" or "PLATFORMIO" in os.environ:
    main() 