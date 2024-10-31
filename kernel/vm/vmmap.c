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

#include "kernel.h"
#include "errno.h"
#include "globals.h"

#include "vm/vmmap.h"
#include "vm/shadow.h"
#include "vm/anon.h"

#include "proc/proc.h"

#include "util/debug.h"
#include "util/list.h"
#include "util/string.h"
#include "util/printf.h"

#include "fs/vnode.h"
#include "fs/file.h"
#include "fs/fcntl.h"
#include "fs/vfs_syscall.h"

#include "mm/slab.h"
#include "mm/page.h"
#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/mmobj.h"
#include "mm/tlb.h"
	
static slab_allocator_t *vmmap_allocator;
static slab_allocator_t *vmarea_allocator;

void
vmmap_init(void)
{
        vmmap_allocator = slab_allocator_create("vmmap", sizeof(vmmap_t));
        KASSERT(NULL != vmmap_allocator && "failed to create vmmap allocator!");
        vmarea_allocator = slab_allocator_create("vmarea", sizeof(vmarea_t));
        KASSERT(NULL != vmarea_allocator && "failed to create vmarea allocator!");
}

vmarea_t *
vmarea_alloc(void)
{
        vmarea_t *newvma = (vmarea_t *) slab_obj_alloc(vmarea_allocator);
        if (newvma) {
                newvma->vma_vmmap = NULL;
        }
        return newvma;
}

void
vmarea_free(vmarea_t *vma)
{
        KASSERT(NULL != vma);
        slab_obj_free(vmarea_allocator, vma);
}

/* a debugging routine: dumps the mappings of the given address space. */
size_t
vmmap_mapping_info(const void *vmmap, char *buf, size_t osize)
{
        KASSERT(0 < osize);
        KASSERT(NULL != buf);
        KASSERT(NULL != vmmap);

        vmmap_t *map = (vmmap_t *)vmmap;
        vmarea_t *vma;
        ssize_t size = (ssize_t)osize;

        int len = snprintf(buf, size, "%21s %5s %7s %8s %10s %12s\n",
                           "VADDR RANGE", "PROT", "FLAGS", "MMOBJ", "OFFSET",
                           "VFN RANGE");

        list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink) {
                size -= len;
                buf += len;
                if (0 >= size) {
                        goto end;
                }

                len = snprintf(buf, size,
                               "%#.8x-%#.8x  %c%c%c  %7s 0x%p %#.5x %#.5x-%#.5x\n",
                               vma->vma_start << PAGE_SHIFT,
                               vma->vma_end << PAGE_SHIFT,
                               (vma->vma_prot & PROT_READ ? 'r' : '-'),
                               (vma->vma_prot & PROT_WRITE ? 'w' : '-'),
                               (vma->vma_prot & PROT_EXEC ? 'x' : '-'),
                               (vma->vma_flags & MAP_SHARED ? " SHARED" : "PRIVATE"),
                               vma->vma_obj, vma->vma_off, vma->vma_start, vma->vma_end);
        } list_iterate_end();

end:
        if (size <= 0) {
                size = osize;
                buf[osize - 1] = '\0';
        }
        /*
        KASSERT(0 <= size);
        if (0 == size) {
                size++;
                buf--;
                buf[0] = '\0';
        }
        */
        return osize - size;
}

/* Create a new vmmap, which has no vmareas and does
 * not refer to a process. */
vmmap_t *
vmmap_create(void)
{
        vmmap_t *map = (vmmap_t *) slab_obj_alloc(vmmap_allocator);
        list_init(&map->vmm_list);
        map->vmm_proc = NULL;
        dbg(DBG_PRINT, "(GRADING3A)\n");
	return map;
}

/* Removes all vmareas from the address space and frees the
 * vmmap struct. */
void
vmmap_destroy(vmmap_t *map)
{
	KASSERT(NULL != map); //required kassert
	dbg(DBG_PRINT, "(GRADING3A 3.a)\n");
	
	vmarea_t *iter;
	list_iterate_begin(&map->vmm_list, iter, vmarea_t, vma_plink) {
        	list_remove(&iter->vma_olink); //internal cleanup
		list_remove(&iter->vma_plink);
                iter->vma_obj->mmo_ops->put(iter->vma_obj);
                vmarea_free(iter);
    	} 
	list_iterate_end();
	
	slab_obj_free(vmmap_allocator, map);
	dbg(DBG_PRINT, "(GRADING3A)\n");
}

/* Add a vmarea to an address space. Assumes (i.e. asserts to some extent)
 * the vmarea is valid.  This involves finding where to put it in the list
 * of VM areas, and adding it. Don't forget to set the vma_vmmap for the
 * area. */
void
vmmap_insert(vmmap_t *map, vmarea_t *newvma)
{
	KASSERT(NULL != map && NULL != newvma); //required kasserts
	dbg(DBG_PRINT, "(GRADING3A 3.b)\n");
	KASSERT(NULL == newvma->vma_vmmap);
	dbg(DBG_PRINT, "(GRADING3A 3.b)\n");
	KASSERT(newvma->vma_start < newvma->vma_end);
	dbg(DBG_PRINT, "(GRADING3A 3.b)\n");
	KASSERT(ADDR_TO_PN(USER_MEM_LOW) <= newvma->vma_start && ADDR_TO_PN(USER_MEM_HIGH) >= newvma->vma_end);
	dbg(DBG_PRINT, "(GRADING3A 3.b)\n");
	
	newvma->vma_vmmap = map; //set new map to map
	vmarea_t *iter;
        list_iterate_begin(&map->vmm_list, iter, vmarea_t, vma_plink) { //loop to find where to insert new vmarea
		if(iter->vma_start >= newvma->vma_end){ //checking if current list item starts after new vmarea
                        list_insert_before(&iter->vma_plink, &newvma->vma_plink);
                        dbg(DBG_PRINT, "(GRADING3A)\n");
			return;
                }	
	}
	list_iterate_end();

	list_insert_tail(&map->vmm_list, &newvma->vma_plink); //reaching this code means we should put it at the end of the list
	dbg(DBG_PRINT, "(GRADING3A)\n");
}

/* Find a contiguous range of free virtual pages of length npages in
 * the given address space. Returns starting vfn for the range,
 * without altering the map. Returns -1 if no such range exists.
 *
 * Your algorithm should be first fit. If dir is VMMAP_DIR_HILO, you
 * should find a gap as high in the address space as possible; if dir
 * is VMMAP_DIR_LOHI, the gap should be as low as possible. */
int
vmmap_find_range(vmmap_t *map, uint32_t npages, int dir)
{
        vmarea_t *iter;
	list_t *curr_list = &map->vmm_list;
	
	/*if(dir == VMMAP_DIR_LOHI) {
		uint32_t iter_addr = ADDR_TO_PN(USER_MEM_LOW);
		list_iterate_begin(curr_list, iter, vmarea_t, vma_plink) {
                        if(iter_addr > 0 && iter->vma_start - iter_addr >= npages) {
                                dbg_print("here1forrange\n");
				return iter_addr;
                        }
                        iter_addr = iter->vma_end;
                }
                list_iterate_end();

		vmarea_t *last = list_tail(curr_list, vmarea_t, vma_plink);
                if(ADDR_TO_PN(USER_MEM_HIGH) - last->vma_end >= npages){
			dbg_print("here2forrange\n");
                        return last->vma_end;
                }
		dbg_print("here3forrange\n");
	} else {*/
	if(dir == VMMAP_DIR_HILO) {
		uint32_t iter_addr = ADDR_TO_PN(USER_MEM_HIGH);
		list_iterate_reverse(curr_list, iter, vmarea_t, vma_plink) {
                        if(iter_addr - iter->vma_end >= npages){
                                dbg(DBG_PRINT, "(GRADING3A)\n");
				return iter_addr - npages;
                        }
                        iter_addr = iter->vma_start;
                }
                list_iterate_end();
		
		vmarea_t *first = list_head(curr_list, vmarea_t, vma_plink);
                if(first->vma_start - ADDR_TO_PN(USER_MEM_LOW) >= npages) { 
			dbg(DBG_PRINT, "(GRADING3D 2)\n"); 
                        return first->vma_start - npages;
                }
		dbg(DBG_PRINT, "(GRADING3D 2)\n");
	}
	dbg(DBG_PRINT, "(GRADING3D 2)\n");
	return -1;
}

/* Find the vm_area that vfn lies in. Simply scan the address space
 * looking for a vma whose range covers vfn. If the page is unmapped,
 * return NULL. */
vmarea_t *
vmmap_lookup(vmmap_t *map, uint32_t vfn)
{
        KASSERT(NULL != map); //required kassert
	dbg(DBG_PRINT, "(GRADING3A 3.c)\n");
	
	vmarea_t *iter;
        list_iterate_begin(&map->vmm_list, iter, vmarea_t, vma_plink) {
                if(iter->vma_end > vfn && iter->vma_start <= vfn) {
			dbg(DBG_PRINT, "(GRADING3A)\n");
                        return iter;
                }
        }
        list_iterate_end();
	dbg(DBG_PRINT, "(GRADING3D 1)\n");
	return NULL;
}

/* Allocates a new vmmap containing a new vmarea for each area in the
 * given map. The areas should have no mmobjs set yet. Returns pointer
 * to the new vmmap on success, NULL on failure. This function is
 * called when implementing fork(2). */
vmmap_t *
vmmap_clone(vmmap_t *map)
{
        vmmap_t *new_map = vmmap_create(); 
	
	vmarea_t *iter;
        list_iterate_begin(&map->vmm_list, iter, vmarea_t, vma_plink) {
                vmarea_t *new_vma = vmarea_alloc();
                new_vma->vma_end = iter->vma_end;
                new_vma->vma_start = iter->vma_start;
                new_vma->vma_flags = iter->vma_flags;
                new_vma->vma_off = iter->vma_off;
                new_vma->vma_prot = iter->vma_prot;
                new_vma->vma_obj = NULL;
                list_init(&new_vma->vma_plink);
                list_init(&new_vma->vma_olink);
                new_vma->vma_vmmap = new_map;
                list_insert_tail(&new_map->vmm_list, &new_vma->vma_plink);
        }
        list_iterate_end();
	dbg(DBG_PRINT, "(GRADING3A)\n");
	return new_map;
}

/* Insert a mapping into the map starting at lopage for npages pages.
 * If lopage is zero, we will find a range of virtual addresses in the
 * process that is big enough, by using vmmap_find_range with the same
 * dir argument.  If lopage is non-zero and the specified region
 * contains another mapping that mapping should be unmapped.
 *
 * If file is NULL an anon mmobj will be used to create a mapping
 * of 0's.  If file is non-null that vnode's file will be mapped in
 * for the given range.  Use the vnode's mmap operation to get the
 * mmobj for the file; do not assume it is file->vn_obj. Make sure all
 * of the area's fields except for vma_obj have been set before
 * calling mmap.
 *
 * If MAP_PRIVATE is specified set up a shadow object for the mmobj.
 *
 * All of the input to this function should be valid (KASSERT!).
 * See mmap(2) for for description of legal input.
 * Note that off should be page aligned.
 *
 * Be very careful about the order operations are performed in here. Some
 * operation are impossible to undo and should be saved until there
 * is no chance of failure.
 *
 * If 'new' is non-NULL a pointer to the new vmarea_t should be stored in it.
 */
int
vmmap_map(vmmap_t *map, vnode_t *file, uint32_t lopage, uint32_t npages,
          int prot, int flags, off_t off, int dir, vmarea_t **new)
{
        KASSERT(NULL != map); /* must not add a memory segment into a non-existing vmmap */
        dbg(DBG_PRINT, "(GRADING3A 3.d)\n");
	KASSERT(0 < npages); /* number of pages of this memory segment cannot be 0 */
        dbg(DBG_PRINT, "(GRADING3A 3.d)\n");
	KASSERT((MAP_SHARED & flags) || (MAP_PRIVATE & flags)); /* must specify whether the memory segment is shared or private */
        dbg(DBG_PRINT, "(GRADING3A 3.d)\n");
	KASSERT((0 == lopage) || (ADDR_TO_PN(USER_MEM_LOW) <= lopage)); /* if lopage is not zero, it must be a user space vpn */
        dbg(DBG_PRINT, "(GRADING3A 3.d)\n");
	KASSERT((0 == lopage) || (ADDR_TO_PN(USER_MEM_HIGH) >= (lopage + npages))); /* if lopage is not zero, the specified page range must lie completely within the user space */
        dbg(DBG_PRINT, "(GRADING3A 3.d)\n");
	KASSERT(PAGE_ALIGNED(off)); /* the off argument must be page aligned */
        dbg(DBG_PRINT, "(GRADING3A 3.d)\n");
	
	if(lopage == 0) {
		int range = vmmap_find_range(map, npages, dir);
		if(range < 0) {
			dbg(DBG_PRINT, "(GRADING3D 2)\n");
                        return -1;
                }
                lopage = range;
		dbg(DBG_PRINT, "(GRADING3A)\n");
        } else if(!vmmap_is_range_empty(map, lopage, npages)) {
		vmmap_remove(map, lopage, npages);
		dbg(DBG_PRINT, "(GRADING3A)\n");
	}

	vmarea_t* new_vma = vmarea_alloc();
        new_vma->vma_start = lopage;
        new_vma->vma_end = lopage + npages;
        new_vma->vma_flags = flags;
	new_vma->vma_off = ADDR_TO_PN(off);
        new_vma->vma_prot = prot;
        list_init(&new_vma->vma_plink);
        list_init(&new_vma->vma_olink);

	mmobj_t *new_anon;
	if (file) {
        	int map_res = file->vn_ops->mmap(file, new_vma, &new_anon);
		dbg(DBG_PRINT, "(GRADING3A)\n");
    	} else {
        	new_anon = anon_create();
		dbg(DBG_PRINT, "(GRADING3A)\n");
    	}

	list_insert_tail(&new_anon->mmo_un.mmo_vmas, &new_vma->vma_olink);
        new_vma->vma_obj = new_anon;

	if(flags & MAP_PRIVATE) {
		mmobj_t *shadow = shadow_create();
		shadow->mmo_shadowed = new_anon;
		
		mmobj_t *bottom;
		bottom = mmobj_bottom_obj(new_anon);
		shadow->mmo_un.mmo_bottom_obj = bottom;
		
		bottom->mmo_ops->ref(bottom);
                new_vma->vma_obj = shadow;
		dbg(DBG_PRINT, "(GRADING3A)\n");
	}

	vmmap_insert(map, new_vma);

	if(new){
                *new = new_vma;
		dbg(DBG_PRINT, "(GRADING3A)\n");
        }
	
	dbg(DBG_PRINT, "(GRADING3A)\n");
	return 0;

}

/*
 * Helper function called in vmmap_remove to determine overlap type.
 * Returns 0 for no overlap, and 1-4 for cases 1-4.
 */
int
overlap_type(vmarea_t *vma, uint32_t lopage, uint32_t npages) {
	uint32_t vma_start = vma->vma_start;
    	uint32_t vma_end = vma->vma_end;
	uint32_t max = lopage + npages;

	if (vma_end <= lopage || vma_start >= max) {
        	dbg(DBG_PRINT, "(GRADING3A)\n");
		return 0;
    	} 
	if (vma_start < lopage && vma_end > max) {
		dbg(DBG_PRINT, "(GRADING3D 1)\n");
      		return 1;
    	} else if (vma_start < lopage && vma_end > lopage && vma_end <= max) {
		dbg(DBG_PRINT, "(GRADING3D 2)\n");
        	return 2;
    	} else if (vma_start >= lopage  && vma_start < max && vma_end > max) {
		dbg(DBG_PRINT, "(GRADING3D 2)\n");
        	return 3;
    	} else {
		dbg(DBG_PRINT, "(GRADING3A)\n");
        	return 4;
    	}
}

/*
 * We have no guarantee that the region of the address space being
 * unmapped will play nicely with our list of vmareas.
 *
 * You must iterate over each vmarea that is partially or wholly covered
 * by the address range [addr ... addr+len). The vm-area will fall into one
 * of four cases, as illustrated below:
 *
 * key:
 *          [             ]   Existing VM Area
 *        *******             Region to be unmapped
 *
 * Case 1:  [   ******    ]
 * The region to be unmapped lies completely inside the vmarea. We need to
 * split the old vmarea into two vmareas. be sure to increment the
 * reference count to the file associated with the vmarea.
 *
 * Case 2:  [      *******]**
 * The region overlaps the end of the vmarea. Just shorten the length of
 * the mapping.
 *
 * Case 3: *[*****        ]
 * The region overlaps the beginning of the vmarea. Move the beginning of
 * the mapping (remember to update vma_off), and shorten its length.
 *
 * Case 4: *[*************]**
 * The region completely contains the vmarea. Remove the vmarea from the
 * list.
 */
int
vmmap_remove(vmmap_t *map, uint32_t lopage, uint32_t npages)
{
        vmarea_t *iter;
	
	list_iterate_begin(&map->vmm_list, iter, vmarea_t, vma_plink) {
		int overlap = overlap_type(iter, lopage, npages);
		
		switch(overlap) {
			case 1:; 
				vmarea_t *new_vma = vmarea_alloc();
                        	new_vma->vma_end = iter->vma_end;
                        	new_vma->vma_start = lopage + npages;
                        	new_vma->vma_flags = iter->vma_flags;
                        	new_vma->vma_off = iter->vma_off + (lopage + npages) - iter->vma_start;
                        	new_vma->vma_prot = iter->vma_prot;
                        	new_vma->vma_obj = iter->vma_obj;
                        	list_init(&new_vma->vma_plink);
                        	list_init(&new_vma->vma_olink);
                        	vmmap_insert(map,new_vma);
				new_vma->vma_obj->mmo_ops->ref(new_vma->vma_obj);
                        	iter->vma_end = lopage;
				dbg(DBG_PRINT, "(GRADING3D 1)\n");
				break;
			case 2:
				iter->vma_end = lopage;
				dbg(DBG_PRINT, "(GRADING3D 2)\n");
				break;
			case 3:
				iter->vma_off += (lopage + npages) - iter->vma_start;
                       	 	iter->vma_start = lopage + npages;
				dbg(DBG_PRINT, "(GRADING3D 2)\n");
				break;
			case 4:
				list_remove(&iter->vma_plink);
                        	list_remove(&iter->vma_olink);
				iter->vma_obj->mmo_ops->put(iter->vma_obj);
				vmarea_free(iter);
				dbg(DBG_PRINT, "(GRADING3A)\n");
				break;
			default:
				dbg(DBG_PRINT, "(GRADING3A)\n");
				break;
		}
	}
	list_iterate_end();
	
	tlb_flush_range((uint32_t) PN_TO_ADDR(lopage), npages);
        pt_unmap_range(curproc->p_pagedir, (uintptr_t) PN_TO_ADDR(lopage), (uintptr_t) PN_TO_ADDR(lopage + npages));
	dbg(DBG_PRINT, "(GRADING3A)\n");
	return 0;
}

/*
 * Returns 1 if the given address space has no mappings for the
 * given range, 0 otherwise.
 */
int
vmmap_is_range_empty(vmmap_t *map, uint32_t startvfn, uint32_t npages)
{
        uint32_t endvfn = startvfn+npages;
        KASSERT((startvfn < endvfn) && (ADDR_TO_PN(USER_MEM_LOW) <= startvfn) && (ADDR_TO_PN(USER_MEM_HIGH) >= endvfn)); /* the specified page range must not be empty and lie completely within the user space */
        dbg(DBG_PRINT, "(GRADING3A 3.e)\n");
	vmarea_t *iter;
	list_iterate_begin(&map->vmm_list, iter, vmarea_t, vma_plink) {
		if(iter->vma_start < endvfn && iter->vma_end > startvfn) {
                        dbg(DBG_PRINT, "(GRADING3A)\n");
			return 0;
                }
	}
	list_iterate_end();
	dbg(DBG_PRINT, "(GRADING3A)\n");
	return 1;
}

/* Read into 'buf' from the virtual address space of 'map' starting at
 * 'vaddr' for size 'count'. To do so, you will want to find the vmareas
 * to read from, then find the pframes within those vmareas corresponding
 * to the virtual addresses you want to read, and then read from the
 * physical memory that pframe points to. You should not check permissions
 * of the areas. Assume (KASSERT) that all the areas you are accessing exist.
 * Returns 0 on success, -errno on error.
 */
int
vmmap_read(vmmap_t *map, const void *vaddr, void *buf, size_t count)
{
        //NOT_YET_IMPLEMENTED("VM: vmmap_read");
        uint32_t curr = (uint32_t) vaddr;
        size_t i = 0;
        uint32_t read_count = 0;
        while (i < count) {

                vmarea_t *vm_area =  vmmap_lookup(map, ADDR_TO_PN(curr));



                // make sure if the leftover is smaller than the remaining in the page.
                uint32_t bytes = MIN(count - read_count, PAGE_SIZE - PAGE_OFFSET(curr));
                uint32_t pagenum = vm_area->vma_off + ADDR_TO_PN(curr) - vm_area->vma_start;
                pframe_t *pf;
                mmobj_t *mmobj = vm_area->vma_obj;
                mmobj->mmo_ops->lookuppage(mmobj, pagenum, 0, &pf);

               


                memcpy((char*) buf + read_count, (char*) pf->pf_addr + PAGE_OFFSET(curr), bytes);


                curr += bytes;
                i += bytes;
                read_count += bytes;
                dbg(DBG_PRINT, "(GRADING3D 1)\n");
        }
        dbg(DBG_PRINT, "(GRADING3D 1)\n");
        return 0;
}

/* Write from 'buf' into the virtual address space of 'map' starting at
 * 'vaddr' for size 'count'. To do this, you will need to find the correct
 * vmareas to write into, then find the correct pframes within those vmareas,
 * and finally write into the physical addresses that those pframes correspond
 * to. You should not check permissions of the areas you use. Assume (KASSERT)
 * that all the areas you are accessing exist. Remember to dirty pages!
 * Returns 0 on success, -errno on error.
 */
int
vmmap_write(vmmap_t *map, void *vaddr, const void *buf, size_t count)
{
        //NOT_YET_IMPLEMENTED("VM: vmmap_write");
        uint32_t curr = (uint32_t) vaddr;
        size_t i = 0;
        uint32_t write_count = 0;
        while (i < count) {

                vmarea_t *vm_area =  vmmap_lookup(map, ADDR_TO_PN(curr));



                uint32_t pagenum = vm_area->vma_off + ADDR_TO_PN(curr) - vm_area->vma_start;
                
                // make sure if the leftover is smaller than the remaining in the page.
                uint32_t bytes = MIN(count - write_count, PAGE_SIZE - PAGE_OFFSET(curr));

                
                
                pframe_t *pf;
                mmobj_t *mmobj = vm_area->vma_obj;

                mmobj->mmo_ops->lookuppage(mmobj, pagenum, 1, &pf);



                memcpy((char*) pf->pf_addr + PAGE_OFFSET(curr), (char*) buf + write_count, bytes);

                pframe_dirty(pf);


                curr += bytes;
                i += bytes;
                write_count += bytes;
                dbg(DBG_PRINT, "(GRADING3D 1)\n");
        }
        dbg(DBG_PRINT, "(GRADING3D 1)\n");
        return 0;
}
