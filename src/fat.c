#include "everdrive.h"

/* FAT directory entry record */
typedef struct {
    u8 name[8];
    u8 ext[3];
    u8 attrib;
    u8 x[8];
    u16 cluster_hi;
    u16 time;
    u16 date;
    u16 cluster_lo;
    u32 size;
} FatRecordHdr;

/* hdr_idx (record location) is 16*sector+index */

typedef struct {
    u32 hdr_idx;                /* location of the first LFN record */
    u32 dat_idx;                /* location of the standard FAT record */
    u32 cluster;                /* first cluster */
    u32 size;                   /* size in bytes */
    u32 sec_available;          /* no. sectors in file */
    u16 is_dir;                 /* directory attribute */
} FatRecord;

typedef struct {
    FatRecord rec;
    u8 name[200];               /* standard filename or LFN */
} FatFullRecord;

typedef struct {
    u32 fat_entry;              /* sector of the first FAT */
    u32 data_entry;             /* sector of the first data cluster */
    u32 sectors_per_fat;        /* size of the FAT in sectors */
    u32 _0C;                    /* no. sectors in this partition */
    u16 _10;                    /* no. clusters in 1024 sectors */
    u8 cluster_size;            /* no. sectors in cluster */
} Fat;

typedef struct {
    u32 table_sec_idx;          /* sector of the table cache */
    u32 data_sec_idx;           /* sector of the data cache */
    u32 _08;                    /* unused; set to 0 in fatInit */
    u8 table_changed;           /* table cache is dirty */
    u8 data_changed;            /* data cache is dirty */
} FatCache;

typedef struct {
    u32 cluster;                /* current cluster */
    u32 sector;                 /* current sector */
    u8 in_cluster_ptr;          /* sector offset in the cluster */
} FatPos;

typedef struct {
    FatPos pos;
    u32 entry_cluster;          /* first cluster */
    u32 _10;                    /* unknown */
    u32 current_idx;            /* current header index */
    u16 size;                   /* no. entries in the directory */
    u8 is_root;                 /* root directory flag */
} Dir;

typedef struct {
    Dir dir;
} FullDir;

typedef struct {
    FatPos pos;
    u32 hdr_idx;                /* header index of this entry */
    FatRecordHdr *hdr;          /* directory entry record */
} Record;

typedef struct {
    FatPos pos;
    u32 sec_available;          /* no. sectors left in file */
    u32 hdr_idx;                /* header index for the file data */
} File;

FatCache fat_cache;
Fat current_fat;
Dir currentDir;
Record currentRecord;
File currentFile;

u32 *recordTable;
u8 *tableSector;
u8 *dataSector;

u32 fatClusterToSector(u32 cluster);
u32 fatSectorToCluster(u32 sector);

static const u8 lfn_char_struct[] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};

/* Set position with cluster */
void FatSetCluster(FatPos *pos, u32 cluster) {

    pos->cluster = cluster;
    pos->sector = fatClusterToSector(cluster);
    pos->in_cluster_ptr = 0;
}

/* Set position with sector */
void FatSetSector(FatPos *pos, u32 sector) {

    pos->cluster = fatSectorToCluster(sector);
    pos->sector = sector;
    pos->in_cluster_ptr = sector - fatClusterToSector(pos->cluster);
}

/* Find the first sector of this cluster */
u32 fatClusterToSector(u32 cluster) {

    return (cluster - 2) * current_fat.cluster_size + current_fat.data_entry;
}

/* Find the cluster that contains this sector */
u32 fatSectorToCluster(u32 sector) {

    return (sector - current_fat.data_entry) / current_fat.cluster_size + 2;
}

/* Get the remaining no. sectors to read/write in the file */
u32 FatGetFileSectors() {

    return currentFile.sec_available;
}

/* Get the location of the record of the file */
u32 FatGetFileHdrIdx() {

    return currentFile.hdr_idx;
}

/* Get the size of the directory */
u16 FatGetDirSize() {

    return currentDir.size;
}

/* Get the first cluster of the directory */
u32 FatGetDirCluster() {

    return currentDir.entry_cluster;
}

u32 fat_80003770() {

    return currentDir._10;
}

/* Get a record location from the record table */
u32 FatGetRecord(u16 index) {

    return recordTable[index];
}

/* No. sectors remaining in cluster */
u16 FatGetRWLen(u32 sectors) {

    u16 val;
    if (currentFile.pos.in_cluster_ptr == 0) {
        return current_fat.cluster_size < sectors ? current_fat.cluster_size : sectors;
    }
    val = current_fat.cluster_size - currentFile.pos.in_cluster_ptr;
    if (val > sectors) val = sectors;
    return val;
}

/* Get little-endian 32-bit value */
u32 FatGet32(void *src) {

    u8 *ptr = src;
    return (u32) ptr[0] | (u32) ptr[1] << 8 | (u32) ptr[2] << 16 | (u32) ptr[3] << 24;
}

/* Get little-endian 16-bit value */
u16 FatGet16(void *src) {

    u8 *ptr = src;
    return (u16) ptr[0] | (u16) ptr[1] << 8;
}

/* Set little-endian 32-bit value */
void FatSet32(u32 val, void *dst) {

    u8 *ptr = dst;
    ptr[0] = (u32) val >> 0;
    ptr[1] = (u32) val >> 8;
    ptr[2] = (u32) val >> 16;
    ptr[3] = (u32) val >> 24;
}

/* Load a sector from the FAT into the table cache */
u8 fatCacheLoadTable(u32 sector) {

    u8 resp = 0;

    if (fat_cache.table_sec_idx == sector) return 0;

    if (fat_cache.table_changed) {
        fat_cache.table_changed = 0;
        fat_cache.table_sec_idx += current_fat.fat_entry;
        resp = DiskWriteFromRam(fat_cache.table_sec_idx, tableSector, 1);
        if (resp) return resp;
        resp = DiskWriteFromRam(fat_cache.table_sec_idx+current_fat.sectors_per_fat, tableSector, 1);
        if (resp) return resp;
        if (sector == 0x0FFFFFFF) return 0;
    }

    fat_cache.table_sec_idx = sector;
    resp = diskReadToRam(sector + current_fat.fat_entry, tableSector, 1);
    return resp;
}

/* Follow the cluster chain for one step */
u8 fatGetTableRecord(FatPos *pos) {

    u8 resp;
    u32 cluster = pos->cluster;
    resp = fatCacheLoadTable(cluster / 128);
    if (resp) return resp;
    cluster = FatGet32(&tableSector[(cluster & 127) * 4]);
    if (cluster == 0x0FFFFFFF) return 0xF3;
    pos->cluster = cluster;
    return 0;
}

/* Seek ahead to the next cluster */
u8 fatGetNextCluster(FatPos *pos) {

    u8 resp = fatGetTableRecord(pos);
    if (resp) return resp;
    pos->in_cluster_ptr = 0;
    pos->sector = fatClusterToSector(pos->cluster);
    return 0;
}

/* Seek ahead a number of sectors within this cluster, and seek to the next */
/* cluster if at the end of this cluster */
u8 FatRWStepSectors(u16 sectors) {

    u8 resp;
    currentFile.pos.in_cluster_ptr += sectors;
    if (currentFile.pos.in_cluster_ptr == current_fat.cluster_size) {
        resp = fatGetNextCluster(&currentFile.pos);
        if (resp) return resp;
    }
    else {
        currentFile.pos.sector += sectors;
    }
    return 0;
}

/* Read/write a number of sectors to/from ROM */
u8 FatRWFileRom(u32 dst, u32 sectors, u8 mode) {

    u8 resp;

    if (sectors > currentFile.sec_available) return 0xF3;
    currentFile.sec_available -= sectors;

    while (sectors) {

        u16 len = FatGetRWLen(sectors);

        if (mode == 1) {
            resp = DiskWriteFromRom(currentFile.pos.sector, dst, len);
            if (resp) return resp;
        }
        else {
            resp = diskReadToRom(currentFile.pos.sector, dst, len);
            if (resp) return resp;
        }
        sectors -= len;
        if (sectors == 0 && currentFile.sec_available == 0) return 0;
        resp = FatRWStepSectors(len);
        if (resp) return resp;
        dst += len;
    }

    return 0;
}

/* Read a number of sectors to ROM */
u8 FatReadFileRom(u32 dst, u32 sectors) {

    return FatRWFileRom(dst, sectors, 0);
}

/* Seek ahead one sector */
u8 FatGetNextSector(FatPos *pos) {

    u8 resp;
    pos->in_cluster_ptr++;
    pos->sector++;
    if (pos->in_cluster_ptr == current_fat.cluster_size) {
        resp = fatGetNextCluster(pos);
        if (resp) return resp;
    }
    return 0;
}

/* Skip a number of sectors */
u8 fatSkipSectors(u16 sectors) {

    u8 resp;
    currentFile.sec_available -= sectors;
    while (sectors) {
        if (!(sectors < current_fat.cluster_size || currentFile.pos.in_cluster_ptr != 0)) {
            resp = fatGetNextCluster(&currentFile.pos);
            if (resp) return resp;
            sectors -= current_fat.cluster_size;
            continue;
        }
        else {
            resp = FatGetNextSector(&currentFile.pos);
            if (resp) return resp;
            sectors--;
            continue;
        }
    }
    return 0;
}

/* Read/write a number of sectors to/from RAM */
u8 FatRWFileRam(void *dst, u16 sectors, u8 mode) {

    u8 resp;

    if (sectors > currentFile.sec_available) return 0xF3;
    currentFile.sec_available -= sectors;

    while (sectors) {

        u16 len = FatGetRWLen(sectors);

        if (mode == 1) {
            resp = DiskWriteFromRam(currentFile.pos.sector, dst, len);
            if (resp) return resp;
        }
        else {
            resp = diskReadToRam(currentFile.pos.sector, dst, len);
            if (resp) return resp;
        }
        sectors -= len;
        if (sectors == 0 && currentFile.sec_available == 0) return 0;
        resp = FatRWStepSectors(len);
        if (resp) return resp;
        dst += (u16)(512 * len);
    }

    return 0;
}

/* Read a number of sectors to RAM */
u8 FatReadFileRam(void *dst, u16 sectors) {

    return FatRWFileRam(dst, sectors, 0);
}

/* Load a sector from the disk into the data cache */
u8 fatCacheLoadData(u32 sector) {

    u8 resp = 0;

    if (fat_cache.data_sec_idx == sector) return 0;

    if (fat_cache.data_changed) {
        fat_cache.data_changed = 0;
        resp = DiskWriteFromRam(fat_cache.data_sec_idx, dataSector, 1);
        if (resp) return resp;
        if (sector == 0x0FFFFFFF) return 0;
    }

    fat_cache.data_sec_idx = sector;
    resp = diskReadToRam(fat_cache.data_sec_idx, dataSector, 1);
    return resp;
}

/* Get the next entry in the directory */
u8 FatNextRecord() {

    u8 resp;
    if ((currentRecord.hdr_idx & 15) != 15) {
        currentRecord.hdr_idx++;
        currentRecord.hdr++;
        return 0;
    }
    resp = FatGetNextSector(&currentRecord.pos);
    if (resp) return resp;
    currentRecord.hdr_idx = currentRecord.pos.sector * 16;
    currentRecord.hdr = (FatRecordHdr *)dataSector;
    resp = fatCacheLoadData(currentRecord.pos.sector);
    return resp;
}

/* Open a record */
u8 FatOpenRecord(u32 hdr_idx) {

    u8 resp;
    FatSetSector(&currentRecord.pos, hdr_idx / 16);
    resp = fatCacheLoadData(currentRecord.pos.sector);
    if (resp) return resp;
    currentRecord.hdr_idx = hdr_idx;
    currentRecord.hdr = (FatRecordHdr *)&dataSector[(hdr_idx & 15) * 32];
    return 0;
}

/* Read a record chain and decode the filename/LFN */
u8 FatReadRecord(u32 hdr_idx, FatRecord *rec, u8 *name) {

    u8 resp;
    FatRecordHdr *hdr;
    u8 *attrib;
    u8 *ptr;
    u8 make_name = 0;

    resp = FatOpenRecord(hdr_idx);
    if (resp) return resp;

    hdr = currentRecord.hdr;
    attrib = &hdr->attrib;

    if (name) make_name = 1;
    if (make_name) *name = 0;

    while (*attrib == 0xF) {
        if (hdr->name[0] == 0xE5) return 0xFA;
        if (make_name) {
            u8 offset = (hdr->name[0]-1) & 15;
            if (offset == 15) offset = 14;
            ptr = &name[13*offset];
            if (hdr->name[0] & 0x40) ptr[13] = 0;
            for (u8 i = 0; i < 13; i++) {
                *ptr++ = hdr->name[lfn_char_struct[i]];
            }
        }
        resp = FatNextRecord();
        if (resp) return resp;
        hdr = currentRecord.hdr;
        attrib = &hdr->attrib;
    }

    if (hdr->name[0] == 0xE5) return 0xFA;

    if (rec) {
        rec->hdr_idx = hdr_idx;
        rec->dat_idx = currentRecord.hdr_idx;
        rec->is_dir = *attrib & 0x10;
        rec->cluster  = FatGet16(&hdr->cluster_hi) << 16;
        rec->cluster |= FatGet16(&hdr->cluster_lo);
        rec->size = FatGet32(&hdr->size);
        rec->sec_available = rec->size / 512;
        if (rec->size & 0x1FF) rec->sec_available++;
    }

    if (make_name && *name == 0) {
        for (u8 i = 0; i < 8; i++) {
            name[i] = hdr->name[i];
        }
        ptr = StrTrimSpace(name+8-1);
        if (hdr->ext[0] != ' ') {
            *ptr++ = '.';
            *ptr++ = hdr->ext[0];
            *ptr++ = hdr->ext[1];
            *ptr++ = hdr->ext[2];
            StrTrimSpace(ptr-1);
        }
    }

    return 0;
}

/* Open a file */
u8 fatOpenFile(u32 hdr_idx, u16 wr_sectors) {

    u8 resp;
    FatRecord rec;
    resp = FatReadRecord(hdr_idx, &rec, NULL);
    if (resp) return resp;
    if (rec.is_dir) return 0xF5;
    FatSetCluster(&currentFile.pos, rec.cluster);
    currentFile.sec_available = rec.sec_available;
    currentFile.hdr_idx = rec.hdr_idx;
    bios_80000568(0);
    return resp;
}

/* Open a file from a loaded record */
u8 FatOpenFileFromRec(FatRecord *rec, u16 wr_sectors) {

    return fatOpenFile(rec->dat_idx, wr_sectors);
}

/* Get a file record and full name */
u8 fatGetFullName(u32 hdr_idx, FatFullRecord *frec) {

    return FatReadRecord(hdr_idx, &frec->rec, frec->name);
}

/* Open a directory */
u8 fatOpenDir(u32 cluster) {

    u8 resp;
    FatRecord rec;

    currentDir.size = 0;
    currentDir.entry_cluster = cluster;
    currentDir.is_root = 0;
    if (cluster > 2) {
        resp = FatReadRecord(cluster, &rec, NULL);
        if (resp) return resp;
        if (!rec.is_dir) return 0xF6;
        cluster = rec.cluster;
    }
    if (cluster <= 2) {
        cluster = 2;
        currentDir._10 = 0;
    }
    FatSetCluster(&currentDir.pos, cluster);
    currentDir.current_idx = currentDir.pos.sector * 16;
    currentDir._10 = currentDir.current_idx | 1;
    return 0;
}

/* Read up to 1024 entries from the directory into the record table */
u8 fatReadDir(u32 cluster) {

    u8 resp;
    FatRecordHdr *hdr;
    FullDir *fdir = (FullDir *)&currentDir;
    u32 hdr_idx;
    u8 skip = 0;

    resp = fatOpenDir(cluster);
    if (resp) return resp;

    hdr_idx = currentDir.pos.sector * 16;
    for (;;) {

        resp = fatCacheLoadData(currentDir.pos.sector);
        if (resp) return resp;

        hdr = (FatRecordHdr *)dataSector;
        for (u8 i = 0; i < 16; i++, hdr++) {
            if (hdr->name[0] == '\0') {
                currentDir.current_idx = hdr_idx | i;
                return 0;
            }
            if (hdr->name[0] == 0xE5 || hdr->name[0] == '.') {
                skip = 0;
                continue;
            }
            if (hdr->attrib == 0xF) {
                if ((hdr->name[0] & 0xF0) == 0x40) {
                    recordTable[currentDir.size] = hdr_idx | i;
                    skip = 1;
                }
                continue;
            }
            if (hdr->attrib & (2|8)) {
                skip = 0;
                continue;
            }
            if (!skip) {
                recordTable[fdir->dir.size] = hdr_idx | i;
            }
            if (++currentDir.size == 1024) {
                currentDir.current_idx = hdr_idx | i;
                return 0;
            }
            skip = 0;
        }
        hdr_idx += 16;
        currentDir.pos.in_cluster_ptr++;
        currentDir.pos.sector++;
        if (currentDir.pos.in_cluster_ptr == current_fat.cluster_size) {
            resp = fatGetNextCluster(&currentDir.pos);
            if (resp == 0xF3) {
                currentDir.is_root = 1;
                break;
            }
            if (resp) return resp;
            hdr_idx = currentDir.pos.sector * 16;
        }
    }
    return 0;
}

/* Find the record location of a path */
u8 fatFindRecord(u8 *name, u32 *hdr_idx) {

    u8 resp;
    u16 i;
    u8 n;
    FatFullRecord frec;
    if (*name != '\0') {
        *hdr_idx = 0;
        for (;;) {
            while (*name == '/') name++;
            for (n = 0; name[n] != '\0'; n++) {
                if (name[n] == '/') break;
            }
            resp = fatReadDir(*hdr_idx);
            if (resp) return resp;
            for (i = 0; i < currentDir.size; i++) {
                resp = FatReadRecord(FatGetRecord(i), &frec.rec, frec.name);
                if (resp) return resp;
                if (frec.name[n] == '\0' && streql(name, frec.name, n)) {
                    *hdr_idx = frec.rec.hdr_idx;
                    if (name[n] == '\0') return 0;
                    break;
                }
            }
            if (i == currentDir.size) return name[n] == '\0' ? 0xF0 : 0xF7;
            name += n;
        }
    }
    return 0xFB;
}

/* Open a file */
u8 fatOpenFileByName(u8 *name, u16 wr_sectors) {

    u8 resp;
    u32 hdr_idx;
    resp = fatFindRecord(name + (name[0] == '/'), &hdr_idx);
    if (resp) return resp;
    resp = fatOpenFile(hdr_idx, wr_sectors);
    return resp;
}

/* Mount the filesystem */
u8 fatInit(FatWork *work) {

    u8 resp;
    u32 pbr_entry;
    u32 fat_entry;
    dataSector = work->data_sector;
    tableSector = work->table_sector;
    recordTable = work->table_buff;
    memfill(&fat_cache, 0xFF, sizeof(FatCache));
    fat_cache._08 = 0;
    fat_cache.table_changed = 0;
    fat_cache.data_changed = 0;
    currentDir.entry_cluster = 0x0FFFFFFF;
    resp = fatCacheLoadData(0);
    if (resp) return resp;
    if (dataSector[0x1FE] != 0x55) return 0xF8;
    if (dataSector[0x1FF] != 0xAA) return 0xF8;
    if (dataSector[0x00] == 0xEB && dataSector[0x02] == 0x90) {
        pbr_entry = 0;
    }
    else {
        u8 x = dataSector[0x1C2] - 11;
        if (x > 1) return 0xF8;
        pbr_entry = FatGet32(&dataSector[0x1C6]);
        resp = fatCacheLoadData(pbr_entry);
        if (resp) return resp;
    }
    if (dataSector[0x16] != 0) return 0xF8;
    if (!StrCmp(&dataSector[0x52], (u8 *)"FAT", 3) && !StrCmp(&dataSector[0x03], (u8 *)"MSDOS", 5)) return 0xF8;
    current_fat.cluster_size = dataSector[0x0D];
    fat_entry = pbr_entry + FatGet16(&dataSector[0x0E]);
    current_fat._0C = FatGet32(&dataSector[0x20]);
    current_fat.sectors_per_fat = FatGet32(&dataSector[0x24]);
    current_fat.fat_entry = fat_entry;
    current_fat.data_entry = current_fat.sectors_per_fat * 2 + fat_entry;
    current_fat._10 = 1024 / current_fat.cluster_size;
    return 0;
}
