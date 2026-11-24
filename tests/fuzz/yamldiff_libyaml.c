/*
 * yamldiff_libyaml.c - the YAML codec read through libyaml, under a name
 * of its own, so that yamldiff.c can hold it beside the codec read
 * through the built-in parser (N-12). Nothing but the include.
 */

#define CF_YAML_LIBYAML 1
#define cf_codec_yaml cf_codec_yaml_libyaml
#include "../../lib/serialize/src/yaml.c"
