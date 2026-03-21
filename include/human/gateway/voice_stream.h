#ifndef HU_VOICE_STREAM_H
#define HU_VOICE_STREAM_H

#include "human/bus.h"
#include "human/gateway/control_protocol.h"
#include "human/gateway/ws_server.h"

void hu_voice_stream_attach_bus(hu_bus_t *bus, hu_control_protocol_t *proto);
void hu_voice_stream_detach_bus(hu_bus_t *bus);

void hu_voice_stream_on_binary(hu_control_protocol_t *proto, hu_ws_conn_t *conn,
                               const char *data, size_t data_len);

void hu_voice_stream_on_conn_close(hu_ws_conn_t *conn);

#endif /* HU_VOICE_STREAM_H */
