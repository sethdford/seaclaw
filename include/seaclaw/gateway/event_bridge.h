#ifndef SC_EVENT_BRIDGE_H
#define SC_EVENT_BRIDGE_H

#include "seaclaw/bus.h"
#include "seaclaw/gateway/control_protocol.h"
#ifdef SC_HAS_PUSH
#include "seaclaw/gateway/push.h"
#endif

typedef struct sc_event_bridge {
    sc_control_protocol_t *proto;
    sc_bus_t *bus;
    sc_bus_subscriber_fn bus_cb;
#ifdef SC_HAS_PUSH
    sc_push_manager_t *push;
#endif
} sc_event_bridge_t;

void sc_event_bridge_init(sc_event_bridge_t *bridge, sc_control_protocol_t *proto, sc_bus_t *bus);
void sc_event_bridge_deinit(sc_event_bridge_t *bridge);

#ifdef SC_HAS_PUSH
void sc_event_bridge_set_push(sc_event_bridge_t *bridge, sc_push_manager_t *push);
#endif

#endif /* SC_EVENT_BRIDGE_H */
