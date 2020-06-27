#include"pm_ehash.h"
 
vector<data_page*> PageList;

/**
 * @description: construct a new instance of PmEHash in a default directory
 * @param NULL
 * @return: new instance of PmEHash
 */
PmEHash::PmEHash() {
	recoverMetadata();
	mapAllPage();
	recoverCatalog();
}
/**
 * @description: persist and munmap all data in NVM
 * @param NULL 
 * @return: NULL
 */
PmEHash::~PmEHash() {
	// 保存metadata ：
	pmem_persist(metadata, sizeof(metadata));
	pmem_unmap(metadata, sizeof(metadata));
	// 保存catalog ：
	size_t map_len;
	int is_pmem;
	ehash_catalog* cat = (ehash_catalog*)pmem_map_file("/mnt/mem/pm_ehash_catalog", sizeof(ehash_catalog), PMEM_FILE_CREATE, 0777, &map_len, &is_pmem);
	*cat = this->catalog;
	pmem_persist(cat, sizeof(cat));
	pmem_unmap(cat, sizeof(cat));
	// 保存page ：
	for(auto p: PageList) {
		WritePageToFile(p);
		pmem_unmap(p, sizeof(p));
	}
}

/**
 * @description: 插入新的键值对，并将相应位置上的位图置1
 * @param kv: 插入的键值对
 * @return: 0 = insert successfully, -1 = fail to insert(target data with same key exist)
 */
int PmEHash::insert(kv new_kv_pair) {
    uint64_t return_val = 0;
    if(search(new_kv_pair.key, return_val) == 0){
    	return -1;
    }
    pm_bucket* bucket = getFreeBucket(new_kv_pair.key);
    kv* freePlace = getFreeKvSlot(bucket, 1);
    *freePlace = new_kv_pair;
    // persist:
    pm_address cur = vAddr2pmAddr[bucket];
    WritePageToFile(PageList[cur.fileId]);
    return 0;
}

/**
 * @description: 删除具有目标键的键值对数据，不直接将数据置0，而是将相应位图置0即可
 * @param uint64_t: 要删除的目标键值对的键
 * @return: 0 = removing successfully, -1 = fail to remove(target data doesn't exist)
 */
int PmEHash::remove(uint64_t key) {
    uint64_t h = hashFunc(key);
    kv* tslot = this->catalog.buckets_virtual_address[h]->slot;
    bool* tbitmap = this->catalog.buckets_virtual_address[h]->bitmap;
    for(int i = 0; i < BUCKET_SLOT_NUM; i++){
		if(tbitmap[i] == 1 && tslot[i].key == key) {
			this->catalog.buckets_virtual_address[h]->bitmap[i] = 0;
			mergeBucket(h);
			return 0;
		}
    }
    return -1;
}
/**
 * @description: 更新现存的键值对的值
 * @param kv: 更新的键值对，有原键和新值
 * @return: 0 = update successfully, -1 = fail to update(target data doesn't exist)
 */
int PmEHash::update(kv kv_pair) {
    uint64_t return_val = 0;
    if(search(kv_pair.key, return_val) == 0) {
    	remove(kv_pair.key);
    	insert(kv_pair);
    	return 0;
    } else {
    	return -1;
    }
}
/**
 * @description: 查找目标键值对数据，将返回值放在参数里的引用类型进行返回
 * @param uint64_t: 查询的目标键
 * @param uint64_t&: 查询成功后返回的目标值
 * @return: 0 = search successfully, -1 = fail to search(target data doesn't exist) 
 */
int PmEHash::search(uint64_t key, uint64_t& return_val) {
    uint64_t h = hashFunc(key);
	if(h >= this->metadata->catalog_size) return -1;
    kv* tslot = this->catalog.buckets_virtual_address[h]->slot;
    bool* tbitmap = this->catalog.buckets_virtual_address[h]->bitmap;
    for(int i=0; i<BUCKET_SLOT_NUM; i++) {
		if(tbitmap[i] == 1 && tslot[i].key == key) {
			return_val = tslot[i].value;
    		return 0;
		}
    }
    return -1;
}

/**
 * @description: 用于对输入的键产生哈希值，然后取模求桶号(自己挑选合适的哈希函数处理)
 * @param uint64_t: 输入的键
 * @return: 返回键所属的桶号
 */
uint64_t PmEHash::hashFunc(uint64_t key) {
	uint64_t gd = this->metadata->global_depth;
	uint64_t h = key & ((1<<gd)-1);
	return h;
}

/**
 * @description: 获得供插入的空闲的桶，无空闲桶则先分裂桶然后再返回空闲的桶
 * @param uint64_t: 带插入的键
 * @return: 空闲桶的虚拟地址
 */
pm_bucket* PmEHash::getFreeBucket(uint64_t key) {
	uint64_t h = hashFunc(key);
	pm_bucket* curbucket = this->catalog.buckets_virtual_address[h];
	if(getFreeKvSlot(curbucket, 0) == NULL) {
		splitBucket(h);
		h = hashFunc(key);
		return this->catalog.buckets_virtual_address[h];
	} else {
		return curbucket;
	}
}

/**
 * @description: 获得空闲桶内第一个空闲的位置供键值对插入, 传入的flag若为0，则只是简单查询是否有空闲slot，若为1，则是真正返回slot，需要将标志位置1
 * @param pm_bucket* bucket， int flag
 * @return: 空闲键值对位置的虚拟地址
 */
kv* PmEHash::getFreeKvSlot(pm_bucket* bucket, int flag) {
	bool* tbitmap = bucket->bitmap;
	for(int i=0; i<BUCKET_SLOT_NUM; i++){
		if(tbitmap[i] == 0) {
			if(flag == 1) {
				(bucket->bitmap)[i] = 1;
			}
			return &(bucket->slot[i]);
		}	
	}
	return NULL;
}

/**
 * @description: 桶满后进行分裂操作，可能触发目录的倍增
 * @param uint64_t: 目标桶在目录中的序号
 * @return: NULL
 */
void PmEHash::splitBucket(uint64_t bucket_id) {

}

/**
 * @description: 桶空后，回收桶的空间，并设置相应目录项指针
 * @param uint64_t: 桶号
 * @return: NULL
 */
void PmEHash::mergeBucket(uint64_t bucket_id) {
    
}

/**
 * @description: 对目录进行倍增，需要重新生成新的目录文件并复制旧值，然后删除旧的目录文件
 * @param NULL
 * @return: NULL
 */
void PmEHash::extendCatalog() {

}

/**
 * @description: 获得一个可用的数据页的新槽位供哈希桶使用，如果没有则先申请新的数据页
 * @param pm_address&: 新槽位的持久化文件地址，作为引用参数返回
 * @return: 新槽位的虚拟地址
 */
void* PmEHash::getFreeSlot(pm_address& new_address) {

}

/**
 * @description: 申请新的数据页文件，并把所有新产生的空闲槽的地址放入free_list等数据结构中
 * @param NULL
 * @return: NULL
 */
void PmEHash::allocNewPage() {

}

/**
 * @description: 读取旧数据文件重新载入哈希，恢复哈希关闭前的状态
 * @param NULL
 * @return: NULL
 */
void PmEHash::recover() {

}

/**
 * @description: 重启时，将所有数据页进行内存映射，设置地址间的映射关系，空闲的和使用的槽位都需要设置 
 * @param NULL
 * @return: NULL
 */
void PmEHash::mapAllPage() {

}

/**
 * @description: 删除PmEHash对象所有数据页，目录和元数据文件，主要供gtest使用。即清空所有可扩展哈希的文件数据，不止是内存上的
 * @param NULL
 * @return: NULL
 */
void PmEHash::selfDestory() {
	char t[100];
	sprintf(t, "rm -rf %s/*", PM_EHASH_DIRECTORY);
	system(t);
}