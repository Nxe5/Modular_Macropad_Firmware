#ifndef USBSERVER_H
#define USBSERVER_H

// The hostname to use for mDNS/NetBIOS access.
#define HOSTNAME "macropad"

// Initializes the USB CDC HTTP server.
void initUSBServer();

// Sets up the server routes
void setupServerRoutes();

// Updates the USB server state - should be called from loop()
void updateUSBServer();

#endif // USBSERVER_H