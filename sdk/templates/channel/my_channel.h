#ifndef SC_MY_CHANNEL_H
#define SC_MY_CHANNEL_H

#include "seaclaw/core/allocator.h"
#include "seaclaw/core/error.h"
#include "seaclaw/channel.h"
#include <stddef.h>

/**
 * Create a custom channel implementing sc_channel_t.
 *
 * Register with channel manager:
 *   sc_channel_t ch;
 *   sc_my_channel_create(&alloc, &ch);
 *   sc_channel_manager_register(&mgr, "my_channel", "default", &ch,
 *       SC_CHANNEL_LISTENER_SEND_ONLY);
 */
sc_error_t sc_my_channel_create(sc_allocator_t *alloc, sc_channel_t *out);

void sc_my_channel_destroy(sc_channel_t *ch);

#endif /* SC_MY_CHANNEL_H */
