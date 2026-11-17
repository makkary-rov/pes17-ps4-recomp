// SPDX-License-Identifier: GPL-2.0-or-later
/* libSceSaveData on host directories:
 *   <user>/savedata/<user id>/<title id>/<dir name>/        files the game writes
 *   <user>/savedata/<user id>/<title id>/<dir name>.sce_sys/ param.bin, icon0.png
 *   <user>/savedata/<user id>/<title id>.memory/memory.dat  SaveDataMemory
 * A mounted directory appears to the guest as /savedata0../savedata15.
 * Metadata lives beside the directory so the guest's own files are untouched. */
#define _GNU_SOURCE
#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#ifndef _WIN32
#include <ftw.h>
#endif

#define ERR_PARAMETER ((int32_t)0x809F0000)
#define ERR_NOT_INITIALIZED ((int32_t)0x809F0001)
#define ERR_NOT_MOUNTED ((int32_t)0x809F0004)
#define ERR_EXISTS ((int32_t)0x809F0007)
#define ERR_NOT_FOUND ((int32_t)0x809F0008)
#define ERR_INTERNAL ((int32_t)0x809F000B)
#define ERR_MOUNT_FULL ((int32_t)0x809F000C)
#define ERR_BAD_MOUNTED ((int32_t)0x809F000D)
#define ERR_INVALID_USER ((int32_t)0x809F0011)
#define ERR_MEMORY_NOT_READY ((int32_t)0x809F0012)
#define MODE_RDONLY 1
#define MODE_CREATE 4
#define MODE_COPY_ICON 16
#define MODE_CREATE2 32
#define SLOTS 16

typedef struct { char data[10]; char pad[6]; } TitleId;
typedef struct { char data[32]; } DirName;
typedef struct { char data[16]; } MountPoint;
typedef struct {
    char title[128], subtitle[128], detail[1024];
    uint32_t user_param; int32_t pad;
    int64_t mtime;
    uint8_t reserved[32];
} Param;
typedef struct { int32_t user; int32_t pad; const TitleId *title; const DirName *dir; const void *fingerprint;
                 uint64_t blocks; uint32_t mode; uint8_t reserved[32]; } Mount1;
typedef struct { int32_t user; int32_t pad; const DirName *dir; uint64_t blocks; uint32_t mode;
                 uint8_t reserved[32]; int32_t pad2; } Mount2;
typedef struct { MountPoint point; uint64_t required_blocks; uint32_t unused, status; uint8_t reserved[28]; int32_t pad; } MountResult;
typedef struct { int32_t user; int32_t pad; const TitleId *title; const DirName *dir; uint32_t unused;
                 uint8_t reserved[32]; int32_t pad2; } Delete;
typedef struct { int32_t user; int32_t pad; const TitleId *title; const DirName *dir; uint32_t key, order;
                 uint8_t reserved[32]; } SearchCond;
typedef struct { uint64_t blocks, free_blocks; uint8_t reserved[32]; } SearchInfo;
typedef struct { uint32_t hits; int32_t pad; DirName *names; uint32_t names_capacity, set_count;
                 Param *params; SearchInfo *infos; uint8_t reserved[12]; int32_t pad2; } SearchResult;
typedef struct { const void *buffer; uint64_t buffer_size, data_size; uint8_t reserved[32]; } Icon;
_Static_assert(sizeof(Param)==1328,"OrbisSaveDataParam layout");
_Static_assert(sizeof(Mount1)==80 && sizeof(Mount2)==64,"OrbisSaveDataMount layouts");
_Static_assert(sizeof(MountResult)==64,"OrbisSaveDataMountResult layout");
_Static_assert(sizeof(SearchCond)==64 && sizeof(SearchResult)==56,"dir name search layouts");

static HostMutex lock=HOST_MUTEX_INIT;
static int initialized;
static char title_id[16]="CUSA04957";
static struct { int used; uint64_t blocks; char host[700], meta[700]; } slots[SLOTS];
static char memory_path[720];
static uint64_t memory_size;
static size_t mounts_done, memory_writes;

void runtime_savedata_configure(const char *title) {
    if (title && *title) snprintf(title_id,sizeof(title_id),"%s",title);
    /* Same layout as root(); 1 is the initial user (USER_ID in runtime_services.c). */
    printf("Save root: %s/savedata/1/%s\n",runtime_file_user_dir(),title_id);
}

static int make_dirs(const char *path) {
    char buffer[700];
    snprintf(buffer,sizeof(buffer),"%s",path);
    for (char *p=buffer+1;*p;++p) if (*p=='/') { *p=0; mkdir(buffer,0755); *p='/'; }
    return mkdir(buffer,0755) && errno!=EEXIST ? -1 : 0;
}
static void root(int32_t user, const char *title, char *out, size_t size) {
    int u = (user <= 0) ? 1 : user;
    const char *t = (title && *title) ? title : (title_id[0] ? title_id : "CUSA04957");
    snprintf(out,size,"%s/savedata/%d/%s",runtime_file_user_dir(),u,t);
}
static int valid_name(const char *name, size_t max) {
    size_t n=strnlen(name,max);
    if (!n || n==max) return 0;
    for (size_t i=0;i<n;++i) if (name[i]=='/' || name[i]=='\\' || (name[i]=='.' && (i==0 || name[i-1]=='.'))) return 0;
    return 1;
}
static void write_param(const char *meta, const Param *p) {
    char path[700]; snprintf(path,sizeof(path),"%s/param.bin",meta);
    FILE *f=fopen(path,"wb");
    if (f) { Param copy=*p; copy.mtime=time(NULL); fwrite(&copy,sizeof(copy),1,f); fclose(f); }
}
static int read_param(const char *meta, Param *p) {
    char path[700]; snprintf(path,sizeof(path),"%s/param.bin",meta);
    memset(p,0,sizeof(*p));
    FILE *f=fopen(path,"rb");
    if (!f) return -1;
    size_t n=fread(p,sizeof(*p),1,f); fclose(f);
    struct stat st;
    if (!stat(path,&st)) p->mtime=st.st_mtime;
    return n==1 ? 0 : -1;
}
#ifndef _WIN32
static int remove_entry(const char *path, const struct stat *st, int flag, struct FTW *ftw) {
    (void)st; (void)flag; (void)ftw; return remove(path);
}
#endif

static ABI int32_t save_initialize(const void *param) {
    (void)param;
    printf("Runtime: sceSaveDataInitialize called\n");
    initialized=1;
    return 0;
}
static ABI int32_t save_terminate(void) {
    printf("Runtime: sceSaveDataTerminate called\n");
    if (!initialized) return ERR_NOT_INITIALIZED;
    initialized=0; return 0;
}
static int32_t mount(int32_t user, const char *title, const DirName *dir, uint32_t mode, uint64_t blocks, MountResult *result) {
    printf("Runtime: sceSaveDataMount(user=%d, title='%s', dir='%s', mode=0x%x, blocks=%llu)\n",
           user, title ? title : "(null)", dir ? dir->data : "(null)", mode, (unsigned long long)blocks);
    if (!initialized) {
        printf("Runtime: sceSaveDataMount FAILED: not initialized\n");
        return ERR_NOT_INITIALIZED;
    }
    int u = (user <= 0) ? 1 : user;
    if (!dir || !result || !valid_name(dir->data,sizeof(dir->data))) {
        printf("Runtime: sceSaveDataMount FAILED: invalid dir or result\n");
        return ERR_PARAMETER;
    }
    char base[600], host[640], meta[680];
    root(u,title,base,sizeof(base));
    snprintf(host,sizeof(host),"%s/%s",base,dir->data);
    snprintf(meta,sizeof(meta),"%s/%s.sce_sys",base,dir->data);
    struct stat st;
    int exists=!stat(host,&st) && S_ISDIR(st.st_mode);
    printf("Runtime: sceSaveDataMount target host='%s', exists=%d\n", host, exists);
    if (!(mode & (MODE_CREATE|MODE_CREATE2)) && !exists) {
        printf("Runtime: sceSaveDataMount FAILED: save does not exist and create flag not set\n");
        return ERR_NOT_FOUND;
    }
    host_lock(&lock);
    int slot=-1;
    for (int i=0;i<SLOTS;++i) {
        if (slots[i].used && !strcmp(slots[i].host,host)) {
            // Already mounted in this slot; reuse cleanly
            slot=i;
            memset(result,0,sizeof(*result));
            snprintf(result->point.data,sizeof(result->point.data),"/savedata%d",slot&15);
            result->status=0; // EXISTING
            if (blocks && !slots[slot].blocks) slots[slot].blocks=blocks;
            host_unlock(&lock);
            printf("Runtime: save data '%s' was already mounted at %s\n", dir->data, result->point.data);
            return 0;
        }
        if (!slots[i].used && slot<0) slot=i;
    }
    if (slot<0) {
        host_unlock(&lock);
        printf("Runtime: sceSaveDataMount FAILED: mount table full\n");
        return ERR_MOUNT_FULL;
    }
    if (!exists && (make_dirs(host) || make_dirs(meta))) {
        host_unlock(&lock);
        printf("Runtime: sceSaveDataMount FAILED: cannot create directories\n");
        return ERR_INTERNAL;
    }
    if (!exists) { Param empty={0}; write_param(meta,&empty); }
    slots[slot].used=1;
    slots[slot].blocks=blocks ? blocks : 96;
    snprintf(slots[slot].host,sizeof(slots[slot].host),"%s",host);
    snprintf(slots[slot].meta,sizeof(slots[slot].meta),"%s",meta);
    memset(result,0,sizeof(*result));
    char point[32];
    snprintf(point,sizeof(point),"/savedata%d",slot&15);
    memcpy(result->point.data,point,strlen(point)+1);
    result->status=exists ? 0 : 1; /* CREATED */
    runtime_file_mount(result->point.data,host);
    ++mounts_done;
    host_unlock(&lock);
    printf("Runtime: save data '%s' mounted at %s (%s%s)\n",dir->data,result->point.data,
           exists ? "existing" : "created",(mode & MODE_RDONLY) ? ", read-only" : "");
    return 0;
}
static ABI int32_t save_mount(const Mount1 *m, MountResult *result) {
    if (!m) return ERR_PARAMETER;
    return mount(m->user,m->title ? m->title->data : NULL,m->dir,m->mode,m->blocks,result);
}
static ABI int32_t save_mount2(const Mount2 *m, MountResult *result) {
    if (!m) return ERR_PARAMETER;
    return mount(m->user,NULL,m->dir,m->mode,m->blocks,result);
}
static int slot_of(const MountPoint *point) {
    if (!point || strncmp(point->data,"/savedata",9)) return -1;
    int slot=atoi(point->data+9);
    return slot>=0 && slot<SLOTS && slots[slot].used ? slot : -1;
}
static ABI int32_t save_umount(const MountPoint *point) {
    if (!initialized) return ERR_NOT_INITIALIZED;
    host_lock(&lock);
    int slot=slot_of(point);
    if (slot>=0) {
        /* PS4 save-commit finalizer: the save service commits staged files when the save
         * memory view is released. PES writes 'data.tmp' and expects to find 'data.bin'
         * afterwards; without this the boot check reports "Edit data is corrupted" and
         * every create attempt ends in "Unable to Save.". Only finalize where the game
         * itself has not (fresh create): data.bin absent + data.tmp present.
         * Generic form: X.tmp promoted to X when X does not exist (tmp+bak pairing from
         * the guest strings at 0x2913a40). */
        char dir[760];
        snprintf(dir,sizeof(dir),"%s",slots[slot].host);
        DIR *d=opendir(dir);
        if (d) {
            struct dirent *de;
            char renamed[32][480]; int rn=0;
            while ((de=readdir(d)) && rn<32) {
                const char *nm=de->d_name;
                size_t len=strnlen(nm,256);
                if (len<5 || memcmp(nm+len-4,".tmp",4)) continue;
                char target[480];
                /* Guest convention (from 0x2913a40 strings): one staging name 'X.tmp'
                 * per file, renamed to its real name right after each write — the tmp
                 * name is REUSED, so 'data.tmp' belongs to 'data.bin' (the guest's own
                 * flow only ever stages one file at a time). Promote 'data.tmp' to
                 * 'data.bin' explicitly; other '<name>.tmp's fade to '<name>'. */
                if (len==8 && !memcmp(nm,"data.tmp",8)) {
                    snprintf(target,sizeof(target),"%s/data.bin",dir);
                } else {
                    snprintf(target,sizeof(target),"%s/%.*s",dir,(int)(len-4),nm);
                }
                struct stat st;
                if (stat(target,&st)==0) continue;      /* real file already exists */
                char src[480];
                snprintf(src,sizeof(src),"%s/%s",dir,nm);
                if (rename(src,target)==0) {
                    snprintf(renamed[rn++],sizeof(renamed[0]),"%s -> %.*s",nm,(int)(len-4),nm);
                } else {
                    printf("Runtime: save commit rename %s failed errno %d\n",nm,errno);
                }
            }
            closedir(d);
            for (int i=0;i<rn;++i)
                printf("Runtime: save data '%s' committed %s\n",slots[slot].host,renamed[i]);
        } else {
            printf("Runtime: save commit scan '%s' failed errno %d\n",dir,errno);
        }
        runtime_file_unmount(point->data);
        slots[slot].used=0;
        printf("Runtime: save data '%s' unmounted\n", point->data);
    } else {
        printf("Runtime: save data unmount '%s' not found\n", point ? point->data : "(null)");
    }
    host_unlock(&lock);
    return slot>=0 ? 0 : ERR_NOT_FOUND;
}
static ABI int32_t save_set_param(const MountPoint *point, uint32_t type, const void *buffer, uint64_t size) {
    static int trace=-1; if (trace<0) { const char *e=getenv("PES_SAVEDATA_TRACE"); trace=e&&e[0]=='1'; }
    if (trace) printf("Runtime: sceSaveDataSetParam(point='%s', type=%u, size=%llu)\n", point?point->data:"(null)", type, (unsigned long long)size);
    if (!initialized) return ERR_NOT_INITIALIZED;
    if (!buffer) return ERR_PARAMETER;
    host_lock(&lock);
    int slot=slot_of(point);
    if (slot<0) { host_unlock(&lock); return ERR_NOT_MOUNTED; }
    Param p; read_param(slots[slot].meta,&p);
    switch (type) {
    case 0: if (size<sizeof(Param)) goto bad; memcpy(&p,buffer,sizeof(p)); break;     /* ALL */
    case 1: snprintf(p.title,sizeof(p.title),"%.*s",(int)size,(const char *)buffer); break;
    case 2: snprintf(p.subtitle,sizeof(p.subtitle),"%.*s",(int)size,(const char *)buffer); break;
    case 3: snprintf(p.detail,sizeof(p.detail),"%.*s",(int)size,(const char *)buffer); break;
    case 4: if (size<4) goto bad; memcpy(&p.user_param,buffer,4); break;
    default: goto bad;
    }
    write_param(slots[slot].meta,&p);
    host_unlock(&lock);
    return 0;
bad:
    host_unlock(&lock);
    return ERR_PARAMETER;
}
static ABI int32_t save_get_param(const MountPoint *point, uint32_t type, void *buffer, uint64_t size) {
    static int trace=-1; if (trace<0) { const char *e=getenv("PES_SAVEDATA_TRACE"); trace=e&&e[0]=='1'; }
    if (!initialized) return ERR_NOT_INITIALIZED;
    if (!buffer) return ERR_PARAMETER;
    host_lock(&lock);
    int slot=slot_of(point);
    if (slot<0) { if (trace) printf("Runtime: sceSaveDataGetParam -> NOT MOUNTED\n"); host_unlock(&lock); return ERR_NOT_MOUNTED; }
    Param p; read_param(slots[slot].meta,&p);
    switch (type) {
    case 0: if (size<sizeof(Param)) goto bad; memcpy(buffer,&p,sizeof(p)); break;     /* ALL */
    case 1: snprintf((char *)buffer,size,"%s",p.title); break;
    case 2: snprintf((char *)buffer,size,"%s",p.subtitle); break;
    case 3: snprintf((char *)buffer,size,"%s",p.detail); break;
    case 4: if (size<4) goto bad; memcpy(buffer,&p.user_param,4); break;
    case 5: if (size<8) goto bad; memcpy(buffer,&p.mtime,8); break;
    default: goto bad;
    }
    host_unlock(&lock);
    if (trace) printf("Runtime: sceSaveDataGetParam(type=%u, size=%llu) -> ok\n", type, (unsigned long long)size);
    return 0;
bad:
    host_unlock(&lock);
    if (trace) printf("Runtime: sceSaveDataGetParam(type=%u, size=%llu) -> ERR_PARAMETER\n", type, (unsigned long long)size);
    return ERR_PARAMETER;
}
static ABI int32_t save_mount_info(const MountPoint *point, MountResult *res) {
    if (!initialized) return ERR_NOT_INITIALIZED;
    if (!res) return ERR_PARAMETER;
    static int trace=-1; if (trace<0) { const char *e=getenv("PES_SAVEDATA_TRACE"); trace=e&&e[0]=='1'; }
    host_lock(&lock);
    int slot=slot_of(point);
    if (slot<0) { if (trace) printf("Runtime: sceSaveDataGetMountInfo('%s') -> NOT MOUNTED\n", point?point->data:"(null)"); host_unlock(&lock); return ERR_NOT_MOUNTED; }
    /* Real OrbisSaveDataMountInfo: { u64 blocks (quota, 32 KiB units) @+0;
     * u64 freeBlocks @+8; reserved[32] }. No mount-point prefix. The old port wrote
     * MountResult semantics here (required_blocks@+16/status@+28), leaving the guest's
     * blocks/freeBlocks untouched; PES's free-space check then saw garbage and reported
     * "Unable to Save." */
    memset(res,0,sizeof(*res));
    memcpy(&res->point,point,sizeof(*point));
    /* quota: the blocks the game requested at mount (fallback 96 = 3 MiB). */
    uint64_t quota = slots[slot].blocks ? slots[slot].blocks : 96;
    /* used: total bytes of files under the host dir, in 32 KiB blocks. */
    uint64_t used = 0;
    char dir[760];
    snprintf(dir,sizeof(dir),"%s",slots[slot].host);
    DIR *d=opendir(dir);
    if (d) {
        struct dirent *de;
        while ((de=readdir(d))) {
            if (de->d_name[0]=='.') continue;
            char fp[800]; snprintf(fp,sizeof(fp),"%s/%s",dir,de->d_name);
            struct stat st;
            if (!stat(fp,&st) && S_ISREG(st.st_mode)) used += (uint64_t)st.st_size;
        }
        closedir(d);
    }
    uint64_t used_blocks = (used + 32767) / 32768;
    uint64_t free_blocks = quota > used_blocks ? quota - used_blocks : 0;
    uint64_t out[2] = { quota, free_blocks };
    memcpy(res, out, sizeof(out));
    host_unlock(&lock);
    if (trace) printf("Runtime: sceSaveDataGetMountInfo('%s') -> blocks=%llu freeBlocks=%llu\n",
                      point?point->data:"(null)", (unsigned long long)quota, (unsigned long long)free_blocks);
    return 0;
}
static ABI int32_t save_load_icon(const MountPoint *point, Icon *icon) {
    if (!initialized) return ERR_NOT_INITIALIZED;
    if (!icon || !icon->buffer || !icon->buffer_size) return ERR_PARAMETER;
    host_lock(&lock);
    int slot=slot_of(point);
    int32_t r=ERR_NOT_MOUNTED;
    if (slot>=0) {
        char path[700]; snprintf(path,sizeof(path),"%s/icon0.png",slots[slot].meta);
        FILE *f=fopen(path,"rb");
        if (f) {
            fseek(f,0,SEEK_END);
            long sz=ftell(f);
            fseek(f,0,SEEK_SET);
            if (sz>0 && (uint64_t)sz<=icon->buffer_size) {
                if (fread((void *)icon->buffer,1,sz,f)==(size_t)sz) {
                    icon->data_size=sz;
                    r=0;
                } else r=ERR_INTERNAL;
            } else r=ERR_PARAMETER;
            fclose(f);
        } else r=ERR_NOT_FOUND;
    }
    host_unlock(&lock);
    return r;
}
static ABI int32_t save_icon(const MountPoint *point, const Icon *icon) {
    if (!initialized) return ERR_NOT_INITIALIZED;
    if (!icon || !icon->buffer) return ERR_PARAMETER;
    host_lock(&lock);
    int slot=slot_of(point);
    int32_t r=ERR_NOT_MOUNTED;
    if (slot>=0) {
        char path[700]; snprintf(path,sizeof(path),"%s/icon0.png",slots[slot].meta);
        FILE *f=fopen(path,"wb");
        r=f && fwrite(icon->buffer,1,icon->data_size,f)==icon->data_size ? 0 : ERR_INTERNAL;
        if (f) fclose(f);
    }
    host_unlock(&lock);
    return r;
}
static ABI int32_t save_delete(const Delete *d) {
    if (!initialized) return ERR_NOT_INITIALIZED;
    if (!d || !d->dir || !valid_name(d->dir->data,sizeof(d->dir->data))) return ERR_PARAMETER;
    char base[600], host[640], meta[680];
    root(d->user,d->title ? d->title->data : NULL,base,sizeof(base));
    snprintf(host,sizeof(host),"%s/%s",base,d->dir->data);
    snprintf(meta,sizeof(meta),"%s/%s.sce_sys",base,d->dir->data);
    struct stat st;
    if (stat(host,&st)) return ERR_NOT_FOUND;
#ifdef _WIN32
    runtime_win_remove_tree(host);
    runtime_win_remove_tree(meta);
#else
    nftw(host,remove_entry,16,FTW_DEPTH|FTW_PHYS);
    nftw(meta,remove_entry,16,FTW_DEPTH|FTW_PHYS);
#endif
    printf("Runtime: save data '%s' deleted\n",d->dir->data);
    return 0;
}
/* Wildcard pattern matching: % and * match any sequence, _ and ? match one character. */
static int like(const char *s, const char *p) {
    if (!*p) return !*s;
    if (*p=='%' || *p=='*') {
        for (;;++s) {
            if (like(s,p+1)) return 1;
            if (!*s) return 0;
        }
    }
    return *s && (*p=='_' || *p=='?' || *p==*s) && like(s+1,p+1);
}
typedef struct { char name[32]; Param param; } Entry;
static uint32_t sort_key, sort_order;
static int compare(const void *a, const void *b) {
    const Entry *x=a, *y=b;
    int c = sort_key==1 ? (x->param.user_param>y->param.user_param)-(x->param.user_param<y->param.user_param)
          : sort_key==3 ? (x->param.mtime>y->param.mtime)-(x->param.mtime<y->param.mtime)
          : strcmp(x->name,y->name);
    return sort_order ? -c : c;
}
static ABI int32_t save_search(const SearchCond *cond, SearchResult *result) {
    if (!initialized) return ERR_NOT_INITIALIZED;
    if (!cond || !result) return ERR_PARAMETER;
    int u = (cond->user <= 0) ? 1 : cond->user;
    char base[600];
    root(u,cond->title ? cond->title->data : NULL,base,sizeof(base));
    char pattern[33] = {0};
    if (cond->dir) {
        memcpy(pattern, cond->dir->data, 32);
        pattern[32] = 0;
    }
    printf("Runtime: sceSaveDataDirNameSearch(user=%d, dir='%s', title='%s', base='%s')\n",
           cond->user, pattern, cond->title ? cond->title->data : "(null)", base);
    Entry *entries=NULL; size_t count=0, capacity=0;
    DIR *d=opendir(base);
    for (struct dirent *e; d && (e=readdir(d));) {
        size_t n=strlen(e->d_name);
        if (e->d_name[0]=='.' || n>=32 || (n>8 && !strcmp(e->d_name+n-8,".sce_sys"))) continue;
        if (pattern[0] && !like(e->d_name,pattern)) continue;
        if (count==capacity) {
            if (capacity>SIZE_MAX/sizeof(*entries)/2) {
                closedir(d); free(entries); return ERR_INTERNAL;
            }
            const size_t next_capacity=capacity ? capacity*2 : 16;
            Entry *next=realloc(entries,next_capacity*sizeof(*entries));
            if (!next) { closedir(d); free(entries); return ERR_INTERNAL; }
            entries=next; capacity=next_capacity;
        }
        snprintf(entries[count].name,32,"%s",e->d_name);
        char meta[680]; snprintf(meta,sizeof(meta),"%s/%s.sce_sys",base,e->d_name);
        read_param(meta,&entries[count].param);
        ++count;
    }
    if (d) closedir(d);
    sort_key=cond->key; sort_order=cond->order;
    if (count) qsort(entries,count,sizeof(*entries),compare);
    result->hits=(uint32_t)count;
    uint32_t set=count<result->names_capacity ? (uint32_t)count : result->names_capacity;
    for (uint32_t i=0;i<set;++i) {
        if (result->names) memcpy(result->names[i].data,entries[i].name,32);
        if (result->params) result->params[i]=entries[i].param;
        if (result->infos) { memset(&result->infos[i],0,sizeof(SearchInfo)); result->infos[i].blocks=32768; result->infos[i].free_blocks=16384; }
    }
    result->set_count=set;
    printf("Runtime: sceSaveDataDirNameSearch -> hits=%u, set_count=%u\n", result->hits, result->set_count);
    for (uint32_t i=0;i<set;++i) {
        printf("  Found save: '%s'\n", entries[i].name);
    }
    free(entries);
    return 0;
}
/* SaveDataMemory: one fixed-size blob per user/title. */
static ABI int32_t memory_setup(int32_t user, uint64_t size, const Param *param) {
    static int trace=-1; if (trace<0) { const char *e=getenv("PES_SAVEDATA_TRACE"); trace=e&&e[0]=='1'; }
    if (trace) printf("Runtime: sceSaveDataSetupSaveDataMemory(user=%d, size=%llu)\n", user, (unsigned long long)size);
    if (!initialized) return ERR_NOT_INITIALIZED;
    if (!size) return ERR_PARAMETER;
    char base[600], dir[640];
    root(user,NULL,base,sizeof(base));
    snprintf(dir,sizeof(dir),"%s.memory",base);
    if (make_dirs(dir)) return ERR_INTERNAL;
    host_lock(&lock);
    snprintf(memory_path,sizeof(memory_path),"%s/memory.dat",dir);
    FILE *f=fopen(memory_path,"r+b");
    if (!f) f=fopen(memory_path,"w+b");
    int32_t r=ERR_INTERNAL;
    if (f) {
        fseek(f,0,SEEK_END);
        if ((uint64_t)ftell(f)<size && ftruncate(fileno(f),(off_t)size)) goto done;
        memory_size=size; r=0;
    done:
        fclose(f);
    }
    if (!r && param) write_param(dir,param);
    host_unlock(&lock);
    if (!r) printf("Runtime: save data memory ready (%llu bytes)\n",(unsigned long long)size);
    return r;
}
static int32_t memory_io(void *buffer, uint64_t size, int64_t offset, int write) {
    if (!initialized) return ERR_NOT_INITIALIZED;
    if (!buffer || offset<0) return ERR_PARAMETER;
    host_lock(&lock);
    if (!memory_size) { host_unlock(&lock); return ERR_MEMORY_NOT_READY; }
    if ((uint64_t)offset+size>memory_size) { host_unlock(&lock); return ERR_PARAMETER; }
    FILE *f=fopen(memory_path,"r+b");
    int32_t r=ERR_INTERNAL;
    if (f && !fseek(f,offset,SEEK_SET) &&
        (write ? fwrite(buffer,1,size,f) : fread(buffer,1,size,f))==size) r=0;
    if (f) fclose(f);
    if (!r && write) ++memory_writes;
    host_unlock(&lock);
    static int trace=-1; if (trace<0) { const char *e=getenv("PES_SAVEDATA_TRACE"); trace=e&&e[0]=='1'; }
    if (trace) printf("Runtime: save data memory %s buf=0x%llx size=%llu @%lld -> %s\n",
                      write ? "SET" : "GET", (unsigned long long)(uintptr_t)buffer,
                      (unsigned long long)size, (long long)offset, r ? "FAIL" : "ok");
    return r;
}
static ABI int32_t memory_get(int32_t user, void *buffer, uint64_t size, int64_t offset) { (void)user; return memory_io(buffer,size,offset,0); }
static ABI int32_t memory_set(int32_t user, void *buffer, uint64_t size, int64_t offset) { (void)user; return memory_io(buffer,size,offset,1); }

static const RuntimeExport exports[]={
    {"sceSaveDataInitialize",save_initialize}, {"sceSaveDataInitialize2",save_initialize},
    {"sceSaveDataInitialize3",save_initialize}, {"sceSaveDataTerminate",save_terminate},
    {"sceSaveDataMount",save_mount}, {"sceSaveDataMount2",save_mount2}, {"sceSaveDataUmount",save_umount},
    {"sceSaveDataSetParam",save_set_param}, {"sceSaveDataGetParam",save_get_param},
    {"sceSaveDataSaveIcon",save_icon}, {"sceSaveDataLoadIcon",save_load_icon},
    {"sceSaveDataGetMountInfo",save_mount_info},
    {"sceSaveDataDelete",save_delete},
    {"sceSaveDataDirNameSearch",save_search},
    {"sceSaveDataSetupSaveDataMemory",memory_setup}, {"sceSaveDataGetSaveDataMemory",memory_get},
    {"sceSaveDataSetSaveDataMemory",memory_set},
};
uintptr_t runtime_savedata_resolve(const char *name) { return RUNTIME_LOOKUP(exports,name); }
void runtime_savedata_report(void) { printf("Runtime: save data mounts=%zu, memory writes=%zu\n",mounts_done,memory_writes); }
