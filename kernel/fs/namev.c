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
#include "globals.h"
#include "types.h"
#include "errno.h"

#include "util/string.h"
#include "util/printf.h"
#include "util/debug.h"

#include "fs/dirent.h"
#include "fs/fcntl.h"
#include "fs/stat.h"
#include "fs/vfs.h"
#include "fs/vnode.h"

#define MAX_STEP_LEN 256

int get_step(char *out, char **cur, char sep, size_t max)
{
    int rc = 0;

    while(**cur == sep && **cur != '\0') // Skip initial separators
    {

        ++*cur;
    }
    if(**cur != '\0')
    {
        while(max > 0 && **cur != '\0' && **cur != sep)
        {
            *out++ = **cur;
            ++*cur;

            --max;
        }
        *out = '\0';
        rc = 1;
    }

    return rc;
}
int isMatch(const char *cur, const char *out) {
    while (*out != '\0' && *cur == *out) {
        ++cur;
        ++out;
    }
    return *out == '\0';
}


char *searchString(const char *pathname, const char *out) {
    const char *cur = pathname;
    while (*cur != '\0') {
        if (isMatch(cur, out)) {
            return (char *)cur;
        }
        ++cur;
    }
    return NULL;
}
/* This takes a base 'dir', a 'name', its 'len', and a result vnode.
 * Most of the work should be done by the vnode's implementation
 * specific lookup() function.
 *
 * If dir has no lookup(), return -ENOTDIR.
 *
 * Note: returns with the vnode refcount on *result incremented.
 */
int
lookup(vnode_t *dir, const char *name, size_t len, vnode_t **result)
{
        KASSERT(NULL != dir); /* the "dir" argument must be non-NULL */
        KASSERT(NULL != name); /* the "name" argument must be non-NULL */
        dbg(DBG_PRINT,"(GRADING2A)\n");
        KASSERT(NULL != result); /* the "result" argument must be non-NULL */
        dbg(DBG_PRINT,"(GRADING2A)\n");
        if(dir->vn_ops->lookup==NULL){
                return -ENOTDIR;
        }else{
                return dir->vn_ops->lookup(dir,name,len,result);
        }
}


/* When successful this function returns data in the following "out"-arguments:
 *  o res_vnode: the vnode of the parent directory of "name"
 *  o name: the `basename' (the element of the pathname)
 *  o namelen: the length of the basename
 *
 * For example: dir_namev("/s5fs/bin/ls", &namelen, &name, NULL,
 * &res_vnode) would put 2 in namelen, "ls" in name, and a pointer to the
 * vnode corresponding to "/s5fs/bin" in res_vnode.
 *
 * The "base" argument defines where we start resolving the path from:
 * A base value of NULL means to use the process's current working directory,
 * curproc->p_cwd.  If pathname[0] == '/', ignore base and start with
 * vfs_root_vn.  dir_namev() should call lookup() to take care of resolving each
 * piece of the pathname.
 *
 * Note: A successful call to this causes vnode refcount on *res_vnode to
 * be incremented.
 */
int
dir_namev(const char *pathname, size_t *namelen, const char **name,
          vnode_t *base, vnode_t **res_vnode)
{
         
        KASSERT(NULL != pathname);
        KASSERT(NULL != namelen);
        dbg(DBG_PRINT,"(GRADING2A 2b)\n");
        KASSERT(NULL != name);
        KASSERT(NULL != res_vnode);
        dbg(DBG_PRINT,"(GRADING2A 2b)\n");
        vnode_t *temp_base;
        if(base==NULL){
                temp_base=curproc->p_cwd;
        }
        else{
                temp_base=base;
        }
        if(pathname[0] == '/')
        {
                temp_base=vfs_root_vn;
        }
        int res;
        char *curr_step=(char*)pathname;
        char step[MAX_STEP_LEN] = {0};

        int ct=0;
        size_t namelen_val=0;

        char temp_step[MAX_STEP_LEN] = {0};
        char *  temp_curr=NULL;
        char initial[MAX_STEP_LEN] = {0};
        char final[MAX_STEP_LEN]={0};
        vnode_t* dir=temp_base;
        //if(dir!=NULL){
                //vref(dir); //1
        //}
       
        vnode_t* curVNode = NULL;
        vnode_t *prevVNode = NULL;

        memcpy(initial,(char*)step,strlen((const char*)step)+1);
        while((get_step((char*)step, &curr_step, '/', MAX_STEP_LEN))!=0)
        {
                ++ct;
                memcpy((char*)temp_step,(char*)step,strlen((const char*)step)+1);
                temp_curr=curr_step;
                if(!get_step(step, &curr_step, '/', MAX_STEP_LEN))
                {

                        break;
                }
                memcpy((char*)step,(char*)temp_step,strlen((const char*)temp_step)+1);
                curr_step=temp_curr;

                *name=searchString(pathname,(const char*)step);
                *namelen=strlen((const char*)step);
                //dbg(DBG_PRINT,"(GRADING2A 2B) name len is %d, name is %s\n",*namelen,*name);
                if(*namelen>NAME_LEN){
                        
                        vput(prevVNode);
                        dbg(DBG_PRINT,"(GRADING2A 2B) name len is %d\n",*namelen);
                        return -ENAMETOOLONG;
                }

                if((res=lookup(dir,*name,*namelen,res_vnode))<0)
                {
                        //dbg(DBG_PRINT,"vput line 190\n");
                        //vput(prevVNode);
                        dbg(DBG_PRINT,"(GRADING2A 2B)\n");
                        return res;
                }
                if(prevVNode != NULL){
                        vput(prevVNode);
                }

                if((*res_vnode)->vn_ops->lookup == NULL){
                        
                        vput(*res_vnode);
                        dbg(DBG_PRINT,"(GRADING2A 2B)\n");
                        return -ENOTDIR;
                }

                dir=*res_vnode;
                prevVNode=*res_vnode;

                KASSERT(NULL != dir); // pathname resolution must start with a valid directory 
                dbg(DBG_PRINT,"(GRADING2A 2b)\n");
                
        }
        dbg(DBG_PRINT,"%d\n", ct);
        if(ct==0||ct==1)
        {
                
                if(ct==0)
                {
                        memcpy(step,(char*)initial,strlen((const char*)initial)+1);
                        *name=(const char*)step;
                        *namelen=strlen((const char*)step);
                        
                }else{
                        *name=searchString(pathname,(const char*)step);
                        *namelen=strlen((const char*)step);
                        if(*namelen>NAME_LEN){
                                //vput(prevVNode);
                                dbg(DBG_PRINT,"(GRADING2A 2B) name len is %d\n",*namelen);
                                return -ENAMETOOLONG;
                        }
                        
                }
                if((res=lookup(temp_base,(const char *)".",1,res_vnode))<0)
                {
                        dbg(DBG_PRINT,"(GRADING2A 2B)\n");
                        return res;
                }


        } else{
                *name=searchString(pathname,(const char*)step);
                *namelen=strlen((const char*)step);
                //dbg(DBG_PRINT,"(GRADING2A 2B) name len is %d, name is %s\n",*namelen,*name);
                if(*namelen>NAME_LEN){
                        vput(prevVNode);
                        dbg(DBG_PRINT,"(GRADING2A 2B) name len is %d\n",*namelen);
                        return -ENAMETOOLONG;
                }
                

        }

        dbg(DBG_PRINT,"(GRADING2A 2B)\n");
        return res;


     
        
}

/*
 * It makes use of dir_namev and lookup to find the specified vnode (if it
 * exists).  flag is right out of the parameters to open(2); see
 * <weenix/fcntl.h>.  If the O_CREAT flag is specified and the file does
 * not exist, call create() in the parent directory vnode. However, if the
 * parent directory itself does not exist, this function should fail - in all
 * cases, no files or directories other than the one at the very end of the path
 * should be created.
 *
 * Note: Increments vnode refcount on *res_vnode.
 */
int
open_namev(const char *pathname, int flag, vnode_t **res_vnode, vnode_t *base)
{
	size_t name_len;
	const char *name;
	vnode_t *dir;

	int name_res = dir_namev(pathname, &name_len, &name, base, &dir);
	if (name_res < 0) {
                dbg(DBG_PRINT,"(GRADING2B)\n");
		return name_res;
	}

        if (name_len == 0) {
                *res_vnode = dir;
		dbg(DBG_PRINT,"(GRADING2B)\n");
                return 0;
        }

	int look_res = lookup(dir, name, name_len, res_vnode);
	int curr_ret = look_res;

	if (look_res == -ENOENT) {
        	if ((flag & O_CREAT)) {
     			KASSERT(NULL != dir->vn_ops->create);
                        dbg(DBG_PRINT,"(GRADING2A)\n");     
			curr_ret = dir->vn_ops->create(dir, name, name_len, res_vnode);
			dbg(DBG_PRINT,"(GRADING2B)\n");
       		} 
	}
	vput(dir);
	dbg(DBG_PRINT,"(GRADING2A)\n");
	return curr_ret;
}

#ifdef __GETCWD__
/* Finds the name of 'entry' in the directory 'dir'. The name is writen
 * to the given buffer. On success 0 is returned. If 'dir' does not
 * contain 'entry' then -ENOENT is returned. If the given buffer cannot
 * hold the result then it is filled with as many characters as possible
 * and a null terminator, -ERANGE is returned.
 *
 * Files can be uniquely identified within a file system by their
 * inode numbers. */
int
lookup_name(vnode_t *dir, vnode_t *entry, char *buf, size_t size)
{
        NOT_YET_IMPLEMENTED("GETCWD: lookup_name");
        return -ENOENT;
}


/* Used to find the absolute path of the directory 'dir'. Since
 * directories cannot have more than one link there is always
 * a unique solution. The path is writen to the given buffer.
 * On success 0 is returned. On error this function returns a
 * negative error code. See the man page for getcwd(3) for
 * possible errors. Even if an error code is returned the buffer
 * will be filled with a valid string which has some partial
 * information about the wanted path. */
ssize_t
lookup_dirpath(vnode_t *dir, char *buf, size_t osize)
{
        NOT_YET_IMPLEMENTED("GETCWD: lookup_dirpath");

        return -ENOENT;
}
#endif /* __GETCWD__ */
