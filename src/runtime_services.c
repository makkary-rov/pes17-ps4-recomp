// SPDX-License-Identifier: GPL-2.0-or-later
/* System services for an offline, single-user console:
 *   - one local user (id 1) who is logged in but not signed in to PSN;
 *   - network cable unplugged: NetCtl disconnected, sockets unavailable;
 *   - common dialogs complete immediately (no UI is drawn yet);
 *   - the whole package is installed (PlayGo reports every chunk local).
 * Every entry here is an explicit contract; unknown functions still stop. */
#define _GNU_SOURCE
#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#define htons(v) __builtin_bswap16(v)
#define ntohs(v) __builtin_bswap16(v)
#define htonl(v) __builtin_bswap32(v)
#define ntohl(v) __builtin_bswap32(v)
#else
#include <arpa/inet.h>
#endif

#define USER_ID 1
#define ORBIS_OK 0
#define USER_INVALID_ARGUMENT ((int32_t)0x80960005)
#define USER_NO_EVENT ((int32_t)0x80960007)
#define SYSTEM_NO_EVENT ((int32_t)0x80A10004)
#define SYSTEM_PARAMETER ((int32_t)0x80A10003)
#define NET_CTL_NOT_CONNECTED ((int32_t)0x80412108)
#define NET_CTL_INVALID_ADDR ((int32_t)0x80412107)
#define NP_SIGNED_OUT ((int32_t)0x80550006)
#define NP_INVALID_ARGUMENT ((int32_t)0x80550003)
#define NET_ENETUNREACH ((int32_t)0x80410133)
#define NET_EINVAL ((int32_t)0x80410116)
#define HTTP_NETWORK ((int32_t)0x80431063)
#define DIALOG_NOT_INITIALIZED ((int32_t)0x80B80003)
#define PLAYGO_BAD_HANDLE ((int32_t)0x80B20009)
#define PLAYGO_BAD_POINTER ((int32_t)0x80B2000A)
#define PLAYGO_BAD_SIZE ((int32_t)0x80B2000B)
#define PLAYGO_BAD_CHUNK ((int32_t)0x80B2000C)
#define AUDIO_IN_NOT_OPENED ((int32_t)0x80260109)
#define TROPHY_INVALID ((int32_t)0x80551604)

static HostMutex lock=HOST_MUTEX_INIT;
static int next_id=1;
static int new_id(void) { host_lock(&lock); int id=next_id++; host_unlock(&lock); return id; }
static void note(const char *what) { printf("Runtime: %s\n",what); }

/* ---- UserService ---- */
static int login_event_pending=1;
static ABI int32_t user_initialize(const void *params) { (void)params; note("UserService initialized (user 1 logged in)"); return 0; }
static ABI int32_t user_terminate(void) { return 0; }
static ABI int32_t user_initial(int32_t *id) { if (!id) return USER_INVALID_ARGUMENT; *id=USER_ID; return 0; }
static ABI int32_t user_list(int32_t *ids) {
    if (!ids) return USER_INVALID_ARGUMENT;
    ids[0]=USER_ID; ids[1]=ids[2]=ids[3]=-1; return 0;
}
static ABI int32_t user_name(int32_t id,char *name,uint64_t size) {
    if (id!=USER_ID || !name) return USER_INVALID_ARGUMENT;
    const char *value=getenv("PES_USER_NAME") ? getenv("PES_USER_NAME") : "Player";
    if (strlen(value)+1>size) return (int32_t)0x8096000a; /* BUFFER_TOO_SHORT */
    strcpy(name,value); return 0;
}
static ABI int32_t user_color(int32_t id, int32_t *color) {
    (void)id;
    if (color) *color = 0; /* blue, as for user 1 */
    return 0;
}
static ABI int32_t user_event(int32_t *event) {
    if (!event) return USER_INVALID_ARGUMENT;
    host_lock(&lock);
    int pending=login_event_pending; login_event_pending=0;
    host_unlock(&lock);
    if (!pending) return USER_NO_EVENT;
    event[0]=0; event[1]=USER_ID; /* LOGIN */
    return 0;
}

/* ---- SystemService ---- */
static int language(void) { const char *v=getenv("PES_LANGUAGE"); return v ? atoi(v) : 1; }
static ABI int32_t system_param(int32_t id,int32_t *value) {
    if (!value) return SYSTEM_PARAMETER;
    switch (id) {
    case 1: *value=language(); break;           /* language (1 = English US, 8 = Russian) */
    case 2: *value=1; break;                    /* date format DD/MM/YYYY */
    case 3: *value=1; break;                    /* 24-hour clock */
#ifdef _WIN32
    case 4: *value=(int32_t)(runtime_utc_offset(time(NULL))/60); break;
#else
    case 4: { time_t now=time(NULL); struct tm t; localtime_r(&now,&t); *value=(int32_t)(t.tm_gmtoff/60); break; }
#endif
    case 5: *value=0; break;                    /* summer time */
    case 7: *value=0; break;                    /* parental level off */
    case 1000: *value=1; break;                 /* enter button = cross */
    default: fprintf(stderr,"STOP: unsupported system parameter %d\n",id); exit(21);
    }
    return 0;
}
static ABI int32_t system_status(unsigned char *status) {
    if (!status) return SYSTEM_PARAMETER;
    memset(status,0,12); /* event_num=0, no overlay, foreground, normal CPU mode */
    return 0;
}
static ABI int32_t system_event(void *event) { (void)event; return SYSTEM_NO_EVENT; }
static ABI int32_t hide_splash(void) { note("SystemService: splash screen hidden"); return 0; }
static ABI int32_t launch_browser(void) { note("SystemService: web browser request ignored (offline)"); return 0; }

/* ---- NetCtl / Net / Http / Ssl: no network ---- */
static int32_t net_errno;
static ABI int32_t net_init(void) { return 0; }
static ABI int32_t net_term(void) { return 0; }
static ABI int32_t *net_errno_loc(void) { return &net_errno; }
static ABI int32_t net_pool_create(const char *name,int size,int flags) { (void)name; (void)size; (void)flags; return new_id(); }
static ABI int32_t net_pool_destroy(int id) { (void)id; return 0; }
static ABI int32_t net_unreachable(void) { static unsigned n; if (n++<6) printf("DIAG: socket call -> ENETUNREACH\n"); net_errno=51; return NET_ENETUNREACH; }
static ABI int32_t net_epoll_create(const char *name,int flags) { (void)name; (void)flags; return new_id(); }
static ABI int32_t net_epoll_destroy(int id) { (void)id; return 0; }
static ABI int32_t net_resolver_create(const char *name,int pool,int flags) { (void)name; (void)pool; (void)flags; return new_id(); }
static ABI int32_t net_resolver_destroy(int id) { (void)id; return 0; }
static ABI uint16_t net_htons(uint16_t v) { return htons(v); }
static ABI uint16_t net_ntohs(uint16_t v) { return ntohs(v); }
static ABI uint32_t net_htonl(uint32_t v) { return htonl(v); }
static ABI uint32_t net_ntohl(uint32_t v) { return ntohl(v); }
static ABI int32_t net_pton(int af,const char *src,void *dst) {
    if (af!=2) { net_errno=47; return NET_EINVAL; }
#ifdef _WIN32
    return runtime_win_inet_pton4(src,dst);
#else
    return inet_pton(AF_INET,src,dst);
#endif
}
static ABI const char *net_ntop(int af,const void *src,char *dst,uint32_t size) {
    if (af!=2) { net_errno=47; return NULL; }
#ifdef _WIN32
    return runtime_win_inet_ntop4(src,dst,size);
#else
    return inet_ntop(AF_INET,src,dst,size);
#endif
}
static ABI int32_t netctl_state(int32_t *state) { static unsigned n; if (n++<6) printf("DIAG: sceNetCtlGetState polled\n"); if (!state) return NET_CTL_INVALID_ADDR; *state=0; return 0; }
static ABI int32_t netctl_info(int code,void *info) { (void)code; (void)info; return NET_CTL_NOT_CONNECTED; }
static ABI int32_t netctl_register(void *cb,void *arg,int32_t *cid) { static unsigned n; if (n++<6) printf("DIAG: sceNetCtlRegisterCallback registered\n"); (void)cb; (void)arg; if (!cid) return NET_CTL_INVALID_ADDR; *cid=new_id(); return 0; }
static ABI int32_t netctl_check(void) { static unsigned n; if (n++<6) printf("DIAG: sceNetCtlCheckCallback pumped\n"); return 0; }
static ABI int32_t netctl_unregister(int cid) { (void)cid; return 0; }
static ABI int32_t netctl_nat(uint32_t *info) {
    if (!info) return NET_CTL_INVALID_ADDR;
    info[1]=0; info[2]=3; info[3]=0; /* stun failed, NAT type 3, no mapped address */
    return 0;
}
static ABI int32_t netctl_init(void) { note("NetCtl initialized (offline)"); return 0; }
static ABI int32_t netctl_result(int code, int *result) { (void)code; if (result) *result = NET_CTL_NOT_CONNECTED; return 0; }
static ABI int32_t lib_init_id(void) { return new_id(); }
static ABI int32_t ok_void(void) { return 0; }
static ABI int32_t http_fail(void) { return HTTP_NETWORK; }
/* Objects are created so setup code proceeds; any transfer fails as unplugged. */
static ABI int32_t http_object(void) { static unsigned n; if (n++<6) printf("DIAG: HTTP object created\n"); return new_id(); }
static ABI int32_t http_epoll(int32_t ctx,void **handle) {
    (void)ctx;
    if (!handle) return (int32_t)0x80431077; /* HTTP INVALID_VALUE */
    *handle=(void *)(uintptr_t)(0x100+new_id()); return 0;
}
static ABI int32_t http_wait(void *handle,void *events,int32_t max,int64_t timeout) {
    static unsigned n; if (n++<6) printf("DIAG: HTTP wait polled (returns no events)\n");
    (void)handle; (void)events; (void)max;
    if (timeout>0) host_sleep_ns((uint64_t)timeout*1000);
    return 0; /* no events: nothing is in flight */
}

/* ---- NP (PSN): signed out ---- */
static ABI int32_t np_state(int32_t user,int32_t *state) {
    if (!state) return NP_INVALID_ARGUMENT;
    (void)user; *state=1; /* SIGNED_OUT */
    return 0;
}
static ABI int32_t np_signed_out(void) { return NP_SIGNED_OUT; }
static ABI int32_t np_register(void *cb,void *arg) { (void)cb; (void)arg; return new_id(); }
static ABI void np_register_void(void *cb,void *arg) { (void)cb; (void)arg; }
static ABI int32_t np_request(const void *param) { (void)param; return new_id(); }
static ABI int32_t np_request_ctx(int32_t ctx,const void *param) { (void)ctx; (void)param; return new_id(); }
static ABI int32_t np_poll(int32_t request,int32_t *result) { (void)request; if (result) *result=NP_SIGNED_OUT; return 0; }
static ABI int32_t np_compare(const void *a,const void *b) {
    if (!a || !b) return NP_INVALID_ARGUMENT;
    return memcmp(a,b,16) ? (int32_t)0x80550609 : 0; /* NP_UTIL NOT_MATCH */
}

/* ---- Voice chat: ports exist, carry no audio ---- */
static ABI int32_t voice_port(void *param,uint32_t *port) { (void)param; if (!port) return (int32_t)0x8029000b; *port=(uint32_t)new_id(); return 0; }
static ABI int32_t voice_read(uint32_t port,void *data,uint32_t *size) { (void)port; (void)data; if (size) *size=0; return 0; }
static ABI int32_t voice_write(uint32_t port,const void *data,uint32_t *size) { (void)port; (void)data; (void)size; return 0; }
static ABI int32_t voice_info(uint32_t port,uint32_t *info) {
    (void)port;
    if (!info) return (int32_t)0x8029000b;
    memset(info,0,40); /* type, state=unconnected, no bytes available */
    return 0;
}

/* ---- Common dialogs: nothing is displayed; an opened dialog finishes. ---- */
typedef struct { const char *name; int initialized, status; } Dialog;
static Dialog dialogs[]={{"CommonDialog",0,0},{"MsgDialog",0,0},{"SaveDataDialog",0,0},
                         {"NpProfileDialog",0,0},{"NpCommerceDialog",0,0},{"ImeDialog",0,0},
                         {"ErrorDialog",0,0},{"InvitationDialog",0,0},{"UsbStorageDialog",0,0}};
static ABI int32_t common_init(void) { return 0; }
static int32_t dialog_init(int i) {
    if (dialogs[i].initialized) return (int32_t)0x80B80004;
    dialogs[i].initialized=1; dialogs[i].status=1; return 0;
}
static unsigned last_save_dialog_mode;
static int32_t dialog_open(int i) {
    if (!dialogs[i].initialized) return DIALOG_NOT_INITIALIZED;
    /* Real CommonDialog protocol: a PROGRESS_BAR dialog stays RUNNING (status=2) while the
     * game streams its work (PES's save writer closes it itself when the loop is done).
     * Returning FINISHED immediately made PES abort its 5 MiB EDIT payload after the first
     * 1 MiB chunk ("Unable to Save."). */
    if (i == 2 && last_save_dialog_mode == 5u /* PROGRESS_BAR */) {
        dialogs[i].status = 2;
        printf("Runtime: SaveDataDialog progress keeps RUNNING (guest closes it)\n");
    } else {
        dialogs[i].status = 3;
        printf("Runtime: %s opened; completed immediately (no dialog UI yet)\n", dialogs[i].name);
    }
    return 0;
}static int32_t dialog_status(int i) { return dialogs[i].status; }
static int32_t dialog_term(int i) {
    if (!dialogs[i].initialized) return DIALOG_NOT_INITIALIZED;
    dialogs[i].initialized=0; dialogs[i].status=0; return 0;
}
#define DIALOG(tag,i) \
    static ABI int32_t tag##_init(void) { return dialog_init(i); } \
    static ABI int32_t tag##_open(const void *p) { (void)p; return dialog_open(i); } \
    static ABI int32_t tag##_status(void) { return dialog_status(i); } \
    static ABI int32_t tag##_term(void) { return dialog_term(i); }
DIALOG(msg,1) DIALOG(profile,3) DIALOG(commerce,4)
DIALOG(error,6) DIALOG(invitation,7) DIALOG(usb,8)
static ABI int32_t save_init(void) { return dialog_init(2); }static ABI int32_t save_status(void) { return dialog_status(2); }
static ABI int32_t save_term(void) { return dialog_term(2); }
static ABI int32_t save_open(const void *p) {
    if (p) {
        const uint8_t *b = (const uint8_t *)p;
        /* Real layout (shadPS4): baseParam(0x30), s32 size, mode, dispType, -, then
         * AnimationParam*, items, userMsgParam, sysMsgParam, errorCodeParam,
         * progressBarParam, userData, optionParam. */
        unsigned size = *(const unsigned *)(b + 0x30);
        unsigned mode = *(const unsigned *)(b + 0x34);
        unsigned disp = *(const unsigned *)(b + 0x38);
        const void *user_p   = *(const void **)(b + 0x50);
        const void *sys_p    = *(const void **)(b + 0x58);
        const void *err_p    = *(const void **)(b + 0x60);
        const void *prog_p   = *(const void **)(b + 0x68);
        last_save_dialog_mode = mode;
        printf("Runtime: SaveDataDialog open: size=%u mode=%u disp=%u\n", size, mode, disp);
        if (user_p) {
            const unsigned *w = (const unsigned *)user_p;
            const char *mptr = *(const char **)((const uint8_t *)user_p + 8);
            if (mptr) {
                char msg[160];
                for (unsigned i = 0; i < sizeof(msg) - 1 && mptr[i]; ++i) msg[i] = mptr[i] < 0x20 ? '?' : mptr[i];
                msg[sizeof(msg) - 1] = 0;
                printf("  USER_MSG: buttonType=%u msgType=%u msg=\"%s\"\n", w[0], w[1], msg);
            } else {
                printf("  USER_MSG: buttonType=%u msgType=%u msg=(null)\n", w[0], w[1]);
            }
        }
        if (sys_p) {
            const unsigned *w = (const unsigned *)sys_p;
            unsigned long long val = *(const unsigned long long *)((const uint8_t *)sys_p + 8);
            printf("  SYS_MSG: msgType=%u value=%llu\n", w[0], (unsigned long long)val);
        }
        if (err_p) {
            const unsigned *w = (const unsigned *)err_p;
            printf("  ERROR_CODE: code=0x%x\n", w[0]);
        }
        if (prog_p) {
            const unsigned *w = (const unsigned *)prog_p;
            printf("  PROGRESS: barType=%u msgPtr=%p sysMsgType=%u\n", w[0],
                   *(const void **)((const uint8_t *)prog_p + 8), *(const unsigned *)((const uint8_t *)prog_p + 16));
        }
    }
    return dialog_open(2);
}
static ABI int32_t dialog_generic_close(void) {
    for (size_t i = 0; i < sizeof(dialogs)/sizeof(dialogs[0]); ++i) {
        if (dialogs[i].initialized) dialogs[i].status = 1; /* Reset to INITIALIZED */
    }
    return 0;
}
static ABI int32_t dialog_generic_result(void *result) {
    if (result) {
        memset(result,0,16);
        int32_t *p = (int32_t *)result;
        p[0] = 0; /* mode */
        p[1] = 0; /* result = 0 (SCE_OK) */
        p[2] = 1; /* buttonId = 1 (SCE_COMMON_DIALOG_BUTTON_ID_OK) */
    }
    return 0;
}
static ABI int32_t save_dialog_close(void) {
    printf("Runtime: SaveDataDialog closed\n");
    /* Real protocol: Close on a RUNNING dialog transitions it to FINISHED (the game's UI
     * then polls UpdateStatus -> FINISHED and calls GetResult). Resetting to INITIALIZED
     * froze PES's post-save flow on a blank screen. */
    dialogs[2].status = 3; /* FINISHED */
    return 0;
}
static ABI int32_t save_dialog_result(void *result) {
    if (result) {
        /* Real OrbisSaveDataDialogResult (shadPS4): { mode @0, result @4, buttonId @8,
         * pad, DirName* @0x10, Param* @0x18, void* userData @0x20, reserved[32] }. */
        memset(result,0,0x50);
        unsigned *w = (unsigned *)result;
        w[0] = last_save_dialog_mode; /* mode of the dialog that was opened */
        w[1] = 0;                     /* result = OK */
        w[2] = 1;                     /* buttonId = OK */
    }
    printf("Runtime: sceSaveDataDialogGetResult -> Success (buttonId=1 / OK)\n");
    return 0;
}
static ABI int32_t dialog_generic_progress(int32_t value) { (void)value; return 0; }
static ABI int32_t ime_panel_size(void *param) { if (param) memset(param, 0, 16); return 0; }
static ABI int32_t profile_result(void *result) { if (result) memset(result,0,4); return 0; }
/* ImeDialog: text typed on the keyboard into the game window (title bar shows it).
 * OrbisImeDialogParam: user, type, languages(8), enter label, method, filter,
 * option, max length, char16 buffer, position, alignment, placeholder, title. */
typedef struct {
    int32_t user; uint32_t type; uint64_t languages; uint32_t enter_label, input_method;
    void *filter; uint32_t option, max_length; uint16_t *buffer;
    float x, y; uint32_t halign, valign; const uint16_t *placeholder, *title; int8_t reserved[16];
} ImeParam;
static struct { int running, finished, end_status; uint16_t *buffer; uint32_t max_length; } ime;
static size_t utf16_to_utf8(const uint16_t *in, size_t limit, char *out, size_t size) {
    size_t n=0;
    for (size_t i=0; in && i<limit && in[i] && n+4<size; ++i) {
        uint32_t c=in[i];
        if (c>=0xD800 && c<0xDC00 && i+1<limit && in[i+1]>=0xDC00 && in[i+1]<0xE000) { c=0x10000+((c-0xD800)<<10)+(in[i+1]-0xDC00); ++i; }
        if (c<0x80) out[n++]=(char)c;
        else if (c<0x800) { out[n++]=(char)(0xC0|c>>6); out[n++]=(char)(0x80|(c&63)); }
        else if (c<0x10000) { out[n++]=(char)(0xE0|c>>12); out[n++]=(char)(0x80|((c>>6)&63)); out[n++]=(char)(0x80|(c&63)); }
        else { out[n++]=(char)(0xF0|c>>18); out[n++]=(char)(0x80|((c>>12)&63)); out[n++]=(char)(0x80|((c>>6)&63)); out[n++]=(char)(0x80|(c&63)); }
    }
    out[n]=0; return n;
}
static void utf8_to_utf16(const char *in, uint16_t *out, uint32_t max) {
    uint32_t n=0;
    for (const unsigned char *p=(const unsigned char *)in; *p && n<max;) {
        uint32_t c; int extra;
        if (*p<0x80) { c=*p; extra=0; } else if ((*p&0xE0)==0xC0) { c=*p&31; extra=1; }
        else if ((*p&0xF0)==0xE0) { c=*p&15; extra=2; } else { c=*p&7; extra=3; }
        ++p;
        for (int k=0;k<extra && (*p&0xC0)==0x80;++k) c=(c<<6)|(*p++&63);
        if (c>=0x10000) { if (n+2>max) break; c-=0x10000; out[n++]=(uint16_t)(0xD800+(c>>10)); out[n++]=(uint16_t)(0xDC00+(c&1023)); }
        else out[n++]=(uint16_t)c;
    }
    out[n<max ? n : max]=0;
}
static void ime_complete(int end_status, const char *text) {
    if (!end_status && ime.buffer && ime.max_length) utf8_to_utf16(text,ime.buffer,ime.max_length);
    ime.end_status=end_status; ime.finished=1; ime.running=0;
    printf("Runtime: ImeDialog %s%s%s\n",end_status ? "cancelled" : "text: ",end_status ? "" : text,"");
}
static ABI int32_t ime_init(const ImeParam *param, const void *extended) {
    (void)extended;
    if (!param || !param->buffer || !param->max_length) return (int32_t)0x80BC0004; /* IME INVALID_ADDRESS */
    if (ime.running) return (int32_t)0x80BC0003;                                  /* BUSY */
    memset(&ime,0,sizeof(ime));
    ime.buffer=param->buffer; ime.max_length=param->max_length; ime.running=1;
    char initial[512], prompt[256];
    utf16_to_utf8(param->buffer,param->max_length,initial,sizeof(initial));
    utf16_to_utf8(param->title,128,prompt,sizeof(prompt));
    const char *preset=getenv("PES_IME_TEXT");
    if (preset) { ime_complete(0,preset); return 0; }
    (void)prompt; /* no on-screen keyboard yet: complete at once with the user name or the initial text */
    const char *name=getenv("PES_USER_NAME");
    ime_complete(0,name ? name : initial[0] ? initial : "Player");
    return 0;
}
static ABI int32_t ime_status(void) {
    return ime.running ? 1 : ime.finished ? 2 : 0; /* Running / Finished / None */
}
static ABI int32_t ime_result(uint32_t *result) {
    if (!ime.finished) return (int32_t)0x80BC0101; /* DIALOG NOT_FINISHED */
    if (result) *result=(uint32_t)ime.end_status;
    return 0;
}
static ABI int32_t ime_term(void) { memset(&ime,0,sizeof(ime)); return 0; }

/* ---- Trophies: kept in <user>/trophies.txt ("id unix_time name" per line), shown by the overlay ----
 * Placeholder list (40 ids, grades of the upstream base); the PES list comes later (open decision). */
enum { BRONZE=1, SILVER, GOLD, PLATINUM };
typedef struct { const char *name, *description; int grade, hidden; } Trophy;
static const Trophy trophies[]={
    {"Trophy 0","",PLATINUM,0},
    {"Trophy 1","",GOLD,1},
    {"Trophy 2","",GOLD,1},
    {"Trophy 3","",GOLD,1},
    {"Trophy 4","",GOLD,1},
    {"Trophy 5","",GOLD,0},
    {"Trophy 6","",GOLD,0},
    {"Trophy 7","",SILVER,0},
    {"Trophy 8","",SILVER,0},
    {"Trophy 9","",SILVER,0},
    {"Trophy 10","",SILVER,1},
    {"Trophy 11","",SILVER,1},
    {"Trophy 12","",SILVER,1},
    {"Trophy 13","",SILVER,1},
    {"Trophy 14","",BRONZE,1},
    {"Trophy 15","",BRONZE,1},
    {"Trophy 16","",BRONZE,1},
    {"Trophy 17","",BRONZE,1},
    {"Trophy 18","",BRONZE,1},
    {"Trophy 19","",BRONZE,1},
    {"Trophy 20","",BRONZE,1},
    {"Trophy 21","",BRONZE,1},
    {"Trophy 22","",BRONZE,1},
    {"Trophy 23","",BRONZE,1},
    {"Trophy 24","",BRONZE,1},
    {"Trophy 25","",BRONZE,1},
    {"Trophy 26","",BRONZE,1},
    {"Trophy 27","",BRONZE,1},
    {"Trophy 28","",BRONZE,1},
    {"Trophy 29","",BRONZE,0},
    {"Trophy 30","",BRONZE,0},
    {"Trophy 31","",BRONZE,0},
    {"Trophy 32","",BRONZE,1},
    {"Trophy 33","",BRONZE,1},
    {"Trophy 34","",GOLD,0},
    {"Trophy 35","",SILVER,1},
    {"Trophy 36","",BRONZE,1},
    {"Trophy 37","",BRONZE,1},
    {"Trophy 38","",BRONZE,1},
    {"Trophy 39","",BRONZE,1},
};
#define TROPHY_COUNT 40
_Static_assert(sizeof(trophies)/sizeof(*trophies)==TROPHY_COUNT,"trophy list");
#define TROPHY_BASE_LAST 33 /* the platinum (id 0) needs 1..33; DLC trophies do not count */
static uint64_t trophies_unlocked;
static int64_t trophy_times[TROPHY_COUNT];
static int trophies_loaded;
static void trophy_path(char *out,size_t size) { snprintf(out,size,"%s/trophies.txt",runtime_file_user_dir()); }
static void trophies_load(void) { /* under lock */
    if (trophies_loaded) return;
    trophies_loaded=1;
    char path[1024]; trophy_path(path,sizeof(path));
    FILE *f=fopen(path,"r");
    if (!f) return;
    char line[256];
    while (fgets(line,sizeof(line),f)) {
        int id; long long when=0;
        if (sscanf(line,"%d %lld",&id,&when)<1 || id<0 || id>=TROPHY_COUNT) continue;
        trophies_unlocked|=1ull<<id;
        trophy_times[id]=when;
    }
    fclose(f);
}
static void trophy_award(int id) { /* under lock */
    trophies_unlocked|=1ull<<id;
    trophy_times[id]=(int64_t)time(NULL);
    char path[1024]; trophy_path(path,sizeof(path));
    FILE *f=fopen(path,"a");
    if (f) { fprintf(f,"%d %lld %s\n",id,(long long)trophy_times[id],trophies[id].name); fclose(f); }
    printf("Runtime: trophy %d unlocked: %s\n",id,trophies[id].name);
}
/* Overlay's trophy list (any thread): 0 past the last id; *unlocked_time is 0 while locked. */
int runtime_trophy(int id,const char **name,const char **description,int *grade,int *hidden,int64_t *unlocked_time) {
    if (id<0 || id>=TROPHY_COUNT) return 0;
    *name=trophies[id].name; *description=trophies[id].description;
    *grade=trophies[id].grade; *hidden=trophies[id].hidden;
    host_lock(&lock); trophies_load();
    *unlocked_time=trophies_unlocked>>id & 1 ? (trophy_times[id] ? trophy_times[id] : 1) : 0;
    host_unlock(&lock);
    return 1;
}
static ABI int32_t trophy_context(int32_t *ctx,int32_t user,uint32_t label,uint64_t options) {
    (void)user; (void)label; (void)options;
    if (!ctx) return TROPHY_INVALID;
    *ctx=new_id(); return 0;
}
static ABI int32_t trophy_handle(int32_t *handle) { if (!handle) return TROPHY_INVALID; *handle=new_id(); return 0; }
static ABI int32_t trophy_register(int32_t ctx,int32_t handle,uint64_t options) { (void)ctx; (void)handle; (void)options; return 0; }
static ABI int32_t trophy_unlock(int32_t ctx,int32_t handle,int32_t id,int32_t *platinum) {
    (void)ctx; (void)handle;
    if (platinum) *platinum=-1;
    if (id<0 || id>=TROPHY_COUNT) { printf("Runtime: unknown trophy %d\n",id); return 0; }
    host_lock(&lock);
    trophies_load();
    if (!(trophies_unlocked>>id & 1)) {
        trophy_award(id);
        const uint64_t base=((1ull<<(TROPHY_BASE_LAST+1))-1) & ~1ull;
        if ((trophies_unlocked&base)==base && !(trophies_unlocked&1)) {
            trophy_award(0);
            if (platinum) *platinum=0;
        }
    }
    host_unlock(&lock);
    return 0;
}
static ABI int32_t trophy_game_info(int32_t ctx,int32_t handle,void *details,void *data) {
    (void)ctx; (void)handle;
    if (details) { uint64_t size; memcpy(&size,details,8); memset((char *)details+8,0,size>8 && size<4096 ? size-8 : 0); }
    if (data) { uint64_t size; memcpy(&size,data,8); memset((char *)data+8,0,size>8 && size<4096 ? size-8 : 0); }
    return 0;
}
static ABI int32_t trophy_info(int32_t ctx,int32_t handle,int32_t id,void *details,void *data) {
    trophy_game_info(ctx,handle,details,data);
    uint64_t size=0;
    if (data) memcpy(&size,data,8);
    if (size>=16 && id>=0 && id<TROPHY_COUNT) { /* SceNpTrophyData: size, trophyId, unlocked */
        host_lock(&lock); trophies_load(); const int unlocked=(int)(trophies_unlocked>>id & 1); host_unlock(&lock);
        memcpy((char *)data+8,&id,4);
        ((char *)data)[12]=(char)unlocked;
    }
    return 0;
}
static ABI int32_t trophy_destroy_context(int32_t ctx) { (void)ctx; return 0; }
static ABI int32_t trophy_abort_handle(int32_t handle) { (void)handle; return 0; }
static ABI int32_t trophy_destroy_handle(int32_t handle) { (void)handle; return 0; }
static ABI int32_t trophy_unlock_state(int32_t ctx, int32_t handle, uint32_t *flags, uint32_t *count) {
    (void)ctx; (void)handle;
    host_lock(&lock);
    trophies_load();
    if (flags) {
        flags[0] = (uint32_t)(trophies_unlocked & 0xFFFFFFFF);
        flags[1] = (uint32_t)(trophies_unlocked >> 32);
    }
    if (count) {
        *count = TROPHY_COUNT;
    }
    host_unlock(&lock);
    return 0;
}
static ABI int32_t np_get_account_id(int32_t user, uint64_t *account_id) {
    (void)user;
    if (account_id) *account_id = 0x12345678ULL;
    return 0;
}
static ABI int32_t np_commerce_show_icon(const void *p) { (void)p; return 0; }
static ABI int32_t np_word_filter_abort(int32_t req) { (void)req; return 0; }
static ABI int32_t np_word_filter_censor(const void *p) { (void)p; return 0; }

/* ---- PlayGo: fully installed package ---- */
static int playgo_handle, playgo_chunks=-1;
typedef struct ScePlayGoProgress {
    uint64_t progress;
    uint64_t total;
} ScePlayGoProgress;

static ABI int32_t playgo_init(const void *params) {
    (void)params;
    int fd=(int)runtime_file_open("/app0/sce_sys/playgo-chunk.dat",0,0);
    unsigned char header[12];
    playgo_chunks=1;
    if (fd>=0) {
        if (runtime_file_read(fd,header,sizeof(header))==sizeof(header)) playgo_chunks=header[10]|header[11]<<8;
        runtime_file_close(fd);
    }
    printf("Runtime: PlayGo initialized; %d chunks, all installed locally\n",playgo_chunks);
    return 0;
}
static ABI int32_t playgo_open(int32_t *handle,const void *param) {
    (void)param;
    if (!handle) return PLAYGO_BAD_POINTER;
    playgo_handle=1; *handle=1;
    printf("Runtime: scePlayGoOpen -> handle=1\n");
    return 0;
}
static ABI int32_t playgo_chunk_ids(int32_t handle,uint16_t *ids,uint32_t count,uint32_t *out) {
    if (handle!=playgo_handle || !handle) return PLAYGO_BAD_HANDLE;
    if (!out) return PLAYGO_BAD_POINTER;
    if (ids && !count) return PLAYGO_BAD_SIZE;
    if (!ids) { *out=(uint32_t)playgo_chunks; return 0; }
    uint32_t n=count<(uint32_t)playgo_chunks ? count : (uint32_t)playgo_chunks;
    for (uint32_t i=0;i<n;++i) ids[i]=(uint16_t)i;
    *out=n; return 0;
}
static ABI int32_t playgo_locus(int32_t handle,const uint16_t *ids,uint32_t count,int8_t *loci) {
    (void)handle;
    if (!ids || !loci) return PLAYGO_BAD_POINTER;
    if (!count) return PLAYGO_BAD_SIZE;
    for (uint32_t i=0;i<count;++i) {
        loci[i]=3; /* LocalFast (3): chunk is present and fast on local disk */
    }
    return 0;
}
static ABI int32_t playgo_speed(int32_t handle,int32_t speed) {
    (void)handle; (void)speed;
    return 0;
}
static ABI int32_t playgo_close(int32_t handle) {
    (void)handle;
    printf("Runtime: scePlayGoClose(handle=%d)\n", handle);
    return 0;
}
static ABI int32_t playgo_get_speed(int32_t handle, int32_t *speed) {
    (void)handle;
    if (speed) *speed = 1; /* Normal */
    return 0;
}
static ABI int32_t playgo_get_lang(int32_t handle, uint32_t *mask) {
    (void)handle;
    if (mask) *mask = 0xFFFFFFFF;
    return 0;
}
static ABI int32_t playgo_set_lang(int32_t handle, uint32_t mask) {
    (void)handle; (void)mask;
    return 0;
}
static ABI int32_t playgo_get_progress(int32_t handle, const uint16_t *chunk_ids, uint32_t count, ScePlayGoProgress *progress) {
    (void)handle; (void)chunk_ids; (void)count;
    if (!progress) return PLAYGO_BAD_POINTER;
    progress->progress = 1000000000ULL;
    progress->total = 1000000000ULL;
    return 0;
}
static ABI int32_t playgo_get_todo(int32_t handle, uint16_t *todo_ids, uint32_t count, uint32_t *out_count) {
    (void)handle; (void)todo_ids; (void)count;
    if (out_count) *out_count = 0;
    return 0;
}
static ABI int32_t playgo_set_todo(int32_t handle, const uint16_t *chunks, uint32_t count) {
    (void)handle; (void)chunks; (void)count;
    return 0;
}
static ABI int32_t playgo_term(void) {
    printf("Runtime: scePlayGoTerminate\n");
    return 0;
}

/* ---- GameLiveStreaming ---- */
static ABI int32_t live_stream_init(void *param) { (void)param; note("GameLiveStreaming initialized (offline)"); return 0; }
static ABI int32_t live_stream_enable(int32_t enable) { (void)enable; return 0; }
static ABI int32_t live_stream_term(void) { return 0; }

/* ---- DiscMap: the game is fully installed, no disc bitmap exists ---- */
#define DISC_MAP_NO_BITMAP ((int32_t)0x81100004)
static ABI int32_t discmap_on_hdd(const char *path,int64_t offset,int64_t size,int32_t *result) {
    (void)path; (void)offset; (void)size; (void)result; return DISC_MAP_NO_BITMAP;
}
static ABI int32_t discmap_8a82(const char *path,int64_t offset,int64_t size,int32_t *flags,int32_t *r1,int32_t *r2) {
    (void)path; (void)offset; (void)size; (void)flags; (void)r1; (void)r2; return DISC_MAP_NO_BITMAP;
}

/* ---- Devices that are absent: mouse, microphone, voice chat ---- */
/* A mouse port opens but never reports a connected device. */
static ABI int32_t mouse_open(int32_t user,int32_t type,int32_t index,const void *param) { (void)user; (void)type; (void)index; (void)param; return new_id(); }
static ABI int32_t mouse_read(int32_t handle,unsigned char *data,int32_t count) {
    (void)handle;
    if (!data || count<1) return (int32_t)0x80DF0001;
    memset(data,0,40); return 1; /* timestamp 0, connected=false */
}
static ABI int32_t mouse_close(int32_t handle) { (void)handle; return 0; }
static ABI int32_t audio_in_open(void) { return AUDIO_IN_NOT_OPENED; }

/* ---- Coredump / Diagnostics ---- */
static ABI int32_t coredump_register(void *handler, size_t size, void *arg) {
    (void)handler; (void)size; (void)arg;
    note("Coredump: registered custom crash handler (OK)");
    return 0;
}

/* ---- ScreenShot ---- */
static ABI int32_t screenshot_enable(void) { return 0; }
static ABI int32_t screenshot_disable(void) { return 0; }

/* ---- Sysmodule ---- */
static ABI int32_t sysmodule_is_loaded(uint16_t id) {
    (void)id;
    return 0;
}
static ABI int32_t sysmodule_load(uint16_t id) {
    printf("Runtime: sceSysmoduleLoadModule(0x%04x)\n", id);
    return 0;
}
static ABI int32_t sysmodule_unload(uint16_t id) {
    (void)id;
    return 0;
}

/* ---- SharePlay ---- */
static ABI int32_t shareplay_init(void *param) { (void)param; note("SharePlay initialized (offline)"); return 0; }
static ABI int32_t shareplay_term(void) { return 0; }
static ABI int32_t shareplay_prohibit(int32_t prohibit) { (void)prohibit; return 0; }
static ABI int32_t shareplay_conn_info(void *info) { (void)info; return (int32_t)0x80AF0001; }

/* ---- WebBrowserDialog ---- */
static ABI int32_t browser_dialog_init(void) { return 0; }
static ABI int32_t browser_dialog_open(const void *param) { (void)param; return 0; }
static ABI int32_t browser_dialog_close(void) { return 0; }
static ABI int32_t browser_dialog_term(void) { return 0; }
static ABI int32_t browser_dialog_status(void) { return 2; /* Finished */ }
static ABI int32_t browser_dialog_result(void *result) { (void)result; return 0; }


static const RuntimeExport exports[]={
    {"sceUserServiceInitialize",user_initialize}, {"sceUserServiceTerminate",user_terminate},
    {"sceUserServiceGetInitialUser",user_initial}, {"sceUserServiceGetLoginUserIdList",user_list},
    {"sceUserServiceGetUserName",user_name}, {"sceUserServiceGetEvent",user_event},
    {"sceUserServiceGetUserColor",user_color},
    {"sceSystemServiceParamGetInt",system_param}, {"sceSystemServiceGetStatus",system_status},
    {"sceSystemServiceReceiveEvent",system_event}, {"sceSystemServiceHideSplashScreen",hide_splash},
    {"sceSystemServiceLaunchWebBrowser",launch_browser},
    {"sceNetInit",net_init}, {"sceNetTerm",net_term}, {"sceNetErrnoLoc",net_errno_loc},
    {"sceNetPoolCreate",net_pool_create}, {"sceNetPoolDestroy",net_pool_destroy},
    {"sceNetEpollCreate",net_epoll_create}, {"sceNetEpollDestroy",net_epoll_destroy},
    {"sceNetResolverCreate",net_resolver_create}, {"sceNetResolverDestroy",net_resolver_destroy},
    {"sceNetHtons",net_htons}, {"sceNetNtohs",net_ntohs}, {"sceNetHtonl",net_htonl}, {"sceNetNtohl",net_ntohl},
    {"sceNetInetPton",net_pton}, {"sceNetInetNtop",net_ntop},
    {"sceNetSocket",net_unreachable}, {"sceNetConnect",net_unreachable}, {"sceNetBind",net_unreachable},
    {"sceNetListen",net_unreachable}, {"sceNetAccept",net_unreachable}, {"sceNetSend",net_unreachable},
    {"sceNetSendto",net_unreachable}, {"sceNetRecv",net_unreachable}, {"sceNetRecvfrom",net_unreachable},
    {"sceNetSetsockopt",net_unreachable}, {"sceNetGetsockopt",net_unreachable},
    {"sceNetGetsockname",net_unreachable}, {"sceNetShutdown",net_unreachable},
    {"sceNetSocketClose",net_unreachable}, {"sceNetSocketAbort",net_unreachable},
    {"sceNetEpollControl",net_unreachable}, {"sceNetEpollWait",net_unreachable}, {"sceNetEpollAbort",net_unreachable},
    {"sceNetResolverStartNtoa",net_unreachable}, {"sceNetResolverStartAton",net_unreachable},
    {"sceNetCtlInit",netctl_init}, {"sceNetCtlTerm",ok_void},
    {"sceNetCtlGetState",netctl_state}, {"sceNetCtlGetInfo",netctl_info},
    {"sceNetCtlGetResult",netctl_result},
    {"sceNetCtlRegisterCallback",netctl_register}, {"sceNetCtlCheckCallback",netctl_check},
    {"sceNetCtlUnregisterCallback",netctl_unregister}, {"sceNetCtlGetNatInfo",netctl_nat},
    {"sceSslInit",lib_init_id}, {"sceSslTerm",ok_void},
    {"sceHttpInit",lib_init_id}, {"sceHttpTerm",ok_void},
    {"sceHttpCreateTemplate",http_object}, {"sceHttpDeleteTemplate",ok_void},
    {"sceHttpCreateConnectionWithURL",http_object}, {"sceHttpCreateRequestWithURL",http_object},
    {"sceHttpSendRequest",http_fail}, {"sceHttpCreateEpoll",http_epoll},
    {"sceHttpSetNonblock",ok_void}, {"sceHttpSetConnectTimeOut",ok_void},
    {"sceHttpsEnableOption",ok_void}, {"sceHttpsDisableOption",ok_void},
    {"sceHttpAddRequestHeader",ok_void}, {"sceHttpSetRequestContentLength",ok_void},
    {"sceHttpDeleteConnection",ok_void}, {"sceHttpDeleteRequest",ok_void},
    {"sceHttpAbortWaitRequest",ok_void}, {"sceHttpDestroyEpoll",ok_void},
    {"sceHttpSetEpoll",ok_void}, {"sceHttpUnsetEpoll",ok_void}, {"sceHttpWaitRequest",http_wait},
    {"sceHttpGetStatusCode",http_fail}, {"sceHttpGetResponseContentLength",http_fail},
    {"sceHttpReadData",http_fail},
    {"sceNpGetState",np_state}, {"sceNpGetOnlineId",np_signed_out}, {"sceNpGetNpId",np_signed_out},
    {"sceNpRegisterStateCallback",np_register}, {"sceNpUnregisterStateCallback",ok_void},
    {"sceNpRegisterGamePresenceCallback",np_register_void}, {"sceNpRegisterPlusEventCallback",np_register},
    {"sceNpUnregisterPlusEventCallback",ok_void}, {"sceNpCheckCallback",ok_void},
    {"sceNpSetNpTitleId",ok_void}, {"sceNpNotifyPlusFeature",ok_void}, {"sceNpSetContentRestriction",ok_void},
    {"sceNpCreateAsyncRequest",np_request}, {"sceNpDeleteRequest",ok_void}, {"sceNpAbortRequest",ok_void},
    {"sceNpPollAsync",np_poll}, {"sceNpCheckNpAvailability",np_signed_out},
    {"sceNpGetParentalControlInfo",np_signed_out}, {"sceNpCheckPlus",np_signed_out},
    {"sceNpGetGamePresenceStatus",np_signed_out},
    {"sceNpCmpNpId",np_compare}, {"sceNpCmpOnlineId",np_compare},
    {"sceNpAuthCreateAsyncRequest",np_request}, {"sceNpAuthDeleteRequest",ok_void},
    {"sceNpAuthPollAsync",np_poll}, {"sceNpAuthGetAuthorizationCode",np_signed_out},
    {"sceNpLookupCreateTitleCtx",np_request}, {"sceNpLookupDeleteTitleCtx",ok_void},
    {"sceNpLookupCreateAsyncRequest",np_request_ctx}, {"sceNpLookupDeleteRequest",ok_void},
    {"sceNpLookupAbortRequest",ok_void}, {"sceNpLookupPollAsync",np_poll}, {"sceNpLookupNpId",np_signed_out},
    {"sceNpScoreCreateNpTitleCtx",np_request_ctx}, {"sceNpScoreDeleteNpTitleCtx",ok_void},
    {"sceNpScoreCreateRequest",np_request}, {"sceNpScoreDeleteRequest",ok_void}, {"sceNpScoreAbortRequest",ok_void},
    {"sceNpWebApiInitialize",lib_init_id}, {"sceNpWebApiTerminate",ok_void},
    {"sceNpWebApiCreateContext",np_signed_out},
    {"sceNpWebApiCreateRequest",np_signed_out}, {"sceNpWebApiSendRequest",np_signed_out},
    {"sceNpWebApiDeleteRequest",ok_void}, {"sceNpWebApiAbortRequest",ok_void},
    {"sceNpWebApiDeleteContext",ok_void}, {"sceNpWebApiReadData",np_signed_out},
    {"sceNpWebApiGetHttpStatusCode",np_signed_out}, {"sceNpWebApiGetHttpResponseHeaderValue",np_signed_out},
    {"sceNpWebApiGetHttpResponseHeaderValueLength",np_signed_out},
    {"sceNpWebApiCreatePushEventFilter",np_signed_out}, {"sceNpWebApiDeletePushEventFilter",ok_void},
    {"sceNpWebApiRegisterPushEventCallback",np_signed_out}, {"sceNpWebApiUnregisterPushEventCallback",ok_void},
    {"sceNpWebApiUtilityParseNpId",np_signed_out},
    {"sceNpMatching2ContextStart",np_signed_out}, {"sceNpMatching2ContextStop",ok_void},
    {"sceNpMatching2DestroyContext",ok_void},
    {"sceNpMatching2RegisterContextCallback",ok_void}, {"sceNpMatching2RegisterLobbyEventCallback",ok_void},
    {"sceNpMatching2RegisterRoomEventCallback",ok_void}, {"sceNpMatching2RegisterSignalingCallback",ok_void},
    {"sceNpMatching2SetDefaultRequestOptParam",ok_void},
    {"sceNpMatching2CreateJoinRoom",np_signed_out}, {"sceNpMatching2JoinRoom",np_signed_out},
    {"sceNpMatching2LeaveRoom",np_signed_out}, {"sceNpMatching2SearchRoom",np_signed_out},
    {"sceNpMatching2GetServerId",np_signed_out}, {"sceNpMatching2GetWorldInfoList",np_signed_out},
    {"sceNpMatching2GetLobbyInfoList",np_signed_out}, {"sceNpMatching2JoinLobby",np_signed_out},
    {"sceNpMatching2LeaveLobby",np_signed_out}, {"sceNpMatching2GrantRoomOwner",np_signed_out},
    {"sceNpMatching2KickoutRoomMember",np_signed_out}, {"sceNpMatching2SetRoomDataExternal",np_signed_out},
    {"sceNpMatching2SetRoomDataInternal",np_signed_out}, {"sceNpMatching2SetRoomMemberDataInternal",np_signed_out},
    {"sceNpMatching2SignalingGetConnectionStatus",np_signed_out}, {"sceNpMatching2SignalingGetPingInfo",np_signed_out},
    {"sceNpSignalingDeleteContext",ok_void}, {"sceNpSignalingActivateConnection",np_signed_out},
    {"sceNpSignalingDeactivateConnection",ok_void}, {"sceNpSignalingGetConnectionStatus",np_signed_out},
    {"sceNpScoreCensorComment",np_signed_out}, {"sceNpScoreSanitizeComment",np_signed_out},
    {"sceNpScoreGetBoardInfo",np_signed_out}, {"sceNpScoreGetGameData",np_signed_out},
    {"sceNpScoreGetRankingByNpIdPcId",np_signed_out}, {"sceNpScoreGetRankingByRange",np_signed_out},
    {"sceNpScoreRecordGameData",np_signed_out}, {"sceNpScoreRecordScore",np_signed_out},
    {"sceNpScoreSetPlayerCharacterId",ok_void},
    {"sceVoiceCreatePort",voice_port}, {"sceVoiceDeletePort",ok_void},
    {"sceVoiceConnectIPortToOPort",ok_void}, {"sceVoiceDisconnectIPortFromOPort",ok_void},
    {"sceVoiceStart",ok_void}, {"sceVoiceStop",ok_void}, {"sceVoiceGetPortInfo",voice_info},
    {"sceVoiceReadFromOPort",voice_read}, {"sceVoiceWriteToIPort",voice_write},
    {"sceAudioInInput",audio_in_open}, {"sceAudioInClose",audio_in_open},
    {"sceNpMatching2Initialize",ok_void}, {"sceNpMatching2Terminate",ok_void},
    {"sceNpMatching2CreateContext",np_signed_out},
    {"sceNpSignalingInitialize",ok_void}, {"sceNpSignalingTerminate",ok_void},
    {"sceNpSignalingCreateContext",np_signed_out},
    {"sceCommonDialogInitialize",common_init},
    {"sceMsgDialogInitialize",msg_init}, {"sceMsgDialogOpen",msg_open},
    {"sceMsgDialogUpdateStatus",msg_status}, {"sceMsgDialogTerminate",msg_term},
    {"sceSaveDataDialogInitialize",save_init}, {"sceSaveDataDialogOpen",save_open},
    {"sceSaveDataDialogUpdateStatus",save_status}, {"sceSaveDataDialogTerminate",save_term},
    {"sceNpProfileDialogInitialize",profile_init}, {"sceNpProfileDialogOpen",profile_open},
    {"sceNpProfileDialogUpdateStatus",profile_status}, {"sceNpProfileDialogTerminate",profile_term},
    {"sceNpProfileDialogGetResult",profile_result},
    {"sceNpCommerceDialogInitialize",commerce_init}, {"sceNpCommerceDialogOpen",commerce_open},
    {"sceNpCommerceDialogUpdateStatus",commerce_status}, {"sceNpCommerceDialogTerminate",commerce_term},
    {"sceImeDialogInit",ime_init}, {"sceImeDialogGetStatus",ime_status}, {"sceImeDialogGetResult",ime_result},
    {"sceImeDialogTerm",ime_term}, {"sceImeDialogAbort",ime_term},
    {"sceNpTrophyCreateContext",trophy_context}, {"sceNpTrophyCreateHandle",trophy_handle},
    {"sceNpTrophyRegisterContext",trophy_register}, {"sceNpTrophyUnlockTrophy",trophy_unlock},
    {"sceNpTrophyGetGameInfo",trophy_game_info}, {"sceNpTrophyGetTrophyInfo",trophy_info},
    {"scePlayGoInitialize",playgo_init}, {"scePlayGoOpen",playgo_open}, {"scePlayGoGetChunkId",playgo_chunk_ids},
    {"scePlayGoGetLocus",playgo_locus}, {"scePlayGoSetInstallSpeed",playgo_speed},
    {"sceMouseInit",ok_void}, {"sceMouseOpen",mouse_open}, {"sceMouseRead",mouse_read}, {"sceMouseClose",mouse_close},
    {"sceAudioInOpen",audio_in_open},
    {"sceDiscMapIsRequestOnHDD",discmap_on_hdd}, {"sceDiscMap_8A828CAEE7EDD5E9",discmap_8a82},
    {"sceVoiceInit",ok_void}, {"sceVoiceEnd",ok_void},
    {"sceErrorDialogInitialize",error_init}, {"sceErrorDialogOpen",error_open},
    {"sceErrorDialogUpdateStatus",error_status}, {"sceErrorDialogClose",dialog_generic_close},
    {"sceErrorDialogTerminate",error_term},
    {"sceInvitationDialogInitialize",invitation_init}, {"sceInvitationDialogOpen",invitation_open},
    {"sceInvitationDialogUpdateStatus",invitation_status}, {"sceInvitationDialogClose",dialog_generic_close},
    {"sceInvitationDialogTerminate",invitation_term},
    {"sceUsbStorageDialogInitialize",usb_init}, {"sceUsbStorageDialogOpen",usb_open},
    {"sceUsbStorageDialogUpdateStatus",usb_status}, {"sceUsbStorageDialogClose",dialog_generic_close},
    {"sceUsbStorageDialogTerminate",usb_term}, {"sceUsbStorageDialogGetResult",dialog_generic_result},
    {"sceMsgDialogClose",dialog_generic_close},
    {"sceSaveDataDialogClose",save_dialog_close},
    {"sceSaveDataDialogGetResult",save_dialog_result},
    {"sceSaveDataDialogProgressBarSetValue",dialog_generic_progress},
    {"sceNpProfileDialogClose",dialog_generic_close},
    {"sceNpCommerceDialogClose",dialog_generic_close},
    {"sceImeDialogGetPanelSize",ime_panel_size},
    {"sceNpTrophyDestroyContext",trophy_destroy_context},
    {"sceNpTrophyAbortHandle",trophy_abort_handle},
    {"sceNpTrophyDestroyHandle",trophy_destroy_handle},
    {"sceNpTrophyGetTrophyUnlockState",trophy_unlock_state},
    {"sceNpGetAccountId",np_get_account_id},
    {"sceNpCommerceShowPsStoreIcon",np_commerce_show_icon},
    {"sceNpWordFilterAbortRequest",np_word_filter_abort},
    {"sceNpWordFilterCensorComment",np_word_filter_censor},
    {"sceCoredumpRegisterCoredumpHandler",coredump_register},
    {"sceCoredumpAttachMemoryRegionAsUserFile",ok_void},
    {"sceCoredumpAttachUserFile",ok_void},
    {"sceCoredumpGetStopInfoCpu",ok_void},
    {"sceCoredumpGetStopInfoGpu",ok_void},
    {"sceCoredumpGetThreadContextInfo",ok_void},
    {"sceCoredumpWriteUserData",ok_void},
    {"sceGameLiveStreamingEnableLiveStreaming",live_stream_enable},
    {"sceGameLiveStreamingInitialize",live_stream_init},
    {"sceGameLiveStreamingTerminate",live_stream_term},
    {"scePlayGoClose",playgo_close},
    {"scePlayGoGetInstallSpeed",playgo_get_speed},
    {"scePlayGoGetLanguageMask",playgo_get_lang},
    {"scePlayGoSetLanguageMask",playgo_set_lang},
    {"scePlayGoGetProgress",playgo_get_progress},
    {"scePlayGoGetToDoList",playgo_get_todo},
    {"scePlayGoSetToDoList",playgo_set_todo},
    {"scePlayGoTerminate",playgo_term},
    {"sceScreenShotDisable",screenshot_disable},
    {"sceScreenShotEnable",screenshot_enable},
    {"sceSysmoduleIsLoaded",sysmodule_is_loaded},
    {"sceSysmoduleLoadModule",sysmodule_load},
    {"sceSysmoduleUnloadModule",sysmodule_unload},
    {"sceSharePlayInitialize",shareplay_init},
    {"sceSharePlayTerminate",shareplay_term},
    {"sceSharePlaySetProhibition",shareplay_prohibit},
    {"sceSharePlayGetCurrentConnectionInfo",shareplay_conn_info},
    {"sceWebBrowserDialogInitialize",browser_dialog_init},
    {"sceWebBrowserDialogOpen",browser_dialog_open},
    {"sceWebBrowserDialogClose",browser_dialog_close},
    {"sceWebBrowserDialogTerminate",browser_dialog_term},
    {"sceWebBrowserDialogUpdateStatus",browser_dialog_status},
    {"sceWebBrowserDialogGetResult",browser_dialog_result},
};
uintptr_t runtime_services_resolve(const char *name) {
    return RUNTIME_LOOKUP(exports,name);
}
