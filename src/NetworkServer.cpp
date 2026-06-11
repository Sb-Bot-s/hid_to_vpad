#include "ControllerProtocol.h"
#include <arpa/inet.h>
#include <controller_patcher/ControllerPatcher.hpp>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <nn/ac.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utils/logger.h>
#include <wups.h>

#include <notifications/notifications.h>

static void ShowNotification(const char *text) {
    NotificationModule_AddInfoNotification(text);
}

static void ShowError(const char *text) {
    NotificationModule_AddErrorNotification(text);
}

#define NETWORK_PORT 8112
#define UDP_PORT     8113

static OSThread networkThread;
static uint8_t networkThreadStack[0x4000] ALIGN_AS(8);
static bool networkThreadRunning = false;

struct ControllerState lastControllerState = {0};

static int server_socket = -1;
static int udp_socket    = -1;


static int NetworkThreadEntryPoint(int argc, const char **argv) {
    DEBUG_FUNCTION_LINE("Network thread started");

    server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket < 0) return -1;

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(NETWORK_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        close(server_socket);
        return -1;
    }

    listen(server_socket, 1);

    udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    while (networkThreadRunning) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket    = accept(server_socket, (struct sockaddr *) &client_addr, &client_len);

        if (client_socket >= 0) {
            char notificationText[128];
            snprintf(notificationText, sizeof(notificationText), "Client connected: %s", inet_ntoa(client_addr.sin_addr));
            ShowNotification(notificationText);
            DEBUG_FUNCTION_LINE("Client connected from %s", inet_ntoa(client_addr.sin_addr));

            // Handshake
            uint8_t version = PROTOCOL_VERSION;
            send(client_socket, &version, 1, 0);

            uint8_t client_version = 0;
            if (recv(client_socket, &client_version, 1, 0) > 0) {
                send(client_socket, &version, 1, 0);

                // Process commands
                while (networkThreadRunning) {
                    uint8_t cmd = 0;
                    if (recv(client_socket, &cmd, 1, 0) <= 0) break;

                    if (cmd == CMD_ATTACH) {
                        uint32_t handle;
                        uint16_t vid, pid;
                        recv(client_socket, &handle, 4, 0);
                        recv(client_socket, &vid, 2, 0);
                        recv(client_socket, &pid, 2, 0);

                        // Fake success response
                        uint8_t resp[] = {0xE0, 0xE8, 0x00, 0x00, 0x00}; // Config found, Userdata OK, Slot 0, Pad 0
                        send(client_socket, resp, sizeof(resp), 0);
                        DEBUG_FUNCTION_LINE("Device attached: 0x%04X:0x%04X", vid, pid);
                    } else if (cmd == CMD_INPUT) {
                        ControllerState state;
                        if (recv(client_socket, &state, sizeof(ControllerState), MSG_WAITALL) == sizeof(ControllerState)) {
                            lastControllerState = state;
                            DEBUG_FUNCTION_LINE("Input received: buttons=0x%08X", state.buttons);
                        }
                    }
                }
            }

            close(client_socket);
            ShowNotification("Client disconnected");
            DEBUG_FUNCTION_LINE("Client disconnected");
        }
        OSSleepTicks(OSMillisecondsToTicks(100));
    }

    close(server_socket);
    close(udp_socket);
    return 0;
}

void StartNetworkServer() {
    if (networkThreadRunning) return;
    networkThreadRunning = true;
    OSCreateThread(&networkThread, NetworkThreadEntryPoint, 0, nullptr, networkThreadStack + sizeof(networkThreadStack), sizeof(networkThreadStack), 20, OS_THREAD_ATTRIB_AFFINITY_ANY);
    OSResumeThread(&networkThread);
}

void StopNetworkServer() {
    networkThreadRunning = false;
    // Potentially join thread here
}
