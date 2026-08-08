/*
 * No-op syslog implementation for the Xbox 360.
 *
 * The console has no syslog daemon; these functions exist only so that
 * code which logs through syslog on POSIX systems can link.
 */

#include <stdarg.h>
#include <syslog.h>

void
openlog(const char *ident, int logopt, int facility)
{
    (void) ident;
    (void) logopt;
    (void) facility;
}

void
closelog(void)
{
}

int
setlogmask(int maskpri)
{
    return maskpri;
}

void
vsyslog(int priority, const char *fmt, va_list ap)
{
    (void) priority;
    (void) fmt;
    (void) ap;
}

void
syslog(int priority, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsyslog(priority, fmt, ap);
    va_end(ap);
}