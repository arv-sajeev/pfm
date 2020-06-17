#include <criterion/criterion.h>
#include <stdio.h>
#include <signal.h>
#include "pfm.h"
#include "pfm_log.h"
#include "pfm_utils.h"

#include "cuup.h"
#include "tunnel.h"
#include "ue_ctx.h"
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
	printf("Suite setup comlete\n");
}


void cleanup()
{
	int ret;
	printf("In test suite cleanup\n");
	ret = rte_eal_cleanup();
	cr_assert(ret == 0,"test suite setup complete\n");
}


TestSuite(ue_ctx_test_suite,.init = setup,.fini = cleanup,
	.description = "Test to verify correctness of UE context interface");


Test(ue_ctx_test_suite,addition,.description = "Tests allocating a new ue_ctx")
{








	


























