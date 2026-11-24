/*
 * yamldiff_builtin.c - the YAML codec read through the built-in parser,
 * under a name of its own, so that yamldiff.c can hold it beside the
 * codec read through libyaml (N-12). Nothing but the include.
 */

#define cf_codec_yaml cf_codec_yaml_builtin
#include "../../lib/serialize/src/yaml.c"
