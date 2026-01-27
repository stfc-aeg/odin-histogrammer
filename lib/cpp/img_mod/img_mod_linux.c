
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>
#include "datamod.h"

#include "os9types.h"
typedef struct _mmap
{
	void *va;
	size_t size;
	int lcount;
    dev_t         st_dev;      /* device */
    ino_t         st_ino;      /* inode */
	char *mname;
	int created;
	struct _mmap *next;
} MMap;

static MMap *top=NULL;
static  pthread_mutex_t img_mod_mutex = PTHREAD_MUTEX_INITIALIZER;

MMap *new_mbuf(struct stat *stat_buf, void *va)
{
	MMap *mbuf = (MMap *)malloc(sizeof(MMap));

	if (mbuf != NULL)
	{
		mbuf->st_dev = stat_buf->st_dev;
		mbuf->st_ino = stat_buf->st_ino;
		mbuf->size = stat_buf->st_size;
		mbuf->va= va;
		mbuf->lcount = 1;
		mbuf->next = top;
		mbuf->mname = NULL;
		mbuf->created = 0;
		top = mbuf;
	}
	else
	{
		fprintf(stderr, "Out of memory saving Shared memory details\n");
	}
	return mbuf;
}
MMap *search_mbuf(struct stat *stat_buf)
{
	MMap *mbuf;
/*	printf("Searching for mbuf with sizeof(st_dev)=%d, sizeof(st_ino)=%d\n ... st_dev=%#llx, st_ino=%#x\n", 
			sizeof(stat_buf->st_dev), sizeof(stat_buf->st_ino), stat_buf->st_dev,  stat_buf->st_ino); */
	for (mbuf = top; mbuf != NULL; mbuf = mbuf->next)
	{
/*		printf("Comparing mbuf at addr %p with sizeof(dev-t)=%d, sizeof(into_t)=%d\n.... st_dev=%#llx,  st_ino=%#x\n", mbuf, 
				sizeof(dev_t), sizeof(ino_t), mbuf->st_dev,  mbuf->st_ino); */
		if (stat_buf->st_dev == mbuf->st_dev &&
			stat_buf->st_ino == mbuf->st_ino)
		{
/*			printf("Found matching mbuf at %p\n", mbuf); */
				return mbuf;
		}
	}
	return NULL;
}

size_t datamod_mod_size(void *mod)
{
	MMap *mbuf;
	for (mbuf = top; mbuf != NULL; mbuf = mbuf->next)
	{
		if (mod == mbuf->va)
		{
			return mbuf->size;
		}
	}
	return (size_t)-1;
}

int _os_datmod (const char *name, size_t size, u_int16 *attr_rev, u_int16 *type_lang, u_int32 perm, void **mod, void **mod_head)
{
	int fd;
	int prot = PROT_READ | PROT_WRITE;
	void *va;
	int len;
	struct stat stat_buf;
	int old_umask;
	char *mname;
	MMap *mbuf;

	len = strlen(name);
	
	if ((mname=malloc(len+2)) == NULL)
	{
		fprintf(stderr, "Out of memory\n");
		return -1;
	}
	if (name[0] != '/')
		strcpy(mname, "/");
	else
		mname[0] = 0;
	strcat(mname, name);
	
	pthread_mutex_lock(&img_mod_mutex);
	old_umask = umask(0);
	if ((fd = shm_open(mname, O_RDWR | O_CREAT | O_EXCL, 0x1FF)) <0 )
	{
		fprintf(stderr, "Cannot create share memory file '%s', errno= %d\n", mname, errno);
		free(mname);
		pthread_mutex_unlock(&img_mod_mutex);
		return -1;
	}
	if (ftruncate(fd, size) <0)
	{
		fprintf(stderr, "_os_datmod:Cannot ftruncate(size=%zd) shared memory file '%s', errno= %d\n", size, mname, errno);
		free(mname);
		pthread_mutex_unlock(&img_mod_mutex);
		return -1;
	}
	if (fstat(fd, &stat_buf) <0)
	{
		fprintf(stderr, "Cannot stat share memory file '%s', errno= %d\n", mname, errno);
	}
	if ((va=mmap(NULL, size, prot, MAP_SHARED, fd, 0)) == MAP_FAILED)
	{
		fprintf(stderr, "Cannot mmap share memory file '%s', errno= %d\n", mname, errno);
		close (fd);
		free(mname);
		pthread_mutex_unlock(&img_mod_mutex);
		return -1;
	}
	
	*mod = va;
	mbuf = new_mbuf(&stat_buf, va);

	if (mod_head != NULL)
	{
		*mod_head = mbuf;
	}
	close (fd);
	umask(old_umask);
	mbuf->mname = mname;
	mbuf->created = 1;
	pthread_mutex_unlock(&img_mod_mutex);
	return 0;

}
int _os_link(const char **namep, void **mod_head, void **mod, u_int16 *type_lang, u_int16 *att_rev)
{
	int fd;
	int prot = PROT_READ | PROT_WRITE;
	void *va;
	int len;
	struct stat stat_buf;
	char *mname;
	const char*name;
	MMap *mbuf;

	pthread_mutex_lock(&img_mod_mutex);
	name = *namep;

	len = strlen(name);
	if ((mname=malloc(len+2)) == NULL)
	{
		fprintf(stderr, "Out of memory\n");
		pthread_mutex_unlock(&img_mod_mutex);
		return -1;
	}
	if (name[0] != '/')
		strcpy(mname, "/");
	else
		mname[0] = 0;
	strcat(mname, name);

	if ((fd = shm_open(mname, O_RDWR, 0)) <0 )
	{
/*		fprintf(stderr, "Cannot open share memory file '%s', errno= %d\n", mname, errno); */
		free(mname);
		pthread_mutex_unlock(&img_mod_mutex);
		return -1;
	}
	if (fstat(fd, &stat_buf) <0)
	{
		fprintf(stderr, "Cannot stat share memory file '%s', errno= %d\n", mname, errno);
		free(mname);
		pthread_mutex_unlock(&img_mod_mutex);
		return -1;
	}
	mbuf = search_mbuf(&stat_buf);
	if (mbuf == NULL)
	{
		/* New mapping for this program */
		if ((va=mmap(NULL, stat_buf.st_size, prot, MAP_SHARED, fd, 0)) == MAP_FAILED)
		{
			fprintf(stderr, "Cannot mmap share memory file '%s', errno= %d\n", mname, errno);
			close (fd);
			free(mname);
			pthread_mutex_unlock(&img_mod_mutex);
			return -1;
		}
		*mod = va;
		mbuf = new_mbuf(&stat_buf, va);
		mbuf->created = 0;
		mbuf->mname = mname;
		if (mod_head != NULL)
			*mod_head = mbuf;
	}
	else
	{
		*mod = mbuf->va;
		mbuf->lcount ++;
		if (mod_head != NULL)
			*mod_head = mbuf;
		free(mname);
	}		
	close (fd);
	pthread_mutex_unlock(&img_mod_mutex);
	return 0;
}

int _os_unlink(void *mod_head)
{
	return munlink(mod_head);
}

int munlink(void *mod_head)
{
	return munlink_linux(mod_head, ImgModUnlinkNone);
}
/**
@param  what ImgModUnlinkNone, ImgModUnlinkCreated, ImgModUnlinkAll
**/
int munlink_linux(void *mod_head, ImgModUnlink unlink_what)
{
	MMap *mbuf;
	MMap *prev;

	pthread_mutex_lock(&img_mod_mutex);
	prev = NULL;
	for (mbuf=top; mbuf != NULL; mbuf = mbuf->next)
	{
		if (mbuf == mod_head)
			break;
		prev = mbuf;
	}
	if (mbuf == NULL)
	{
		fprintf(stderr,  " Cannot find module head pointer 0x%0x in list so not touching anything\n", mod_head);
		pthread_mutex_unlock(&img_mod_mutex);
		return -1;
	}
	mbuf->lcount--;
	if (mbuf->lcount == 0 || unlink_what == ImgModUnlinkCreated || unlink_what == ImgModUnlinkAll)
	{
		munmap(mbuf->va, mbuf->size);
		if (prev == NULL)
			top = mbuf->next;
		else
			prev->next = mbuf->next;
		if ((unlink_what == ImgModUnlinkCreated && mbuf->created) || unlink_what == ImgModUnlinkAll)
		{
			if (shm_unlink(mbuf->mname) == -1)
				fprintf(stderr, "munlink_linux: Cannot unlink module %s; errno=%d => %s\n", mbuf->mname, errno, strerror(errno));
		}
		free (mbuf->mname);
		free(mbuf);
	}
	pthread_mutex_unlock(&img_mod_mutex);
	return 0;
}

size_t datamod_size(void *mod_head)
{
	MMap *mbuf= mod_head;
	return mbuf->size;
}
