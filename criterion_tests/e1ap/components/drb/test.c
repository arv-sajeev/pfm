#include <criterion/criterion.h>
#include <criterion/logging.h>

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
	printf("In test suite setup\n");
	pfm_retval_t ret;
	char *v[0];
	v[0] = strdup("/home/arv-sajeev/DPDK/dpdk-19.11/examples/pfm/criterion_tests/e1ap/components/build/tunnel_test.exe");
	ret = pfm_init(1,v);
	cr_assert(ret == PFM_SUCCESS);
	printf("Suite setup complete\n");
}

void cleanup()
{
	int ret;
	printf("In test suite cleanup\n");
	ret = rte_eal_cleanup();
	cr_assert(ret == 0,"Test suite cleanup complete\n");
}

void test1_setup()
{
	printf("\tIn test1 setup\n");
}
void test1_cleanup()
{
	printf("\tIn test1 cleanup\n");
}


void test2_setup()
{
	printf("\tIn test2 setup\n");
}
void test2_cleanup()
{
	printf("\tIn test2 cleanup\n");
}


TestSuite(test_suite,.init = setup,.fini = cleanup);


Test(test_suite,test1,.init = test1_setup,.fini = test1_cleanup)
{
	pfm_retval_t ret;
	tunnel_t  *entry1,*entry2;
	const tunnel_t* const_entry;
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
	const_entry = tunnel_get(&tunnel_key1);
	cr_assert(const_entry != NULL,"Entry should exist after committing");
	cr_assert((const_entry->key.ip_addr == tunnel_key1.ip_addr) && (const_entry->key.te_id == tunnel_key1.te_id),"Same entry");
	// try adding another with same key
	entry1 = tunnel_add(&tunnel_key1);
	cr_assert(entry1 == NULL,"entry with key that already exists");
}

Test(test_suite,test2,.init = test2_setup,.fini = test2_cleanup)
{
	pfm_retval_t ret;
	tunnel_t  *entry1,*entry2;
	tunnel_key_t tunnel_key1,tunnel_key2;
	int ret_int,i;
	printf("\t\tIn test2\n");
	ret = tunnel_key_alloc(0,TUNNEL_TYPE_PDUS,&tunnel_key2);
	cr_expect(ret == PFM_SUCCESS);

	entry1 = tunnel_add(&tunnel_key2);
	cr_expect(entry1 != NULL);

	ret = tunnel_commit(entry1);
	cr_expect(ret == PFM_SUCCESS);

	for (i = 1;i < MAX_TUNNEL_COUNT;i++)
	{
		ret = tunnel_key_alloc(0,TUNNEL_TYPE_PDUS,&tunnel_key1);
		cr_expect(ret == PFM_SUCCESS,"expect a tunnel key to be allocated %d",i);
	
		entry1 = tunnel_add(&tunnel_key1);
		cr_expect(entry1 != NULL,"Expect a tunnel entry be allocated as long as there is space %d",i);

		ret = tunnel_commit(entry1);
		cr_expect(ret == PFM_SUCCESS,"Expect the commit to be succesful %d",i);
	}
	printf("Finished filling it up\n");
	// When the table is full all should fail
	ret = tunnel_key_alloc(0,TUNNEL_TYPE_PDUS,&tunnel_key1);
	cr_expect(ret == PFM_FAILED,"expect a tunnel key to be allocated %d",i);
	printf("\nOverflow detected\n");
	
	entry1 = tunnel_get(&tunnel_key2);
	cr_assert(entry1 != NULL,"Should get something to delete");

	ret = tunnel_remove(&tunnel_key2);
	cr_assert(ret == PFM_SUCCESS,"Expect removal to go well");

	ret = tunnel_commit(entry1);
	cr_assert(ret == PFM_SUCCESS,"Committed removal successfully");

	entry1 = tunnel_add(&tunnel_key2);
	cr_expect(entry1 != NULL,"Expect free space after the tunnel_remove");

	ret = tunnel_commit(entry1);
	cr_expect(ret == PFM_SUCCESS,"Expect commit to be successful");
	
}
