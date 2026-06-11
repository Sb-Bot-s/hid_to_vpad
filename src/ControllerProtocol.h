#ifndef CONTROLLER_PROTOCOL_H
#define CONTROLLER_PROTOCOL_H

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// Protocolo de red versión 0x15 (incrementado desde 0x14)
#define PROTOCOL_VERSION 0x15

// Comandos
#define CMD_ATTACH 0x01
#define CMD_INPUT  0x02

// Estructura de estado del mando (mapeo directo a lo que necesita el parcheador)
// Usamos __attribute__((packed)) para asegurar que no haya padding entre campos
// y que el tamaño sea idéntico entre diferentes arquitecturas/compiladores.
struct ControllerState {
    uint32_t buttons;      // Bits para botones (usar máscaras de VPAD)
    int16_t stick_l_x;     // Stick Izquierdo X
    int16_t stick_l_y;     // Stick Izquierdo Y
    int16_t stick_r_x;     // Stick Derecho X
    int16_t stick_r_y;     // Stick Derecho Y
    uint8_t trigger_l;     // Gatillo L
    uint8_t trigger_r;     // Gatillo R
} __attribute__((packed));

extern struct ControllerState lastControllerState;

#ifdef __cplusplus
}
#endif

#endif // CONTROLLER_PROTOCOL_H

