#include <criterion/criterion.h>
#include <criterion/logging.h>

#include <stdio.h>
#include <signal.h>
#include "pfm.h"
#include "pfm_log.h"
#include "pfm_utils.h"

#include "cuup.h"
#include "tunnel.h"
#include "ue_ctx.h"
#include "drb.h"
#include "pdus.h"
#include <rte_hash.h>
#include <rte_jhash.h>
#include "e1ap_comm.h"
#include "e1ap_bearer_setup.h"
#include "e1ap_bearer_modify.h"

void setup()
{
	printf("In test suite setup\n");
	pfm_retval_t ret;
	char *v[0];
	v[0] = strdup("/home/arv-sajeev/DPDK/dpdk-19.11/examples/pfm/criterion_tests/e1ap/pdus/build/pdus_test.exe");
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

TestSuite(setup_suite,.init = setup,.fini = cleanup,.description = 
"\nChecks for \n\t1. Successful pdus_setup\n\t2.Appropriate succ and fail rsp\n");

Test(setup_suite,test1,.description = 
"\nChecks for \n\t1. complete successful setup\n\t2. Appropriate succ rsp\n\t3. Failure to add duplicate")
{
	pfm_retval_t ret_val;
	int i,ret;
	ue_ctx_t ue_ctx;
	pdus_setup_req_info_t req;
	pdus_setup_succ_rsp_info_t succ_rsp;
	pdus_setup_fail_rsp_info_t fail_rsp;


	// Setting up ue_ctx
	ue_ctx.cuup_ue_id = 4;
	ue_ctx.cucp_ue_id = 5;
	ue_ctx.pdus_count = 0;
	ue_ctx.drb_count  = 0;

	// Setting up pdus req
	req.pdus_id 		= 3;
	req.drb_count 		= 2;
	req.pdus_ul_ip_addr 	= pfm_str2ip("192.168.0.1");
	req.pdus_ul_teid	= 3;

	// Setting up drb req
	req.drb_list[0].drb_id 		= 1;
	req.drb_list[0].drb_dl_ip_addr 	= pfm_str2ip("192.168.0.1");
	req.drb_list[0].drb_dl_teid	= 0;

	req.drb_list[1].drb_id 		= 2;
	req.drb_list[1].drb_dl_ip_addr 	= pfm_str2ip("192.168.0.2");
	req.drb_list[1].drb_dl_teid	= 1;

	// pdus setup
	ret_val = pdus_setup(&ue_ctx,&req,&succ_rsp,&fail_rsp);
	cr_assert(ret_val ==  PFM_SUCCESS,"successful setup");
	cr_assert(succ_rsp.pdus_id == req.pdus_id && succ_rsp.drb_setup_succ_count == req.drb_count,
		"Appropriate succ rsp");
}

















