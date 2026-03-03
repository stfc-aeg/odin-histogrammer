/*****************************************************************************
*
*	img_mod.c
*
*	Code for creating, deleting and manipulating data modules.
*
*	By W.I. Helsby.
*
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#if defined(USE_GTK) || defined(__LINUX__)
#include <inttypes.h>
#include <sys/types.h>
#include "os9types.h"
#include <sys/stat.h>
#include <fcntl.h>
#if defined (__MINGW32__)
#define S_IRGRP 0
#define S_IROTH 0
#endif
#if defined (_MSC_VER)
#define S_IRUSR 0
#define S_IWUSR 0
#define S_IRGRP 0
#define S_IROTH 0
#endif
#else
#include <module.h>
#include <sgstat.h>
#include <types.h>
#endif

#include "datamod.h"
#define ID_MAX_ERR_MSG 200
MOD_IMAGE *id_mkmod (const char *name, int num_x, int num_y, const char *x_lab, const char *y_lab, int data_type, mh_com **mod_head)
{
	MOD_IMAGE *mod;
	char err_msg[ID_MAX_ERR_MSG];
	mod = id_mkmod_err_msg (name, num_x, num_y, x_lab, y_lab, data_type, mod_head, err_msg, ID_MAX_ERR_MSG);
	if (mod == NULL)
		printf("%s\n", err_msg);
	return mod;
}
	
MOD_IMAGE *id_mkmod_err_msg ( const char *name, int num_x, int num_y, const char *x_lab, const char *y_lab, int data_type, mh_com **mod_head, char *err_msg, int max_err_msg)
{
	MOD_IMAGE *mod;
	mh_com *lmod_head;
    u_int16 attr_rev = mkattrevs(MA_REENT,1);
    u_int32 perm =    MP_OWNER_READ | MP_OWNER_WRITE | MP_GROUP_READ | MP_WORLD_READ;
    u_int16 type_lang = mktypelang(MT_DATA, ML_ANY);
    error_code err;
	int elem_size = 4;

	
	if (data_type == 0)
	{
		printf("WARNING caller to id_mkmod using old data_float value 0 , assuming  DATA_LONG\n");
		data_type = DATA_LONG;
	}

	if (data_type == DATA_LONG)
		elem_size = 4;
	else if (data_type == DATA_SHORT)
		elem_size = 2;
	else if (data_type == DATA_FLOAT)
		elem_size = 4;
	else if (data_type == DATA_DOUBLE)
		elem_size = 8;
	else if (data_type >= DATA_SHORT_DIG1 && data_type <= DATA_SHORT_DIG2)
		elem_size = 2;
	else if (data_type >= DATA_FIXED8_DIG1 && data_type <= DATA_FIXED8_DIG8)
		elem_size = 4;

	if (mod_head == NULL)
		mod_head = &lmod_head;	/* So don't bus error */
    
   	err = _os_link(&name, mod_head, (void **)&mod, &type_lang, &attr_rev);
    if (err == 0)
    {
		if (IMG_MOD_GET_TYPE(mod) != IMG_MOD_2D)
		{
			snprintf(err_msg, max_err_msg, "Error: Module '%s' exists but is not a 2D module, cannot change type", name);
			munlink (*mod_head);
			return NULL;
		}
		/* Allow a negative num_y to be a -default and minimum num y */
    	if ((num_x >0 && mod->head.num_x != num_x) || (num_x < 0 && mod->head.num_x < -num_x) || (num_y >0 && mod->head.num_y != num_y) || (num_y <0 && mod->head.num_y < -num_y))
    	{
    		snprintf(err_msg, max_err_msg, "Error, module '%s' exists but is wrong size (%d,%d) not (%d, %d)\n", name, mod->head.num_x, mod->head.num_y, num_x, num_y);
	        munlink (*mod_head);
			return NULL;
		}
		if (mod->head.data_type != data_type)
    	{
    		snprintf(err_msg, max_err_msg, "Error, module '%s' exists but is wrong data type %d not %d\n", name, mod->head.data_type, data_type);
	        munlink (*mod_head);
			return NULL;
		}
        return mod;
    }
	if (num_x < 0)
		num_x *= -1;
	/* If num_y <0 -num_y is the default number of rows */
	if (num_y < 0)
		num_y *= -1;
    err = _os_datmod (name, (off_t)num_x*(off_t)num_y*(off_t)elem_size+sizeof(struct mod_header),
                        &attr_rev, &type_lang, perm, (void **)&mod, (mh_data **) mod_head);
                        
    if (err)
    {
        snprintf(err_msg, max_err_msg, "ERROR: Cannot create data module \"%s\", errno = %d.", name, err);
        return NULL;
    }
	/* Now link again ready to use, so first unlink does not make it fall out */
   	err = _os_link(&name, mod_head, (void **)&mod, &type_lang, &attr_rev);
	if (err > 0)
	{
		snprintf(err_msg, max_err_msg, "Cannot link to module I just made. Why?");
		return NULL;
	}

    mod->head.data_type = data_type;
    mod->head.disp_type = DISP_1D_Y;
    mod->head.num_x     = num_x;
    mod->head.num_y     = num_y;
    mod->head.version   = 0;
    mod->head.aspect    = -1.0;
    if (x_lab == NULL)
        strcpy(mod->head.x_label,"X");
    else
    {
        strncpy(mod->head.x_label,x_lab, MOD_LABLEN);
        mod->head.x_label[MOD_LABLEN] = 0;
    }
    if (y_lab == NULL)
        strcpy(mod->head.y_label,"Y");
    else
    {
        strncpy(mod->head.y_label,y_lab, MOD_LABLEN);
        mod->head.y_label[MOD_LABLEN] = 0;
    }
    strcpy(mod->head.z_label,"COUNTS");
    strcpy(mod->head.title, name);
    return mod;
}

MOD_IMAGE3D *id_mkmod3d (const char *name, int num_x, int num_y, int num_t, const char *x_lab, const char *y_lab, const char *t_lab, char ** labels, int data_type, mh_com **mod_head)
{
	MOD_IMAGE3D *mod3d;
	char err_msg[ID_MAX_ERR_MSG];

	mod3d = id_mkmod3d_err_msg ( name, num_x, num_y, num_t, x_lab, y_lab, t_lab, labels, data_type, mod_head, err_msg, ID_MAX_ERR_MSG);
	if (mod3d == NULL)
		printf("%s\n", err_msg);
	return mod3d;
}	

MOD_IMAGE3D *id_mkmod3d_err_msg (const char *name, int num_x, int num_y, int num_t, const char *x_lab, const char *y_lab, const char *t_lab, char ** labels, int data_type, mh_com **mod_head, char *err_msg, int max_err_msg)
{
	MOD_IMAGE3D *mod;
	mh_com *lmod_head;
    u_int16 attr_rev = mkattrevs(MA_REENT,1);
    u_int32 perm =    MP_OWNER_READ | MP_OWNER_WRITE | MP_GROUP_READ | MP_WORLD_READ;
    u_int16 type_lang = mktypelang(MT_DATA, ML_ANY);
    error_code err;
	int elem_size = 4;
	int labels_num, labels_size;
	int i;
	char *lab;

	if (data_type == 0)
	{
		printf("WARNING caller to id_mkmod3d using old data_float value 0 , assuming  DATA_LONG\n");
		data_type = DATA_LONG;
	}

	if (data_type == DATA_LONG)
		elem_size = 4;
	else if (data_type == DATA_SHORT)
		elem_size = 2;
	else if (data_type == DATA_FLOAT)
		elem_size = 4;
	else if (data_type == DATA_DOUBLE)
		elem_size = 8;
	else if (data_type >= DATA_SHORT_DIG1 && data_type <= DATA_SHORT_DIG2)
		elem_size = 2;
	else if (data_type >= DATA_FIXED8_DIG1 && data_type <= DATA_FIXED8_DIG8)
		elem_size = 4;

	if (mod_head == NULL)
		mod_head = &lmod_head;	/* So don't bus error */
    

	labels_num=0;
	labels_size=0;
	if (labels != NULL)
	{
		for (i=0; i<num_y && labels[i] != NULL; i++)
		{
			labels_num++;
			labels_size += strlen(labels[i])+1;
		}
	}
	labels_size += sizeof(int)*labels_num;
	labels_size = (labels_size+15)/16;		// Align labels_size up to 16 byte boundaries .. hope this is enough.
	labels_size *= 16;
	err = _os_link(&name, mod_head, (void **)&mod, &type_lang, &attr_rev);
    if (err == 0)
    {
		if (IMG_MOD_GET_TYPE(mod) != IMG_MOD_3D)
		{
			snprintf(err_msg, max_err_msg, "Error: Module '%s' exists but is not a 3D module, cannot change type", name);
			munlink (*mod_head);
			return NULL;
		}
    	if ((num_x >0 && mod->head.num_x != num_x) || (num_x < 0 && mod->head.num_x < -num_x) || (num_y >0 && mod->head.num_y != num_y) || (num_y <0 && mod->head.num_y < -num_y) || 
    		(num_t > 0 && mod->head.num_t != num_t) || (num_t < 0 && mod->head.num_t < -num_t))
    	{
    		snprintf(err_msg, max_err_msg, "Error, module '%s' exists but is wrong size (%d,%d, %d) not (%d, %d, %d)",
					name, mod->head.num_x, mod->head.num_y, mod->head.num_t, num_x, num_y,num_t);
	        munlink (*mod_head);
			return NULL;
		}
		if (mod->head.data_type != data_type)
    	{
    		snprintf(err_msg, max_err_msg, "Error, module '%s' exists but is wrong data type %d not %d",
					name, mod->head.data_type, data_type);
	        munlink (*mod_head);
			return NULL;
		}
		if (mod->head.labels_size < labels_size)
    	{
    		snprintf(err_msg, max_err_msg, "Error, module '%s' exists has only %d bytes allowed for labels, now need %d",
					name, mod->head.labels_size, labels_size);
	        munlink (*mod_head);
			return NULL;
		}

        return mod;
    }
	if (num_x < 0)
		num_x *= -1;
	/* If num_y <0 -num_y is the default number of rows */
	if (num_y < 0)
		num_y *= -1;
	if (num_t < 0 )
		num_t *= -1;

    err = _os_datmod (name, (size_t)num_x*(size_t)num_y*(size_t)num_t*elem_size+sizeof(struct mod_header3d)+labels_size,
                        &attr_rev, &type_lang, perm, (void **)&mod, (mh_data **) mod_head);
                        
    if (err)
    {
        snprintf(err_msg, max_err_msg, "Cannot create data module \"%s\", errno = %d.", name, err);
        return NULL;
    }
	/* Now link again ready to use, so first unlink doe snot make it fall out */
   	err = _os_link(&name, mod_head, (void **)&mod, &type_lang, &attr_rev);
	if (err > 0)
	{
		snprintf(err_msg, max_err_msg, "Cannot link to module I just made. Why?");
		return NULL;
	}

    mod->head.data_type = data_type;
    mod->head.disp_type = IMG_MOD_SET_DISP_TYPE(IMG_MOD_3D, DISP_1D_Y);
    mod->head.num_x     = num_x;
    mod->head.num_y     = num_y;
    mod->head.num_t     = num_t;
    mod->head.version   = 0;
    mod->head.aspect    = -1.0;
    if (x_lab == NULL)
        strcpy(mod->head.x_label,"X");
    else
    {
        strncpy(mod->head.x_label,x_lab, MOD_LABLEN);
        mod->head.x_label[MOD_LABLEN] = 0;
    }
    if (y_lab == NULL)
        strcpy(mod->head.y_label,"Y");
    else
    {
        strncpy(mod->head.y_label,y_lab, MOD_LABLEN);
        mod->head.y_label[MOD_LABLEN] = 0;
    }
	if (t_lab == NULL)
        strcpy(mod->head.t_label,"T");
    else
    {
        strncpy(mod->head.t_label,t_lab, MOD_LABLEN);
        mod->head.t_label[MOD_LABLEN] = 0;
    }
    strcpy(mod->head.z_label,"COUNTS");
    strcpy(mod->head.title, name);
	mod->head.labels_num = labels_num;
	mod->head.labels_size = labels_size;

	if (labels_num > 0)
	{
		lab = (char *)mod;
		lab += sizeof (struct mod_header3d);
		lab += sizeof (int)*labels_num;
		for (i=0;i<labels_num; i++)
		{
			mod->labels_offset[i] = lab-(char*)mod;
			strcpy(lab, labels[i]);
			lab += strlen(labels[i])+1;
		}
	}		
	mod->head.data_offset = sizeof (struct mod_header3d) + mod->head.labels_size;

    return mod;
}

char *id_get_label(void *p, int row)
{
	MOD_IMAGE3D *mod   = (MOD_IMAGE3D *) p;
	char *cp = (char *) p;
	int i;
	if (IMG_MOD_GET_TYPE(mod) == IMG_MOD_2D)
		return NULL;
	
	if (row < 0 || row >= mod->head.labels_num)
		return NULL;
	cp += mod->labels_offset[row];
	return cp;
}


int id_get_num_x(void *p)
{
	MOD_IMAGE   *mod   = (MOD_IMAGE *) p;
	MOD_IMAGE3D *mod3d   = (MOD_IMAGE3D *) p;

	if (IMG_MOD_GET_TYPE(mod) == IMG_MOD_2D)
		return mod->head.num_x;
	else
		return mod3d->head.num_x;
}
int id_get_num_y(void *p)
{
	MOD_IMAGE   *mod   = (MOD_IMAGE *) p;
	MOD_IMAGE3D *mod3d   = (MOD_IMAGE3D *) p;

	if (IMG_MOD_GET_TYPE(mod) == IMG_MOD_2D)
		return mod->head.num_y;
	else
		return mod3d->head.num_y;
}
int id_get_num_t(void *p)
{
	MOD_IMAGE   *mod   = (MOD_IMAGE *) p;
	MOD_IMAGE3D *mod3d   = (MOD_IMAGE3D *) p;

	if (IMG_MOD_GET_TYPE(mod) == IMG_MOD_2D)
		return 1;
	else
		return mod3d->head.num_t;
}
int id_get_data_type(void *p)
{
	MOD_IMAGE   *mod   = (MOD_IMAGE *) p;
	MOD_IMAGE3D *mod3d   = (MOD_IMAGE3D *) p;

	if (IMG_MOD_GET_TYPE(mod) == IMG_MOD_2D)
		return mod->head.data_type;
	else
		return mod3d->head.data_type;
}

u_int32 *id_get_ptr(void *p, int x, int y, int t)
{
	MOD_IMAGE   *mod   = (MOD_IMAGE *) p;
	MOD_IMAGE3D *mod3d = (MOD_IMAGE3D *) p;
	int elem_size;
	char *cp;

	switch (mod->head.data_type)
	{
	case DATA_LONG:
		elem_size = 4;
		break;

	case DATA_SHORT:
	case DATA_SHORT_DIG1:
	case DATA_SHORT_DIG2:
		elem_size = 2;
		break;

	case DATA_FLOAT:
		elem_size = 4;
		break;

	case DATA_DOUBLE:
		elem_size = 8;
		break;

	case DATA_FIXED8_DIG1:	
	case DATA_FIXED8_DIG2:	
	case DATA_FIXED8_DIG3:	
	case DATA_FIXED8_DIG4:	
	case DATA_FIXED8_DIG5:	
	case DATA_FIXED8_DIG6:	
	case DATA_FIXED8_DIG7:	
	case DATA_FIXED8_DIG8:
		elem_size = 4;
		break;

	default:
		printf("id_get_ptr:Unknown data type %d \n", mod->head.data_type);
		return NULL;
	}

	if (IMG_MOD_GET_TYPE(mod) == IMG_MOD_2D)
	{
		cp = (char *) mod->data;
		cp += elem_size *(x+mod->head.num_x*(size_t)y);
	}
	else
	{
		cp = (char *) mod3d;
		cp += mod3d->head.data_offset;
		cp += elem_size *(x+(size_t)y*mod3d->head.num_x + (size_t)t*mod3d->head.num_x*mod3d->head.num_y);
	}
	return (u_int32 *)cp;
}

u_int32 *id_get_ptr_safe(void *p, int x, int y, int t)
{
	MOD_IMAGE   *mod   = (MOD_IMAGE *) p;
	MOD_IMAGE3D *mod3d = (MOD_IMAGE3D *) p;
	int elem_size;
	char *cp;

	switch (mod->head.data_type)
	{
	case DATA_LONG:
		elem_size = 4;
		break;

	case DATA_SHORT:
	case DATA_SHORT_DIG1:
	case DATA_SHORT_DIG2:
		elem_size = 2;
		break;

	case DATA_FLOAT:
		elem_size = 4;
		break;

	case DATA_DOUBLE:
		elem_size = 8;
		break;

	case DATA_FIXED8_DIG1:	
	case DATA_FIXED8_DIG2:	
	case DATA_FIXED8_DIG3:	
	case DATA_FIXED8_DIG4:	
	case DATA_FIXED8_DIG5:	
	case DATA_FIXED8_DIG6:	
	case DATA_FIXED8_DIG7:	
	case DATA_FIXED8_DIG8:
		elem_size = 4;
		break;

	default:
		printf("id_get_ptr:Unknown data type %d \n", mod->head.data_type);
		return NULL;
	}

	if (IMG_MOD_GET_TYPE(mod) == IMG_MOD_2D)
	{
		if (x <0 ||  x >= mod->head.num_x || y <0 || y >= mod->head.num_y)
		{
			fprintf(stderr, "*** id_get_ptr_safe(%d, %d) does not fit in module (%d, %d)\n", x, y, mod->head.num_x, mod->head.num_y);
			exit(-1);
		}
		cp = (char *) mod->data;
		cp += elem_size *(x+mod->head.num_x*(size_t)y);
	}
	else
	{
		if (x <0 ||  x >= mod3d->head.num_x || y <0 || y >= mod3d->head.num_y || t <0 || t >= mod3d->head.num_t)
		{
			fprintf(stderr, "*** id_get_ptr_safe(%d, %d, %d) does not fit in module (%d, %d, %d)\n", 
						x, y, t, mod3d->head.num_x, mod3d->head.num_y, mod3d->head.num_t);
			exit(-1);
		}
		cp = (char *) mod3d;
		cp += mod3d->head.data_offset;
		cp += elem_size *(x+(size_t)y*mod3d->head.num_x + (size_t)t*mod3d->head.num_x*mod3d->head.num_y);
	}
	return (u_int32 *)cp;
}

int id_clear_mod(void *p)
{
	MOD_IMAGE   *mod   = (MOD_IMAGE *) p;
	MOD_IMAGE3D *mod3d = (MOD_IMAGE3D *) p;
	char *cp;
	size_t i, size;
	u_int16 *p16;
	u_int32 *p32;
	float *fp;
	double *dp;

	if (IMG_MOD_GET_TYPE(mod) == IMG_MOD_2D)
	{
		cp = (char *) mod->data;
		size = (size_t)(mod->head.num_x) * mod->head.num_y;
	}
	else
	{
		cp = (char *) mod3d;
		cp += mod3d->head.data_offset;
		size = (size_t)(mod3d->head.num_x) * mod3d->head.num_y * mod3d->head.num_t;
//		printf("size=%zx\n", size);
	}
	switch (mod->head.data_type)
	{
	case DATA_LONG:
	case DATA_FIXED8_DIG1:	
	case DATA_FIXED8_DIG2:	
	case DATA_FIXED8_DIG3:	
	case DATA_FIXED8_DIG4:	
	case DATA_FIXED8_DIG5:	
	case DATA_FIXED8_DIG6:	
	case DATA_FIXED8_DIG7:	
	case DATA_FIXED8_DIG8:
		p32 = (u_int32 *) cp;
		for (i=0; i<size; i++)
			*p32++ = 0;
		break;

	case DATA_SHORT:
	case DATA_SHORT_DIG1:
	case DATA_SHORT_DIG2:
		p16 = (u_int16 *) cp;
		for (i=0; i<size; i++)
			*p16++ = 0;
		break;

	case DATA_FLOAT:
		fp = (float *) cp;
		for (i=0; i<size; i++)
			*fp++ = 0.0;
		break;

	case DATA_DOUBLE:
		dp = (double *) cp;
		for (i=0; i<size; i++)
			*dp++ = 0.0;
		break;

	default:
		printf("Unknown data type %d \n", mod->head.data_type);
		return -1;
	}
	return 0;
}

int id_copy_mod(void *s, void *d)
{
	MOD_IMAGE   *src   = (MOD_IMAGE *) s;
	MOD_IMAGE3D *src3d = (MOD_IMAGE3D *) s;
	MOD_IMAGE   *dst   = (MOD_IMAGE *) d;
	MOD_IMAGE3D *dst3d = (MOD_IMAGE3D *) d;
	char *cp, *dp;

	long i, size;
	u_int16 *s16, *d16;
	u_int32 *s32, *d32;
	float *sf, *df;
	double *sd, *dd;

	if (IMG_MOD_GET_TYPE(src) != IMG_MOD_GET_TYPE(dst))
	{
		printf("Modules are different type %d and %d\n", IMG_MOD_GET_TYPE(src), IMG_MOD_GET_TYPE(dst));
		return -1;
	}
	if (IMG_MOD_GET_TYPE(src) == IMG_MOD_2D)
	{
		cp = (char *) src->data;
		dp = (char *) dst->data;
		size = src->head.num_x * src->head.num_y;
		if (src->head.num_x != dst->head.num_x ||  src->head.num_y != dst->head.num_y)
		{
			printf("Modules are different sizes (%d, %d) and (%d, %d)\n", src->head.num_x, src->head.num_y, dst->head.num_x, dst->head.num_y);
			return -1;
		}
	}
	else
	{
		cp = (char *) src3d;
		cp += src3d->head.data_offset;
		dp = (char *) dst3d;
		dp += dst3d->head.data_offset;
		size = src3d->head.num_x * src3d->head.num_y * src3d->head.num_t;
		if (src3d->head.num_x != dst3d->head.num_x ||  src3d->head.num_y != dst3d->head.num_y || src3d->head.num_t != dst3d->head.num_t)
		{
			printf("Modules are different sizes (%d, %d, %d) and (%d, %d, %d)\n", src3d->head.num_x, src3d->head.num_t, src3d->head.num_t, dst3d->head.num_x, dst3d->head.num_y, dst3d->head.num_t);
			return -1;
		}
	}
	switch (src->head.data_type)
	{
	case DATA_LONG:
	case DATA_FIXED8_DIG1:	
	case DATA_FIXED8_DIG2:	
	case DATA_FIXED8_DIG3:	
	case DATA_FIXED8_DIG4:	
	case DATA_FIXED8_DIG5:	
	case DATA_FIXED8_DIG6:	
	case DATA_FIXED8_DIG7:	
	case DATA_FIXED8_DIG8:
		s32 = (u_int32 *) cp;
		d32 = (u_int32 *) dp;
		for (i=0; i<size; i++)
			*d32++ = *s32++;
		break;

	case DATA_SHORT:
	case DATA_SHORT_DIG1:
	case DATA_SHORT_DIG2:
		s16 = (u_int16 *) cp;
		d16 = (u_int16 *) dp;
		for (i=0; i<size; i++)
			*d16++ = *s16++;
		break;

	case DATA_FLOAT:
		sf = (float *) cp;
		df = (float *) dp;
		for (i=0; i<size; i++)
			*df++ = *sf++;
		break;

	case DATA_DOUBLE:
		sd = (double *) cp;
		dd = (double *) dp;
		for (i=0; i<size; i++)
			*dd++ = *sd++;
		break;

	default:
		printf("Unknown data type %d \n", src->head.data_type);
		return -1;
	}
	return 0;
}

int id_mod_get_shape(void *m, int *num_x, int *num_y, int *num_t)
{
	MOD_IMAGE   *mod   = (MOD_IMAGE *) m;
	MOD_IMAGE3D *mod3d = (MOD_IMAGE3D *) m;
	int elem_size;

	if (IMG_MOD_GET_TYPE(mod) == IMG_MOD_2D)
	{
		if (num_x != NULL)
			*num_x = mod->head.num_x;
		if (num_y != NULL)
			*num_y = mod->head.num_y;
		if (num_t != NULL)
			*num_t = 1;
	}
	else
	{
		if (num_x != NULL)
			*num_x = mod3d->head.num_x;
		if (num_y != NULL)
			*num_y = mod3d->head.num_y;
		if (num_t != NULL)
			*num_t = mod3d->head.num_t;
	}
	switch (mod->head.data_type)
	{
	case DATA_LONG:
		elem_size = 4;
		break;

	case DATA_SHORT:
	case DATA_SHORT_DIG1:
	case DATA_SHORT_DIG2:
		elem_size = 2;
		break;

	case DATA_FLOAT:
		elem_size = 4;
		break;

	case DATA_DOUBLE:
		elem_size = 8;
		break;

	case DATA_FIXED8_DIG1:	
	case DATA_FIXED8_DIG2:	
	case DATA_FIXED8_DIG3:	
	case DATA_FIXED8_DIG4:	
	case DATA_FIXED8_DIG5:	
	case DATA_FIXED8_DIG6:	
	case DATA_FIXED8_DIG7:	
	case DATA_FIXED8_DIG8:
		elem_size = 4;
		break;

	default:
		printf("Unknown data type %d \n", mod->head.data_type);
		return 4;
	}

	return elem_size;
}
#ifdef __LINUX__
int id_mod_resize(MOD_IMAGE *mod, int num_x, int num_y, int num_t)
{
	MOD_IMAGE3D *mod3d=(MOD_IMAGE3D *)mod;
	size_t required_size;
	size_t elem_size=4;
	int data_type = mod->head.data_type;			

	if (data_type == DATA_LONG)
		elem_size = 4;
	else if (data_type == DATA_SHORT)
		elem_size = 2;
	else if (data_type == DATA_FLOAT)
		elem_size = 4;
	else if (data_type == DATA_DOUBLE)
		elem_size = 8;
	else if (data_type >= DATA_SHORT_DIG1 && data_type <= DATA_SHORT_DIG2)
		elem_size = 2;
	else if (data_type >= DATA_FIXED8_DIG1 && data_type <= DATA_FIXED8_DIG8)
		elem_size = 4;

	if (IMG_MOD_GET_TYPE(mod) == IMG_MOD_3D)
	{
		required_size = (size_t)num_x*(size_t)num_y*(size_t)num_t*(size_t)elem_size+mod3d->head.data_offset;
		if (datamod_size(mod) < required_size)
		{
			printf("Data module '%s' size (%d, %d, %d) too small to resize to (%d, %d, %d)\n", mod3d->head.title, mod3d->head.num_x, mod3d->head.num_y, mod3d->head.num_t, num_x, num_y, num_t);
			return -1;
		}
		mod3d->head.num_x = num_x;
		mod3d->head.num_y = num_y;
		mod3d->head.num_t = num_t;
	}
	else
	{
		if (num_t > 1)
		{
			printf("Cannot resize a 2-d data module to 3-d (num_t > 1)\n");
			return -1;
		}
		required_size = (size_t)num_x*(size_t)num_y*(size_t)elem_size+sizeof(struct mod_header);
		if (datamod_size(mod) < required_size)
		{
			printf("Data module '%s' size (%d, %d) too small to resize to (%d, %d)\n", mod->head.title, mod->head.num_x, mod->head.num_y, num_x, num_y);
			return -1;
		}
		mod->head.num_x = num_x;
		mod->head.num_y = num_y;
	}
	return 0;
}
int _os_link2(const char *name, void **mod_head, void **mod, u_int16 *type_lang, u_int16 *att_rev)
{
	return _os_link(&name, mod_head, mod, type_lang, att_rev);
}

#endif
	
