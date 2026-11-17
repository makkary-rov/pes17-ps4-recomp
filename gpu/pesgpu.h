// SPDX-License-Identifier: GPL-2.0-or-later
/* C interface of the GPU library (gpu/): shadPS4's Liverpool/Vulkan video core,
 * GnmDriver, VideoOut and kernel event queues, adapted to the native loader. */
#ifndef PESGPU_H
#define PESGPU_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    const char *title;          /* window title */
    const char *serial;         /* CUSA id, names the pipeline cache */
    const char *user_dir;       /* pipeline cache/logs directory */
    uint32_t sdk_version;       /* from the eboot's procparam */
    uint32_t psf_attributes;    /* param.sfo ATTRIBUTE */
    int32_t width, height;      /* initial window size */
} PesGpuConfig;
/* Registers kernel event queues (needed with or without graphics). */
void pesgpu_register_kernel(void);
/* Start networking after game metadata, guest TLS and linked modules are ready. */
void pesgpu_start_online(void);
/* Creates window, Vulkan device, presenter and GPU command processor. */
int pesgpu_init(const PesGpuConfig *config);
/* Function for an imported NID ("NID#lib#mod"), or 0 when the GPU library does not provide it. */
uintptr_t pesgpu_resolve(const char *scoped_nid);
/* Called first by the loader's SIGSEGV handler: 1 when a GPU page-tracking fault was handled.
 * The context is the handler's ucontext_t (Linux) or the vectored exception handler's
 * EXCEPTION_POINTERS (Windows). */
int pesgpu_handle_fault(void *ucontext, void *address);
/* PES_WRITE_LOG=1: prints the logged GPU-side writes to guest memory near the fault
 * (same context as pesgpu_handle_fault). */
void pesgpu_dump_guest_writes(void *ucontext);
/* Keyboard text entry through the game window (IME dialog). begin returns 0 when
 * no window exists; poll returns 0 typing, 1 confirmed, 2 cancelled (UTF-8 text). */
int pesgpu_text_input_begin(const char *initial_utf8, const char *prompt_utf8);
int pesgpu_text_input_poll(char *out_utf8, uint64_t size);
/* 1 while the in-game settings menu is open: the game's pad input is held neutral. */
int pesgpu_overlay_captures_input(void);
/* Shows the trophy banner over the game for a few seconds, with its chime
 * (UTF-8 name; grade 1 bronze, 2 silver, 3 gold, 4 platinum). */
void pesgpu_trophy_popup(const char *name_utf8, int grade);
/* Patches the loaded image before the game runs (image still writable): libGnm entry hooks. */
void pesgpu_patch_image(unsigned char *image, uint64_t size);
/* Number of symbols registered by the vendored libraries (diagnostics). */
unsigned pesgpu_symbol_count(void);
#ifdef __cplusplus
}
#endif
#endif
