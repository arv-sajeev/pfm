#include <stdio.h>
#include <stdlib.h>
#include <rte_common.h>
#include <rte_distributor.h>
#include <rte_hash.h>

#include "pfm.h"
#include "pfm_log.h"
#include "pfm_comm.h"
#include "pfm_route.h"

static struct route_table{
	struct rte_hash h;
	route_t entries[MAX_ROUTE_TABLE_ENTRIES];
} ROUTE;


