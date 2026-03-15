#pragma once
#include <stddef.h>

/* Bouw "ND<nodeId>", bv. node_id="30" -> "ND30" */
int topic_root(char *out, size_t out_len, const char *node_id);

/* Bouw "NDxx/<chan>/<device>/<field>" */
int topic_build(char *out, size_t out_len,
                const char *root,      /* bv. "ND30" */
                const char *chan,      /* "OUT" | "STATUS" */
                const char *device,    /* "SHT41-1", "DS18B20-2", ... */
                const char *field);    /* "TEMP", "RH", "CO2", "uptime", ... */