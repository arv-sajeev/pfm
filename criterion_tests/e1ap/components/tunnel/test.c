#include <criterion/criterion.h>
#include <stdio.h>
#include <signal.h>
#include "pfm.h"
#include "pfm_log.h"
#include "pfm_utils.h"

#include "cuup.h"
#include "tunnel.h"
#include <rte_hash.h>
#include <rte_jhash.h>


void setup()
{
	pfm_retval_t ret;
	char *v[0];
	v[0] = strdup("/home/arv-sajeev/DPDK/dpdk-19.11/examples/pfm/pfm/criterion_tests/e1ap/comps/tunnel_tests/test.c");
	ret = pfm_init(1,v);
	printf("Suite setup complete\n");
	cr_assert(ret == PFM_SUCCESS);
}
	
TestSuite(tunnel_tests,.init = setup);



Test(tunnel_tests,add_and_commit)
{
	pfm_retval_t ret;
	tunnel_t  *entry1,*entry2;
	tunnel_key_t tunnel_key1,tunnel_key2;
	int ret_int;
	// Test if key alloc is working 
	printf("value of ENOENT %d\n",ENOENT);
	printf("In test 1\n");
	ret = tunnel_key_alloc(0,TUNNEL_TYPE_PDUS,&tunnel_key1);
	cr_assert(ret == PFM_SUCCESS,"tunnel key allocation");

	// Test if tunnel_add works
	entry1 = tunnel_add(&tunnel_key1);
	cr_assert(entry1 != NULL,"allocate tunnel entry");
	// Storing this key for future use
	tunnel_key2 = tunnel_key1;

	// Commit this tunnel
	entry1->remote_ip = pfm_str2ip("192.168.77.33");
	entry1->remote_te_id = 5;
	ret = tunnel_commit(entry1);
	cr_assert(ret == PFM_SUCCESS,"commit the entry");

	// get back this entry
	entry1 = tunnel_get(&tunnel_key1);
	cr_assert(entry1 != NULL,"Entry should exist after committing");
	cr_assert((entry1->key.ip_addr == tunnel_key1.ip_addr) && (entry1->key.te_id == tunnel_key1.te_id),"Same entry");
	// try adding another with same key
	entry1 = tunnel_add(&tunnel_key1);
	cr_assert(entry1 == NULL,"Not allowed to entry with same key when one already exists");
	/*XXX// Fill the table
	for (int i = 1;i < MAX_TUNNEL_COUNT;i++)
	{
		ret = tunnel_key_alloc(0,TUNNEL_TYPE_PDUS,&tunnel_key1);
		cr_expect(ret == PFM_SUCCESS,"expect a tunnel key to be allocated");
	
		entry1 = tunnel_add(&tunnel_key1);
		cr_expect(entry1 != NULL,"Expect a tunnel entry be allocated as long as there is space");

		ret = tunnel_commit(entry1);
		cr_expect(ret == PFM_SUCCESS,"Expect the commit to be succesful");
	}
	// When the table is full all should fail
	ret = tunnel_key_alloc(0,TUNNEL_TYPE_PDUS,&tunnel_key1);
	cr_expect(ret == PFM_FAILED,"expect a tunnel key to be allocated");

	entry1 = tunnel_add(&tunnel_key1);
	cr_expect(entry1 == NULL,"Expect a tunnel entry be allocated as long as there is space");

	ret = tunnel_commit(entry1);
	cr_expect(ret == PFM_FAILED);
	XXX*/
	
	// You cannot modify tunnels with tunnel get
	entry1 = tunnel_get(&tunnel_key2);
	entry1->remote_ip = pfm_str2ip("255.255.255.255");

	// Try modifying tunnels
	entry1 = tunnel_modify(&tunnel_key2);
	entry1->remote_ip = pfm_str2ip("0.0.0.0");
	ret = tunnel_commit(entry1);
	cr_expect(ret == PFM_SUCCESS,"Commit is a success after modification");
	entry1 = tunnel_get(&tunnel_key2);
	cr_expect(entry1->remote_ip == pfm_str2ip("0.0.0.0"),"Check if modification is recorded");
	
	// Remove a tunnel
	ret = tunnel_remove(&tunnel_key2);
	cr_assert(ret == PFM_SUCCESS,"The tunnel remove is successful");
	ret = tunnel_commit(entry1);
	cr_assert(ret == PFM_SUCCESS,"Committed a removed entry");
	entry1 = tunnel_get(&tunnel_key2);
	cr_assert(entry1 == NULL,"Tunnel is removed successfully");
}


	
