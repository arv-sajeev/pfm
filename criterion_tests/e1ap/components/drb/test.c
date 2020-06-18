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
	v[0] = strdup("/home/arv-sajeev/DPDK/dpdk-19.11/examples/pfm/criterion_tests/e1ap/components/drb/build/drb_test.exe");
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


TestSuite(setup_suite,.init = setup,.fini = cleanup);

Test(setup_suite,test1,.description = "\nChecks for \n\t1. Successful drb_setup\n\t2. Appropriate succ rsp")
{
	pfm_retval_t ret_val;
	int ret,i;
	ue_ctx_t ue_ctx;
	drb_setup_req_info_t req;
	drb_setup_fail_rsp_info_t fail_rsp;
	drb_setup_succ_rsp_info_t succ_rsp;

	// Setup a ue to do all this stuff on 
	ue_ctx.cuup_ue_id = 4;
	ue_ctx.cucp_ue_id = 5;
	ue_ctx.pdus_count = 0;
	ue_ctx.drb_count  = 0;

	//Setup request
	req.drb_id = 3;
	req.drb_dl_ip_addr = pfm_str2ip("192.168.0.10");
	req.drb_dl_teid = 4;

	ret_val = drb_setup(&ue_ctx,&req,&succ_rsp,&fail_rsp);
	cr_assert(ret_val !=  PFM_FAILED,"Successful drb_setup");
	cr_assert(succ_rsp.drb_id == req.drb_id,"DRB from req and rsp match");
	cr_assert(ue_ctx.drb_tunnel_list[0]->remote_ip == req.drb_dl_ip_addr,"DRB entry an request contents match");
	cr_assert(ue_ctx.drb_tunnel_list[0]->remote_te_id == req.drb_dl_teid,"DRB entry an request contents match");
	
	
}


Test(setup_suite,test2,.description = "\nChecks for \n\t1. Successful drb_setup till MAX_DRB_PER_UE \n\t2.Fail for overflow\n\t3.Fail rsp")
{
	pfm_retval_t ret_val;
	int ret,i;

	ue_ctx_t ue_ctx;
	drb_setup_req_info_t req;
	drb_setup_fail_rsp_info_t fail_rsp;
	drb_setup_succ_rsp_info_t succ_rsp;

	// Setup a ue to do all this stuff on 
	ue_ctx.cuup_ue_id = 4;
	ue_ctx.cucp_ue_id = 5;
	ue_ctx.pdus_count = 0;
	ue_ctx.drb_count  = 0;

	//Setup request
	req.drb_id = 3;
	req.drb_dl_ip_addr = pfm_str2ip("192.168.0.10");
	req.drb_dl_teid = 4;

	for (i = 0;i < MAX_DRB_PER_UE;i++)
	{	

		ret_val = drb_setup(&ue_ctx,&req,&succ_rsp,&fail_rsp);
		cr_assert(ret_val !=  PFM_FAILED,"Successful drb_setup");
		cr_assert(succ_rsp.drb_id == req.drb_id,"DRB from req and rsp match");

		cr_assert(ue_ctx.drb_tunnel_list[i]->remote_ip == req.drb_dl_ip_addr,
						"DRB entry an request contents match");

		cr_assert(ue_ctx.drb_tunnel_list[i]->remote_te_id == req.drb_dl_teid,
						"DRB entry an request contents match");


		req.drb_id++;
		req.drb_dl_ip_addr++;
		req.drb_dl_teid++;
	}
	ret_val = drb_setup(&ue_ctx,&req,&succ_rsp,&fail_rsp);
	cr_assert(ret_val ==  PFM_FAILED,"Overflow detected");  
	cr_assert(fail_rsp.drb_id == req.drb_id,"req and fail DRB id match");
	cr_assert(fail_rsp.cause == FAIL_CAUSE_RNL_RESOURCE_UNAVAIL);
}

TestSuite(modify_suite,.init = setup,.fini = cleanup);
Test(modify_suite,test1,.description = "\nChecks for \n\t1. Successful modification \n\t2. Appropriate succ rsp \n\t3. Modify fails if drb doesn't exist")
{
	pfm_retval_t ret_val;
	int ret,i;
	ue_ctx_t ue_ctx;
	drb_setup_req_info_t req;
	drb_setup_fail_rsp_info_t fail_rsp;
	drb_setup_succ_rsp_info_t succ_rsp;
	drb_modify_req_info_t mod_req;

	// Setup a ue to do all this stuff on 
	ue_ctx.cuup_ue_id = 4;
	ue_ctx.cucp_ue_id = 5;
	ue_ctx.pdus_count = 0;
	ue_ctx.drb_count  = 0;

	//Setup request
	req.drb_id = 3;
	req.drb_dl_ip_addr = pfm_str2ip("192.168.0.10");
	req.drb_dl_teid = 4;

	ret_val = drb_setup(&ue_ctx,&req,&succ_rsp,&fail_rsp);
	cr_assert(ret_val !=  PFM_FAILED,"Successful drb_setup");
	cr_assert(succ_rsp.drb_id == req.drb_id,"DRB from req and rsp match");
	cr_assert(ue_ctx.drb_tunnel_list[0]->remote_ip == req.drb_dl_ip_addr,"DRB entry and request contents match");
	cr_assert(ue_ctx.drb_tunnel_list[0]->remote_te_id == req.drb_dl_teid,"DRB entry and request contents match");
	cr_assert(ue_ctx.drb_tunnel_list[0]->drb_info.mapped_flow_idx == 0,"Check a parameter that is default");
	printf("\nPassed Successful drb setup\n");
	// modify without commit
	mod_req.drb_id = req.drb_id;
	ret_val = drb_modify(&ue_ctx,&mod_req,&succ_rsp,&fail_rsp,0);
	cr_assert(ret_val == PFM_FAILED,"Cannot modify before commit");
	cr_assert(fail_rsp.cause == FAIL_CAUSE_RNL_UNKNOWN_DRB_ID,"appropriate response");
	
	// Commit the drb without using ue_ctx commit
	ret_val  = drb_commit(ue_ctx.drb_tunnel_list[0]);
	cr_assert(ret_val == PFM_SUCCESS,"Commit the drb tunnel");
	printf("\nPassed can't modify drb tunnel\n");
	
	// Setup mod request
	mod_req.drb_id = req.drb_id;
	ret_val = drb_modify(&ue_ctx,&mod_req,&succ_rsp,&fail_rsp,0);
	cr_assert(ret_val != PFM_FAILED,"DRB modify succeeded");
	cr_assert(ue_ctx.drb_tunnel_list[0]->drb_info.mapped_flow_idx == 1,"Check modification");
	printf("\nPassed drb modify");
}

Test(modify_suite,test2,.description = "\nChecks for \n\t1. Failure for overflow\n\t2. Appropriate failure response")
{

	pfm_retval_t ret_val;
	int ret,i;
	ue_ctx_t ue_ctx;
	drb_setup_req_info_t req;
	drb_setup_fail_rsp_info_t fail_rsp;
	drb_setup_succ_rsp_info_t succ_rsp;
	drb_modify_req_info_t mod_req;

	tunnel_t *entry;
	tunnel_key_t tunnel_key;
	

	// Setup a ue to do all this stuff on 
	ue_ctx.cuup_ue_id = 4;
	ue_ctx.cucp_ue_id = 5;
	ue_ctx.pdus_count = 0;
	ue_ctx.drb_count  = 0;

	//Setup request
	req.drb_id = 3;
	req.drb_dl_ip_addr = pfm_str2ip("192.168.0.10");
	req.drb_dl_teid = 4;

	ret_val = drb_setup(&ue_ctx,&req,&succ_rsp,&fail_rsp);
	cr_assert(ret_val !=  PFM_FAILED,"Successful drb_setup");
	cr_assert(succ_rsp.drb_id == req.drb_id,"DRB from req and rsp match");

	cr_assert(ue_ctx.drb_tunnel_list[0]->remote_ip == req.drb_dl_ip_addr,
			"DRB entry and request contents match");
	cr_assert(ue_ctx.drb_tunnel_list[0]->remote_te_id == req.drb_dl_teid,
			"DRB entry an request contents match");
	printf("\ndrb setup passed\n");
	// Commit the drb
	ret_val  = drb_commit(ue_ctx.drb_tunnel_list[0]);
	cr_assert(ret_val == PFM_SUCCESS,"Commit the drb tunnel");
	printf("\nFilling till overflow\n");
	for (i = 1;i < MAX_TUNNEL_COUNT;i++)
	{
		ret_val = tunnel_key_alloc(0,TUNNEL_TYPE_PDUS,&tunnel_key);
		cr_assert(ret == PFM_SUCCESS,"key allocated");
		
		entry = tunnel_add(&tunnel_key);
		cr_assert(entry != NULL,"tunnel entry allocated");

		ret = tunnel_commit(entry);
		cr_assert(ret == PFM_SUCCESS,"tunnel commit is successful");
	}
	
		ret_val = tunnel_key_alloc(0,TUNNEL_TYPE_PDUS,&tunnel_key);
		//cr_expect(ret != PFM_SUCCESS,"key allocated");
		
		entry = tunnel_add(&tunnel_key);
		cr_expect(entry == NULL,"tunnel entry allocated");

		ret = tunnel_commit(entry);
		cr_expect(ret != PFM_SUCCESS,"tunnel commit is successful");
	printf("\nFilled till full\n");

	mod_req.drb_id = req.drb_id;
	ret_val = drb_modify(&ue_ctx,&mod_req,&succ_rsp,&fail_rsp,0);
	cr_assert(ret_val == PFM_FAILED,"Overflow detected");
	cr_assert(fail_rsp.cause == FAIL_CAUSE_RNL_UNKNOWN_DRB_ID,"appropriate fail_rsp");
	printf("\nOverflow test passed\n");
}
