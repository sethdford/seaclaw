/*
 * Skill scaffolding — generates skill project templates.
 */

#include "human/skill_scaffold.h"
#include "human/core/string.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *category_str(hu_skill_category_t cat) {
    switch (cat) {
    case HU_SKILL_CATEGORY_DATA:
        return "data";
    case HU_SKILL_CATEGORY_AUTOMATION:
        return "automation";
    case HU_SKILL_CATEGORY_INTEGRATION:
        return "integration";
    case HU_SKILL_CATEGORY_ANALYSIS:
        return "analysis";
    default:
        return "general";
    }
}

hu_error_t hu_skill_scaffold_manifest(hu_allocator_t *alloc, const hu_skill_scaffold_opts_t *opts,
                                      char **out, size_t *out_len) {
    if (!alloc || !opts || !opts->name || !out)
        return HU_ERR_INVALID_ARGUMENT;

    const char *desc = opts->description ? opts->description : "A custom human skill";
    const char *author = opts->author ? opts->author : "";
    const char *cat = category_str(opts->category);

    /* Category-specific parameter templates */
    const char *params;
    switch (opts->category) {
    case HU_SKILL_CATEGORY_DATA:
        params = "    \"properties\": {\n"
                 "      \"query\": {\n"
                 "        \"type\": \"string\",\n"
                 "        \"description\": \"SQL query or data lookup expression\"\n"
                 "      },\n"
                 "      \"format\": {\n"
                 "        \"type\": \"string\",\n"
                 "        \"enum\": [\"table\", \"json\", \"csv\"],\n"
                 "        \"description\": \"Output format for the data\"\n"
                 "      }\n"
                 "    },\n"
                 "    \"required\": [\"query\"]";
        break;
    case HU_SKILL_CATEGORY_AUTOMATION:
        params = "    \"properties\": {\n"
                 "      \"action\": {\n"
                 "        \"type\": \"string\",\n"
                 "        \"description\": \"The automation action to perform\"\n"
                 "      },\n"
                 "      \"schedule\": {\n"
                 "        \"type\": \"string\",\n"
                 "        \"description\": \"Cron expression or interval (optional)\"\n"
                 "      },\n"
                 "      \"dry_run\": {\n"
                 "        \"type\": \"boolean\",\n"
                 "        \"description\": \"Preview the action without executing\"\n"
                 "      }\n"
                 "    },\n"
                 "    \"required\": [\"action\"]";
        break;
    case HU_SKILL_CATEGORY_INTEGRATION:
        params = "    \"properties\": {\n"
                 "      \"endpoint\": {\n"
                 "        \"type\": \"string\",\n"
                 "        \"description\": \"API endpoint or webhook URL\"\n"
                 "      },\n"
                 "      \"method\": {\n"
                 "        \"type\": \"string\",\n"
                 "        \"enum\": [\"GET\", \"POST\", \"PUT\", \"DELETE\"],\n"
                 "        \"description\": \"HTTP method\"\n"
                 "      },\n"
                 "      \"payload\": {\n"
                 "        \"type\": \"string\",\n"
                 "        \"description\": \"Request body (JSON string)\"\n"
                 "      }\n"
                 "    },\n"
                 "    \"required\": [\"endpoint\"]";
        break;
    case HU_SKILL_CATEGORY_ANALYSIS:
        params =
            "    \"properties\": {\n"
            "      \"input\": {\n"
            "        \"type\": \"string\",\n"
            "        \"description\": \"Text or data to analyze\"\n"
            "      },\n"
            "      \"analysis_type\": {\n"
            "        \"type\": \"string\",\n"
            "        \"enum\": [\"summary\", \"sentiment\", \"keywords\", \"classification\"],\n"
            "        \"description\": \"Type of analysis to perform\"\n"
            "      }\n"
            "    },\n"
            "    \"required\": [\"input\"]";
        break;
    default:
        params = "    \"properties\": {\n"
                 "      \"input\": {\n"
                 "        \"type\": \"string\",\n"
                 "        \"description\": \"Primary input for the skill\"\n"
                 "      }\n"
                 "    },\n"
                 "    \"required\": [\"input\"]";
        break;
    }

    char buf[4096];
    int n = snprintf(buf, sizeof(buf),
                     "{\n"
                     "  \"name\": \"%s\",\n"
                     "  \"version\": \"0.1.0\",\n"
                     "  \"description\": \"%s\",\n"
                     "  \"author\": \"%s\",\n"
                     "  \"category\": \"%s\",\n"
                     "  \"enabled\": true,\n"
                     "  \"parameters\": {\n"
                     "    \"type\": \"object\",\n"
                     "%s\n"
                     "  },\n"
                     "  \"tags\": [\"%s\"]\n"
                     "}\n",
                     opts->name, desc, author, cat, params, cat);
    if (n < 0 || (size_t)n >= sizeof(buf))
        return HU_ERR_OUT_OF_MEMORY;

    size_t len = (size_t)n;
    char *result = (char *)alloc->alloc(alloc->ctx, len + 1);
    if (!result)
        return HU_ERR_OUT_OF_MEMORY;
    memcpy(result, buf, len + 1);
    *out = result;
    if (out_len)
        *out_len = len;
    return HU_OK;
}

hu_error_t hu_skill_scaffold_instructions(hu_allocator_t *alloc,
                                          const hu_skill_scaffold_opts_t *opts, char **out,
                                          size_t *out_len) {
    if (!alloc || !opts || !opts->name || !out)
        return HU_ERR_INVALID_ARGUMENT;

    const char *desc = opts->description ? opts->description : "A custom human skill";

    const char *when_to_use;
    const char *instructions;
    const char *example_user;
    const char *example_asst;
    const char *constraints;

    switch (opts->category) {
    case HU_SKILL_CATEGORY_DATA:
        when_to_use = "Use this skill when the user needs to query, fetch, or transform data.";
        instructions =
            "1. Parse the user's query to identify the data source and filters.\n"
            "2. Execute the query against the appropriate data source.\n"
            "3. Format the results according to the requested format (table, JSON, CSV).\n"
            "4. Summarize large result sets — show row count and key patterns.";
        example_user = "Show me all orders from last week";
        example_asst = "Here are the 47 orders from March 16-23. Total revenue: $12,450.";
        constraints = "- Never expose raw database credentials in output.\n"
                      "- Limit result sets to 100 rows; summarize larger results.\n"
                      "- Validate SQL inputs to prevent injection.";
        break;
    case HU_SKILL_CATEGORY_AUTOMATION:
        when_to_use = "Use this skill when the user wants to automate a task, set up a scheduled "
                      "job, or trigger a workflow.";
        instructions = "1. Identify the action and any scheduling requirements.\n"
                       "2. Validate that the action is safe and permitted.\n"
                       "3. If dry_run is set, preview the action without executing.\n"
                       "4. Execute the automation and report success or failure.";
        example_user = "Set up a daily backup of my notes";
        example_asst =
            "Created a daily backup job running at 2:00 AM. First run scheduled for tomorrow.";
        constraints = "- Always support dry_run mode for destructive actions.\n"
                      "- Log all automation executions for audit.\n"
                      "- Respect rate limits and resource quotas.";
        break;
    case HU_SKILL_CATEGORY_INTEGRATION:
        when_to_use = "Use this skill when the user needs to connect to an external API, webhook, "
                      "or service.";
        instructions = "1. Validate the endpoint URL and method.\n"
                       "2. Construct the request with proper headers and authentication.\n"
                       "3. Execute the API call and handle response codes.\n"
                       "4. Parse and present the response in a readable format.";
        example_user = "Check the status of my deployment on Vercel";
        example_asst = "Your latest deployment (abc123) is live. Build time: 45s, status: READY.";
        constraints = "- Never log or expose API keys or tokens.\n"
                      "- Handle timeouts and rate limits gracefully.\n"
                      "- Validate URLs before making requests.";
        break;
    case HU_SKILL_CATEGORY_ANALYSIS:
        when_to_use = "Use this skill when the user wants to analyze text, data, or patterns.";
        instructions =
            "1. Accept the input text or data for analysis.\n"
            "2. Apply the requested analysis type (summary, sentiment, keywords, classification).\n"
            "3. Present findings with confidence scores where applicable.\n"
            "4. Highlight key insights and actionable takeaways.";
        example_user = "Analyze the sentiment of these customer reviews";
        example_asst = "Analyzed 150 reviews: 72% positive, 18% neutral, 10% negative. Top "
                       "concerns: shipping speed, packaging.";
        constraints = "- Provide confidence scores for classifications.\n"
                      "- Handle multilingual input gracefully.\n"
                      "- Limit analysis to reasonable input sizes.";
        break;
    default:
        when_to_use = "Use this skill when the user asks about topics related to this domain.";
        instructions = "1. Parse the user's input to understand the request.\n"
                       "2. Perform the necessary processing or lookup.\n"
                       "3. Return a clear, concise result.";
        example_user = "Show me an example";
        example_asst = "Here is a demonstration of the skill.";
        constraints = "- Keep responses concise and relevant.\n"
                      "- Handle edge cases gracefully.";
        break;
    }

    char buf[4096];
    int n = snprintf(buf, sizeof(buf),
                     "---\n"
                     "name: %s\n"
                     "version: 0.1.0\n"
                     "---\n"
                     "\n"
                     "# %s\n"
                     "\n"
                     "%s\n"
                     "\n"
                     "## When to Use\n"
                     "\n"
                     "%s\n"
                     "\n"
                     "## Instructions\n"
                     "\n"
                     "%s\n"
                     "\n"
                     "## Examples\n"
                     "\n"
                     "**User**: %s\n"
                     "**Assistant**: %s\n"
                     "\n"
                     "## Constraints\n"
                     "\n"
                     "%s\n",
                     opts->name, opts->name, desc, when_to_use, instructions, example_user,
                     example_asst, constraints);
    if (n < 0 || (size_t)n >= sizeof(buf))
        return HU_ERR_OUT_OF_MEMORY;

    size_t len = (size_t)n;
    char *result = (char *)alloc->alloc(alloc->ctx, len + 1);
    if (!result)
        return HU_ERR_OUT_OF_MEMORY;
    memcpy(result, buf, len + 1);
    *out = result;
    if (out_len)
        *out_len = len;
    return HU_OK;
}

hu_error_t hu_skill_scaffold_init(hu_allocator_t *alloc, const hu_skill_scaffold_opts_t *opts) {
    if (!alloc || !opts || !opts->name)
        return HU_ERR_INVALID_ARGUMENT;

    if (strlen(opts->name) == 0)
        return HU_ERR_INVALID_ARGUMENT;

#if HU_IS_TEST
    /* Validate inputs only in test mode — no filesystem access */
    char *manifest = NULL;
    hu_error_t err = hu_skill_scaffold_manifest(alloc, opts, &manifest, NULL);
    if (err != HU_OK)
        return err;
    alloc->free(alloc->ctx, manifest, strlen(manifest) + 1);
    return HU_OK;
#else
    const char *base_dir = opts->output_dir ? opts->output_dir : ".";
    char dir_path[1024];
    int n = snprintf(dir_path, sizeof(dir_path), "%s/%s", base_dir, opts->name);
    if (n < 0 || (size_t)n >= sizeof(dir_path))
        return HU_ERR_OUT_OF_MEMORY;

#ifndef _WIN32
#include <sys/stat.h>
    mkdir(dir_path, 0755);
#endif

    /* Write manifest.json */
    char *manifest = NULL;
    size_t manifest_len = 0;
    hu_error_t err = hu_skill_scaffold_manifest(alloc, opts, &manifest, &manifest_len);
    if (err != HU_OK)
        return err;

    char file_path[1100];
    snprintf(file_path, sizeof(file_path), "%s/manifest.json", dir_path);
    FILE *f = fopen(file_path, "w");
    if (f) {
        fwrite(manifest, 1, manifest_len, f);
        fclose(f);
    }
    alloc->free(alloc->ctx, manifest, manifest_len + 1);

    /* Write SKILL.md */
    char *instructions = NULL;
    size_t instr_len = 0;
    err = hu_skill_scaffold_instructions(alloc, opts, &instructions, &instr_len);
    if (err != HU_OK)
        return err;

    snprintf(file_path, sizeof(file_path), "%s/SKILL.md", dir_path);
    f = fopen(file_path, "w");
    if (f) {
        fwrite(instructions, 1, instr_len, f);
        fclose(f);
    }
    alloc->free(alloc->ctx, instructions, instr_len + 1);

    return HU_OK;
#endif
}
