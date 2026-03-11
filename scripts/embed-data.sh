#!/bin/bash
# embed-data.sh: embed data files at compile time
# Usage: embed-data.sh <data_dir> <output_dir>

set -e

DATA_DIR="${1:-.}"
OUTPUT_DIR="${2:-src/data}"

if [ ! -d "$DATA_DIR" ]; then
    echo "Error: data directory not found: $DATA_DIR" >&2
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

# Collect all data files and generate C source
while IFS= read -r -d '' file; do
    # Get relative path from DATA_DIR
    rel_path="${file#$DATA_DIR/}"

    # Convert path to valid C identifier (replace / and . with _)
    var_name="data_$(echo "$rel_path" | sed 's/[\/.]/_/g')"

    # Generate C source file with xxd if available
    c_file="$OUTPUT_DIR/${var_name}.c"

    if command -v xxd &> /dev/null; then
        {
            echo "#include <stddef.h>"
            echo ""
            # xxd -i outputs "unsigned char name[] = { ... }; unsigned int name_len = ...;"
            # We want "const unsigned char name[] = { ... }; const size_t name_len = ...;"
            xxd -i "$file" | sed "s/unsigned char /const unsigned char /g; s/unsigned int ${var_name}_len/const size_t ${var_name}_len/"
        } > "$c_file"
    else
        # Fallback: manual hex encoding
        {
            echo "#include <stddef.h>"
            echo ""
            echo "const unsigned char ${var_name}[] = {"
            od -A n -t x1 "$file" | tr -s ' ' ',' | sed 's/^,//; s/,$//; s/,/, 0x/g; s/^/    0x/'
            echo "};"
            echo ""
            echo "const size_t ${var_name}_len = sizeof(${var_name});"
        } > "$c_file"
    fi

    echo "Embedded: $rel_path -> $var_name"
done < <(find "$DATA_DIR" -type f -print0 | sort -z)

# Count entries
ENTRY_COUNT=0
while IFS= read -r -d '' file; do
    ((ENTRY_COUNT++))
done < <(find "$DATA_DIR" -type f -print0)

# Generate the registry
registry_file="$OUTPUT_DIR/embedded_registry.c"

{
    cat << 'EOF'
#include <stddef.h>
#include <string.h>

/* Forward declarations for embedded data arrays */
EOF

    # Add forward declarations
    while IFS= read -r -d '' file; do
        rel_path="${file#$DATA_DIR/}"
        var_name="data_$(echo "$rel_path" | sed 's/[\/.]/_/g')"
        echo "extern const unsigned char ${var_name}[];"
        echo "extern const size_t ${var_name}_len;"
    done < <(find "$DATA_DIR" -type f -print0 | sort -z)

    cat << 'EOF'

typedef struct {
    const char *path;
    const unsigned char *data;
    const size_t *len_ptr;
} hu_embedded_data_entry_t;

static const hu_embedded_data_entry_t hu_embedded_data_registry[] = {
EOF

    # Add entries with len_ptr pattern (address of extern _len variable)
    while IFS= read -r -d '' file; do
        rel_path="${file#$DATA_DIR/}"
        var_name="data_$(echo "$rel_path" | sed 's/[\/.]/_/g')"
        echo "    { \"$rel_path\", ${var_name}, &${var_name}_len },"
    done < <(find "$DATA_DIR" -type f -print0 | sort -z)

    cat << EOF
    { NULL, NULL, NULL }  /* Sentinel */
};

static const size_t hu_embedded_data_count = ${ENTRY_COUNT};  /* excluding sentinel */

typedef struct {
    const char *path;
    const unsigned char *data;
    size_t len;
} hu_embedded_data_result_t;

static hu_embedded_data_result_t hu_embedded_data_result;

const hu_embedded_data_result_t *hu_embedded_data_lookup(const char *path) {
    if (path == NULL)
        return NULL;

    for (size_t i = 0; i < hu_embedded_data_count; i++) {
        if (strcmp(hu_embedded_data_registry[i].path, path) == 0) {
            hu_embedded_data_result.path = hu_embedded_data_registry[i].path;
            hu_embedded_data_result.data = hu_embedded_data_registry[i].data;
            hu_embedded_data_result.len = *hu_embedded_data_registry[i].len_ptr;
            return &hu_embedded_data_result;
        }
    }

    return NULL;
}
EOF
} > "$registry_file"

echo "Generated registry: $registry_file"
echo "Total embedded files: $ENTRY_COUNT"
