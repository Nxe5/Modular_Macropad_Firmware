# SvelteKit Integration for Modular Macropad

This document explains how the Modular Macropad firmware has been enhanced to support SvelteKit-based web applications.

## Overview

The firmware now includes a specialized web server implementation designed to handle SvelteKit's routing and file structure. Key changes include:

1. Transition from SPIFFS to LittleFS for improved file handling
2. Expanded partition size for the file system
3. Specialized WebServerManager for SvelteKit support
4. Automated build script for integrating SvelteKit builds

## Key Features

- **SPA Routing Support**: The WebServerManager properly handles client-side routing by directing unmatched routes to index.html
- **GZIP Compression**: All web assets are automatically compressed to save storage space
- **Optimized File Serving**: Specific routes for SvelteKit's structure (_app/, assets/, etc.)
- **API Integration**: A structured way to add API endpoints that integrate with the frontend

## Build Process

The integration includes an automated build process that:

1. Builds the SvelteKit application from the `Modular_Macropad_Front_End_v2` directory
2. Compresses the build output with GZIP
3. Copies the files to the appropriate location for LittleFS

## File Structure

SvelteKit files are stored in the LittleFS filesystem at `/web/`, maintaining the structure from the SvelteKit build output:

```
/web/
  /_app/           # SvelteKit app files
  /assets/         # Static assets
  /favicon.png     # Favicon
  index.html       # Main entry point
```

## Configuration

The SvelteKit application should be configured using the static adapter with these settings:

```javascript
// svelte.config.js
import adapter from '@sveltejs/adapter-static';

export default {
  kit: {
    adapter: adapter({
      pages: 'build',
      assets: 'build',
      fallback: 'index.html',  // Important for SPA mode
      precompress: false
    }),
    paths: {
      relative: false
    }
  }
};
```

## Usage

1. Develop your SvelteKit application in the `Modular_Macropad_Front_End_v2` directory
2. The build script will automatically build and deploy it during firmware compilation
3. Access the application via the ESP32's IP address when connected

## API Integration

The WebServerManager provides a structured way to add API endpoints:

```cpp
// Add a basic endpoint
webServerManager->addAPIEndpoint("status", statusHandler);

// Add a more complex endpoint with JSON response
webServerManager->addAPIEndpoint("config/led", [](AsyncWebServerRequest *request) {
  DynamicJsonDocument doc(1024);
  // Add data to doc
  String output;
  serializeJson(doc, output);
  request->send(200, "application/json", output);
});
```

## Troubleshooting

If you encounter issues with the SvelteKit integration:

1. Check the LittleFS diagnostics output on the serial console
2. Verify that your SvelteKit build is using the static adapter with the correct settings
3. Ensure the partition table has sufficient space for your web application
4. Check that all necessary directories exist in the LittleFS filesystem 