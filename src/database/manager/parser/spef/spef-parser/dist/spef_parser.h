/*
Gutengerg Post Parser, the C bindings.

Warning, this file is autogenerated by `cbindgen`.
Do not modify this manually.

*/

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * Spef Exchange data structure with cpp
 */
typedef struct SpefExchange SpefExchange;

/**
 * Store everthing about a net
 * Conn entry example: 3 *1:2 0.000520945
 * name: "1:2"
 * direction: ConnectionType::INPUT
 * coordinates: (633.84, 0.242)
 * driving_cell: "sky130_fd_sc_hd__dfxtp_1"
 */
typedef struct SpefNet SpefNet;

void free_c_char(char *s);

void *rust_parser_spef(const char *spef_path);

void *rust_covert_spef_file(struct SpefExchange *c_spef_data);

void *rust_convert_spef_net(struct SpefNet *c_spef_net);
