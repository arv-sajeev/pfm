#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "pfm.h"
#include "pfm_comm.h"
#include "pfm_log.h"
#include "pfm_link.h"
#include "pfm_kni.h"
#include "pfm_route.h"
#include "pfm_cli.h"

#define CLI_CMD_LINE_LEN	80
#define	CLI_MAX_OBJ_COUNT	25
#define CLI_TOKEN_DELIMITER	" \t"


static pfm_cli_object_t *cli_object_list_g[CLI_MAX_OBJ_COUNT];
static int object_count_g = 0;

static pfm_cli_action_t *cli_get_action(const char *action_name,
		pfm_cli_object_t *object_ptr)
{
	int i;
	int len;
	len = strlen(action_name);
	for (i=0; i < object_ptr->action_count; i++)
	{
		if (0 == strncmp(action_name,
			object_ptr->action_list[i]->action_name,len))
		{
			return object_ptr->action_list[i];
		}
	}
	return NULL;
}
static pfm_cli_object_t *cli_get_object(const char *object_name)
{
	int i;
	int len;
	len = strlen(object_name);
	for (i=0; i < object_count_g; i++)
	{
		if (0 == strncmp(object_name,
			cli_object_list_g[i]->object_name,len))
		{
			return cli_object_list_g[i];
		}
	}
	return NULL;
}

static pfm_cli_retval_t action_callback_link_list(FILE* fpout,
		__attribute__((unused)) char *args)
{
	pfm_link_list_print(fpout);
	return PFM_CLI_CONTINUE;
}

static pfm_cli_retval_t action_callback_link_show(FILE* fpout,
		__attribute__((unused)) char *args)
{
	int link_id;
	char *token;
	int len;
	int i;

	token = strtok(args,CLI_TOKEN_DELIMITER);
	if ((NULL == token) || ((len=strlen(token)) <= 0))
	{
		fprintf(fpout,"ERROR: <linkId> not specified\n");
		return PFM_CLI_ERROR;
	}
	
	for(i=0; i < len; i++)
	{
		if (!isdigit(token[i]))
		{
			fprintf(fpout,
				"ERROR: <linkId> needs to be numeric\n");
			return PFM_CLI_ERROR;
		}
	}
	link_id = atoi(token);

	pfm_link_show_print(fpout,link_id);
	return PFM_CLI_CONTINUE;
}

static pfm_cli_retval_t action_callback_ipv4_list(FILE* fpout,
			__attribute__((unused)) char *args)
{
	kni_ipv4_list_print(fpout);
	return PFM_CLI_CONTINUE;
}

static pfm_cli_retval_t action_callback_ipv4_show(FILE* fpout, char *args)
{
	char *token;
	int len;

	token = strtok(args,CLI_TOKEN_DELIMITER);
	if ((NULL == token) || ((len=strlen(token)) <= 0))
	{
		fprintf(fpout,"ERROR: <ifName> not specified\n");
		return PFM_CLI_ERROR;
	}
	
	kni_ipv4_show_print(fpout,token);
	return PFM_CLI_CONTINUE;
}

static pfm_cli_retval_t action_callback_arp_list(FILE* fpout, char *args)
{
	printf("SAJEEV Invoking action_callback_arp_list(fp=%p,args=%p)\n",
		fpout, args);
	return PFM_CLI_CONTINUE;
}

static pfm_cli_retval_t action_callback_route_list(FILE* fpout, char *args)
{
	printf("SAJEEVInvoking action_callback_route_list(fp=%p,args=%p)\n",
			fpout,args);
	route_print(fpout);

	return PFM_CLI_CONTINUE;
}

static void print_help(FILE *fpout)
{
	int i;
	fprintf(fpout,
		"\n   Syntax:  <objectName> [<action> [<args>...]]\n");
	fprintf(fpout,"\nwhere:\n   <objectName> is:\n");
	for (i=0; i < object_count_g; i++)
	{
		fprintf(fpout,"\t%-10s- %s\n",
				cli_object_list_g[i]->object_name,
				cli_object_list_g[i]->object_desc);
	}
	fprintf(fpout,"   <action> - is the action on object\n");
	fprintf(fpout,"   <args>   - is the arguments for action\n\n");
	fprintf(fpout,"Use 'help <objectName> [<action> [<args>...]]' "
				"for more detailed help\n\n");
		
	return;
}

static pfm_cli_retval_t object_callback_help(
		FILE* fpout, char *oname, char* args)
{
	char *action = NULL;
	pfm_cli_object_t *object_ptr;
	pfm_cli_action_t *action_ptr;

	action = strtok(args,CLI_TOKEN_DELIMITER);
	if (NULL == oname)
	{
		print_help(fpout);
		return PFM_CLI_CONTINUE;
	}

	object_ptr = (pfm_cli_object_t *)cli_get_object(oname);
	if (NULL == object_ptr)
	{
		fprintf(fpout,"ERROR: Invalid object Name '%s'\n", oname);
		return PFM_CLI_ERROR;
	}

	if (NULL == action)
	{
		if (0 >= object_ptr->action_count )
		{
			fprintf(fpout,"%s\n\n",object_ptr->object_desc);
			fprintf(fpout,"   Syntax: %s %s\n\n",
					object_ptr->object_name,
					object_ptr->object_syntax);
			// puposefully put ERROR to stop further parsing
			return PFM_CLI_ERROR; 
		}
		if (1 == object_ptr->action_count )
		{
			fprintf(fpout,"%s\n\n",
				object_ptr->action_list[0]->action_desc);
			fprintf(fpout,"   Syntax: %s %s %s\n\n",
				object_ptr->object_name,
				object_ptr->action_list[0]->action_name,
				object_ptr->action_list[0]->action_syntax);
			// puposefully put ERROR to stop further parsing
			return PFM_CLI_ERROR;
		}

		/* more than 1 action for the object */
		fprintf(fpout,"%s\n\n",object_ptr->object_desc);
		fprintf(fpout,"   Syntax: %s <action> [<args>...]\n",
				object_ptr->object_name);
		fprintf(fpout,"\nwhere <action> is one of:\n");
		for(int i=0; i < object_ptr->action_count; i++)
		{
			fprintf(fpout,"   %-10s - %s\n",
				object_ptr->action_list[i]->action_name,
				object_ptr->action_list[i]->action_desc);
		}
		fprintf(fpout,"\nUse 'help %s <action>' "
				"for more detailed help\n\n",
				object_ptr->object_name);
		
		// puposefully put ERROR to stop further parsing
		return PFM_CLI_ERROR; 
	}

	action_ptr = (pfm_cli_action_t *)cli_get_action(action,object_ptr);
	if (NULL == action_ptr)
	{
		fprintf(fpout,"ERROR: Invalid action '%s'\n", action);
		return PFM_CLI_ERROR;
	}
	fprintf(fpout,"%s\n\n",action_ptr->action_desc);
	fprintf(fpout,"   Syntax: %s %s %s\n\n",
			object_ptr->object_name,
			action_ptr->action_name,
			action_ptr->action_syntax);
	
	return PFM_CLI_CONTINUE;
}

static pfm_cli_retval_t object_callback_link(FILE* fpout,
		__attribute__((unused))char *action,
		__attribute__((unused))char *args)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
	return  object_callback_help(fpout,(char *) "link",action);
#pragma GCC diagnostic pop
}

static pfm_cli_retval_t object_callback_ipv4(FILE* fpout,
		__attribute__((unused))char *action,
		__attribute__((unused))char *args)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
	return  object_callback_help(fpout,(char *) "ipv4",action);
#pragma GCC diagnostic pop
}

static pfm_cli_retval_t object_callback_arp(FILE* fpout,
		__attribute__((unused))char *action,
		__attribute__((unused))char *args)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
	return  object_callback_help(fpout,(char *) "arp",action);
#pragma GCC diagnostic pop
}

static pfm_cli_retval_t object_callback_route(FILE* fpout,
		__attribute__((unused))char *action,
		__attribute__((unused))char *args)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
	return  object_callback_help(fpout,(char *) "route",action);
#pragma GCC diagnostic pop
}

static pfm_cli_retval_t object_callback_disconnect(
		FILE* fpout,
		__attribute__((unused))char *action,
		__attribute__((unused))char* args)
{
	if (fpout == stdout)
	{
		fprintf(fpout,"'disconnect' cannot be initiated "
				"from console.\n");
		fprintf(fpout,"Cannot disconnect from console.\n");
		fprintf(fpout,"Use 'shutdown' if you want to "
				"quite applicaion\n");
		return PFM_CLI_CONTINUE;
	}
	fprintf(fpout,"Disconnecting...\n");
	return PFM_CLI_EXIT;
}

static pfm_cli_retval_t object_callback_shutdown(
		FILE *fpout,
		__attribute__((unused))char *action,
		__attribute__((unused))char* args)
{
	fprintf(fpout,"Shutting down...\n");
	pfm_terminate();
	return PFM_CLI_SHUTDOWN;
}

void pfm_cli_init(void)
{
	static pfm_cli_object_t object_help =
	{
		.object_name		= "help",
		.object_desc		= "Display help of commands",
		.object_syntax		= "<objectName> [<action>]",
		.object_callback	= object_callback_help,
	};

	static pfm_cli_object_t object_disconnect =
	{
		.object_name		= "disconnect",
		.object_desc		= "Disconnect and exit.",
		.object_syntax		= "",
		.object_callback	= object_callback_disconnect,
	};

	static pfm_cli_object_t object_shutdown =
	{
		.object_name		= "shutdown",
		.object_desc		= "Shutdown Application",
		.object_syntax		= "",
		.object_callback	= object_callback_shutdown,
	};

	static pfm_cli_object_t object_link =
	{
		.object_name		= "link",
		.object_desc		= "Ethernet link details",
		.object_syntax		= "show",
		.object_callback	= object_callback_link,
	};
	static pfm_cli_action_t action_link_list =
	{
		.action_name		= "list",
		.action_desc		= "List Ethernet link details",
		.action_syntax		= "",
		.action_callback	= action_callback_link_list,
	};
	static pfm_cli_action_t action_link_show =
	{
		.action_name		= "show",
		.action_desc		= "Show details of Ethernet link "
				"with specified <linkId>",
		.action_syntax		= "<linkId>",
		.action_callback	= action_callback_link_show,
	};

	static pfm_cli_object_t object_ipv4 =
	{
		.object_name		= "ipv4",
		.object_desc		= "Local IPv4 address details",
		.object_syntax		= "show",
		.object_callback	= object_callback_ipv4,
	};
	static pfm_cli_action_t action_ipv4_list =
	{
		.action_name		= "list",
		.action_desc		= "List local IPv4 address details",
		.action_syntax		= "",
		.action_callback	= action_callback_ipv4_list,
	};
	static pfm_cli_action_t action_ipv4_show =
	{
		.action_name		= "show",
		.action_desc		= "Show details of a <ifName>",
		.action_syntax		= "<ifName>",
		.action_callback	= action_callback_ipv4_show,
	};

	static pfm_cli_object_t object_arp =
	{
		.object_name		= "arp",
		.object_desc		= "ARP Table details",
		.object_syntax		= "show",
		.object_callback	= object_callback_arp,
	};
	static pfm_cli_action_t action_arp_list =
	{
		.action_name		= "list",
		.action_desc		= "Dispaly ARP Table",
		.action_syntax		= "",
		.action_callback	= action_callback_arp_list,
	};

	static pfm_cli_object_t object_route =
	{
		.object_name		= "route",
		.object_desc		= "Routing table details",
		.object_syntax		= "list",
		.object_callback	= object_callback_route,
	};
	static pfm_cli_action_t action_route_list =
	{
		.action_name		= "list",
		.action_desc		= "Display routing table",
		.action_syntax		= "",
		.action_callback	= action_callback_route_list,
	};

	object_count_g = 0;
	pfm_cli_object_add(&object_help);
	pfm_cli_object_add(&object_disconnect);
	pfm_cli_object_add(&object_shutdown);
	pfm_cli_object_add(&object_link);
	pfm_cli_object_add(&object_ipv4);
	pfm_cli_object_add(&object_arp);
	pfm_cli_object_add(&object_route);

	pfm_cli_action_add(&object_link,&action_link_list);
	pfm_cli_action_add(&object_link,&action_link_show);
	pfm_cli_action_add(&object_ipv4,&action_ipv4_list);
	pfm_cli_action_add(&object_ipv4,&action_ipv4_show);
	pfm_cli_action_add(&object_arp,&action_arp_list);
	pfm_cli_action_add(&object_route,&action_route_list);


	return;
}

void pfm_cli_object_add( pfm_cli_object_t *object_ptr)
{
	object_ptr->action_count = 0;
	cli_object_list_g[object_count_g] = object_ptr;
	object_count_g++;
	return;
}

void pfm_cli_action_add( pfm_cli_object_t *object,
			 pfm_cli_action_t *action)
{
	object->action_list[object->action_count] = action;
	object->action_count++;
}

pfm_cli_retval_t pfm_cli_exec(FILE *fpin, FILE *fpout)
{
	char cli[CLI_CMD_LINE_LEN];
	int line_len;
	int len;
	char *oname = NULL;
	char *action = NULL;
	char *s;
	char *args = NULL;
	pfm_cli_object_t *object_ptr = NULL;
	pfm_cli_action_t *action_ptr = NULL;
	pfm_cli_retval_t retval;

	fprintf(fpout,">>>>");
	fflush(fpout);
	s = fgets(cli,sizeof(cli),fpin);
	if (NULL == s)
	{
		return PFM_CLI_CONTINUE;
	}
	line_len = strlen(cli);
	if (cli[line_len-1] == 10)
	{
		cli[line_len-1]=' ';
	}
	/* Make a private copy. and make all lowercase*/
	line_len = strlen(cli);
	if (CLI_CMD_LINE_LEN < line_len)
	{
		line_len = CLI_CMD_LINE_LEN;
	}
	for(; 0 <= line_len; line_len--)
	{
		cli[line_len] = tolower(cli[line_len]);
	}

	/* Get action */
	oname = strtok(cli,CLI_TOKEN_DELIMITER);
	if (NULL == oname)
	{
		// empty line. do nothing
		return PFM_CLI_CONTINUE;
	}
	object_ptr = cli_get_object(oname);
	if (NULL == object_ptr)
	{
		fprintf(fpout,"ERROR: Invalid object Name '%s'\n", oname);
		return PFM_CLI_ERROR;
	}
	action = strtok(NULL,CLI_TOKEN_DELIMITER);
	if (NULL != action)
	{
		args = &action[strlen(action)+1];
		len = strlen(oname);
		if (0 != strncmp(oname,"help",len))
		{
			action_ptr = cli_get_action(action, object_ptr);
			if (NULL == action_ptr)
			{
				fprintf(fpout,
					"ERROR: Invalid action '%s'\n",
					action);
				return PFM_CLI_ERROR;
			}
		}
	}
	if (NULL != action_ptr)
	{
		if (NULL != action_ptr->action_callback)
		{
			retval = action_ptr->action_callback( fpout, args);
			return retval;
		}
	}
	if (NULL != object_ptr)
	{
		if (NULL != object_ptr->object_callback)
		{
			retval = object_ptr->object_callback(
					fpout,action,args);
			return retval;
		}
	}
	return PFM_CLI_CONTINUE;
}





