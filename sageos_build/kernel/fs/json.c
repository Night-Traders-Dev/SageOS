#include <stdint.h>
#include <stddef.h>

/*
 * json.c - Legacy C-resident JSON parser (DEPRECATED)
 *
 * This implementation is deprecated in favor of the pure-SageLang 
 * standard library module 'lib/json.sage', which provides full 
 * cJSON-compliant parsing and generation.
 */

// Legacy stub preserved for link compatibility if needed, 
// though no active kernel components should call this.
void json_parse_command(const char *data, char *name, char *binary) {
    (void)data;
    (void)name;
    (void)binary;
}
