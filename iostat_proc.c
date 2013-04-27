// =====================================================================================
// 
//       Filename:  iostat_proc.c
// 
//    Description:  
// 
//        Version:  1.0
//        Created:  03/29/2013 16:29:22
//       Revision:  none
//       Compiler:  gcc
// 
//         Author:  cl , chenli@chinanetcenter.com
//        Company:  chinanetcenter
// 
// =====================================================================================

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

#define MAX_NAMELEN  72
#define MAX_BUFLEN   256
#define MAX_DEV      16
#define SUCCESS      1
#define FAILURE      0
#define SECTOR_SIZE  512
#define SHIFT        10

#define S_VAL(m,n,p)	(((double) ((n) - (m))) / (p))
#define S_DIV(m,p)      ((double) (m) / (p))
#define SECT_TO_KB(m)   ((unsigned long )((m) * (SECTOR_SIZE)) >> SHIFT) 

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
const char file_path[] = "/sys/kernel/dk_iostat/";
const char file_name[] = "dk_iostat";
const char init_format[] = "%u%u%s";
const char full_format[] = "%u%u%s%lu%lu%lu%lu%lu%lu%lu%lu%lu%lu%lu%lu%lu%lu%lu%lu";
const char show_format[] = "%8s %8.2f %8.2f %8.2f %8.2f %8.2f %8.2f %8.2f %8.2f %8.2f %8.2f %8.2f %8.2f\n";
const char show_title[]  = "     dev     rd/s    rdm/s  rtp/kbs     wt/s    wtm/s  wtp/kbs  rsz/kbs  wsz/kbs   h_rate    await    svctm     util\n";

struct part_info info[MAX_DEV][2];

int dev_acct;
int curr;
int itv;
int res_acct;

void cal_part_info()
{
	struct part_info  *pre;
	struct part_info  *pos;
	
	int     i;
	double  await;
	double  nr_ios;
	double  rd_nr_ios;
	double  wt_nr_ios;
	double  util;
	double  svctm;
	double  rd_sz;
	double  wt_sz;
	double  nr_cache[2];
	double  hitrate;
	double  rd_thruput;
	double  wt_thruput;

	fprintf(stdout,show_title);
	for(i=0; i<dev_acct; ++i)
	{
		pos = &info[i][curr];
		pre = &info[i][!curr];

		rd_nr_ios = (pos->f_ios[0]-pre->f_ios[0]);
		wt_nr_ios = (pos->f_ios[1]-pre->f_ios[1]);
		nr_ios = rd_nr_ios + wt_nr_ios;
		await  = nr_ios ? ((pos->ticks[0]-pre->ticks[0])+
			(pos->ticks[1]-pre->ticks[1]))/nr_ios : 0.0;
		util   = S_VAL(pre->io_ticks,pos->io_ticks,itv*10);
		svctm  = nr_ios ? ((pos->io_ticks-pre->io_ticks)/nr_ios) : 0.0;

		rd_sz  = rd_nr_ios ? SECT_TO_KB(pos->f_sec[0]-pre->f_sec[0])/rd_nr_ios : 0.0;
		wt_sz  = wt_nr_ios ? SECT_TO_KB(pos->f_sec[1]-pre->f_sec[1])/wt_nr_ios : 0.0;
		
		rd_thruput  = SECT_TO_KB(pos->f_sec[0]-pre->f_sec[0]);
		wt_thruput  = SECT_TO_KB(pos->f_sec[1]-pre->f_sec[1]);

		nr_cache[0] = (double)(pos->p_count[0]-pre->p_count[0]);
		nr_cache[1] = (double)(pos->p_count[1]-pre->p_count[1]);
		hitrate     = nr_cache[0] ? nr_cache[1]*100/nr_cache[0] : 0.0;

		fprintf(stdout,show_format,pre->dev_name,

			S_VAL(pre->f_ios[0],pos->f_ios[0],itv),
			S_VAL(pre->merges[0],pos->merges[0],itv),
			S_DIV(rd_thruput,itv),

			S_VAL(pre->f_ios[1],pos->f_ios[1],itv),
			S_VAL(pre->merges[1],pos->merges[1],itv),
			S_DIV(wt_thruput,itv),

			rd_sz,wt_sz,hitrate,await,svctm,util);
	}
}

void get_part_info(const char *name,const int curr)
{
	int   i;
	int   res;
	int   major;
	int   minor;
	FILE  *file;
	char  buf[MAX_BUFLEN];
	char  dev_name[MAX_NAMELEN];
	struct part_info *tmp;

	tmp = NULL;
	memset(dev_name,0,MAX_NAMELEN);
	memset(buf,0,MAX_BUFLEN);
	
	file = fopen(name,"r"); 

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
	fclose(file);
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

void alarm_handler(int signo)
{
	alarm(itv);
	return;
}
int main(int args , char **argv)
{
	itv = 5;
	res_acct = 0;

	switch(args)
	{
		case 1:
			break;
		case 3:
			if(argv[1][1] == 't')
				itv = atoi(argv[2]);
			else if(argv[1][1] == 'n')
				res_acct = atoi(argv[2]);
			else
				return 0;
			break;
		case 5:
			if(argv[1][1] == 't')
			{
				itv = atoi(argv[2]);
				if(argv[3][1] == 'n')
					res_acct = atoi(argv[4]);
				else
					return 0;
				break;
			}
			else if(argv[1][1] == 'n')
			{
				res_acct =  atoi(argv[2]);
				if(argv[3][1] == 't')
					itv = atoi(argv[4]);
				else
					return 0;
			}
			else
				return 0;
			break;
		default:
			return 0;
	}
			
	char file_name_buf[MAX_NAMELEN];
	FILE *s_file;
	int  res;
	
	curr = 0;
	res  = 0;
	
	memset(file_name_buf,0,MAX_NAMELEN);
	memset(info,0,(sizeof(struct part_info) * MAX_DEV * 2));
	sprintf(file_name_buf,"%s%s",file_path,file_name);

	init_part_info(file_name_buf);

	get_part_info(file_name_buf, curr);

	signal(SIGALRM,alarm_handler);
	alarm(itv);
	while(res < res_acct)
	{
		curr = !curr;
		pause();
		get_part_info(file_name_buf, curr);
		cal_part_info(curr);
		++res;
	}
	return 0;

}
