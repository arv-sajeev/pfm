#ifndef __PFM_RING_H__
#define __PFM_RING_H__ 1

void ring_write( struct rte_ring *rx_ring_ptr,
                struct rte_mbuf *pkt_burst[],
                const int num_pkts);


#endif
