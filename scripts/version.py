import os
import json
from datetime import datetime
import re

# Path to version files
VERSION_FILE = "src/version.h"
BUILD_INFO_FILE = ".build_info.json"
PLATFORMIO_INI = "platformio.ini"

def get_semantic_version():
    """Get the semantic version from platformio.ini."""
    with open(PLATFORMIO_INI, 'r') as f:
        content = f.read()
        match = re.search(r'version\s*=\s*"([^"]+)"', content)
        if match:
            return match.group(1)
    return "1.0.0"  # Default version if not found

def get_current_build_number():
    """Get the current build number from the build info file."""
    if os.path.exists(BUILD_INFO_FILE):
        with open(BUILD_INFO_FILE, 'r') as f:
            data = json.load(f)
            return data.get('build_number', 1)
    return 1

def save_build_info(semantic_version, build_number):
    """Save the build info to the build info file."""
    data = {
        'semantic_version': semantic_version,
        'build_number': build_number,
        'last_build': datetime.now().isoformat()
    }
    with open(BUILD_INFO_FILE, 'w') as f:
        json.dump(data, f, indent=2)

def update_version_header(semantic_version, build_number):
    """Update the version.h file with current version info."""
    version_content = f"""#ifndef VERSION_H
#define VERSION_H

#define FIRMWARE_VERSION "{semantic_version}"
#define FIRMWARE_BUILD_NUMBER {build_number}
#define FIRMWARE_BUILD_DATE "{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}"

#endif // VERSION_H
"""
    with open(VERSION_FILE, 'w') as f:
        f.write(version_content)

def main():
    # Get semantic version and build number
    semantic_version = get_semantic_version()
    current_build = get_current_build_number()
    new_build = current_build + 1
    
    # Save build info and update version header
    save_build_info(semantic_version, new_build)
    update_version_header(semantic_version, new_build)
    
    # Set environment variables for PlatformIO
    os.environ['BUILD_NUMBER'] = str(new_build)
    os.environ['SEMANTIC_VERSION'] = semantic_version
    
    print(f"Version: {semantic_version}")
    print(f"Build number: {new_build}")

if __name__ == "__main__":
    main() 