#ifndef HU_CHANNEL_WEBHOOK_H
#define HU_CHANNEL_WEBHOOK_H

#include "human/channel.h"
#include "human/core/allocator.h"
#include "human/core/error.h"
#include <stdint.h>

typedef struct hu_webhook_channel_config {
    char *name;
    char *callback_url;  /* outbound POST URL */
    char *secret;       /* HMAC secret for verification */
    char *message_field; /* JSON field for message body (default "message") */
    char *sender_field; /* JSON field for sender ID (default "sender") */
    uint16_t max_message_len;
} hu_webhook_channel_config_t;

hu_error_t hu_webhook_channel_create(hu_allocator_t *alloc,
                                     const hu_webhook_channel_config_t *cfg,
                                     hu_channel_t *out);

void hu_webhook_channel_destroy(hu_channel_t *ch, hu_allocator_t *alloc);

/* Webhook handler for gateway integration */
hu_error_t hu_webhook_on_message(const char *body, size_t body_len,
                                 const hu_webhook_channel_config_t *cfg,
                                 char *sender_out, size_t sender_cap,
                                 char *message_out, size_t message_cap);

#endif
