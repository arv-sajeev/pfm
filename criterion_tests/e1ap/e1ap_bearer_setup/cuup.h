#ifndef __CUUP_H__
#define __CUUP_H__ 1

#define MAX_UE_COUNT		1000
#define MAX_PDUS_PER_UE		4	// range 1 to 256
#define MAX_DRB_PER_PDUS	4	// range 1 to 32
#define MAX_FLOWS_PER_PDUS	4	// rnage 1 to 32
#define MAX_DRB_PER_UE		MAX_DRB_PER_PDUS	// range 1 to 32


#define MAX_TUNNEL_COUNT (MAX_UE_COUNT * MAX_PDUS_PER_UE * MAX_DRB_PER_UE)

#define NEW_UL_FLOW_NOTIFY_TIMEOUT	100	// in msec
#define NEW_UL_FLOW_NOTIFY_BUNDLE_SIZE  	

#endif
