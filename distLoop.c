#include <stdio.h>
#include <stdlib.>
#include <rte_common.h>
#include <rte_ethdev.h>
#include <rte_distributor.h>
#include "pfm.h"
#include "pfh_comm.h"
#include "link.h"
#include "pfm_ring.h"
#include "distLoop.h"



int distLoop(__attribute__((unused)) void * args)	{
	struct rte_ring *rx_dist_ring = sys_info_g.rx_ring_ptr;
	struct rte_ring *dist_tx_ring = sys_info_g.tx_ring_ptr;
	struct rte_distributor = sys_info_g.dist_ptr;



}
