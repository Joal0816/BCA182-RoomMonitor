#include <stdint.h>
#include <string.h>
#include <unity.h>
long write(int, const void*, unsigned long);
int _shim_fails=0, _shim_tests=0, _shim_cur_failed=0;
static char _buf[512]; static int _n=0;
static void _put(const char*s){ while(*s && _n<510) _buf[_n++]=*s++; }
static void _putnum(long long v){ char t[24]; int i=0; if(v<0){_put("-");v=-v;}
  if(!v)t[i++]='0'; while(v){t[i++]='0'+(v%10); v/=10;} while(i) _buf[_n++]=t[--i]; }
static void _flush(void){ if(_n){ _buf[_n++]='\n'; write(1,_buf,_n); _n=0; } }
int _shim_begin(void){ _shim_fails=0; _shim_tests=0; return 0; }
static void _msg(const char*p, const char*q, int line){ _put(p); if(q){_put(" (");_put(q);
  _put(":"); _putnum(line); _put(")");} _flush(); }
void _shim_fail(const char*m, int line, const char*f){ _msg("  FAIL: ", m, 0);
  _put("    at line "); _putnum(line); _put(" of "); _put(f); _flush(); }
void _shim_pass(const char*name){ _put("  PASS: "); _put(name); _flush(); }
int _shim_end(void){ _put(""); _putnum(_shim_tests-_shim_fails); _put(" passed, ");
  _putnum(_shim_fails); _put(" failed, "); _putnum(_shim_tests); _put(" total"); _flush();
  return _shim_fails ? 1 : 0; }
int strcmp(const char*a,const char*b){ while(*a&&*a==*b){a++;b++;} return (unsigned char)*a-(unsigned char)*b; }
size_t strlen(const char*s){ const char*p=s; while(*p)p++; return p-s; }
void *memset(void*d,int c,size_t n){ unsigned char*p=d; while(n--)*p++=(unsigned char)c; return d; }
void *memcpy(void*d,const void*s,size_t n){ unsigned char*a=d; const unsigned char*b=s;
  while(n--)*a++=*b++; return d; }
