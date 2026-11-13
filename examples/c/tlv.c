/*
 * tlv.c - a device's binary settings, read through a layout.
 *
 * Many devices keep their settings as tag, length, value: two bytes of
 * tag, two of length, the value, and an end tag. There is no standard to
 * name, so the program describes its own - one table, a key and a type
 * for each tag - and the library makes a codec that reads it (D-93).
 * Every value it reads says which tag at which byte gave it:
 *
 *     net.port = 400  (int)
 *       device      400      selected   dev.cfg, byte 12
 *       default     80       shadowed
 *
 * The file is written here from the same layout, as a device would; no
 * real device's bytes are needed to try it.
 *
 *     ./tlv
 *
 * Every line of its output is checked by the suite.
 */

#include <configforge/cf_serialize.h>
#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

#define FILE_NAME "dev.cfg"

/* The layout's tags, in ascending order: a record the program does not
   need is skipped by saying so. */
static const cf_tlv_tag dev_tags[] = {
    {0x0001, "name", CF_TLV_TEXT, 0, 0, 0, NULL, 0, NULL},
    {0x0002, "net.port", CF_TLV_UINT, 2, 0, 0, NULL, 0, NULL},
    {0x0003, "net.dhcp", CF_TLV_BOOL, 1, 0, 0, NULL, 0, NULL},
    {0x0010, NULL, CF_TLV_RECORD, 12, 0, CF_TLV_SKIP, NULL, 0, NULL}};

/* One element: the tag and the length little-endian, then the value. */
static void put(FILE* f, unsigned tag, const void* value, unsigned len) {
    unsigned char head[4];

    head[0] = (unsigned char)(tag & 0xFFu);
    head[1] = (unsigned char)(tag >> 8);
    head[2] = (unsigned char)(len & 0xFFu);
    head[3] = (unsigned char)(len >> 8);
    fwrite(head, 1, sizeof head, f);
    fwrite(value, 1, len, f);
}

/* What the device would have written. */
static int write_device_file(void) {
    static const unsigned char name[]   = {'u', 'n', 'i', 't', '-', '7', 0, 0};
    static const unsigned char port[]   = {0x90, 0x01}; /* 400 */
    static const unsigned char dhcp[]   = {0x01};
    static const unsigned char record[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    static const unsigned char end[]    = {0xFF, 0xFF};
    FILE*                      f        = fopen(FILE_NAME, "wb");

    if (f == NULL) {
        return 0;
    }
    put(f, 0x0001, name, sizeof name);
    put(f, 0x0002, port, sizeof port);
    put(f, 0x0003, dhcp, sizeof dhcp);
    put(f, 0x0010, record, sizeof record);
    fwrite(end, 1, sizeof end, f);
    fclose(f);
    return 1;
}

int main(void) {
    /* Static, as any codec given to a builder is: the layout and the codec
       must outlive every configuration read with them. */
    static cf_tlv_layout dev = CF_TLV_LAYOUT_INIT;
    static cf_codec      devcfg;
    static char          why[1024];
    cf_error             err = CF_ERROR_INIT;
    cf_builder*          b;
    cf_config*           c;

    dev.name         = "devcfg";
    dev.tag_width    = 2;
    dev.length_width = 2;
    dev.order        = CF_TLV_LITTLE;
    dev.flags        = CF_TLV_END;
    dev.end_tag      = 0xFFFFu;
    dev.tags         = dev_tags;
    dev.ntags        = sizeof dev_tags / sizeof dev_tags[0];
    if (!cf_tlv_codec(&devcfg, &dev, &err)) {
        fprintf(stderr, "%s\n", err.message);
        return 1;
    }
    if (!write_device_file()) {
        fprintf(stderr, "cannot write %s\n", FILE_NAME);
        return 1;
    }

    b = cf_build(NULL);
    cf_builder_codec(b, &devcfg);
    cf_builder_file(b, "device", CF_RANK_USER_FILE, FILE_NAME, "devcfg", 0);
    cf_builder_int(b, "net.port", 80, 1, 65535, 0);
    c = cf_builder_create(b, &err);
    if (c == NULL) {
        fprintf(stderr, "%s\n", err.message);
        remove(FILE_NAME);
        return 1;
    }
    printf("net.port = %ld\n", (long)cf_get_int(c, "net.port", 0));
    cf_explain(c, "net.port", why, sizeof why);
    printf("%s\n", why);
    cf_config_free(c);
    remove(FILE_NAME);
    return 0;
}
