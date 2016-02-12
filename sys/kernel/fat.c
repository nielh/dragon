#undef DEBUG

#include "kernel.h"
#include "printk.h"
#include "libc.h"
#include "kmalloc.h"
#include "thread.h"
#include "object.h"
#include "vsnprintf.h"
#include "fat.h"
#include "vmalloc.h"
#include "page.h"
#include "page_map.h"
#include "rtc.h"

#define FAT_TYPE				1

#define FAT_READ				1
#define FAT_WRITE				2

#define FAT_PROCESS_BREAK		-1
#define FAT_PROCESS_CONTINUE	-2
#define FAT_PROCESS_SUCCESS		0

#define FAT_CLUSTER12(c)		( c & 0x00000FFF )	// get the low 12 bits
#define FAT_CLUSTER16(c)		( c & 0x0000FFFF )	// get the low 16 bits
#define FAT_CLUSTER31(c)		( c & 0x0FFFFFFF )	// get the low 28 bits
#define FAT_FREECLUSTER			0x00000000
#define FAT_ENDOFCLUSTER		0xFFFFFFFF
#define FAT_BADCLUSTER			0x0FFFFFF7
#define FAT_RESERVERCLUSTER		0x0FFFFFF8

#define FAT_ENTRY_DELETED		0xE5

#define FAT_PADu8				0x20

#define FAT_12					12
#define FAT_16					16
#define FAT_32					32

#define FAT_MAGIC				0xAA55

// for FAT 32
struct FAT_BOOTSECTOR32
{
    u32 BPB_FATSz32;
    u16 BPB_ExtFlags;
    u16 BPB_FSVer;
    u32 BPB_RootClus;
    u16 BPB_FSInfo;
    u16 BPB_BkBootSec;
    u8 BPB_Reserved[12];
    u8 BS_DrvNum;
    u8 BS_Reserved1;
    u8 BS_BootSig;
    u32 BS_VolID;
    u8 BS_VolLab[11];
    u8 BS_FilSysType[8];
} PACKED;

struct FAT_BOOTSECTOR
{
    u8 jmp[3];
    u8 oem_id[8];
    u16 bytes_per_sector;
    u8 sectors_per_cluster;
    u16 reserved_sectors;
    u8 num_fats;
    u16 num_root_dir_ents;
    u16 total_sectors;
    u8 media_id_byte;
    u16 sectors_per_fat;
    u16 sectors_per_track;
    u16 heads;
    u32 hidden_sectors;
    u32 total_sectors_large;
    union
    {
        struct FAT_BOOTSECTOR32 bs32;
        u8 boot_code[474];
    } PACKED A;

    u16 magic;
} PACKED;



typedef struct iobuf_t
{
	struct iobuf_t* next;
    void* cluster_buf;
    int cluster_idx;
    int locked;
    int acccnt;
	int dirty;

}iobuf_t;

typedef struct fatfile_t fatfile_t;

typedef struct fatfs_t
{
    void* device;

    int root_cluster;
    int root_size;
    u8 type;
    u32* fat_data;
    int fat_size;
    int cluster_size;
    int total_clusters;
    //
    u16 bytes_per_sector;
    u8 sectors_per_cluster;
    u16 reserved_sectors;
    u8 num_fats;
    u16 sectors_per_fat;
    //
    iobuf_t* iobuflist;
    sem_t sem_iobuflist;
    //spinlock_t iobuflock;
    int iobuf_cnt;
    int iobuf_max;

    fatfile_t* root;

} fatfs_t;

typedef struct fatfile_t
{
    ops_t* ops;
    sem_t sem;
    int ref;

    spinlock_t lock;
    fatfs_t* fatfs;
    fatfile_t* parent;
    fatfile_t* next;
    fatfile_t* child;

    int index;
    int dirty; // entry update flag
    int cluster;

    char* name;
    u32 size;
    struct FAT_ATTRIBUTE attr;

    rtcdate cdatetime;
    rtcdate wdatetime;
    rtcdate adatetime;
} fatfile_t;

static void fat_setFATCluster(fatfs_t* fatfs, int cluster, int value, int commit)
{
    fatfs->fat_data[cluster] = FAT_CLUSTER31(value);
}

static int fat_getFATCluster(fatfs_t* fatfs, int cluster)
{
    if(cluster < 2)
        return -1;

    int next_cluster = fatfs->fat_data[cluster];
    if (next_cluster == FAT_CLUSTER31(FAT_ENDOFCLUSTER))
        return -2;

    return next_cluster;
}

static int fat_getFreeCluster(fatfs_t* fatfs)
{
    int cluster, index;
    for (index = 0; index < fatfs->total_clusters; index++)
    {
        cluster = fat_getFATCluster(fatfs, index);
        if (cluster == FAT_FREECLUSTER)
            return index;
    }
    return -1;
}

static int fat_cluster2block(fatfs_t* fatfs, int cluster)
{
    return fatfs->reserved_sectors +
        fatfs->num_fats * fatfs->sectors_per_fat +
        (cluster - 2) * fatfs->sectors_per_cluster;
}

static int fat_rwCluster(fatfs_t* fatfs, int cluster, u8 * clusterBuffer, int mode)
{
    ENTER();


    LOG("%s cluster %d\n", mode == FAT_READ ? "read" : "write", cluster);

    s64 part_start = (s64)fat_cluster2block(fatfs, cluster) * (s64)fatfs->bytes_per_sector;

    s64 ret;

    if (mode == FAT_READ)
        ret = read(fatfs->device, clusterBuffer, (s64)fatfs->cluster_size, part_start);
    else if (mode == FAT_WRITE)
        ret = write(fatfs->device, clusterBuffer, (s64)fatfs->cluster_size, part_start);
    else
        ret = -1;

    LEAVE();

    return ret;
}

iobuf_t* lock_iobuf(fatfs_t* fatfs, int cluster)
{
    ENTER();

    down(&fatfs->sem_iobuflist);

	iobuf_t* ptr = fatfs->iobuflist;
	iobuf_t* ptr_prev = ptr;
	iobuf_t* ptr_prev_prev = ptr;

	while(ptr) // 查找缓冲块
	{
		if(ptr->cluster_idx == cluster)
			break;

		ptr_prev_prev = ptr_prev;
		ptr_prev = ptr;
		ptr = ptr->next;
	}

	if(ptr)
	{
		//交换到链表头部
		if(ptr_prev != ptr) // 不是第一个缓冲块
		{
			ptr_prev->next = ptr->next;
			ptr->next = fatfs->iobuflist;//放到链表头部
			fatfs->iobuflist = ptr;
		}
		LOG("cluster %d iobuf found, acccnt:%d locked:%d\n", cluster, ptr->acccnt, ptr->locked);
	}
    else // 没有发现缓冲块
	{
		if(fatfs->iobuf_cnt < fatfs->iobuf_max) // 分配新的缓冲块
		{
			// 缓冲头部
			ptr = kmalloc(sizeof(iobuf_t));
			assert(ptr);
			ptr->cluster_buf =  kmalloc(fatfs->cluster_size);
			assert(ptr->cluster_buf);
			fatfs->iobuf_cnt ++;

			LOG("cluster %d iobuf alloc\n", cluster);
		}
		else // 缓冲块用完，淘汰最后的缓冲块使用
		{
			ptr_prev_prev->next = 0; // 连接结尾
			ptr = ptr_prev;
			if(ptr->dirty) // 刷新缓冲块
			{
				if (fat_rwCluster(fatfs, ptr->cluster_idx, ptr->cluster_buf, FAT_WRITE) == -1)
				{
					panic("write cluster %d error\n", cluster);
					return 0;
				}

				LOG("cluster %d iobuf dirty, write to disk\n", cluster);
			}

			LOG("cluster %d iobuf reuse\n", cluster);
		}

		ptr->acccnt= 0;// 清零
		ptr->locked= 0;
		ptr->dirty = 0;
		ptr->cluster_idx = cluster;

		ptr->next = fatfs->iobuflist;//放到链表头部
		fatfs->iobuflist = ptr;

		if (fat_rwCluster(fatfs, cluster, ptr->cluster_buf, FAT_READ) == -1)
		{
			panic("read cluster %d error\n", cluster);
			return 0;
		}
	}

	ptr->acccnt ++;
	ptr->locked ++;

	up_one(&fatfs->sem_iobuflist);
	LEAVE();

	return ptr;
}

iobuf_t* unlock_iobuf(fatfs_t* fatfs, iobuf_t* ptr, int dirty, int commit)
{
    //ENTER();

	ptr->locked --;
	if(dirty)
        ptr->dirty = TRUE;

	if(commit)
	{
		if (fat_rwCluster(fatfs, ptr->cluster_idx, ptr->cluster_buf, FAT_WRITE) < 0)
		{
			printk("write cluster %d error\n", ptr->cluster_idx);
			return 0;
		}

		ptr->dirty = 0;
	}

	LOG("cluster %d unlock, dirty:%d, locked:%d\n", ptr->cluster_idx, dirty, ptr->locked);

	//LEAVE();
	return 0;
}

static int fat_setFileName(struct FAT_ENTRY* entry, char * filename)
{
    int i, x;
    // these are defined in the offical FAT spec
/*    u8 illegalBytes[16] = { 0x22, 0x2A, 0x2B, 0x2C, 0x2F, 0x3A, 0x3B,
    						  0x3C, 0x3D, 0x3E, 0x3F, 0x5B, 0x5C, 0x5D, 0x7C };
    // convert the name to UPPERCASE and test for illegal bytes
    int nbytes = strlen(filename);

    for (i = 0; i < nbytes; i++)
	{
		 if(filename[i] < FAT_PADu8 )
		 {
		 	printk("fat_setFileName: failed, filename error[%s]\n", filename);
		 	return -1;
		 }

		 for( x=0; x< sizeof(illegalBytes); x++ )
		 {
			 if((u8)filename[i] == illegalBytes[x] )
			 {
				printk("fat_setFileName: failed, filename error[%s]\n", filename);
				return -2;
			 }
		 }
	}
*/
    // clear the entry's name and extension
    memset(entry->name, FAT_PADu8, FAT_NAMESIZE);
    memset(entry->extention, FAT_PADu8, FAT_EXTENSIONSIZE);
    // set the name
    for (i = 0; i < FAT_NAMESIZE; i++)
    {
        if (filename[i] == 0x00 || filename[i] == '.')
            break;

        entry->name[i] = filename[i];
    }
    // if theirs an extension, set it
    if (filename[i++] == '.')
    {
        for (x = 0; x < FAT_EXTENSIONSIZE; x++, i++)
        {
            if (filename[i] == 0x00)
                break;

            entry->extention[x] = filename[i];
        }
    }

    return 0;
}

static void getEntryCreateDate(fatfile_t* file, struct FAT_ENTRY* entry)
{
    file->cdatetime.year = entry->cdate.year + 1980;
    file->cdatetime.month = entry->cdate.month;
    file->cdatetime.day = entry->cdate.date;

    file->cdatetime.hour = entry->ctime.hours;
    file->cdatetime.minute = entry->ctime.minutes;
    file->cdatetime.second = entry->ctime.twosecs << 1; // *2
}

static void setEntryCreateDate(fatfile_t* file, struct FAT_ENTRY* entry)
{
    entry->cdate.year = file->cdatetime.year - 1980;
    entry->cdate.month = file->cdatetime.month;
    entry->cdate.date = file->cdatetime.day;

    entry->ctime.hours = file->cdatetime.hour;
    entry->ctime.minutes = file->cdatetime.minute;
    entry->ctime.twosecs = file->cdatetime.second >> 1; // /2
}

static void getEntryWriteDate(fatfile_t* file, struct FAT_ENTRY* entry)
{
    file->wdatetime.year = entry->wdate.year + 1980;
    file->wdatetime.month = entry->wdate.month;
    file->wdatetime.day = entry->wdate.date;

    file->wdatetime.hour = entry->wtime.hours;
    file->wdatetime.minute = entry->wtime.minutes;
    file->wdatetime.second = entry->wtime.twosecs << 1; // *2
}

static void setEntryWriteDate(fatfile_t* file, struct FAT_ENTRY* entry)
{
    entry->wdate.year = file->wdatetime.year - 1980;
    entry->wdate.month = file->wdatetime.month;
    entry->wdate.date = file->wdatetime.day;

    entry->wtime.hours = file->wdatetime.hour;
    entry->wtime.minutes = file->wdatetime.minute;
    entry->wtime.twosecs = file->wdatetime.second >> 1; // /2
}

static int fat_updateFileEntry(fatfile_t* file)
{
    if(file->parent == NULL) // root dir
        return 0;

    if(!file->dirty)
        return 0;

    iobuf_t* iobuf = lock_iobuf(file->fatfs, file->parent->cluster);
    assert(iobuf);

    struct FAT_ENTRY* dir = (struct FAT_ENTRY*)iobuf->cluster_buf;

    // alloc a free entry
    if(file->index <= 0)
    {
        int index;
        int flag = file->fatfs->cluster_size / sizeof(struct FAT_ENTRY);

        for (index = 0; index < flag; index++)
        {
            // skip it if it has negative size
            if (dir[index].file_size == -1)
                continue;
            // if this is the end of the entries, advance the end by one
            if (dir[index].name[0] == 0x00)
                break;
        }

        if (index >= flag)
        {
            unlock_iobuf(file->fatfs, iobuf, FALSE, FALSE);
            return -1;
        }

        dir[index + 1].name[0] = 0x00;
        file->index = index;
    }

    struct FAT_ENTRY* entry = &dir[file->index];

    fat_setFileName(entry, file->name);
    entry->start_clusterHI = (u16)(file->cluster >> 16);
    entry->start_clusterLO = (u16)(file->cluster);
    entry->file_size = file->size;
    entry->attribute = file->attr;

    setEntryCreateDate(file, entry);
    setEntryWriteDate(file, entry);

    unlock_iobuf(file->fatfs, iobuf, TRUE, FALSE);

    return 0;
}

static int flush_entry(fatfile_t* file)
{
    fat_updateFileEntry(file);

    fatfile_t* child = file->child; // update child
	while(child)
    {
        flush_entry(child);
        child = child->next;
    }

    return 0;
}

static int fat_flush(fatfs_t* fatfs)
{
    // flush dir entry
    flush_entry(fatfs->root);

    // write fat data
    int start_pos = fatfs->reserved_sectors * fatfs->bytes_per_sector;
    int bytes = write(fatfs->device,
                      fatfs->fat_data,
                      fatfs->fat_size,
                      start_pos);

    assert(bytes == fatfs->fat_size);

    // flush iobuf
	iobuf_t* ptr = fatfs->iobuflist;

	while(ptr) // 查找缓冲块
	{
		if(ptr->dirty) // 刷新缓冲块
		{
		    printk("write cluster %d\n", ptr->cluster_idx);

			if (fat_rwCluster(fatfs, ptr->cluster_idx, ptr->cluster_buf, FAT_WRITE) < 0)
			{
				printk("write cluster %d error\n", ptr->cluster_idx);
				return -1;
			}

			ptr->dirty = 0;
		}

		ptr = ptr->next;
	}

	return 0;
}



static int fat_compareName(struct FAT_ENTRY * entry, char * name)
{
    int i, x;

    //to do: check past the end of the extension
    for (i = 0; i < FAT_NAMESIZE; i++)
    {
        if (entry->name[i] == FAT_PADu8)
            break;

        if (name[i] != entry->name[i])
            return -1;
    }

    if (name[i] == '.' || entry->extention[0] != FAT_PADu8)
    {
        i++;
        for (x = 0; x < FAT_EXTENSIONSIZE; x++)
        {
            if (entry->extention[x] == FAT_PADu8)
                break;

            if (name[i + x] != entry->extention[x])
                return -1;
        }
    }
    return 0;
}

static int getStartCluster(struct FAT_ENTRY* entry)
{
    return (entry->start_clusterHI << 16) |  entry->start_clusterLO;
}
/*
static void setStartCluster(struct FAT_ENTRY* entry, int start_cluster)
{
    entry->start_clusterHI = (u16)(start_cluster >> 16);
    entry->start_clusterLO = (u16)(start_cluster);
}
*/
static int fat_getIndex(struct FAT_ENTRY* dir, int flag, char * name)
{
    int i;
    for (i = 0; i < flag; i++)
    {
        if (dir[i].name[0] == 0x00)
            break;

        if (dir[i].name[0] == FAT_ENTRY_DELETED)
            continue;

        int start_cluster = getStartCluster(&dir[i]);

        if (start_cluster == FAT_FREECLUSTER && !dir[i].attribute.archive)
            continue;

        if (fat_compareName(&dir[i], name) == 0)
            return i;
    }
    return -1;
}
/*

static int name2entry(fatfile_t* dir, char * filename, struct FAT_ENTRY * entry)
{
    iobuf_t* iobuf = lock_iobuf(dir->fatfs, dir->entry.start_cluster);
    assert(iobuf);

    int dir_index = fat_getIndex((struct FAT_ENTRY*)iobuf->cluster_buf, filename);
    if (dir_index > 0)
        memcpy(entry, &((struct FAT_ENTRY*) iobuf->cluster_buf)[dir_index], sizeof(struct FAT_ENTRY));

    unlock_iobuf(dir->fatfs, iobuf, FALSE, FALSE);

    return dir_index;
}
*/

static int fat_rw(fatfile_t* file, u8 * buffer, s64 size, s64 pos, int mode)
{
    assert(file);
    assert(buffer);
    assert(size > 0);
    assert(pos >= 0);

    down(&file->sem);

    int bytes_to_rw = 0, bytes_rw = 0, cluster_offset = 0;
    int cluster, i;

    u64 file_size = file->size;

    if(pos > file_size)
    {
        up_one(&file->sem);
        return -1;
    }


    cluster = file->cluster;
    // get the correct cluster to begin reading/writing from
    i = pos / file->fatfs->cluster_size;
    // we traverse the cluster chain i times
    while (i--)
    {
        // get the next cluster in the file
        cluster = fat_getFATCluster(file->fatfs, cluster);
        // fail if we have gone beyond the files cluster chain
        if (cluster == FAT_FREECLUSTER || cluster == -1)
        {
            up_one(&file->sem);
            return -1;
        }
    }

    //iobuf_t* dir_iobuf = lock_iobuf(file->fatfs, file->dir_cluster);
    //struct FAT_ENTRY* entry = &((struct FAT_ENTRY*)dir_iobuf->cluster_buf)[file->dir_index];

    // reduce size if we are trying to read past the end of the file
    if (pos + size > file_size)
    {
        // but if we are writing we will need to expand the file size
        if (mode == FAT_WRITE)
        {
            int new_clusters = ((pos + size - file_size) / file->fatfs->cluster_size) + 1;
            int prev_cluster = cluster, next_cluster;
            // alloc more clusters
            while (new_clusters--)
            {
                // get a free cluster
                next_cluster = fat_getFreeCluster(file->fatfs);
                if (next_cluster < 0)
                {
                    up_one(&file->sem);
                    return -1;
                }

                if(prev_cluster == 0)
                {
                    file->cluster = cluster = next_cluster;
                    file->dirty = TRUE; // update entry
                }
                else
                    fat_setFATCluster(file->fatfs, prev_cluster, next_cluster, FALSE);// add it on to the cluster chain
                // update our previous cluster number
                prev_cluster = next_cluster;
            }

            fat_setFATCluster(file->fatfs, prev_cluster, FAT_ENDOFCLUSTER, FALSE);
        }
        else
            size = file_size - pos;
    }

    while (TRUE)
    {
        cluster_offset = pos % file->fatfs->cluster_size;
        bytes_to_rw = file->fatfs->cluster_size - cluster_offset;

        if(bytes_to_rw > size)
            bytes_to_rw = size;

        iobuf_t* iobuf = lock_iobuf(file->fatfs, cluster);
        assert(iobuf);
        if (mode == FAT_WRITE)
        {
            memcpy((iobuf->cluster_buf + cluster_offset), buffer, bytes_to_rw);
            unlock_iobuf(file->fatfs, iobuf, TRUE, FALSE);
        }
        else
		{
			memcpy(buffer, (iobuf->cluster_buf + cluster_offset), bytes_to_rw);
			unlock_iobuf(file->fatfs, iobuf, FALSE, FALSE);
		}

        buffer += bytes_to_rw;
        pos += bytes_to_rw;
        bytes_rw += bytes_to_rw;
        size -= bytes_to_rw;

        if (size <= 0)
            break;

        cluster = fat_getFATCluster(file->fatfs, cluster);
        assert(cluster > 1 && cluster != FAT_FREECLUSTER);
    }

    if (mode == FAT_WRITE)
    {
        if(bytes_rw > 0)
        {
            cmostime(&file->wdatetime);
            file->dirty = TRUE; // update entry

            if(file->size < pos + size)
                file->size = pos + size;
        }
    }

    up_one(&file->sem);
    return bytes_rw;
}

static struct ops_t file_ops;

fatfile_t* findChild(fatfile_t* dir, char* name)
{
	fatfile_t* file = dir->child;
	while(file)
	{
		if(strcmp(name, file->name) == 0)
			return file;

        file = file->next;
	}

	return NULL;
}

void insertChild(fatfile_t* dir, fatfile_t* file)
{
	//spin_lock(&fatfs->filelist_lk);
	file->parent = dir;
	file->next = dir->child;
	dir->child = file;
	//spin_unlock(&fatfs->filelist_lk);
}

int findEntryByName(fatfile_t* file, char* name, struct FAT_ENTRY* entry)
{
    iobuf_t* iobuf = lock_iobuf(file->fatfs, file->cluster);
    assert(iobuf);

     int flag = file->fatfs->cluster_size / sizeof(struct FAT_ENTRY);

    struct FAT_ENTRY* cluster_buf = (struct FAT_ENTRY*)iobuf->cluster_buf;
    int index = fat_getIndex(cluster_buf, flag, name);
    if (index < 0)
    {
        unlock_iobuf(file->fatfs, iobuf, FALSE, FALSE);
        return -1;
    }

    *entry = cluster_buf[index];
    unlock_iobuf(file->fatfs, iobuf, FALSE, FALSE);
    return index;
}

static void file_trunc(fatfile_t* file)
{
    fatfs_t* fatfs = file->fatfs;
    int cluster = file->cluster;

	while(1)
	{
		int next_cluster = fat_getFATCluster(fatfs, cluster);
		if (next_cluster < 0)
			break;

		fat_setFATCluster(fatfs, cluster, FAT_FREECLUSTER, FALSE);
		cluster = next_cluster;
	}

	fat_setFATCluster(fatfs, cluster, FAT_FREECLUSTER, TRUE);

    file->cluster = 0;
    file->size = 0;
}

static void* file_open(void* obj, char* path, s64 flag, s64 mode)
{
	assert(IS_PTR(obj));

    fatfile_t* file = (fatfile_t*)obj;

    down(&file->sem);

    if(*path == NULL) // last name in path
    {
        file->ref ++;
        if((!file->attr.directory) && (flag & O_TRUNC))
            file_trunc(file);

		up_one(&file->sem);
        return file;
    }

    if(!file->attr.directory) // 路径中的名称必须是目录
    {
    	up_one(&file->sem);
    	return (void*)-1;
    }

    char filename[32];
    char* end = strchr(path, '/');
    strncpy(filename, path, end -path);
    UpperStr(filename);

    if(*end == '/') // skip '/'
        end ++;

    // find in child list
    fatfile_t* child = findChild(file, filename);
    if(child == NULL)
    {
        child = (fatfile_t*)kmalloc(sizeof(fatfile_t));
        memset(child, 0, sizeof(fatfile_t));
        child->ops = &file_ops;
        sem_init(&child->sem, 1, "file sem");
        child->fatfs = file->fatfs;
        child->name = strdup(filename);

        struct FAT_ENTRY entry;
        int index = findEntryByName(file, filename, &entry);
        if(index >= 0)
        {
            child->index = index;
            child->cluster = (entry.start_clusterHI << 16) | entry.start_clusterLO;
            child->size = entry.file_size;
            child->attr = entry.attribute;

            getEntryCreateDate(child, &entry);
            getEntryWriteDate(child, &entry);
        }
        else
        {
            if((flag &O_CREAT) && (*end == NULL))
            {
                rtcdate rtc;
                cmostime(&rtc);
                child->cdatetime = rtc;
                child->wdatetime = rtc;

                child->dirty = TRUE; // update entry
            }
            else
            {
                kmfree(child->name);
                kmfree(child);

                up_one(&file->sem);
                return (void*)-2;
            }
        }

        insertChild(file, child);
    }

    up_one(&file->sem);
    return file_open(child, end, flag, mode);
}

static s64 file_close(void* obj)
{
	assert(IS_PTR(obj));

	fatfile_t* file = (fatfile_t*)obj;

	down(&file->sem);

    assert(file->ref > 0);
    file->ref --;

    up_one(&file->sem);
    return 0;
}

static s64 file_read(void* obj, void* buf, s64 len, s64 pos)
{
	return fat_rw((fatfile_t*)obj, buf, len, pos, FAT_READ);
}

static s64 file_write(void* obj, void* buf, s64 len, s64 pos)
{
	return fat_rw((fatfile_t*)obj, buf, len, pos, FAT_WRITE);
}

static s64 file_ioctl(void* obj, s64  cmd, void* buf, s64 len)
{
	assert(IS_PTR(obj));

	fatfile_t* file = (fatfile_t*)obj;

	down(&file->sem);

	if(cmd == IOCTL_GETSTAT)
    {
        struct FAT_ENTRY* entry = (struct FAT_ENTRY*)buf;

        fat_setFileName(entry, file->name);
        entry->start_clusterHI = (u16)(file->cluster >> 16);
        entry->start_clusterLO = (u16)(file->cluster);
        entry->file_size = file->size;
        entry->attribute = file->attr;

        setEntryCreateDate(file, entry);
        setEntryWriteDate(file, entry);
    }
    else if(cmd == IOCTL_LOCKPAGE)
	{
		s64 file_offset = (s64)buf;
		int cluster = file_offset >> 12; // to page
        iobuf_t* iobuf = lock_iobuf(file->fatfs, cluster);
        assert(iobuf);

        up_one(&file->sem);
        return (s64)vir_to_phy(_PT4, iobuf->cluster_buf);
	}

	up_one(&file->sem);
	return 0;
}

static s64 file_delete(void* obj, char* path)
{
    fatfile_t* file = file_open(obj, path, 0, 0);
    if(!IS_PTR(file))
        return (s64)file;

    if(file->ref > 1)
        return -2;

	down(&file->sem);

    file_trunc(file);
    file->size = -1;
    file->name[0] = 0xE5;
    file->dirty = TRUE;

    up_one(&file->sem);
    return 0;
}

static ops_t file_ops =
{
    .open = file_open,
    .close = file_close,
	.read = file_read,
	.write = file_write,
	.ioctl = file_ioctl,
	.delete = file_delete,
};

void* fat_create(char* dev_path)
{
	ENTER();

    fatfs_t* fat = (fatfs_t*) kmalloc(sizeof(fatfs_t));
    assert(fat);
    memset(fat, 0, sizeof(fatfs_t));

    //open the device we wish to mount
    fat->device = open(HANDLE_ROOT, dev_path, NULL, NULL);
    assert(IS_PTR(fat->device));

    // read in the bootsector
    struct FAT_BOOTSECTOR* bootsector = kmalloc(sizeof(struct FAT_BOOTSECTOR));

    int read_bytes = read(fat->device, bootsector, sizeof(struct FAT_BOOTSECTOR), 0);
    assert(read_bytes == sizeof(struct FAT_BOOTSECTOR));
    // make sure we have a valid bootsector
    assert (bootsector->magic == FAT_MAGIC);

    int total_sectors;
    int data_sectors;

    if (bootsector->sectors_per_fat != 0)
        fat->sectors_per_fat = bootsector->sectors_per_fat;
    else
        fat->sectors_per_fat = bootsector->A.bs32.BPB_FATSz32;

    if (bootsector->total_sectors != 0)
        total_sectors = bootsector->total_sectors;
    else
        total_sectors = bootsector->total_sectors_large;

    data_sectors = total_sectors - bootsector->reserved_sectors - bootsector->num_fats * fat->sectors_per_fat;

    fat->total_clusters = data_sectors / bootsector->sectors_per_cluster;

    fat->sectors_per_cluster = bootsector->sectors_per_cluster;
    fat->bytes_per_sector = bootsector->bytes_per_sector;
    fat->reserved_sectors = bootsector->reserved_sectors;
    fat->num_fats = bootsector->num_fats;

    if (fat->total_clusters < 4085)
        fat->type = FAT_12;
    else if (fat->total_clusters < 65525)
        fat->type = FAT_16;
    else
        fat->type = FAT_32;

    if (fat->type != FAT_32)
        panic("Only support FAT32");

    fat->cluster_size = bootsector->bytes_per_sector * bootsector->sectors_per_cluster;
    assert(fat->cluster_size <= 4096);
    fat->fat_size = fat->sectors_per_fat * bootsector->bytes_per_sector;

    fat->fat_data = (u32*)kmalloc(fat->fat_size);
    int start_pos = fat->reserved_sectors * fat->bytes_per_sector;
    read_bytes = read(fat->device, fat->fat_data, fat->fat_size, start_pos);
    assert(read_bytes == fat->fat_size);

    fat->root_cluster = bootsector->A.bs32.BPB_RootClus;
    fat->root_size = bootsector->num_root_dir_ents * sizeof(fatfile_t);
	fat->iobuf_max = 8;

    sem_init(&fat->sem_iobuflist, 1, "iobuflist sem");

    fatfile_t* root = (fatfile_t*) kmalloc(sizeof(fatfile_t));
    memset(root, 0, sizeof(fatfile_t));
    root->ops = &file_ops;
    root->fatfs = fat;
    root->name = "";
    root->attr.directory = 1;
    root->cluster = fat->root_cluster;
    root->size = fat->root_size;
    sem_init(&root->sem, 1, "root dir sem");


    fat->root = root;

	LEAVE();
    return root;
}

void test_create(char* path)
{
    LOG("open file %s ....\n", path);
    void* file = open(HANDLE_ROOT, path, O_CREAT, NULL);
    assert(IS_PTR(file));

    char* data = "test data !!!!123";
    LOG("write to file :\n[%s]\n", data);
    int write_bytes = write(file, data, strlen(data), 0);
    assert(write_bytes > 0);

    char buf[256] = {0};
    int read_bytes = read(file, buf, sizeof(buf), 0);
    assert(read_bytes > 0);
    LOG("read from file:\n[%s]\n", buf);
}

void log_time()
{
    rtcdate now;
    cmostime(&now);
    char str[64];
    snprintf(str, sizeof(str), "Machine boot at %d-%d-%d %d:%d:%d ...\r\n",
			now.year, now.month, now.day, now.hour, now.minute, now.second);

    void* file = open(HANDLE_ROOT, "sys/boot.txt", O_CREAT, NULL);
    assert(IS_PTR(file));

    struct FAT_ENTRY stat;
    ioctl(file, IOCTL_GETSTAT, &stat, 0);

    int pos = stat.file_size;

	write(file, str, strlen(str), pos);

    /*
    for(int i = 0; i < 1024; i ++)
    {
        write(file, "0123456789abcdef\n", 17, pos);
        pos += 17;
    }
    */



	//delete(HANDLE_ROOT, "sys/bn/test4.txt");

    //test_create("sys/bn/test2.txt");

}

void fat_init()
{
    ENTER();
    char* dev = "obj/sd0p0";
    fatfile_t* root = fat_create(dev);
    assert(root);

	append(HANDLE_ROOT, "sys", root);
	LOG("mount %s to /sys\n", dev);

	//log_time();
	//fat_flush(root->fatfs);

	LEAVE();
}
