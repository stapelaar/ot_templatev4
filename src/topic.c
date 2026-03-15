#include "topic.h"
#include <string.h>
#include <stdio.h>
#include <zephyr/sys/printk.h>

int topic_root(char *out, size_t out_len, const char *node_id)
{
    if (!out || out_len == 0 || !node_id || !*node_id) return -1;
    int n = snprintk(out, out_len, "ND%s", node_id);
    return (n > 0 && n < (int)out_len) ? 0 : -1;
}

int topic_build(char *out, size_t out_len,
                const char *root,
                const char *chan,
                const char *device,
                const char *field)
{
    if (!out || out_len == 0 || !root || !chan || !device || !field) return -1;
    int n = snprintk(out, out_len, "%s/%s/%s/%s", root, chan, device, field);
    return (n > 0 && n < (int)out_len) ? 0 : -1;
}