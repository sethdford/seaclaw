#ifndef SC_AGENT_TOOL_CONTEXT_H
#define SC_AGENT_TOOL_CONTEXT_H

typedef struct sc_agent sc_agent_t;

/* Set/clear the current agent for tool execution (thread-local).
 * Called by agent loop at turn start/end so tools like send_message can access
 * the calling agent (mailbox, agent id). */
void sc_agent_set_current_for_tools(sc_agent_t *agent);
void sc_agent_clear_current_for_tools(void);
sc_agent_t *sc_agent_get_current_for_tools(void);

#endif /* SC_AGENT_TOOL_CONTEXT_H */
