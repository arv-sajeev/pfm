#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <inttypes.h>

#include <rte_eal.h>
#include <rte_mbuf.h>
#include <rte_common.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_ring.h>
#include <rte_ether.h>
#include <rte_mbuf.h>
#include <rte_hash.h>
#include <rte_jhash.h>
#include <rte_timer.h>
#include <rte_cycles.h>
#include <rte_memcpy.h>
#include <rte_random.h>
#include <rte_lcore.h>
#include <rte_malloc.h>
#include <rte_errno.h>


#define PFM_ARP_TABLE_ENTRIES 32
#define PFM_ARP_HASH_NAME "ARP_TABLE_HASH"
#define PFM_ARP_HASH_KEY_LEN 32
#define HASH_SEED 26



static struct rte_hash* hash_mapper;



static int 
arp_init(void)	{

	struct rte_hash_parameters hash_params = {
		.name			=	PFM_ARP_HASH_NAME,
		.entries 		= 	PFM_ARP_TABLE_ENTRIES,
		.reserved		= 	0,
		.key_len		=	PFM_ARP_HASH_KEY_LEN,
		.hash_func		= 	rte_jhash,
		.hash_func_init_val	=	0,
		.socket_id		=	(int)rte_socket_id()
	};

	hash_mapper	=	rte_hash_create(&hash_params);
	if (hash_mapper == NULL)	{
			    printf("Error during arp init");
		return -1;
	}
	printf("Hash is up\n");
	return 0;
}

int main (int argc,char* argv[])	
{
	int ret = rte_eal_init(argc,argv);
	arp_init();
	int opt,key,value,pos;
	int table[PFM_ARP_TABLE_ENTRIES];

	const void *key_ptr;
	void *data_ptr;
	uint32_t ptr = 0;
	uint32_t hash_count = 0;

	while (1)	
	{
		printf("Enter operation \n1.Add key \n2.Delete key \n3.Query key\n4.Display table\n");
		scanf("%d",&opt);
		printf("Enter key : ");
		scanf("%d",&key);
		printf("Enter value : ");
		scanf("%d",&value);
		printf("key : %d and value : %d \n",key,value);
		switch (opt)	{
			case 1: 
					printf("Adding key\n");
					pos = rte_hash_lookup(hash_mapper,(void *)&key);
					if (pos == -ENOENT)	
						printf("New entry\n");
					if (pos >= 0)	{
						printf("Existing key updating value\n");
						table[pos] = value;
					}

					else {
						printf("Adding new entry\n");
						pos = rte_hash_add_key(hash_mapper,
								       (void*)&key);
						table[pos] = value;
					}
					break;
			case 2:
					printf("Deleting entry\n");
					ret = rte_hash_del_key(hash_mapper,
							       (void*)&key);
					if (ret == -ENOENT)	{
						printf("No entry\n");
					}
					if (ret >= 0)	{
						table[ret] = 0;
						printf("\nDeleted value");
					}
					break;

			case 3:
					printf("looking up hash table\n");
					pos = rte_hash_lookup(hash_mapper,
							      (void *)&key);
					if (pos == -ENOENT)
						printf("No entry found\n");
					if (pos >= 0)	{
						printf("\n table value is %d",
							table[pos]);
					}
					break;
			case 4:
					printf("Printing table\n");
					hash_count = rte_hash_count(hash_mapper);
					ptr = 0;
					while ((pos = rte_hash_iterate(hash_mapper,
								       &key_ptr,
								       &data_ptr,
								       &ptr)) >= 0)
					{
						printf("%d \n",table[pos]);
					}
					break;

			default :
				printf("\nInvalid option\n");
		}



	}
}
