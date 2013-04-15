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

struct part_info {
	struct list_head list;
	struct hd_struct *part;
	struct part_info *part0_info;
	struct gendisk   *disk;
	unsigned long    f_ios[2];
	unsigned long    f_merges[2];
	unsigned long    f_ticks[2];
	unsigned long    f_sectors[2];
	unsigned long    r_ios[2];
	unsigned long    r_sectors[2];
	unsigned long    p_count[3];
	unsigned long    io_ticks;
	unsigned long    time_in_queue;
};
static struct list_head  part_info_list;

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
			"%lu %lu %lu %lu %lu %lu %lu\n",
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
			tmp->p_count[0],tmp->p_count[1],
			tmp->p_count[2]
			);
		pos = buf + res;
	}
	return res;
}

static struct kobj_attribute dk_iostat_attribute =
	__ATTR(dk_iostat,0444,dk_iostat_show,NULL);

static struct attribute *attrs[] = {
	&dk_iostat_attribute.attr,
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

/* ����������ҳ�滺�� */
static int find_get_pages_contig_pre_ret_handler(struct kretprobe_instance *ri,
			struct pt_regs *regs)
{
	unsigned int                      nr_pages;
	struct address_space              *mapping;
	struct block_device               *bdev;
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
	if(unlikely(mapping == NULL))
	{
		printk("address_space isnot exist!\n");
		goto out;
	}
	if(unlikely(mapping->host == NULL))
		goto out;
	if(unlikely(mapping->host->i_sb == NULL))
		goto out;
	bdev = mapping->host->i_sb->s_bdev;
	if(unlikely(bdev == NULL))
		goto out;
	part = bdev->bd_part;
	list_for_each_entry(info,&part_info_list,list)
	{
		if(info->part == part)
			goto count;
	}
	goto out;

count:
	info->p_count[0] += nr_pages;
	if(info->part0_info != NULL)
	{
		info->part0_info->p_count[0] += nr_pages;
	}
	data->info = info;
	return 0;
out:
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
	if(data->info == NULL)
		goto out;
	else
		goto count;
	return 0;
count:
	info = data->info;
	info->p_count[1] += nr_pages;
	if(info->part0_info != NULL)
	{
		info->part0_info->p_count[1] += nr_pages;
	}
	data->info = NULL;
	return 0;
out:
	data->info = NULL;
	return 0;
}

/* ���ҵ���ҳ�滺�� */
static int find_get_page_pre_ret_handler(struct kretprobe_instance *ri,
			struct pt_regs *regs)
{
	struct address_space      *mapping;
	struct block_device       *bdev;
	struct hd_struct          *part;
	struct part_info          *info;
	struct find_get_page_data *data;
#ifndef __KERNEL__
	mapping = (struct address_space *)(regs->rdi);
#else
	mapping = (struct address_space *)(regs->di);
#endif
	data = (struct find_get_page_data *)ri->data;
	if(unlikely(mapping == NULL))
	{
		printk("address_space isnot exist!\n");
		goto out;
	}
	if(unlikely(mapping->host == NULL))
		goto out;
	if(unlikely(mapping->host->i_sb == NULL))
		goto out;
	bdev = mapping->host->i_sb->s_bdev;
	if(unlikely(bdev == NULL))
		goto out;
	part = bdev->bd_part;
	list_for_each_entry(info,&part_info_list,list)
	{
		if(info->part == part)
			goto count;
	}
	goto out;

count:
	++(info->p_count[0]);
	if(info->part0_info != NULL)
	{
		++info->part0_info->p_count[0];
	}
	data->info = info;
	return 0;
out:
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
	if(page == NULL || data->info == NULL)
		goto out;
	else
		goto count;
	return 0;
count:
	info = data->info;
	++(info->p_count[1]);
	if(info->part0_info != NULL)
	{
		++info->part0_info->p_count[1];
	}
	data->info = NULL;
	return 0;
out:
	data->info = NULL;
	return 0;
}

/* �ύbio����������ͳ��bio�ύ������������ */
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
		list_for_each_entry(info,&part_info_list,list)
		{
			if(info->part == bi_part)
				goto count;
		}
	}
	return 0;
count:
	if(rw & WRITE)
	{
		++info->r_ios[1];
		info->r_sectors[1] += bio_sectors(bio);
		if(info->part0_info != NULL)
		{
			++info->part0_info->r_ios[1];
			info->part0_info->r_sectors[1] += bio_sectors(bio);
		}
	}
	else
	{
		++info->r_ios[0];
		info->r_sectors[0] += bio_sectors(bio);
		if(info->part0_info != NULL)
		{
			++info->part0_info->r_ios[0];
			info->part0_info->r_sectors[0] += bio_sectors(bio);
		}
	}
	return 0;
}

static void submit_bio_post_probe(struct kprobe *probe,
			struct pt_regs *regs,unsigned long flags)
{
	return;
}

/* �ļ�Ԥ������������ͳ��Ԥ��ҳ�� */
static int __do_page_cache_readahead_pre_ret_handler(struct kretprobe_instance *ri,
			struct pt_regs *regs)
{
	struct address_space *mapping;
	struct block_device  *bdev;
	struct hd_struct     *part;
	struct part_info     *info;

	struct __do_page_cache_readahead_data *data;
#ifndef __KERNEL__
	mapping = (struct address_space *)(regs->rdi);
#else
	mapping = (struct address_space *)(regs->di);
#endif
	data = (struct __do_page_cache_readahead_data *)ri->data;
	if(unlikely(mapping == NULL))
	{
		printk("address_space isnot exist!\n");
		goto out;
	}
	if(unlikely(mapping->host == NULL))
		goto out;
	if(unlikely(mapping->host->i_sb == NULL))
		goto out;
	bdev = mapping->host->i_sb->s_bdev;
	if(unlikely(bdev == NULL))
		goto out;
	part = bdev->bd_part;
	list_for_each_entry(info,&part_info_list,list)
	{
		if(info->part == part)
		{
			data->info = info;
			return 0;
		}
	}
	goto out;
out:
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
	if(info == NULL)
		return 0;
	info->p_count[2] += nr_pages;
	if(info->part0_info != NULL)
	{
		info->part0_info->p_count[2] += nr_pages;
	}
	data->info = NULL;
	return 0;
}

/* bio�ϲ�����������ͳ��bio�ϲ����� */
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
	if(blk_do_io_stat(rq) && !(rq->cmd_flags & REQ_FLUSH_SEQ))
		part = disk_map_sector_rcu(rq->rq_disk, blk_rq_pos(rq));
	else
		return 0;
	list_for_each_entry(info,&part_info_list,list)
	{
		if(info->part == part)
		{
			++info->f_merges[rw];
			if(info->part0_info != NULL)
			{
				++info->part0_info->f_merges[rw];
			}
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

/* IO�������ʱ���ã�����ͳ��IO����������õ�ʱ�� */
/* ���ʱ���������ʣ����Ŀ�����仯�����ͳ��һ�εȴ�ʱ�� */
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
	if(blk_do_io_stat(rq) && !(rq->cmd_flags & REQ_FLUSH_SEQ))
		part = disk_map_sector_rcu(rq->rq_disk, blk_rq_pos(rq));
	else
		return 0;
	list_for_each_entry(info,&part_info_list,list)
	{
		if(info->part == part)
			goto count;
	}
	return 0;

count:
	++info->f_ios[rw];
	duration = jiffies - rq->start_time;
	info->f_ticks[rw]   += duration;
	if(info->part0_info != NULL)
	{
		++info->part0_info->f_ios[rw];
		info->part0_info->f_ticks[rw]   += duration;
	}

	/* blk_account_io_done */
	if(part_in_flight(part))
	{
		now = jiffies;
		info->io_ticks      += now - part->stamp;
		info->time_in_queue += part_in_flight(part) * (now - part->stamp);
		if(info->part0_info != NULL)
		{
			info->part0_info->io_ticks      += now - part->stamp;
			info->part0_info->time_in_queue += part_in_flight(part) * (now - part->stamp);
		}
	}
	return 0;
}
static void blk_finish_request_pos_handler(struct kprobe *probe,
			struct pt_regs *regs,unsigned long flags)
{
	return;
}

/* ͳ��IO��ɵ������� */
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
	if(blk_do_io_stat(rq) && !(rq->cmd_flags & REQ_FLUSH_SEQ))
		part = disk_map_sector_rcu(rq->rq_disk, blk_rq_pos(rq));
	else
		return 0;
	list_for_each_entry(info,&part_info_list,list)
	{
		if(info->part == part)
			goto count;
	}
	return 0;

count:
	info->f_sectors[rw] += bytes >> 9;
	if(info->part0_info != NULL)
	{
		info->part0_info->f_sectors[rw] += bytes >> 9;
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
	if(blk_do_io_stat(rq) && !(rq->cmd_flags & REQ_FLUSH_SEQ))
		part = disk_map_sector_rcu(rq->rq_disk, blk_rq_pos(rq));
	else
		return 0;
	list_for_each_entry(info,&part_info_list,list)
	{
		if(info->part == part)
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
	unsigned long    now;
	struct part_info *info;
	struct hd_struct *part;
	info = ((struct elv_merge_requests_data *)ri->data)->info;
	part = ((struct elv_merge_requests_data *)ri->data)->part;
	if(info == NULL || part == NULL)
		return 0;
	if(part_in_flight(part))
	{
		now = jiffies;
		info->io_ticks      += now - part->stamp;
		info->time_in_queue += part_in_flight(part) * (now - part->stamp);
		if(info->part0_info != NULL)
		{
			info->part0_info->io_ticks      += now - part->stamp;
			info->part0_info->time_in_queue += part_in_flight(part) * (now - part->stamp);
		}
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
	list_for_each_entry(info,&part_info_list,list)
	{
		if(info->part == part)
			goto count;
	}
	return 0;

count:
	if(part_in_flight(part))
	{
		now = jiffies;
		info->io_ticks      += now - part->stamp;
		info->time_in_queue += part_in_flight(part) * (now - part->stamp);
		if(info->part0_info != NULL)
		{
			info->part0_info->io_ticks      += now - part->stamp;
			info->part0_info->time_in_queue += part_in_flight(part) * (now - part->stamp);
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
	size_t part_info_len;


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
	struct part_info *tmp;
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
	if(!list_empty(&part_info_list))
		list_for_each_entry(tmp,&part_info_list,list)
			kfree(tmp);
}

module_init(iostat_init);
module_exit(iostat_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("D___");
