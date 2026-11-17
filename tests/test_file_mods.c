// SPDX-License-Identifier: GPL-2.0-or-later
/* Exercise actual guest file operations on a linked overlay and writable saves. */
#ifdef _WIN32
/* runtime_file.c calls the Windows file helpers (mkdir, rename, pread) that this test does not link. */
#include "../src/win32_compat.c"
#endif
#include "../src/runtime_file.c"
#include <assert.h>
#ifdef _WIN32
/* No mkdtemp or symlink privilege on Windows: a fresh directory in the temp directory, and a hard link as the overlay file. */
#define symlink(target,path) (CreateHardLinkA((path),(target),NULL) ? 0 : -1)
static int make_root(char *root,size_t size) {
    char dir[MAX_PATH],file[MAX_PATH];
    (void)size;
    if (!GetTempPathA(sizeof dir,dir) || !GetTempFileNameA(dir,"pes",0,file)) return -1;
    DeleteFileA(file);
    if (!CreateDirectoryA(file,NULL)) return -1;
    strcpy(root,file);
    return 0;
}
#else
static int make_root(char *root,size_t size) {
    snprintf(root,size,"/tmp/pes-mod-files-XXXXXX");
    return mkdtemp(root) ? 0 : -1;
}
#endif
static int32_t guest_errno;
int32_t *runtime_errno(void) { return &guest_errno; }
int32_t runtime_guest_errno(int e) { return e; }
/* Reads into guest memory tell the GPU side (runtime_memory.c); nothing to tell here. */
void runtime_memory_note_write(uintptr_t address, uint64_t size) { (void)address; (void)size; }
uintptr_t runtime_lookup(const RuntimeExport *table,size_t n,const char *name) {
    for (size_t i=0;i<n;++i) if (!strcmp(table[i].name,name)) return (uintptr_t)table[i].function;
    return 0;
}
int main(void) {
    char root[512];
    assert(make_root(root,sizeof root)==0);
    char game[512],user[512],source[512],link[512];
    snprintf(game,sizeof(game),"%s/game",root);
    snprintf(user,sizeof(user),"%s/user",root);
    snprintf(source,sizeof(source),"%s/mod.dcx",root);
    snprintf(link,sizeof(link),"%s/game/asset.dcx",root);
    assert(!mkdir(game,0755));
    FILE *f=fopen(source,"w"); assert(f); assert(fputs("modded",f)>=0); assert(!fclose(f));
    assert(!symlink(source,link));
    runtime_file_configure(game,user);
    for (int i=0;i<3;++i) {
        const char *path=i==0 ? "/app0/asset.dcx" : i==1 ? "/hostapp/asset.dcx" : "asset.dcx";
        int fd=(int)do_open(path,0,0); assert(fd>=3);
        char content[8]={0}; assert(do_read(fd,content,6)==6 && !strcmp(content,"modded"));
        GuestStat info; assert(!do_stat(path,&info) && info.size==6);
        assert(!do_close(fd));
        assert(do_open(path,2,0)==-EROFS);
        assert(do_open(path,0x400,0)==-EROFS);
        assert(do_truncate(path,0)==-EROFS);
        assert(path_op(path,2,0)==-EROFS);
        assert(do_rename(path,"/data/moved")==-EROFS);
    }
    int dir=(int)do_open("/app0",0x20000,0); assert(dir>=3);
    char entries[1024]; int64_t count=do_getdents(dir,entries,sizeof(entries),NULL); assert(count>0);
    int found=0;
    for (int64_t p=0;p<count;) {
        uint16_t length; memcpy(&length,entries+p+4,2);
        assert(length);
        if (!strcmp(entries+p+8,"asset.dcx")) { assert(entries[p+6]==8); found=1; }
        p+=length;
    }
    assert(found && !do_close(dir));
    int save=(int)do_open("/data/test-save",0x202,0644); assert(save>=3);
    assert(do_write(save,"save",4)==4 && !do_close(save));
    assert(!path_op("/data/test-save",2,0));
    assert(!unlink(link) && !unlink(source) && !rmdir(game));
    const char *dirs[]={"temp0","download0","data"};
    for (int i=0;i<3;++i) { char p[1024]; snprintf(p,sizeof(p),"%s/%s",user,dirs[i]); assert(!rmdir(p)); }
    assert(!rmdir(user) && !rmdir(root));
    puts("Guest mod files: reads, stat, merged listing, readonly assets and writable saves PASS");
}
