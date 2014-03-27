/*
 * Pipe FS header file for OSM exam at DIKU 2014.
 *
 * You may need to modify this file.
 */

#include "kernel/sleepq.h"
#include "kernel/interrupt.h"
#include "kernel/config.h"
#include "kernel/kmalloc.h"
#include "kernel/assert.h"
#include "vm/pagepool.h"
#include "drivers/gbd.h"
#include "fs/vfs.h"
#include "fs/pipe.h"
#include "lib/libc.h"

/* Data structure for use internally in pipefs. We allocate space for this
 * dynamically during initialization */
typedef struct {
  semaphore_t *lock;
  pipe_t pipes[CONFIG_MAX_PIPES];
  int free_pipes;
} pipefs_t;

/***********************************
 * fs_t function implementations
 ***********************************/

/* Initialize pipefs. We allocate one page of dynamic memory for the
 * fs_t and pipefs_t structures.  Note that, in contrast to other
 * filesystems, we take no disk parameter.  You may want to extend
 * this function. */
fs_t *pipe_init(void)
{
  kprintf("pipe_init\n");
  uint32_t addr;
  fs_t *fs;
  pipefs_t *pipefs;
  semaphore_t *sem;

  /* check semaphore availability before memory allocation */
  sem = semaphore_create(1);
  if (sem == NULL) {
    kprintf("pipe_init: could not create a new semaphore.\n");
    return NULL;
  }

  addr = pagepool_get_phys_page();
  if(addr == 0) {
    semaphore_destroy(sem);
    kprintf("pipe_init: could not allocate memory.\n");
    return NULL;
  }
  addr = ADDR_PHYS_TO_KERNEL(addr);    /* transform to vm address */

  /* Assert that one page is enough */
  KERNEL_ASSERT(PAGE_SIZE >= (sizeof(pipefs_t)+sizeof(fs_t)));

  /* fs_t, pipefs_t and all required structure will most likely fit
     in one page, so obtain addresses for each structure and buffer
     inside the allocated memory page. */
  fs  = (fs_t *)addr;
  pipefs = (pipefs_t *)(addr + sizeof(fs_t));

  /* save semaphore to the pipefs_t */
  pipefs->lock = sem;
  pipefs->free_pipes = CONFIG_MAX_PIPES;
  fs->internal = (void *)pipefs;

  /* We always have this name. */
  stringcopy(fs->volume_name, "pipe", VFS_NAME_LENGTH);

  fs->unmount   = pipe_unmount;
  fs->open      = pipe_open;
  fs->close     = pipe_close;
  fs->create    = pipe_create;
  fs->remove    = pipe_remove;
  fs->read      = pipe_read;
  fs->write     = pipe_write;
  fs->getfree   = pipe_getfree;
  fs->filecount = pipe_filecount;
  fs->file      = pipe_file;
  return fs;
}

/* You have to implement the following functions.  Some of them may
   just return an error.  You have to carefully consider what to do.*/

int pipe_unmount(fs_t *fs)
{
  fs=fs;
  return VFS_OK;
}

int pipe_open(fs_t *fs, char *filename)
{
  pipefs_t *pfs;
  pfs = (pipefs_t*) fs->internal;
  int i;
  semaphore_P(pfs->lock);
  // Find matching pipe.
  for (i = 0; i <= CONFIG_MAX_PIPES; i++) {
    if (stringcmp(pfs->pipes[i].name,filename) == 0) {
      semaphore_V(pfs->lock);
      return i;
    }
  }
  semaphore_V(pfs->lock);
  return VFS_NOT_FOUND;
}

int pipe_close(fs_t *fs, int fileid)
{
  fs = fs; fileid = fileid;
  // No effect.
  return VFS_OK;
}

int pipe_create(fs_t *fs, char *filename, int size)
{
  semaphore_t *read_sem;
  semaphore_t *write_sem;
  pipefs_t *pfs;
  pfs = (pipefs_t*) fs->internal;
  int pid, i;
  semaphore_P(pfs->lock);
  pid = -1;
  // Find free pipe, return error if none left or one with same name exists.
  for (i = 0; i <= CONFIG_MAX_PIPES; i++) {
    if ((pfs->pipes[i].state != PIPE_FREE &&
         stringcmp(pfs->pipes[i].name,filename) == 0)) {
      semaphore_V(pfs->lock);
      return VFS_ERROR;
    }
    if (pid < 0 && pfs->pipes[i].state == PIPE_FREE){
      pid = i;
    }
  }
  if (pid < 0) {
    semaphore_V(pfs->lock);
    return VFS_ERROR;
  }
  read_sem = semaphore_create(0);
  write_sem = semaphore_create(0);
  stringcopy(pfs->pipes[pid].name,filename,CONFIG_PIPE_MAX_NAME);
  pfs->pipes[pid].size = size;
  pfs->pipes[pid].state = PIPE_OCCUPIED;
  pfs->pipes[pid].offset = 0;
  pfs->pipes[pid].write_sem = write_sem;
  pfs->pipes[pid].read_sem = read_sem;
  pfs->free_pipes --;
  semaphore_V(pfs->lock);
  return VFS_OK;
}

int pipe_remove(fs_t *fs, char *filename)
{
  pipefs_t *pfs;
  pfs = (pipefs_t*) fs->internal;
  int i;
  semaphore_P(pfs->lock);
  // Find matching pipe.
  for (i = 0; i <= CONFIG_MAX_PIPES; i++) {
    if (stringcmp(pfs->pipes[i].name,filename)==0) {
      pfs->pipes[i].state = PIPE_FREE;
      pfs->free_pipes ++;
      sleepq_wake_all(pfs->pipes[i].read_sem);
      sleepq_wake_all(pfs->pipes[i].write_sem);
      semaphore_V(pfs->lock);
      return VFS_OK;
    }
  }
  semaphore_V(pfs->lock);
  return VFS_ERROR;
}

int pipe_read(fs_t *fs, int fileid, void *buffer, int bufsize, int offset)
{
//  kprintf("pipe_read\n");
  pipefs_t *pfs;
  pfs = (pipefs_t*) fs->internal;
  interrupt_status_t intr_state;
  int new_size, bytesread, bytes_remaining;
  new_size = CONFIG_PIPE_BUFFER_SIZE;
  bytesread = 0;
  bytes_remaining = bufsize;
  while (1) {
    semaphore_P(pfs->lock);
    switch (pfs->pipes[fileid].state) {
      case PIPE_FREE:
        return VFS_ERROR;
      case PIPE_LISTENING:
        // Nothing happens, it is added to sleepqueue afterwards.
        break;
      case PIPE_STREAMING:
        pfs->pipes[fileid].state = PIPE_INUSE;
        semaphore_V(pfs->lock);
        while (pfs->pipes[fileid].state == PIPE_INUSE && bufsize > 0) {
          semaphore_P(pfs->pipes[fileid].read_sem);
          new_size = CONFIG_PIPE_BUFFER_SIZE - pfs->pipes[fileid].offset;
          if (bytes_remaining < CONFIG_PIPE_BUFFER_SIZE - pfs->pipes[fileid].offset) {
            new_size = bytes_remaining;
          }
//          kprintf("r newsize = %d, offset = %d\n", new_size, pfs->pipes[fileid].offset);
          memcopy(new_size,
                  buffer+offset, pfs->pipes[fileid].buffer +
                                 pfs->pipes[fileid].offset);
          bytesread += new_size;
          offset += new_size;
          pfs->pipes[fileid].size -= new_size;
          if (bufsize == bytesread) {
            pfs->pipes[fileid].offset = new_size;
            pfs->pipes[fileid].state = PIPE_STREAMING;
            if (pfs->pipes[fileid].size == 0) {
              pfs->pipes[fileid].state = PIPE_OCCUPIED;
              sleepq_wake(pfs->pipes[fileid].write_sem);
            }
            semaphore_V(pfs->pipes[fileid].read_sem);
            sleepq_wake(pfs->pipes[fileid].read_sem);
            return bytesread;
          }
          bytes_remaining -= (CONFIG_PIPE_BUFFER_SIZE - pfs->pipes[fileid].offset);
          semaphore_V(pfs->pipes[fileid].write_sem);
        }
        break;
      case PIPE_OCCUPIED:
        pfs->pipes[fileid].state = PIPE_LISTENING;
        sleepq_wake(pfs->pipes[fileid].write_sem);
        break;
      case PIPE_INUSE:
        // Nothing happens, it is added to sleepqueue afterwards
        break;
      default:
        semaphore_V(pfs->lock);
        return VFS_ERROR;
    }
    intr_state = _interrupt_disable();
    sleepq_add(pfs->pipes[fileid].read_sem);
    _interrupt_set_state(intr_state);
    semaphore_V(pfs->lock);
    thread_switch();
  }
}

int pipe_write(fs_t *fs, int fileid, void *buffer, int datasize, int offset)
{
  kprintf("pipe_write\n");
  pipefs_t *pfs;
  pfs = (pipefs_t*) fs->internal;
  interrupt_status_t intr_state;
  int write_return;
  while (1) {
    switch (pfs->pipes[fileid].state) {
      case PIPE_FREE:
        return VFS_ERROR;
      case PIPE_LISTENING:
        semaphore_P(pfs->lock);
        pfs->pipes[fileid].size = datasize;
        pfs->pipes[fileid].state = PIPE_STREAMING;
        semaphore_V(pfs->lock);
        sleepq_wake(pfs->pipes[fileid].read_sem);
        write_return = datasize;
        while ((pfs->pipes[fileid].state == PIPE_STREAMING ||
                pfs->pipes[fileid].state == PIPE_INUSE)  && datasize > 0) {
          kprintf("w\n");
          memcopy(CONFIG_PIPE_BUFFER_SIZE, pfs->pipes[fileid].buffer, buffer + offset);
          pfs->pipes[fileid].offset = 0;
          datasize -= CONFIG_PIPE_BUFFER_SIZE;
          offset += CONFIG_PIPE_BUFFER_SIZE;
          semaphore_V(pfs->pipes[fileid].read_sem);
          semaphore_P(pfs->pipes[fileid].write_sem);
        }
        pfs->pipes[fileid].state = PIPE_OCCUPIED;
        if (datasize < 0) {
          return write_return;
        } else {
          return write_return - datasize;
        }
        break;
      case PIPE_STREAMING:
        // Nothing happens, it is added to sleepqueue afterwards.
        break;
      case PIPE_OCCUPIED:
        break;
      default:
        return VFS_ERROR;
    }
    semaphore_P(pfs->lock);
    intr_state = _interrupt_disable();
    sleepq_add(pfs->pipes[fileid].write_sem);
    _interrupt_set_state(intr_state);
    semaphore_V(pfs->lock);
    sleepq_wake(pfs->pipes[fileid].read_sem);
    thread_switch();
  }
}

int pipe_getfree(fs_t *fs)
{
  pipefs_t *pfs;
  pfs = (pipefs_t*) fs->internal;
  int retval;
  semaphore_P(pfs->lock);
  retval = pfs->free_pipes;
  semaphore_V(pfs->lock);
  return retval;
}

int pipe_filecount(fs_t *fs, char *dirname)
{
  // Directories not used in pipe implementation.
  dirname = dirname;
  pipefs_t *pfs;
  pfs = (pipefs_t*) fs->internal;
  int retval;
  semaphore_P(pfs->lock);
  retval = pfs->free_pipes;
  semaphore_V(pfs->lock);
  return CONFIG_MAX_PIPES - retval;
}

int pipe_file(fs_t *fs, char *dirname, int idx, char *buffer)
{
  // It does not support directories.
  dirname = dirname;

  if (idx >= CONFIG_MAX_PIPES){
    return VFS_ERROR;
  }

  pipefs_t *pfs;
  pfs = (pipefs_t*) fs->internal;
  semaphore_P(pfs->lock);
  if (pfs->pipes[idx].state == PIPE_FREE) {
    return VFS_ERROR;
  }

  stringcopy(buffer, pfs->pipes[idx].name,CONFIG_PIPE_MAX_NAME);
  semaphore_V(pfs->lock);

  return VFS_OK;
}
