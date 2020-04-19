#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <syslog.h>
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_log.h>
#include <rte_hexdump.h>
#include <rte_errno.h>

#include "pfm_log.h"

#define MAX_APP_NAME_LEN 8
#define MAX_LOG_MSG_LEN	1000
#define MAX_TOKEN_LEN	50
#define BYTES_PER_LINE 10

#define LOGTYPE_LOG	RTE_LOGTYPE_USER7
#define LOGTYPE_TRACE	RTE_LOGTYPE_USER8

#define LOGPRIORITY_TRACE	PFM_LOG_DEBUG

static char app_name_g[MAX_APP_NAME_LEN+1];

#ifndef CONSLOG
static ssize_t
console_log_write(__attribute__((unused)) void *c,
				const char *buf, size_t size)
{

        /* Syslog error levels are from 0 to 7, so subtract 1 to convert */
        syslog(rte_log_cur_msg_loglevel() - 1, "%.*s", (int)size, buf);

        return size;
}
#endif


void pfm_log_open( const char *app_name, const pfm_log_priority_t priority)
{
#ifndef CONSLOG
	FILE *log_steam;

	static cookie_io_functions_t console_log_func =
	{
		.read	= NULL,
        	.write	= console_log_write,
		.seek	= NULL,
		.close	= NULL
	};

        log_steam = fopencookie(NULL, "w+", console_log_func);
        if (log_steam != NULL)
	{
		rte_openlog_stream(log_steam);
	}
#endif

	strncpy(app_name_g,app_name,MAX_APP_NAME_LEN);
	app_name_g[MAX_APP_NAME_LEN] = 0;

	rte_log_set_global_level(priority);

        rte_log_set_level (LOGTYPE_LOG, priority);
        rte_log_set_level (LOGTYPE_TRACE, PFM_LOG_DEBUG);

	return;
}

void pfm_log_close(void)
{
	return;
}

void pfmlog_msg( const char *src_file_name,
                const int line_no,
                const char *func_name,
		const pfm_log_priority_t priority,
                const char *fmt, ...)
{
	char msg[MAX_LOG_MSG_LEN];
	va_list ap;

	snprintf(msg,MAX_LOG_MSG_LEN,"%s|%s|%d|%s()|%s\n",
		app_name_g,basename(src_file_name),line_no,func_name,fmt);

	va_start (ap, fmt);
	rte_vlog(priority,LOGTYPE_LOG,msg,ap);
	va_end (ap);

	return;
}

void pfmlog_std_err( const char *src_file_name,
                const int line_no,
                const char *func_name,
		const pfm_log_priority_t priority,
                const char *fmt, ...)
{
	char msg[MAX_LOG_MSG_LEN];
	va_list ap;

	snprintf(msg,MAX_LOG_MSG_LEN,"%s|%s|%d|%s()|err=%s(%d)|%s\n",
		app_name_g,basename(src_file_name),line_no,func_name,
		strerror(errno),errno,fmt);

	va_start (ap, fmt);
	rte_vlog(priority,LOGTYPE_LOG,msg,ap);
	va_end (ap);

	return;
}

void pfmlog_rte_err( const char *src_file_name,
                const int line_no,
                const char *func_name,
		const pfm_log_priority_t priority,
                const char *fmt, ...)
{
	char msg[MAX_LOG_MSG_LEN];
	va_list ap;

	snprintf(msg,MAX_LOG_MSG_LEN,
		"%s|%s|%d|%s()|err=%s(%d)|rteErr=%s(%d)|%s\n",
		app_name_g,basename(src_file_name),line_no,
		func_name,strerror(errno),
		errno,rte_strerror(rte_errno),rte_errno,fmt);

	va_start (ap, fmt);
	rte_vlog(priority,LOGTYPE_LOG,msg,ap);
	va_end (ap);

	return;
}

void pfmtrace_msg( const char *src_file_name,
                const int line_no,
                const char *func_name,
                const char *fmt, ...)
{
	char msg[MAX_LOG_MSG_LEN];
	va_list ap;

	snprintf(msg,MAX_LOG_MSG_LEN,"%s|%s|%d|%s()|%s\n",
		app_name_g,basename(src_file_name),line_no,func_name,fmt);

	va_start (ap, fmt);
	rte_vlog(LOGPRIORITY_TRACE, LOGTYPE_TRACE, msg, ap);
	va_end (ap);
}


void pfmtrace_pkt( const char *src_file_name,
                const int line_no,
                const char *func_name,
                const unsigned char *pkt,
                const int pkt_len,
                const char *fmt, ...)
{
	char msg[MAX_LOG_MSG_LEN];
	char token[MAX_TOKEN_LEN];
	va_list ap;
	int idx;
	int j;

	snprintf(msg,MAX_LOG_MSG_LEN,
		"%s|%s|%d|%s()|pkt=%p,pkt_len=%d|%s\n",
		app_name_g,basename(src_file_name),
		line_no,func_name,pkt,pkt_len,fmt);

	va_start (ap, fmt);
	rte_vlog(LOGPRIORITY_TRACE,LOGTYPE_TRACE,msg,ap);
	va_end (ap);

	strcpy(msg,"   ");
	for (idx=0; idx < pkt_len; idx++)
	{
		if (0 == (idx % BYTES_PER_LINE))
		{
			snprintf(token,MAX_TOKEN_LEN,"%04d:",idx);
			strncat(msg,token,MAX_LOG_MSG_LEN);
		}

		snprintf(token,MAX_TOKEN_LEN,"%02X:",pkt[idx]);
		strncat(msg,token,MAX_LOG_MSG_LEN);

		if (9 == (idx % 10 ))
		{
			strncat(msg," ",MAX_LOG_MSG_LEN);
			if ((BYTES_PER_LINE-1) == (idx  % BYTES_PER_LINE))
			{
				strncat(msg," ",MAX_LOG_MSG_LEN);
				for(j=(idx-(BYTES_PER_LINE-1));
					j <= idx; j++)
				{
					if (isprint(pkt[j]))
					{
						snprintf(token,
							MAX_TOKEN_LEN,
							"%c",pkt[j]);
						strncat(msg,token,
							MAX_LOG_MSG_LEN);
					}
					else
					{
						strncat(msg,".",
							MAX_LOG_MSG_LEN);
					}
				}
				rte_log(LOGPRIORITY_TRACE,LOGTYPE_TRACE,
					"%s\n",msg);
				strncpy(msg,"   ", MAX_LOG_MSG_LEN);
			}
		}
	}

	for (j=0; j < (BYTES_PER_LINE - (idx % BYTES_PER_LINE)); j++)
	{
		strncat(msg,"   ", MAX_LOG_MSG_LEN);
		if (9 == (j % 10 ))
			strncat(msg," ", MAX_LOG_MSG_LEN);
	}
	strncat(msg,"  ", MAX_LOG_MSG_LEN);

	j = idx - (idx % BYTES_PER_LINE);
	for(; j < idx; j++)
	{
		if (isprint(pkt[j]))
		{
			snprintf(token,MAX_TOKEN_LEN,"%c",pkt[j]);
			strncat(msg,token, MAX_LOG_MSG_LEN);
		}
		else
		{
			strncat(msg,".", MAX_LOG_MSG_LEN);
		}
	}
	rte_log(LOGPRIORITY_TRACE,LOGTYPE_TRACE, "%s\n",msg);
	return;
}

void pfmtrace_pkt_hdr( const char *src_file_name,
                const int line_no,
                const char *func_name,
                const unsigned char *pkt,
                const int pkt_len,
                const char *fmt, ...)
{
	char msg[MAX_LOG_MSG_LEN];
	char token[MAX_TOKEN_LEN];
	unsigned short ether_type;
	unsigned short tci;
	va_list ap;
	int idx;

	snprintf(msg,MAX_LOG_MSG_LEN,
		"%s|%s|%d|%s()|pkt=%p,pkt_len=%d|%s\n",
		app_name_g,basename(src_file_name),line_no,
		func_name,pkt, pkt_len,fmt);

	va_start (ap, fmt);
	rte_vlog(LOGPRIORITY_TRACE,LOGTYPE_TRACE,msg,ap);
	va_end (ap);

	if (14 > pkt_len)
	{
		rte_log(LOGPRIORITY_TRACE,LOGTYPE_TRACE,
			"pkt_len=%d too short to print header\n",pkt_len);
		return;
	}

	snprintf(msg,MAX_TOKEN_LEN,
			"DstMAC=%02X:%02X:%02X:%02X:%02X:%02X "
			"SrcMAC=%02X:%02X:%02X:%02X:%02X:%02X ",
			pkt[0],pkt[1],pkt[2],pkt[3],pkt[4],pkt[5],
			pkt[6],pkt[7],pkt[8],pkt[9],pkt[10],pkt[11]);

	ether_type = ((pkt[12] << 8) | pkt[13]);

	idx = 14;
	if ( 0x8100 == ether_type)
	{
		tci = ((pkt[14] << 8) | pkt[15]);
		snprintf(token,MAX_TOKEN_LEN,"TPID=%d, TCI=%d",
						ether_type,tci);
		strncat(msg,token, MAX_LOG_MSG_LEN);
		ether_type = ((pkt[16] << 8) | pkt[17]);
		idx = 18;
	}
	snprintf(token,MAX_TOKEN_LEN," EtherType=%d",ether_type);
	strncat(msg,token, MAX_LOG_MSG_LEN);

	if ((20 + idx) <= pkt_len)
	{
		snprintf(token,MAX_TOKEN_LEN," SrcIP=%d.%d.%d.%d",
			pkt[idx+12],pkt[idx+13],pkt[idx+14],pkt[idx+15]);
		strncat(msg,token, MAX_LOG_MSG_LEN);
	
		snprintf(token,MAX_TOKEN_LEN," DstIP=%d.%d.%d.%d",
			pkt[idx+16],pkt[idx+17],pkt[idx+18],pkt[idx+19]);
		strncat(msg,token, MAX_LOG_MSG_LEN);

		snprintf(token,MAX_TOKEN_LEN," Proto=%d",pkt[idx+9]);
		strncat(msg,token, MAX_LOG_MSG_LEN);
	}
	rte_log(LOGPRIORITY_TRACE,LOGTYPE_TRACE,"%s\n",msg);
	return;
}

/* End of file */
