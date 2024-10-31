/******************************************************************************/
/* Important Fall 2023 CSCI 402 usage information:                            */
/*                                                                            */
/* This fils is part of CSCI 402 kernel programming assignments at USC.       */
/*         53616c7465645f5fd1e93dbf35cbffa3aef28f8c01d8cf2ffc51ef62b26a       */
/*         f9bda5a68e5ed8c972b17bab0f42e24b19daa7bd408305b1f7bd6c7208c1       */
/*         0e36230e913039b3046dd5fd0ba706a624d33dbaa4d6aab02c82fe09f561       */
/*         01b0fd977b0051f0b0ce0c69f7db857b1b5e007be2db6d42894bf93de848       */
/*         806d9152bd5715e9                                                   */
/* Please understand that you are NOT permitted to distribute or publically   */
/*         display a copy of this file (or ANY PART of it) for any reason.    */
/* If anyone (including your prospective employer) asks you to post the code, */
/*         you must inform them that you do NOT have permissions to do so.    */
/* You are also NOT permitted to remove or alter this comment block.          */
/* If this comment block is removed or altered in a submitted file, 20 points */
/*         will be deducted.                                                  */
/******************************************************************************/

#include "globals.h"
#include "errno.h"
#include "types.h"

#include "mm/mm.h"
#include "mm/tlb.h"
#include "mm/mman.h"
#include "mm/page.h"

#include "proc/proc.h"

#include "util/string.h"
#include "util/debug.h"

#include "fs/vnode.h"
#include "fs/vfs.h"
#include "fs/file.h"

#include "vm/vmmap.h"
#include "vm/mmap.h"

/*
 * This function implements the mmap(2) syscall, but only
 * supports the MAP_SHARED, MAP_PRIVATE, MAP_FIXED, and
 * MAP_ANON flags.
 *
 * Add a mapping to the current process's address space.
 * You need to do some error checking; see the ERRORS section
 * of the manpage for the problems you should anticipate.
 * After error checking most of the work of this function is
 * done by vmmap_map(), but remember to clear the TLB.
 */
int
do_mmap(void *addr, size_t len, int prot, int flags,
        int fd, off_t off, void **ret)
{
        //NOT_YET_IMPLEMENTED("VM: do_mmap");

        if (!PAGE_ALIGNED(off) || len <= 0 || (uint32_t)addr + len > USER_MEM_HIGH ||(addr != NULL && !PAGE_ALIGNED(addr))) {
                dbg(DBG_PRINT, "(GRADING3D 1)\n");
                return -EINVAL; 
        }        
        
        if (flags & MAP_FIXED && addr == NULL) {
                dbg(DBG_PRINT, "(GRADING3D 1)\n");
                return -EINVAL;
        }
        


        if(!(flags & MAP_PRIVATE) && !(flags & MAP_SHARED)){
                dbg(DBG_PRINT, "(GRADING3D 1)\n");
                return -EINVAL;
        }
        


        uint32_t page;
        if (addr == NULL) {
                page = 0;
                dbg(DBG_PRINT, "(GRADING3D 2)\n");
        } else {
                page = ADDR_TO_PN(addr);
                dbg(DBG_PRINT, "(GRADING3D 2)\n");
        }

        file_t *file = NULL;
        vmarea_t *vm_area = NULL;
        int res;
        vnode_t *vn = NULL;
        
        if ((flags & MAP_ANON) != MAP_ANON) {
                if (fd > NFILES || fd < 0) {
                        dbg(DBG_PRINT, "(GRADING3D 1)\n");
                        return -EBADF;
                }
                file = fget(fd);
                if (file == NULL) {
                        dbg(DBG_PRINT, "(GRADING3D 2)\n");
                        return -EBADF;
                }
                vn = file->f_vnode;
                if ((flags & MAP_SHARED) && (prot & PROT_WRITE) && (file->f_mode == FMODE_READ)) {       
                        fput(file);
                        dbg(DBG_PRINT, "(GRADING3D 2)\n");
                        return -EACCES;
                }
        }
        
        res = vmmap_map(curproc->p_vmmap, vn, page, (len - 1) / PAGE_SIZE + 1, prot, flags, off, VMMAP_DIR_HILO, &vm_area);
        
        if (file != NULL) {
                fput(file);
                dbg(DBG_PRINT, "(GRADING3D 2)\n");
        }
        
        if (res < 0) {
                dbg(DBG_PRINT, "(GRADING3D 2)\n");
                return res;
        }
        
        *ret = PN_TO_ADDR(vm_area->vma_start);
        tlb_flush_all();

        KASSERT(NULL != curproc->p_pagedir);
        dbg(DBG_PRINT, "(GRADING3A 2.a)\n");
        dbg(DBG_PRINT, "(GRADING3D 2)\n");
        return 0;
}


/*
 * This function implements the munmap(2) syscall.
 *
 * As with do_mmap() it should perform the required error checking,
 * before calling upon vmmap_remove() to do most of the work.
 * Remember to clear the TLB.
 */
int
do_munmap(void *addr, size_t len)
{
        //NOT_YET_IMPLEMENTED("VM: do_munmap");
        
         if ((uint32_t)addr + len < USER_MEM_LOW ){
                dbg(DBG_PRINT, "(GRADING3D 1)\n");
                return -EINVAL;
        }

        if ((uint32_t)addr + len > USER_MEM_HIGH) {
                dbg(DBG_PRINT, "(GRADING3D 1)\n");
                return -EINVAL;
        }
        
        if (len == 0 || len > USER_MEM_HIGH) {
                dbg(DBG_PRINT, "(GRADING3D 2)\n");
                return -EINVAL;
        }

        int res = vmmap_remove(curproc->p_vmmap, ADDR_TO_PN(addr), (len - 1) / PAGE_SIZE + 1);
        tlb_flush_all();
        dbg(DBG_PRINT, "(GRADING3D 2)\n");
        return 0;
}

