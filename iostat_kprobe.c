// =====================================================================================
// 
//       Filename:  iostat_beta.c
// 
//    Description:  
// 
//        Version:  1.0
//        Created:  03/26/2013 10:07:22 AM
//       Revision:  none
//       Compiler:  gcc
// 
//         Author:  cl , chenli@chinanetcenter.com
//        Company:  chinanetcenter
// 
// =====================================================================================

#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/kprobes.h>
#include<linux/kallsyms.h>
#include<linux/kobject.h>
#include<linux/string.h>
#include<linux/sysfs.h>
#include<linux/bio.h>
#include<linux/version.h>
#include<linux/sched.h>

#if(LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36))
#include<linux/blk_types.h>
#endif

#include<linux/fs.h>
#include<linux/list.h>
#include<linux/device.h>
#include<linux/genhd.h>
#include<linux/ctype.h>
#include<linux/spinlock.h>
#include<linux/blkdev.h>

#include<linux/hash.h>

#define SHIFT         10
#define IOS           0x0000000000000001
#define MERGES        0x0000000000000002
#define TICKS         0x0000000000000004
#define SECTORS       0x0000000000000008
#define PCACHE        0x0000000000000010
#define RIOS          0x0000000000000020
#define RSECTORS      0x0000000000000040
#define IO_TICKS      0x0000000000000080
#define TIME_IN_QUEUE 0x0000000000000100
#define MAX_HASH_LIST (1UL << SHIFT)

static unsigned long  p_block_class               = 0;
static unsigned long  p_disk_type                 = 0;
static unsigned long  p___do_page_cache_readahead = 0;
static unsigned long  p_elv_bio_merged            = 0;
static unsigned long  p_blk_finish_request        = 0;
static unsigned long  p_elv_merge_requests        = 0;
static unsigned long  p_get_request_wait          = 0;

module_param(p_block_class,ulong,S_IRUGO);
module_param(p_disk_type,ulong,S_IRUGO);
module_param(p___do_page_cache_readahead,ulong,S_IRUGO);
module_param(p_elv_bio_merged,ulong,S_IRUGO);
module_param(p_blk_finish_request,ulong,S_IRUGO);
module_param(p_elv_merge_requests,ulong,S_IRUGO);
module_param(p_get_request_wait,ulong,S_IRUGO);

struct proc_info {
	struct list_head  list;
	char              comm[TASK_COMM_LEN];
	pid_t             pid;
	pid_t             tgid;
	unsigned long     ios[2];
	unsigned long     merges[2];
	/* useless */
	unsigned long     ticks[2];
	unsigned long     sectors[2];
	/* pcache[2] useless */
	unsigned long     pcache[3];
};
struct part_info {
	struct list_head  list;
	struct hd_struct  *part;
	struct part_info  *part0_info;
	struct gendisk    *disk;
	unsigned long     f_ios[2];
	unsigned long     f_merges[2];
	unsigned long     f_ticks[2];
	unsigned long     f_sectors[2];
	unsigned long     r_ios[2];
	unsigned long     r_sectors[2];
	unsigned long     p_cache[3];
	unsigned long     io_ticks;
	unsigned long     time_in_queue;
};
static struct list_head  proc_info_hlist[MAX_HASH_LIST];
static struct list_head  part_info_list;

static ssize_t dk_iostat_show(struct kobject *kobj,
			struct kobj_attribute *attr,char *buf)
{
	char *pos;
	int  res;
	char   name_buf[BDEVNAME_SIZE];
	struct part_info *tmp;
	pos = buf;
	res = 0;
	list_for_each_entry(tmp,&part_info_list,list)
	{
		res += sprintf(pos,"%4d %4d %s %lu %lu %lu "
			"%u %lu %lu %lu %u %u %u "
			"%lu %lu %lu %lu %lu %lu %lu \n",
			MAJOR(part_devt(tmp->part)),
			MINOR(part_devt(tmp->part)),
			disk_name(tmp->disk, tmp->part->partno, name_buf),
			tmp->f_ios[0],tmp->f_merges[0],
			tmp->f_sectors[0],
			jiffies_to_msecs(tmp->f_ticks[0]),
			tmp->f_ios[1],tmp->f_merges[1],
			tmp->f_sectors[1],
			jiffies_to_msecs(tmp->f_ticks[1]),
			jiffies_to_msecs(tmp->io_ticks),
			jiffies_to_msecs(tmp->time_in_queue),
			tmp->r_ios[0],tmp->r_sectors[0],
			tmp->r_ios[1],tmp->r_sectors[1],
			tmp->p_cache[0],tmp->p_cache[1],
			tmp->p_cache[2]
			);
		pos = buf + res;
	}
	return res;
}

static struct kobj_attribute dk_iostat_attribute =
	__ATTR(dk_iostat,0444,dk_iostat_show,NULL);

static ssize_t proc_iostat_show(struct kobject *kobj,
			struct kobj_attribute *attr,char *buf)
{
	char              *pos;
	int               res;
	int               acct;
	struct list_head  *hlist;
	struct list_head  *tmp;
	struct proc_info  *info;

	pos = buf;
	res = 0;
	hlist = proc_info_hlist;
	for(acct=0;acct<MAX_HASH_LIST;++acct)
	{
		tmp = hlist + acct;
		if(!list_empty(tmp))
			list_for_each_entry(info,tmp,list)
			{
				res += sprintf(pos,"%8d %16s(%8d):R:%8lu W:%8lu MR:%8lu MW:%8lu "
						"SR:%8lu SW:%8lu PA:%8lu PH:%8lu PR:%8lu \n",
						info->pid,
						info->comm,info->tgid,
						info->ios[0],info->ios[1],
						info->merges[0],info->merges[1],
						info->sectors[0],info->sectors[1],
						info->pcache[0],info->pcache[1],
						info->pcache[2]
						);
				pos = buf + res;
			}

	}
	if(res >> 12 != 0)
		return 4098;
	return res;
}

static struct kobj_attribute proc_iostat_attribute =
	__ATTR(proc_iostat,0444,proc_iostat_show,NULL);

static struct attribute *attrs[] = {
	&dk_iostat_attribute.attr,
	&proc_iostat_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *dk_iostat_kobj;

struct find_get_page_data {
	struct part_info *info;
};
struct find_get_pages_contig_data {
	struct part_info *info;
};
struct __do_page_cache_readahead_data {
	struct part_info *info;
};
struct elv_merge_requests_data {
	struct hd_struct *part;
	struct part_info *info;
};
struct get_request_wait_data {
	struct bio *bio;
};

static inline int blk_do_io_stat(struct request *rq)
{
	return rq->rq_disk &&
	       (rq->cmd_flags & REQ_IO_STAT) &&
	       (rq->cmd_type == REQ_TYPE_FS ||
	        (rq->cmd_flags & REQ_DISCARD));
}

static inline struct hd_struct *mapping_to_part(const struct address_space *mapping)
{
	struct block_device  *bdev;

	if(unlikely(mapping == NULL))
		return NULL;
	if(unlikely(mapping->host == NULL))
		return NULL;
	if(unlikely(mapping->host->i_sb == NULL))
		return NULL;
	bdev = mapping->host->i_sb->s_bdev;
	if(unlikely(bdev == NULL))
		return NULL;
	return bdev->bd_part;
}

static void part_info_free(struct list_head *plist)
{
	struct part_info   *tmp;
	
	if(!list_empty(plist))
		list_for_each_entry(tmp,plist,list)
			kfree(tmp);
	return;
}

static inline struct part_info *list_for_part(const struct list_head *plist,
				const struct hd_struct *part)
{
	struct part_info *info;

	if(list_empty(plist))
		return NULL;
	list_for_each_entry(info,plist,list)
	{
		if(info->part == part)
			return info;
	}
	return NULL;
}

static inline void __part_stat_acct(struct part_info *info,unsigned int rw,
			unsigned long val, unsigned long flag)
{
	switch(flag)
	{
		case IOS:
			info->f_ios[rw]     += val;
			break;
		case MERGES:
			info->f_merges[rw]  += val;
			break;
		case TICKS:
			info->f_ticks[rw]   += val;
			break;
		case SECTORS:
			info->f_sectors[rw] += val;
			break;
		case RIOS:
			info->r_ios[rw]     += val;
			break;
		case RSECTORS:
			info->r_sectors[rw] += val;
			break;
		case PCACHE:
			info->p_cache[rw]   += val;
			break;
		case IO_TICKS:
			info->io_ticks      += val;
			break;
		case TIME_IN_QUEUE:
			info->time_in_queue += val;
			break;
		default:
			break;
	}
	return;
}
static void part_stat_acct(struct part_info *info, unsigned int rw,
			unsigned long val, unsigned long flag)
{
	if(info->part0_info != NULL)
		__part_stat_acct(info->part0_info,rw,val,flag);
	__part_stat_acct(info,rw,val,flag);
	return;
}

#ifdef __PROC_INFO_ACCT__
static void proc_info_free(struct list_head *list, unsigned long list_len)
{
	unsigned long       i;
	struct list_head    *tmp;
	struct proc_info    *info;

	for(i=0; i<list_len; ++i)
	{
		tmp = list+i;
		if(!list_empty(tmp))
			list_for_each_entry(info,tmp,list)
			{
#ifdef __SHOW__
				printk("%8d %16s(%8d):R:%8lu W:%8lu MR:%8lu MW:%8lu "
					"SR:%8lu SW:%8lu PA:%8lu PH:%8lu \n",
						info->pid,
						info->comm,info->tgid,
						info->ios[0],info->ios[1],
						info->merges[0],info->merges[1],
						info->sectors[0],info->sectors[1],
						info->pcache[0],info->pcache[1]
						);
#endif
				kfree(info);
			}
	}
	return;
}
#else
static void proc_info_free(struct list_head *list, unsigned long list_len)
{
	return;
}
#endif

static inline void init_proc_info(struct proc_info *info)
{
	memset(info,0,sizeof(struct proc_info));
	INIT_LIST_HEAD(&info->list);
	info->tgid = task_tgid_nr(current);
	info->pid  = task_pid_nr(current);
	memcpy(info->comm,current->comm,TASK_COMM_LEN);
	return;
}
static inline void copy_task_comm(char *dst, char *src)
{
	if(memcmp(dst,src,TASK_COMM_LEN))
		memcpy(dst,src,TASK_COMM_LEN);
	return;
}

static inline void __proc_stat_acct(struct proc_info *info,unsigned int rw,
		unsigned long val,unsigned long flag)
{
	switch(flag)
	{
		case IOS:
			info->ios[rw]     += val;	
			break;
		case MERGES:
			info->merges[rw]  += val;
			break;
		case TICKS:
			info->ticks[rw]   += val;
			break;
		case SECTORS:
			info->sectors[rw] += val;
			break;
		case PCACHE:
			info->pcache[rw]  += val;
			break;
		default:
			return;
	}
	copy_task_comm(info->comm,current->comm);
	return;
}

#ifdef __PROC_INFO_ACCT__
static void proc_stat_acct(struct list_head *hash_list,unsigned int rw,
		unsigned long val,unsigned long flag)
{
	pid_t            tgid;
	unsigned long    tmp;
	struct proc_info *info;
	tgid = task_tgid_nr(current);
	tmp  = hash_long(tgid,SHIFT);
	if(!list_empty(&hash_list[tmp]))
		list_for_each_entry(info,&hash_list[tmp],list)
		{
			if(info->tgid == tgid)
			{
				__proc_stat_acct(info,rw,val,flag);
				return;
			}
		}
	info = kmalloc(sizeof(struct proc_info),GFP_ATOMIC);
	if(info == NULL)
		return;
	init_proc_info(info);
	list_add_tail(&info->list,&hash_list[tmp]);
	__proc_stat_acct(info,rw,val,flag);
}
#else
static void proc_stat_acct(struct list_head *hash_list,unsigned int rw,
		unsigned long val,unsigned long flag)
{
	return;
}
#endif


/* 查找连续的页面缓存 */
static int find_get_pages_contig_pre_ret_handler(struct kretprobe_instance *ri,
			struct pt_regs *regs)
{
	unsigned int                      nr_pages;
	struct address_space              *mapping;
	struct hd_struct                  *part;
	struct part_info                  *info;
	struct find_get_pages_contig_data *data;
#ifndef __KERNEL__
	mapping  = (struct address_space *)(regs->rdi);
	nr_pages = (unsigned int)(regs->rsi);
#else
	mapping = (struct address_space *)(regs->di);
	nr_pages = (unsigned int)(regs->si);
#endif
	data = (struct find_get_pages_contig_data *)ri->data;
	part = mapping_to_part(mapping);
	info = list_for_part(&part_info_list,part);
	if(info != NULL)
	{
		proc_stat_acct(proc_info_hlist,0,nr_pages,PCACHE);
		part_stat_acct(info,0,nr_pages,PCACHE);
		data->info = info;
		return 0;
	}	

	data->info = NULL;
	return 0;
}

static int find_get_pages_contig_pos_ret_handler(struct kretprobe_instance *ri,
				struct pt_regs *regs)
{
	unsigned int                      nr_pages;
	struct part_info                  *info;
	struct find_get_pages_contig_data *data;
#ifndef __KERNEL__
	nr_pages = (unsigned int)(regs->rax);
#else
	nr_pages = (unsigned int)(regs->ax);
#endif
	data = (struct find_get_pages_contig_data *)ri->data;
	if(data->info != NULL)
	{
		proc_stat_acct(proc_info_hlist,1,nr_pages,PCACHE);
		info = data->info;
		part_stat_acct(info,1,nr_pages,PCACHE);
		data->info = NULL;
		return 0;
	}

	data->info = NULL;
	return 0;
}

/* 查找单个页面缓存 */
static int find_get_page_pre_ret_handler(struct kretprobe_instance *ri,
			struct pt_regs *regs)
{
	struct address_space      *mapping;
	struct hd_struct          *part;
	struct part_info          *info;
	struct find_get_page_data *data;
#ifndef __KERNEL__
	mapping = (struct address_space *)(regs->rdi);
#else
	mapping = (struct address_space *)(regs->di);
#endif
	data = (struct find_get_page_data *)ri->data;
	part = mapping_to_part(mapping);
	info = list_for_part(&part_info_list,part);
	if(info != NULL)
	{
		proc_stat_acct(proc_info_hlist,0,1,PCACHE);
		part_stat_acct(info,0,1,PCACHE);
		data->info = info;
		return 0;
	}

	data->info = NULL;
	return 0;
}

static int find_get_page_pos_ret_handler(struct kretprobe_instance *ri,
			struct pt_regs *regs)
{
	struct page               *page;
	struct part_info          *info;
	struct find_get_page_data *data;

#ifndef __KERNEL__
	page = (struct page *)(regs->rax);
#else
	page = (struct page *)(regs->ax);
#endif
	data = (struct find_get_page_data *)ri->data;
	if(page != NULL && data->info != NULL)
	{
		proc_stat_acct(proc_info_hlist,1,1,PCACHE);
		info = data->info;
		part_stat_acct(info,1,1,PCACHE);
		data->info = NULL;
		return 0;
	}

	data->info = NULL;
	return 0;
}

/* 提交bio函数，用来统计bio提交次数和扇区数 */
static int submit_bio_pre_probe(struct kprobe *probe,
			struct pt_regs *regs)
{
	int rw;
	struct bio *bio;
	struct part_info *info;
	struct hd_struct *bi_part;
#ifndef __KERNEL__
	bio = (struct bio *)(regs->rsi);
	rw  = regs->rdi;
#else
	bio = (struct bio *)(regs->si);
	rw  = regs->di;
#endif
	if(bio_has_data(bio) && !(rw & REQ_DISCARD))
	{
		bi_part = bio->bi_bdev->bd_part;
		info = list_for_part(&part_info_list,bi_part);
		if(info != NULL)
		{
			if(rw & WRITE)
			{
				proc_stat_acct(proc_info_hlist,1,1,IOS);
				proc_stat_acct(proc_info_hlist,1,bio_sectors(bio),SECTORS);
				part_stat_acct(info,1,1,RIOS);
				part_stat_acct(info,1,bio_sectors(bio),RSECTORS);
			}
			else
			{
				proc_stat_acct(proc_info_hlist,0,1,IOS);
				proc_stat_acct(proc_info_hlist,0,bio_sectors(bio),SECTORS);
				part_stat_acct(info,0,1,RIOS);
				part_stat_acct(info,0,bio_sectors(bio),RSECTORS);
			}
		}
	}

	return 0;
}

static void submit_bio_post_probe(struct kprobe *probe,
			struct pt_regs *regs,unsigned long flags)
{
	return;
}

/* 文件预读函数，用来统计预读页数 */
static int __do_page_cache_readahead_pre_ret_handler(struct kretprobe_instance *ri,
			struct pt_regs *regs)
{
	struct address_space *mapping;
	struct hd_struct     *part;
	struct part_info     *info;

	struct __do_page_cache_readahead_data *data;
#ifndef __KERNEL__
	mapping = (struct address_space *)(regs->rdi);
#else
	mapping = (struct address_space *)(regs->di);
#endif
	data = (struct __do_page_cache_readahead_data *)ri->data;
	part = mapping_to_part(mapping);
	info = list_for_part(&part_info_list,part);
	if(info != NULL)
	{
		data->info = info;
		return 0;
	}

	data->info = NULL;
	return 0;
}

static int __do_page_cache_readahead_pos_ret_handler(struct kretprobe_instance *ri,
			struct pt_regs *regs)
{
	unsigned long    nr_pages;
	struct part_info *info;
	
	struct __do_page_cache_readahead_data *data;
#ifndef __KERNEL__
	nr_pages = (unsigned long)(regs->rax);
#else
	nr_pages = (unsigned long)(regs->ax);
#endif
	data = (struct __do_page_cache_readahead_data *)ri->data;
	info = data->info;
	if(info != NULL)
	{
		part_stat_acct(info,2,nr_pages,PCACHE);
	}

	data->info = NULL;
	return 0;
}

/* bio合并函数，用来统计bio合并次数 */
static int elv_bio_merged_pre_handler(struct kprobe *probe,
			struct pt_regs *regs)
{
	unsigned int     rw;
	struct request   *rq;
	struct bio       *bio;
	struct part_info *info;
	struct hd_struct *part;
#ifndef __KERNEL__
	bio = (struct bio *)(regs->rdx);
	rq  = (struct request *)(regs->rsi);
#else
	bio = (struct bio *)(regs->dx);
	rq  = (struct request *)(regs->si);
#endif
	rw = rq_data_dir(rq);
	proc_stat_acct(proc_info_hlist,rw,1,MERGES);
	if(blk_do_io_stat(rq) && !(rq->cmd_flags & REQ_FLUSH))
	{
		part = disk_map_sector_rcu(rq->rq_disk, blk_rq_pos(rq));
		info = list_for_part(&part_info_list,part);
		if(info != NULL)
		{
			part_stat_acct(info,rw,1,MERGES);
			return 0;
		}
	}

	return 0;
}
static void elv_bio_merged_pos_handler(struct kprobe *probe,
			struct pt_regs *regs,unsigned long flags)
{
	return;
}

/* IO请求完成时调用，用来统计IO完成数和所用的时间 */
/* 完成时请求队列中剩余数目发生变化，因此统计一次等待时间 */
static int blk_finish_request_pre_handler(struct kprobe *probe,
			struct pt_regs *regs)
{
	unsigned int     rw;
	unsigned long    duration;
	unsigned long    now;
	struct request   *rq;
	struct part_info *info;
	struct hd_struct *part;
#ifndef __KERNEL__
	rq = (struct request *)(regs->rdi);
#else
	rq = (struct request *)(regs->di);
#endif
	rw   = rq_data_dir(rq);

	duration = jiffies - rq->start_time;
	
	if(blk_do_io_stat(rq) && !(rq->cmd_flags & REQ_FLUSH))
	{
		part = disk_map_sector_rcu(rq->rq_disk, blk_rq_pos(rq));
		info = list_for_part(&part_info_list,part);
		if(info != NULL)
		{
			part_stat_acct(info,rw,1,IOS);
			part_stat_acct(info,rw,duration,TICKS);
			/* blk_account_io_done */
			if(part_in_flight(part))
			{
				now      = jiffies;
				duration = now - part->stamp;
				part_stat_acct(info,0,duration,IO_TICKS);
				duration = part_in_flight(part)*(now - part->stamp);
				part_stat_acct(info,0,duration,TIME_IN_QUEUE);
			}
		}
	}
	return 0;
}
static void blk_finish_request_pos_handler(struct kprobe *probe,
			struct pt_regs *regs,unsigned long flags)
{
	return;
}

/* 统计IO完成的扇区数 */
static int blk_update_request_pre_handler(struct kprobe *probe,
			struct pt_regs *regs)
{
	unsigned int     rw;
	unsigned int     bytes;
	struct request   *rq;
	struct part_info *info;
	struct hd_struct *part;
#ifndef __KERNEL__
	rq    = (struct request *)(regs->rdi);
	bytes = (unsigned int)(regs->rdx);
#else
	rq    = (struct request *)(regs->di);
	bytes = (unsigned int)(regs->dx);
#endif
	if(!rq->bio)
		return 0;
	rw   = rq_data_dir(rq);

	if(blk_do_io_stat(rq) && !(rq->cmd_flags & REQ_FLUSH))
	{
		part = disk_map_sector_rcu(rq->rq_disk, blk_rq_pos(rq));
		info = list_for_part(&part_info_list,part);
		if(info != NULL)
		{
			part_stat_acct(info,rw,bytes >> 9,SECTORS);
			return 0;
		}
	}
	return 0;
}
static void blk_update_request_pos_handler(struct kprobe *probe,
			struct pt_regs *regs,unsigned long flags)
{
	return;
}

static int elv_merge_requests_pre_ret_handler(struct kretprobe_instance *ri,
			struct pt_regs *regs)
{
	struct part_info               *info;
	struct hd_struct               *part;
	struct request                 *rq;
	struct elv_merge_requests_data *data;
#ifndef __KERNEL__
	rq = (struct request *)(regs->rsi);
#else
	rq = (struct request *)(regs->si);
#endif
	data = (struct elv_merge_requests_data *)ri->data;
	if(blk_do_io_stat(rq) && !(rq->cmd_flags & REQ_FLUSH))
	{
		part = disk_map_sector_rcu(rq->rq_disk, blk_rq_pos(rq));
		info = list_for_part(&part_info_list,part);
		if(info != NULL)
		{
			data->part = part;
			data->info = info;
			return 0;
		}
	}

	data->part = NULL;
	data->info = NULL;
	return 0;
}

static int elv_merge_requests_pos_ret_handler(struct kretprobe_instance *ri,
			struct pt_regs *regs)
{
	unsigned long      now;
	unsigned long      duration;
	struct part_info   *info;
	struct hd_struct   *part;

	info = ((struct elv_merge_requests_data *)ri->data)->info;
	part = ((struct elv_merge_requests_data *)ri->data)->part;
	if(info == NULL || part == NULL)
		return 0;
	if(part_in_flight(part))
	{
		now      = jiffies;
		duration = now - part->stamp;
		part_stat_acct(info,0,duration,IO_TICKS);
		duration = part_in_flight(part) * (now - part->stamp);
		part_stat_acct(info,0,duration,TIME_IN_QUEUE);
	}
	return 0;
}

static int get_request_wait_pre_ret_handler(struct kretprobe_instance *ri,
			struct pt_regs *regs)
{
	struct bio *bio;
	struct get_request_wait_data *data;
#ifndef __KERNEL__
	bio = (struct bio *)(regs->rdx);
#else
	bio = (struct bio *)(regs->dx);
#endif
	data = (struct get_request_wait_data *)ri->data;
	data->bio = bio;
	return 0;
}
static int get_request_wait_pos_ret_handler(struct kretprobe_instance *ri,
			struct pt_regs *regs)
{
	sector_t                     sector;
	unsigned long                now;
	unsigned long                duration;
	struct part_info             *info;
	struct hd_struct             *part;
	struct gendisk               *disk;
	struct get_request_wait_data *data;
	data = (struct get_request_wait_data *)ri->data;
	if(data->bio == NULL)
		return 0;
	sector = data->bio->bi_sector;
	disk   = data->bio->bi_bdev->bd_disk;
	if(disk == NULL || sector)
		return 0;
	part = disk_map_sector_rcu(disk, sector);
	info = list_for_part(&part_info_list,part);
	if(info != NULL)
	{
		if(part_in_flight(part))
		{
			now      = jiffies;
			duration = now - part->stamp;
			part_stat_acct(info,0,duration,IO_TICKS);
			duration = part_in_flight(part) * (now - part->stamp);
			part_stat_acct(info,0,duration,TIME_IN_QUEUE);
		}
	}
	return 0;
}
static struct kprobe submit_bio_probe = {
	.pre_handler  = submit_bio_pre_probe,
	.post_handler = submit_bio_post_probe,
	.symbol_name  = "submit_bio",
};
static struct kretprobe find_get_page_ret_probe = {
	.handler        = find_get_page_pos_ret_handler,
	.entry_handler  = find_get_page_pre_ret_handler,
	.data_size      = sizeof(struct find_get_page_data),
	.maxactive      = 20, 
	.kp.symbol_name = "find_get_page", 
};
static struct kretprobe find_get_pages_contig_ret_probe = {
	.handler        = find_get_pages_contig_pos_ret_handler,
	.entry_handler  = find_get_pages_contig_pre_ret_handler,
	.data_size      = sizeof(struct find_get_pages_contig_data),
	.maxactive      = 20,
	.kp.symbol_name = "find_get_pages_contig",
};
static struct kretprobe __do_page_cache_readahead_ret_probe = {
	.entry_handler  = __do_page_cache_readahead_pre_ret_handler,
	.handler        = __do_page_cache_readahead_pos_ret_handler,
	.data_size      = sizeof(struct __do_page_cache_readahead_data),
	.maxactive      = 20,
};
static struct kprobe elv_bio_merged_probe = {
	.pre_handler  = elv_bio_merged_pre_handler,
	.post_handler = elv_bio_merged_pos_handler,
};
static struct kprobe blk_finish_request_probe = {
	.pre_handler  = blk_finish_request_pre_handler,
	.post_handler = blk_finish_request_pos_handler,
};
static struct kprobe blk_update_request_probe = {
	.pre_handler  = blk_update_request_pre_handler,
	.post_handler = blk_update_request_pos_handler,
	.symbol_name  = "blk_update_request",
};
static struct kretprobe elv_merge_requests_ret_probe = {
	.entry_handler = elv_merge_requests_pre_ret_handler,
	.handler       = elv_merge_requests_pos_ret_handler,
	.data_size     = sizeof(struct elv_merge_requests_data),
	.maxactive     = 20,
};
static struct kretprobe get_request_wait_ret_probe = {
	.entry_handler = get_request_wait_pre_ret_handler,
	.handler       = get_request_wait_pos_ret_handler,
	.data_size     = sizeof(struct get_request_wait_data),
	.maxactive     = 20,
};

char *disk_name(struct gendisk *hd, int partno, char *buf)
{
	if (!partno)
		snprintf(buf, BDEVNAME_SIZE, "%s", hd->disk_name);
	else if (isdigit(hd->disk_name[strlen(hd->disk_name)-1]))
		snprintf(buf, BDEVNAME_SIZE, "%sp%d", hd->disk_name, partno);
	else
		snprintf(buf, BDEVNAME_SIZE, "%s%d", hd->disk_name, partno);

	return buf;
}

static int __init iostat_init(void)
{
	int    res;
	struct class          *block_class;
	struct device_type    *disk_type;
	struct class_dev_iter iter;
	struct device         *dev;
	struct gendisk        *disk;
	struct disk_part_iter piter;
	struct hd_struct      *part;
	struct hd_struct      *part0;
	struct part_info      *tmp;
	struct part_info      *info;
	struct part_info      *part0_info;
	size_t                 part_info_len;
	unsigned long          iterator;
	unsigned long          max_hlist;

	max_hlist = MAX_HASH_LIST;
	for(iterator = 0; iterator < max_hlist; ++iterator)
		INIT_LIST_HEAD(proc_info_hlist + iterator);

	INIT_LIST_HEAD(&part_info_list);
	part_info_len = sizeof(struct part_info);

	if(p_block_class == 0 || p_disk_type == 0)
		return 0;
	if(p___do_page_cache_readahead == 0)
		return 0;
	if(p_elv_bio_merged == 0 || p_blk_finish_request == 0)
		return 0;
	if(p_elv_merge_requests == 0 || p_get_request_wait == 0)
		return 0;

	block_class = (struct class *)p_block_class;
	disk_type   = (struct device_type *)p_disk_type;

	class_dev_iter_init(&iter,block_class,NULL,disk_type);
	while((dev = class_dev_iter_next(&iter)))
	{
		disk = dev_to_disk(dev);

		/*
		 * Don't show empty devices or things that have been
		 * surpressed
		 */
		if (get_capacity(disk) == 0 ||
		    (disk->flags & GENHD_FL_SUPPRESS_PARTITION_INFO))
			continue;
		/*
		 * Note, unlike /proc/partitions, I am showing the
		 * numbers in hex - the same format as the root=
		 * option takes.
		 */
		disk_part_iter_init(&piter, disk, DISK_PITER_INCL_PART0);
		while ((part = disk_part_iter_next(&piter)))
		{
			tmp = (struct part_info *)kmalloc(part_info_len,GFP_KERNEL);
			memset(tmp,0,part_info_len);
			tmp->part = part;
			tmp->disk = disk;
			list_add(&(tmp->list),&part_info_list);
		}
		disk_part_iter_exit(&piter);
	}
	if(list_empty(&part_info_list))
		return 0;

	list_for_each_entry(info,&part_info_list,list)
	{
		if(!info->part->partno)
			info->part0_info = NULL;
		else
		{
			part0 = &part_to_disk(info->part)->part0;
			list_for_each_entry(part0_info,&part_info_list,list)
				if(part0_info->part == part0)
				{
					info->part0_info = part0_info;
					break;
				}
		}
	}
#ifdef __PART0_INFO_TEST__
	list_for_each_entry(info,&part_info_list,list)
	{
		if(info->part0_info != NULL)
			printk("%4d %4d---->%4d %4d\n",MAJOR(part_devt(info->part)),
						       MINOR(part_devt(info->part)),
						       MAJOR(part_devt(info->part0_info->part)),
						       MINOR(part_devt(info->part0_info->part)));
		else
			printk("%4d %4d\n",MAJOR(part_devt(info->part)),
						       MINOR(part_devt(info->part)));

	}
#endif
	dk_iostat_kobj = kobject_create_and_add("dk_iostat",kernel_kobj);
	if(!dk_iostat_kobj)
	{
		printk("dk_iostat directory create failed!\n");
		return 1;
	}
	res = sysfs_create_group(dk_iostat_kobj,&attr_group);
	if(res)
	{
		kobject_put(dk_iostat_kobj);
		printk("sysfs file create failed!\n");
		return 1;
	}
	res = register_kprobe(&submit_bio_probe);
	if(res < 0)
	{
		printk("%s register failed!\n",
				submit_bio_probe.symbol_name);
		return 1;
	}
	res = register_kretprobe(&find_get_page_ret_probe);
	if(res < 0)
	{
		printk("%s register failed!\n",
				find_get_page_ret_probe.kp.symbol_name);
		return 1;
	}
	res = register_kretprobe(&find_get_pages_contig_ret_probe);
	if(res < 0)
	{
		printk("%s register failed!\n",
				find_get_pages_contig_ret_probe.kp.symbol_name);
		return 1;
	}
	__do_page_cache_readahead_ret_probe.kp.addr = (kprobe_opcode_t *)p___do_page_cache_readahead;
	res = register_kretprobe(&__do_page_cache_readahead_ret_probe);
	if(res < 0)
	{
		printk("__do_page_cache_readahead register failed!\n");
		return 1;
	}
	elv_bio_merged_probe.addr = (kprobe_opcode_t *)p_elv_bio_merged;
	res = register_kprobe(&elv_bio_merged_probe);
	if(res < 0)
	{
		printk("elv_bio_merged register failed!\n");
		return 1;
	}
	blk_finish_request_probe.addr = (kprobe_opcode_t *)p_blk_finish_request;
	res = register_kprobe(&blk_finish_request_probe);
	if(res < 0)
	{
		printk("blk_finish_request register failed!\n");
		return 1;
	}
	res = register_kprobe(&blk_update_request_probe);
	if(res < 0)
	{
		printk("%s register failed!\n",
				blk_update_request_probe.symbol_name);
		return 1;
	}
	elv_merge_requests_ret_probe.kp.addr = (kprobe_opcode_t *)p_elv_merge_requests;
	res = register_kretprobe(&elv_merge_requests_ret_probe);
	if(res < 0)
	{
		printk("elv_merge_requests register failed!\n");
	}
	get_request_wait_ret_probe.kp.addr = (kprobe_opcode_t *)p_get_request_wait;
	res = register_kretprobe(&get_request_wait_ret_probe);
	if(res < 0)
	{
		printk("get_request_wait register failed!\n");
	}

	return 0;
}

static void __exit iostat_exit(void)
{
	kobject_put(dk_iostat_kobj);
	unregister_kprobe(&submit_bio_probe);
	unregister_kretprobe(&find_get_page_ret_probe);
	unregister_kretprobe(&find_get_pages_contig_ret_probe);
	unregister_kretprobe(&__do_page_cache_readahead_ret_probe);
	unregister_kprobe(&elv_bio_merged_probe);
	unregister_kprobe(&blk_finish_request_probe);
	unregister_kprobe(&blk_update_request_probe);
	unregister_kretprobe(&elv_merge_requests_ret_probe);
	unregister_kretprobe(&get_request_wait_ret_probe);
	part_info_free(&part_info_list);
	proc_info_free(proc_info_hlist,MAX_HASH_LIST);
}

module_init(iostat_init);
module_exit(iostat_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("D___");

