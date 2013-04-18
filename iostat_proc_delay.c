/*
 *     Filename:  iostat_proc_delay.c
 *
 *  Description:  
 *
 *      Version:  1.0
 *      Created:  04/17/2013 06:15:34 PM
 *     Revision:  none
 *     Compiler:  gcc
 *
 *       Author:  R&D System Group Xiamen (xmsys), xmsys@chinanetcenter.com
 * Organization:  chinanetcenter
 */

#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<errno.h>
#include<sched.h>
#include<fcntl.h>
#include<stdlib.h>
#include<signal.h>

#include<sys/types.h>
#include<sys/stat.h>

#define MAX_NAMELEN   72
#define MAX_BUFLEN    256
#define MAX_DEV       16

#define S_VAL(m,n,p)  (((double) ((n) - (m))) /(p))

#pragma pack(2)
struct part_info {
	char dev_name[MAX_NAMELEN];
	unsigned int  major        ;
	unsigned int  minor        ;
	unsigned long f_sec[2] ;
	unsigned long f_ios[2]     ;
	unsigned long merges[2]    ;
	unsigned long ticks[2]     ;
	unsigned long r_ios[2]     ;
	unsigned long r_sec[2] ;
	unsigned long p_count[2]   ;
	unsigned long io_ticks     ;
	unsigned long time_in_queue;
};
#pragma pack()

const char source_path[] = "/home/cl/workplace/kprobe/iostats";
const char init_format[] = "%u%u%s";
const char full_format[] = "%u%u%s%lu%lu%lu%lu%lu%lu%lu%lu%lu%lu%lu%lu%lu%lu%lu%lu";
const char show_format[] = "%8d %8.2f %8.2f %8.2f %8.2f %8.2f %8.2f %8.2f %8.2f %8.2f %8.2f %8.2f %8.2f %8.2f %8.2f %8.2f %8.2f\n";
const char show_title[]  = "     seq     rd/s    rds/s    rdm/s    rdq/s   rdqs/s     wt/s    wts/s    wtm/s    wtq/s   wtqs/s    rd_sz    wt_sz   h_rate    await    svctm     util\n";

struct part_info info[MAX_DEV][2];

int dev_acct;
int itv;
int curr;
int seq;

void cal_part_info(int curr)
{
	struct part_info  *pre;
	struct part_info  *pos;
	
	int     i;
	FILE    *tmp;
	double  await;
	double  nr_ios;
	double  util;
	double  svctm;
	double  rd_sz;
	double  wt_sz;
	double  nr_cache[2];
	double  hitrate;
	for(i=0; i<dev_acct; ++i)
	{
		pos = &info[i][curr];
		pre = &info[i][!curr];

		nr_ios = (pos->f_ios[0]-pre->f_ios[0])+
			(pos->f_ios[1]-pre->f_ios[1]);
		await  = nr_ios ? ((pos->ticks[0]-pre->ticks[0])+
			(pos->ticks[1]-pre->ticks[1]))/nr_ios : 0.0;
		util   = S_VAL(pre->io_ticks,pos->io_ticks,itv*10);
		svctm  = nr_ios ? ((pos->io_ticks-pre->io_ticks)/nr_ios) : 0.0;

		rd_sz  = nr_ios ? (pos->f_sec[0]-pre->f_sec[0])/nr_ios : 0.0;
		wt_sz  = nr_ios ? (pos->f_sec[1]-pre->f_sec[1])/nr_ios : 0.0;
		
		nr_cache[0] = (double)(pos->p_count[0]-pre->p_count[0]);
		nr_cache[1] = (double)(pos->p_count[1]-pre->p_count[1]);
		hitrate     = nr_cache[0] ? nr_cache[1]*100/nr_cache[0] : 0.0;

		tmp = fopen(pre->dev_name ,"a");
		fprintf(tmp,show_format,seq,
			S_VAL(pre->f_ios[0],pos->f_ios[0],itv),
			S_VAL(pre->f_sec[0],pos->f_sec[0],itv),
			S_VAL(pre->merges[0],pos->merges[0],itv),
			S_VAL(pre->r_ios[0],pos->r_ios[0],itv),
			S_VAL(pre->r_sec[0],pos->r_sec[0],itv),

			S_VAL(pre->f_ios[1],pos->f_ios[1],itv),
			S_VAL(pre->f_sec[1],pos->f_sec[1],itv),
			S_VAL(pre->merges[1],pos->merges[1],itv),
			S_VAL(pre->r_ios[1],pos->r_ios[1],itv),
			S_VAL(pre->r_sec[1],pos->r_sec[1],itv),

			rd_sz,wt_sz,hitrate,await,svctm,util);
		fclose(tmp);
	}
	++seq;
}
int get_part_info(FILE *file,const int curr)
{
	int   i;
	int   res;
	int   major;
	int   minor;
	char  buf[MAX_BUFLEN];
	char  dev_name[MAX_NAMELEN];
	struct part_info *tmp;

	tmp = NULL;
	memset(dev_name,0,MAX_NAMELEN);
	memset(buf,0,MAX_BUFLEN);

	for(i=0; (i<dev_acct)&&(fgets(buf,MAX_BUFLEN,file)!=NULL); ++i)
	{
		res = sscanf(buf, init_format, &major, &minor, dev_name);
		if(res != 3)
		{
			--i;
			continue;
		}
		tmp = &info[i][curr];

		if((tmp->major == major) && (tmp->minor == minor) &&
				!strcmp(tmp->dev_name,dev_name))
		{
			res = sscanf(buf,full_format,&(tmp->major),
				&(tmp->minor),tmp->dev_name,
				&(tmp->f_ios[0]),&(tmp->merges[0]),
				&(tmp->f_sec[0]),&(tmp->ticks[0]),
				&(tmp->f_ios[1]),&(tmp->merges[1]),
				&(tmp->f_sec[1]),&(tmp->ticks[1]),
				&(tmp->io_ticks),&(tmp->time_in_queue),
				&(tmp->r_ios[0]),&(tmp->r_sec[0]),
				&(tmp->r_ios[1]),&(tmp->r_sec[1]),
				&(tmp->p_count[0]),&(tmp->p_count[1]));
		}
	}
	if(i == dev_acct)
		return 1;
	return 0;
}

void init_part_info(const char *file)
{
	FILE   *ptr;
	FILE   *pfile;
	int    res;
	int    major;
	int    minor;
	char   dev_name[MAX_NAMELEN];
	char   buf[MAX_BUFLEN];
	struct part_info *tmp;

	memset(dev_name, 0, MAX_NAMELEN);
	memset(buf, 0, MAX_BUFLEN);
	
	tmp = NULL;
	ptr = fopen(file,"r");
	if(ptr == NULL)
	{
		fprintf(stderr,"%s:%d %s\n",file,errno,strerror(errno));
		return;
	}

	dev_acct = 0;
	while(fgets(buf, MAX_BUFLEN, ptr) != NULL)
	{
		res = sscanf(buf,init_format,&major,&minor,dev_name);
		if(res != 3)
			continue;
		if(major == info[0][0].major && minor == info[0][0].minor &&
				!strcmp(dev_name,info[0][0].dev_name))
		{
			fclose(ptr);
			return;
		}

		pfile = fopen(dev_name, "w");
		fprintf(pfile,"%s",show_title);
		fclose(pfile);

		tmp = &info[dev_acct][0];
		tmp->major = major;
		tmp->minor = minor;
		memcpy(tmp->dev_name,dev_name,MAX_NAMELEN);
		tmp = &info[dev_acct][1];
		tmp->major = major;
		tmp->minor = minor;
		memcpy(tmp->dev_name,dev_name,MAX_NAMELEN);
		dev_acct++;
		if(dev_acct == MAX_DEV)
			break;
	}
	fclose(ptr);
}
int main(int args, char **argv)
{
	FILE *s_file;
	int  res;

	curr = 0;
	itv  = 180;
	seq  = 0;
	memset(info,0,(sizeof(struct part_info) * MAX_DEV * 2));
	init_part_info(source_path);

	s_file = fopen(source_path,"r");
	res = get_part_info(s_file, curr);
	while(1)
	{
		curr = !curr;
		res = get_part_info(s_file, curr);
		if(!res)
			break;
		cal_part_info(curr);
	}
	fclose(s_file);
	return 0;
}


