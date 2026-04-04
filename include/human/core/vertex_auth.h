#ifndef HU_VERTEX_AUTH_H
#define HU_VERTEX_AUTH_H

#include "allocator.h"
#include "error.h"
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

typedef struct hu_vertex_auth {
    char *client_id;
    char *client_secret;
    char *refresh_token;
    char *access_token;
    size_t access_token_len;
    time_t token_expires_at;
    hu_allocator_t *alloc;
} hu_vertex_auth_t;

/* Load ADC credentials from GOOGLE_APPLICATION_CREDENTIALS or
 * ~/.config/gcloud/application_default_credentials.json.
 * Only "authorized_user" credential type is supported. */
hu_error_t hu_vertex_auth_load_adc(hu_vertex_auth_t *auth, hu_allocator_t *alloc);

/* Ensure a valid OAuth2 access token is available, refreshing if needed.
 * Refreshes when within 120 seconds of expiry. */
hu_error_t hu_vertex_auth_ensure_token(hu_vertex_auth_t *auth, hu_allocator_t *alloc);

/* Fill a pre-allocated buffer with "Bearer <access_token>".
 * Returns HU_ERR_PROVIDER_AUTH if no token available. */
hu_error_t hu_vertex_auth_get_bearer(const hu_vertex_auth_t *auth, char *buf, size_t buf_cap);

/* Free all owned strings. The struct itself is not freed. */
void hu_vertex_auth_free(hu_vertex_auth_t *auth);

#endif /* HU_VERTEX_AUTH_H */
