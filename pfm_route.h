#ifndef __PFM_ROUTE_H__
#define __PFM_ROUTE_H__ 1


#include <rte_ether.h>
#include <rte_lpm.h>

#define PFM_ROUTE_LPM_MAX_ENTRIES 	32
#define PFM_ROUTE_LPM_MAX_TBL8S		8
#define PFM_ROUTE_LPM_FLAGS		0
#define PFM_ROUTE_LPM_NAME		"PFM_ROUTE_LPM"




typedef struct route_table_entry {
	uint8_t 	link_id;
	pfm_ip_addr_t 	net_mask;
	uint8_t	net_mask_depth;
	pfm_ip_addr_t	gateway_addr;
} route_t;

typedef struct route_table	{
	struct rte_lpm *lpm_mapper;
	route_t entries[PFM_ROUTE_LPM_MAX_ENTRIES];
} route_table_t;



/*****************
 *
 * 	Function to craete lpm handler using the pfm_lpm_config structure provided
 *	@params
 *	 void
 *
 * 	@returns
 * 	 - 0 if success, -1 on error
 *
 *
 * **************/
 
 int lpm_init(void);


/*****************
 *
 * 	
 *
 * 	search for entry exisiting in ROUTE table, if not add a new entry and update LPM
 *
 *	@params
 *	 link_id 	- 	the linkid that is to be updated	(uint16_t)
 *	 net_mask	-	net_mask that is to be updated		(pfm_ip_addr_t)
 *	 net_mask_depth - 	depth of the net_mask to be updated	(uint16_t)
 *	 gateway_addr	-	address of gateway to be taken 	(pfm_ip_addr_t)
 *
 *	@returns
 *	 0 if success or -error number
 *
 *
 *****************/


int pfm_route_add(pfm_ip_addr_t net_mask,uint8_t net_mask_depth,pfm_ip_addr_t gateway_addr,uint16_t link);


/********************
 *
 * 	route_query
 *
 * 	search for entry for given ip address and return route info
 *
 * 	@params
 * 	 ip_addr	-	key to query route table		(pfm_ip_addr_t)
 * 
 * 	@returns 
 * 	 the route table entry if found, null other wise 		(route_t)
 *
 *********************/

route_t *pfm_route_query(pfm_ip_addr_t ip_addr);

/**********************
 *
 * 	route_print
 *
 * 	list all the current table entrie in route table,
 * 	and print to the file stram provided
 *
 * 	@params
 *	 fp		- 	File stream to print the output to 	(FILE *)
 *
 **********************/

void pfm_route_print(FILE *fp);

#endif
