#ifndef __PFM_CLI_H__
#define __PFM_CLI_H__ 1

#define	CLI_MAX_ACT_COUNT	10
typedef enum
{
	PFM_CLI_CONTINUE,
	PFM_CLI_ERROR,
	PFM_CLI_EXIT,
	PFM_CLI_SHUTDOWN
} pfm_cli_retval_t;

typedef struct
{
	const char *action_name;
	const char *action_desc;
	const char *action_syntax;
	pfm_cli_retval_t (*action_callback ) (FILE* fpout, char *args);
} pfm_cli_action_t;

typedef struct 
{
	const char *object_name;
	const char *object_desc;
	const char *object_syntax;
	pfm_cli_retval_t (*object_callback)
		(FILE* fpout, char *action, char *args);
	int action_count;
	pfm_cli_action_t *action_list[CLI_MAX_ACT_COUNT];
} pfm_cli_object_t;

void pfm_cli_init(void);
void pfm_cli_object_add( pfm_cli_object_t *object);
void pfm_cli_action_add( pfm_cli_object_t *object,
			 pfm_cli_action_t *action);

pfm_cli_retval_t pfm_cli_exec(FILE *fpin, FILE *fpout );

#endif
