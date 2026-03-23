#include <stdio.h>
#include <stdlib.h>  // strtol 함수 포함
#include <stdarg.h>

#include "esp_log.h"
#include "report.h"
#include "lpc_isp.h"

#define ARRAY_SIZE(x)       (sizeof(x)/sizeof((x)[0]))
#define MIN(a,b)            (((a)<(b))?(a):(b))

static FILE
*fp;

#if 0
static FILE
    *fp;

static const char
	*errorCodes[]=
	{
		"success",
		"invalid command",
		"source address error",
		"destination address error",
		"source address not mapped",
		"destination address not mapped",
		"count error",
		"invalid sector",
		"sector not blank",
		"sector not prepared for write operation",
		"compare error",
		"busy",
		"param error",
		"address error",
		"address not mapped",
		"command locked",
		"invalid code",
		"invalid baud rate",
		"invalid stop bit",
		"code read protection enabled",
	};

static int  reportLevel=REPORT_DEBUG_FULL;

const char *GetErrorString(const char *s)
// report error code
{
	int
		code;
	static char
		string[256];

	code=strtol(s,NULL,10);
	if(code<=ARRAY_SIZE(errorCodes))
	{
		return(errorCodes[code]);
	}
	sprintf(string,"unknown response code: \"%s\"\n",s);
	return(string);
}



void ReportString(int level,const char *format,...)
// print a diagnostic message if level<=reportLevel
{
	va_list p;

	if(level<=reportLevel)
	{
	    va_start(p, format); // 가변 인자 처리 시작
//	    esp_log_writev(ESP_LOG_DEBUG, "", format, p); // 가변 인자를 이용한 로깅 출력
//	    ESP_LOGI("", "format", p); // 문자 출력
	    va_end(p); // 가변 인자 처리 종료

	}
}

 void ReportChar(int level,const char c)
// print a character if level<=reportLevel
{
	if(level<=reportLevel)
	{
        ESP_LOGI("", "%c", c); // 문자 출력
	}
}







 void ReportBufferCtrl(int level,const unsigned char *buffer, unsigned int length)
// print a diagnostic message, if enabled.  convert control characters to printable, e.g,
// control-C becomes <03>
{
	ReportChar(level,'[');
	ESP_LOGI(TAG_LPCISP,"(%d) ",length);
	while(length--)
	{
		ESP_LOGI(TAG_LPCISP,"%02x ",(unsigned char)*buffer);
		buffer++;
	}
	ESP_LOGI(TAG_LPCISP,"]\n");
}

 void ReportCharCtrl(int level, const char c)
{
	if((c>=' ')&&(c<='~'))
	{
		ReportChar(level,c);
	}
	else
	{
		ESP_LOGI(TAG_LPCISP,"<%02x>",(unsigned char)c);
	}
}

 void ReportStringCtrl(int level,const char *string)
// print a diagnostic message, if enabled.  convert control characters to printable, e.g,
// control-C becomes <03>
{
	while(*string)
	{
		ReportCharCtrl(level,*string);
		string++;
	}
}

void LPCISP_SetReportLevel(int level)
{
	reportLevel=level;
}

void LPCISP_ReportStream(FILE *p)
// allow application to change log output to a different stream
{
	fp=p;
}

#else


static const char
*errorCodes[] =
{
    "success",
    "invalid command",
    "source address error",
    "destination address error",
    "source address not mapped",
    "destination address not mapped",
    "count error",
    "invalid sector",
    "sector not blank",
    "sector not prepared for write operation",
    "compare error",
    "busy",
    "param error",
    "address error",
    "address not mapped",
    "command locked",
    "invalid code",
    "invalid baud rate",
    "invalid stop bit",
    "code read protection enabled",
};

static int
reportLevel = REPORT_DEBUG_FULL;

const char *GetErrorString(const char *s)
// report error code
{
    int
            code;
    static char
            string[256];

    code = strtol(s,NULL, 10);
    if (code <= ARRAY_SIZE(errorCodes)) {
        return (errorCodes[code]);
    }
    sprintf(string, "unknown response code: \"%s\"\n", s);
    return (string);
}


void ReportString(int level, const char *format, ...)
// print a diagnostic message if level<=reportLevel
{
    va_list
            p;

    if (level <= reportLevel) {
        va_start(p, format); // Initialize start of variable-length argument list
        vprintf(format, p);
        va_end(p);
        fflush(stdout); // 강제로 출력
        //      fflush(fp);
    }
}

void ReportChar(int level, const char c)
// print a character if level<=reportLevel
{
    if (level <= reportLevel) {
        printf("%c", c);
        //       fflush(fp);
    }
}

void ReportBufferCtrl(int level, const unsigned char *buffer, unsigned int length)
// print a diagnostic message, if enabled.  convert control characters to printable, e.g,
// control-C becomes <03>
{
    ReportChar(level, '[');
    ReportString(level, "(%d) ", length);
    while (length--) {
        ReportString(level, "%02x ", (unsigned char) *buffer);
        buffer++;
    }
    ReportString(level, "]\n");
}

void ReportCharCtrl(int level, const char c) {
    if ((c >= ' ') && (c <= '~')) {
        ReportChar(level, c);
    }
    else {
        ReportString(level, "<%02x>", (unsigned char) c);
    }
}

void ReportStringCtrl(int level, const char *string)
// print a diagnostic message, if enabled.  convert control characters to printable, e.g,
// control-C becomes <03>
{
    while (*string) {
        ReportCharCtrl(level, *string);
        string++;
    }
}

void LPCISP_SetReportLevel(int level) {
    reportLevel = level;
}

void LPCISP_ReportStream(FILE *p)
// allow application to change log output to a different stream
{
    fp = p;
}


#endif
