#ifndef SERVE_CONFIG_H
#define SERVE_CONFIG_H

#include <ESPAsyncWebServer.h>

// Function declarations
void initializeConfigServer();

// Global variables
extern AsyncWebServer configServer;

#endif // SERVE_CONFIG_H