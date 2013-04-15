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

#define MAX_NAMELEN 72
#define MAX_BUFLEN   256
#define MAX_DEV      16
#define SUCCESS      1
#define FAILURE      0

#define S_VAL(m,n,p)	(((double) ((n) - (m))) / (p))

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
const char result_path[] = "/home/cl/";
const char result_name[] = "iostats";
const char init_format[] = "%u%u%s";
const char full_format[] = "%u%u%s%lu%lu%lu%lu%lu%lu%lu\
			    %lu%lu%lu%lu%lu%lu%lu%lu%lu";
const char show_format[] = "%8s%8.2f%8.2f%8.2f%8.2f%8.2f%8.2f%8.2f%8.2f%8.2f%8.2f%8.2f%8.2f%8.2f%8.2f\n";

struct part_info info[MAX_DEV][2];

int dev_count;
int res_count;
int curr;
long  itv;

void show_part_info()
{
	int i;
	struct part_info *pre;
	struct part_info *pos;

	double await;
	double nr_ios;
	double util;
	double svctm;
	double rd_sz;
	double wt_sz;
	double nr_count[2];
	double hit_rate;
	fprintf(stdout,"     dev     rd/s    rds/s    rdm/s    rdq/s   rdqs/s"
			"     wt/s    wts/s    wtm/s    wtq/s   wtqs/s"
			"    rd_sz    wt_sz   h_rate    await    svctm     util\n");

	for(i=0;i<dev_count;++i)
	{
		pre = &info[i][!curr];
		pos = &info[i][curr];

		nr_ios = (pos->f_ios[0]-pre->f_ios[0])+
			(pos->f_ios[1]-pre->f_ios[1]);
		await  = nr_ios ? ((pos->ticks[0]-pre->ticks[0])+
			(pos->ticks[1]-pre->ticks[1]))/nr_ios : 0.0;
		util   = S_VAL(pre->io_ticks,pos->io_ticks,itv*10);
		svctm  = nr_ios ? ((pos->io_ticks-pre->io_ticks)/nr_ios) : 0.0;

		rd_sz  = nr_ios ? (pos->f_sec[0]-pre->f_sec[0])/nr_ios : 0.0;
		wt_sz  = nr_ios ? (pos->f_sec[1]-pre->f_sec[1])/nr_ios : 0.0;
		
		nr_count[0] = (double)(pos->p_count[0]-pre->p_count[0]);
		nr_count[1] = (double)(pos->p_count[1]-pre->p_count[1]);
		hit_rate    = nr_count[0] ? nr_count[1]*100/nr_count[0] : 100.0;
#ifdef __DIRECT_SHOW__
		fprintf(stdout,	"%8s %8.2f %8.2f %8.2f %8.2f %8.2f "
				"%8.2f %8.2f %8.2f %8.2f %8.2f "
				"%8.2f %8.2f %8.2f %8.2f %8.2f %8.2f\n",
				pre->dev_name,
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

			rd_sz,wt_sz,hit_rate,await,svctm,util);
#endif
#ifdef __FILE_STORE__
		
		FILE *tmp;
		tmp = fopen(pre->dev_name,"a");
		fprintf(tmp,"%8s %8.2f %8.2f %8.2f %8.2f %8.2f "
				"%8.2f %8.2f %8.2f %8.2f %8.2f "
				"%8.2f %8.2f %8.2f %8.2f %8.2f %8.2f\n",
				pre->dev_name,
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

			rd_sz,wt_sz,hit_rate,await,svctm,util);
		fclose(tmp);
#endif
	}
	return;
}

void init_part_info(const char *file)
{
	FILE *ptr;
	int  res;
	int  major;
	int  minor;
	char dev_name[MAX_NAMELEN];
	char buf[MAX_BUFLEN];

	struct part_info *tmp;

	memset(dev_name,0,MAX_NAMELEN);
	memset(buf,0,MAX_BUFLEN);
	tmp  = NULL;
	curr = 0;

	ptr = fopen(file,"r");
	if(ptr == NULL)
	{
		fprintf(stderr,"%s:%d %s\n",file,errno,strerror(errno));
		return;
	}

	dev_count = 0;
	while(fgets(buf,MAX_BUFLEN,ptr) != NULL)
	{
		res = sscanf(buf,init_format,&major,&minor,dev_name);
		if(res != 3)
		{
			fprintf(stderr,"format wrong!\n");
			return;
		}
		tmp = &info[dev_count][curr];
		tmp->major = major;
		tmp->minor = minor;
		memcpy(tmp->dev_name,dev_name,MAX_NAMELEN);
		tmp = &info[dev_count][!curr];
		tmp->major = major;
		tmp->minor = minor;
		memcpy(tmp->dev_name,dev_name,MAX_NAMELEN);
		dev_count++;
		if(dev_count == MAX_DEV)
			break;
#ifdef __DEBUG__
		printf("%u %u %s\n",major,minor,dev_name);
		printf("%u %u %s\n",tmp->major,tmp->minor,tmp->dev_name);
#endif
	}
	fclose(ptr);
}

void get_part_info(const char *file)
{
	int  i;
	int  res;
	int  major;
	int  minor;
	int  begin;
	FILE *ptr;
	char *buf;
	char dev_name[MAX_NAMELEN];

	struct part_info *tmp;

	buf = malloc(MAX_BUFLEN);

	memset(dev_name,0,MAX_NAMELEN);
	memset(buf,0,MAX_BUFLEN);
	tmp   = NULL;
	curr  = 0;
	begin = 0;

repeat:
	ptr = fopen(file,"r");
	if(ptr == NULL)
	{
		fprintf(stderr,"%s:%d %s\n",file,errno,strerror(errno));
		free(buf);
		return;
	}

	memcpy(buf,ptr,10);
	for(i=0;(i<dev_count)&&(fgets(buf,MAX_BUFLEN,ptr)!=NULL);++i)
	{
		res = sscanf(buf,init_format,&major,&minor,dev_name);
		if(res != 3)
		{
			fprintf(stderr,"format wrong!\n");
			free(buf);
			return;
		}
		tmp = &info[i][curr];
#ifdef __DEBUG__
		printf("%u %u %s\n",major,minor,dev_name);
		printf("%u %u %s\n",tmp->major,tmp->minor,tmp->dev_name);
#endif
		if((tmp->major == major) &&(tmp->minor == minor) &&
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
		else
		{
			fprintf(stderr,"dev chagned!\n");
			free(buf);
			return;
		}
	}
	fclose(ptr);
	if(begin)
		show_part_info();
	curr = !curr;
	if(res_count == 0)
		return;
	--res_count;
	++begin;
	pause();
	goto repeat;
}



void alarm_handler(int signo)
{
	alarm(itv);
	return;
}
int main(int args , char **argv)
{
	itv = 5;
	res_count = 1;

	switch(args)
	{
		case 1:
			break;
		case 3:
			if(argv[1][1] == 't')
				itv = atoi(argv[2]);
			else if(argv[1][1] == 'n')
				res_count = atoi(argv[2]);
			else
				return 0;
			break;
		case 5:
			if(argv[1][1] == 't')
			{
				itv = atoi(argv[2]);
#ifdef __DEBUG__
				printf("%d----\n",itv);
#endif
				if(argv[3][1] == 'n')
					res_count = atoi(argv[4]);
				else
					return 0;
				break;
			}
			else if(argv[1][1] == 'n')
			{
				res_count =  atoi(argv[2]);
				if(argv[3][1] == 't')
					itv = atoi(argv[4]);
				else
					return 0;
#ifdef __DEBUG__
				printf("%d\n",itv);
#endif
			}
			else
				return 0;
			break;
		default:
			return 0;
	}
			
	char file_name_buf[MAX_NAMELEN];
	char res_name_buf[MAX_NAMELEN];
	
	memset(file_name_buf,0,MAX_NAMELEN);
	memset(res_name_buf,0,MAX_NAMELEN);
	memset(info,0,(sizeof(struct part_info)*MAX_DEV*2));

	sprintf(file_name_buf,"%s%s",file_path,file_name);
	sprintf(res_name_buf,"%s%s",result_path,result_name);

	dev_count = 0;
	init_part_info(file_name_buf);

	signal(SIGALRM,alarm_handler);

	alarm(itv);
	get_part_info(file_name_buf);
	return 0;
}
