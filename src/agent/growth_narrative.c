#include "human/agent/growth_narrative.h"
#include "human/core/string.h"
#include <string.h>
#include <time.h>

enum { HU_GROWTH_WEEK_SEC = 7 * 24 * 3600 };

void hu_growth_narrative_init(hu_growth_narrative_t *gn) {
    if (!gn)
        return;
    memset(gn, 0, sizeof(*gn));
    gn->surface_cooldown = 5;
}

void hu_growth_narrative_deinit(hu_allocator_t *alloc, hu_growth_narrative_t *gn) {
    if (!alloc || !gn)
        return;
    for (size_t i = 0; i < gn->observation_count; i++) {
        hu_str_free(alloc, gn->observations[i].contact_id);
        hu_str_free(alloc, gn->observations[i].observation);
        hu_str_free(alloc, gn->observations[i].evidence);
        gn->observations[i].contact_id = NULL;
        gn->observations[i].observation = NULL;
        gn->observations[i].evidence = NULL;
    }
    gn->observation_count = 0;
    for (size_t i = 0; i < gn->milestone_count; i++) {
        hu_str_free(alloc, gn->milestones[i].contact_id);
        hu_str_free(alloc, gn->milestones[i].description);
        gn->milestones[i].contact_id = NULL;
        gn->milestones[i].description = NULL;
    }
    gn->milestone_count = 0;
}

hu_error_t hu_growth_narrative_add_observation(hu_allocator_t *alloc, hu_growth_narrative_t *gn,
                                               const char *contact_id, const char *observation,
                                               const char *evidence, float confidence,
                                               uint64_t observed_at) {
    if (!alloc || !gn)
        return HU_ERR_INVALID_ARGUMENT;
    if (!contact_id || !observation)
        return HU_ERR_INVALID_ARGUMENT;
    if (gn->observation_count >= HU_GROWTH_MAX_OBSERVATIONS)
        return HU_ERR_LIMIT_REACHED;

    const char *ev = evidence ? evidence : "";
    size_t ev_len = evidence ? strlen(evidence) : 0;

    char *cid = hu_strndup(alloc, contact_id, strlen(contact_id));
    char *obs = hu_strndup(alloc, observation, strlen(observation));
    char *evc = hu_strndup(alloc, ev, ev_len);
    if (!cid || !obs || !evc) {
        hu_str_free(alloc, cid);
        hu_str_free(alloc, obs);
        hu_str_free(alloc, evc);
        return HU_ERR_OUT_OF_MEMORY;
    }

    size_t k = gn->observation_count++;
    gn->observations[k].contact_id = cid;
    gn->observations[k].observation = obs;
    gn->observations[k].evidence = evc;
    gn->observations[k].confidence = confidence;
    gn->observations[k].observed_at = observed_at;
    gn->observations[k].surfaced = false;
    return HU_OK;
}

hu_error_t hu_growth_narrative_add_milestone(hu_allocator_t *alloc, hu_growth_narrative_t *gn,
                                           const char *contact_id, const char *description,
                                           uint64_t timestamp, float significance) {
    if (!alloc || !gn)
        return HU_ERR_INVALID_ARGUMENT;
    if (!contact_id || !description)
        return HU_ERR_INVALID_ARGUMENT;
    if (gn->milestone_count >= HU_GROWTH_MAX_MILESTONES)
        return HU_ERR_LIMIT_REACHED;

    char *cid = hu_strndup(alloc, contact_id, strlen(contact_id));
    char *desc = hu_strndup(alloc, description, strlen(description));
    if (!cid || !desc) {
        hu_str_free(alloc, cid);
        hu_str_free(alloc, desc);
        return HU_ERR_OUT_OF_MEMORY;
    }

    size_t k = gn->milestone_count++;
    gn->milestones[k].contact_id = cid;
    gn->milestones[k].description = desc;
    gn->milestones[k].timestamp = timestamp;
    gn->milestones[k].significance = significance;
    return HU_OK;
}

hu_error_t hu_growth_narrative_build_context(hu_allocator_t *alloc, hu_growth_narrative_t *gn,
                                           const char *contact_id, char **out, size_t *out_len) {
    if (!alloc || !gn || !out || !out_len)
        return HU_ERR_INVALID_ARGUMENT;

    *out = NULL;
    *out_len = 0;

    gn->conversations_since_last_surface++;

#ifndef HU_IS_TEST
    uint64_t now = (uint64_t)time(NULL);
#else
    uint64_t now = 2000000000ULL;
#endif

    size_t pick = (size_t)-1;
    if (contact_id) {
        for (size_t i = 0; i < gn->observation_count; i++) {
            hu_growth_observation_t *o = &gn->observations[i];
            if (o->surfaced)
                continue;
            if (o->confidence <= 0.5f)
                continue;
            if (!o->contact_id || strcmp(o->contact_id, contact_id) != 0)
                continue;
            if (gn->conversations_since_last_surface < gn->surface_cooldown)
                continue;
            pick = i;
            break;
        }
    }

    static const char mpref[] = " Milestone: ";
    size_t mile_total = 0;
    if (contact_id) {
        for (size_t i = 0; i < gn->milestone_count; i++) {
            hu_relational_milestone_t *m = &gn->milestones[i];
            if (!m->contact_id || strcmp(m->contact_id, contact_id) != 0)
                continue;
            if (m->timestamp > now || now - m->timestamp > (uint64_t)HU_GROWTH_WEEK_SEC)
                continue;
            size_t dlen = m->description ? strlen(m->description) : 0;
            mile_total += sizeof(mpref) - 1U + dlen;
        }
    }

    char *milestones_blob = NULL;
    if (mile_total > 0) {
        milestones_blob = (char *)alloc->alloc(alloc->ctx, mile_total + 1U);
        if (!milestones_blob)
            return HU_ERR_OUT_OF_MEMORY;
        char *w = milestones_blob;
        if (contact_id) {
            for (size_t i = 0; i < gn->milestone_count; i++) {
                hu_relational_milestone_t *m = &gn->milestones[i];
                if (!m->contact_id || strcmp(m->contact_id, contact_id) != 0)
                    continue;
                if (m->timestamp > now || now - m->timestamp > (uint64_t)HU_GROWTH_WEEK_SEC)
                    continue;
                size_t dlen = m->description ? strlen(m->description) : 0;
                memcpy(w, mpref, sizeof(mpref) - 1U);
                w += sizeof(mpref) - 1U;
                if (dlen && m->description)
                    memcpy(w, m->description, dlen);
                w += dlen;
            }
        }
        *w = '\0';
    }

    if (pick != (size_t)-1) {
        hu_growth_observation_t *o = &gn->observations[pick];
        char *obs_line = hu_sprintf(
            alloc,
            "[GROWTH: You've noticed %s — %s. If it comes up naturally, you could name that "
            "growth. Don't force it.%s]",
            o->observation ? o->observation : "",
            o->evidence ? o->evidence : "",
            milestones_blob ? milestones_blob : "");
        if (milestones_blob)
            alloc->free(alloc->ctx, milestones_blob, mile_total + 1U);
        milestones_blob = NULL;

        if (!obs_line)
            return HU_ERR_OUT_OF_MEMORY;

        o->surfaced = true;
        gn->conversations_since_last_surface = 0;

        *out = obs_line;
        *out_len = strlen(obs_line);
        return HU_OK;
    }

    if (milestones_blob && mile_total > 0) {
        char *wrap = hu_sprintf(alloc, "[GROWTH:%s]", milestones_blob);
        alloc->free(alloc->ctx, milestones_blob, mile_total + 1U);
        if (!wrap)
            return HU_ERR_OUT_OF_MEMORY;
        *out = wrap;
        *out_len = strlen(wrap);
        return HU_OK;
    }

    if (milestones_blob)
        alloc->free(alloc->ctx, milestones_blob, mile_total + 1U);
    return HU_OK;
}
