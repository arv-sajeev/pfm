
/* This file contains a wrapper for RTE log APIs so that it is easy to
   replace them with any other logging lib like rsyslib without much
   application code changes. Application code should not use RTE
   log APIs directrly. Instead they should use these wrapper functions.

   pfm_trace_msg(), pfm_trace_pkt() and pfm_trace_pkt_hdr() will write
   trace during code exection for easy debugging. They will write trace 
   only if the code is compiled with -D=TRACE argument. i.e, tracing can
   be enabled by include -D=TRACE argument during compilation. If tracing
   is NOT enabled using -D=TRACE argument, there is no extra procesing
   due to these function calls.  Hence, enable tracing only  during 
   development and debugging. When the system is stable remove -D=TRACE 
   argument to disable tracing.

*/

/* NOTE:
   By defaul, the logs and traces are written to /var/log/syslog.
   If this need to be redirected to a different file, the following needs
   to be done

   1. Create a new configuration file /etc/rsyslog.d/30-pfm.conf
      with the following content.

        local6.*        /var/log/pfm.log

   2. Restart rsyslogd using the following command (or reboot the system)

        sudo kill -9 `cat /run/rsyslogd.pid`

   3. Use EAL parameter "--syslog local6" when starting the DPDK Applicaiton
      e.g: 

       sudo ./cuup.exe --lcore '0@0,1@1,2@0,3@1,4@0,5@1' --syslog local6

*/


#ifndef __PFM_LOG_H__
#define __PFM_LOG_H__ 1
#include <rte_log.h>
#include <rte_hexdump.h>


#define SYSLOG_FACILITY_LOG	LOG_LOCAL6
#define SYSLOG_FACILITY_TRACE	LOG_LOCAL7

/* Below are the log pritory supported 
PFM_LOG_EMERG		- The message says the system is unusable. 
PFM_LOG_ALERT		- Action on the message must be taken immediately. 
PFM_LOG_CRIT		- The message states a critical condition. 
PFM_LOG_ERR		- The message describes an error. 
PFM_LOG_WARNING	- The message is a warning. 
PFM_LOG_NOTICE		- The message describes a normal but important event. 
PFM_LOG_INFO		- The message is purely informational. 
PFM_LOG_DEBUG		- The message is only for debugging purposes
*/


typedef enum
{
	PFM_LOG_EMERG		= RTE_LOG_EMERG,
	PFM_LOG_ALERT		= RTE_LOG_ALERT,
	PFM_LOG_CRIT		= RTE_LOG_CRIT,
	PFM_LOG_ERR		= RTE_LOG_ERR,
	PFM_LOG_WARNING		= RTE_LOG_WARNING,
	PFM_LOG_NOTICE		= RTE_LOG_NOTICE,
	PFM_LOG_INFO		= RTE_LOG_INFO,
	PFM_LOG_DEBUG		= RTE_LOG_DEBUG
} pfm_log_priority_t;

#define pfm_log_msg(p,...)	pfmlog_msg(__FILE__,__LINE__,__FUNCTION__,p,__VA_ARGS__)
#define pfm_log_std_err(p,...)	pfmlog_std_err(__FILE__,__LINE__,__FUNCTION__,p,__VA_ARGS__)
#define pfm_log_rte_err(p,...)	pfmlog_rte_err(__FILE__,__LINE__,__FUNCTION__,p,__VA_ARGS__)

#ifdef TRACE
#define pfm_trace_msg(...)	pfmtrace_msg(__FILE__,__LINE__,__FUNCTION__,__VA_ARGS__)
#define pfm_trace_pkt_hdr(pkt,pkt_len,...) pfmtrace_pkt_hdr(__FILE__,__LINE__,__FUNCTION__,pkt,pkt_len,__VA_ARGS__)
#define pfm_trace_pkt(pkt,pkt_len,...) pfmtrace_pkt(__FILE__,__LINE__,__FUNCTION__,pkt,pkt_len,__VA_ARGS__)
#else
#define pfm_trace_msg(...)
#define pfm_trace_pkt_hdr(pkt,pkt_len,...)
#define pfm_trace_pkt(pkt,pkt_len,...)
#endif

void pfm_log_open(const char *app_name, const pfm_log_priority_t priority);
void pfm_log_close(void);


/* below functions should not be invoked directly from the applicaiton code.
   instead use the above macros */

void pfmlog_msg( const char *src_file_name,
                const int line_no,
                const char *func_name,
		const pfm_log_priority_t priority,
                const char *format, ...);

void pfmlog_std_err( const char *src_file_name,
                const int line_no,
                const char *func_name,
		const pfm_log_priority_t priority,
                const char *format, ...);

void pfmlog_rte_err( const char *src_file_name,
                const int line_no,
                const char *func_name,
		const pfm_log_priority_t priority,
                const char *format, ...);

void pfmtrace_msg( const char *src_file_name,
                const int line_no,
                const char *func_name,
                const char *format, ...);
void pfmtrace_pkt( const char *src_file_name,
                const int line_no,
                const char *func_name,
		const unsigned char *pkt,
		const int pkt_len,
                const char *format, ...);
void pfmtrace_pkt_hdr( const char *src_file_name,
                const int line_no,
                const char *func_name,
		const unsigned char *pkt,
		const int pkt_len,
                const char *format, ...);
#endif

/* End of file */

