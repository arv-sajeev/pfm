#ifndef __PFM_H__
#define __PFM_H__ 1

#include <rte_mbuf.h>
#include "pfm_log.h"

typedef enum
{
	PROTOCOL_UDP    = 17
} pfm_protocol_t;

typedef uint32_t pfm_ip_addr_t;

typedef enum 
{
	PFM_SUCCESS,
	PFM_FAILED,
	PFM_NOT_FOUND
} pfm_retval_t;

typedef enum
{
	PFM_FALSE	= 0,
	PFM_TRUE	= 1
} pfm_bool_t;

pfm_retval_t pfm_init(int argc, char *argv[]);


#endif
