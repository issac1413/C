/*
 * iostat.c for 2.6.* with file /proc/diskstat
 * Linux I/O performance monitoring utility
 */
//#include "tsar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <sys/param.h>
#include <linux/major.h>

#include <string>
using namespace std;

//char *io_usage = "    --io                Linux I/O performance";

#define LEN_4096 4096
//#define U_64 unsigned long long
#define MAX_PARTITIONS 16
#define IO_FILE "/proc/diskstats"
#define ITEM_SPLIT      ";"
#define DATA_SPLIT      ","

struct PartInfo
{
    PartInfo& operator=(const PartInfo& source)
    {
        memcpy(this, &source, sizeof(*this));
        return *this;
    }

    unsigned int major; /* Device major number */
    unsigned int minor; /* Device minor number */
    char name[48];
};

PartInfo* aPartition = NULL;//[MAX_PARTITIONS];

struct blkio_info {
    unsigned long long rd_ios;  /* Read I/O operations */
    unsigned long long rd_merges;   /* Reads merged */
    unsigned long long rd_sectors; /* Sectors read */
    unsigned long long rd_ticks;    /* Time in queue + service for read */
    unsigned long long wr_ios;  /* Write I/O operations */
    unsigned long long wr_merges;   /* Writes merged */
    unsigned long long wr_sectors; /* Sectors written */
    unsigned long long wr_ticks;    /* Time in queue + service for write */
    unsigned long long ticks;   /* Time of requests in queue */
    unsigned long long aveq;    /* Average queue length */
};
blkio_info* new_blkio = NULL;//[MAX_PARTITIONS];

#define STATS_IO_SIZE       (sizeof(struct blkio_info))

char buffer[256];       /* Temporary buffer for parsing */
FILE *iofp;                     /* /proc/diskstats*/
int print_partition = 0;
int print_device = 1;

unsigned int n_partitions;  /* Number of partitions */

//static struct mod_info io_info[] = {
//    {" rrqms", DETAIL_BIT,  MERGE_SUM,  STATS_NULL},
//    {" wrqms", DETAIL_BIT,  MERGE_SUM,  STATS_NULL},
//    {"    rs", DETAIL_BIT,  MERGE_SUM,  STATS_NULL},
//    {"    ws", DETAIL_BIT,  MERGE_SUM,  STATS_NULL},
//    {" rsecs", DETAIL_BIT,  MERGE_SUM,  STATS_NULL},
//    {" wsecs", DETAIL_BIT,  MERGE_SUM,  STATS_NULL},
//    {"rqsize", DETAIL_BIT,  MERGE_AVG,  STATS_NULL},
//    {"qusize", DETAIL_BIT,  MERGE_AVG,  STATS_NULL},
//    {" await", DETAIL_BIT,  MERGE_AVG,  STATS_NULL},
//    {" svctm", DETAIL_BIT,  MERGE_AVG,  STATS_NULL},
//    {"  util", SUMMARY_BIT,  MERGE_AVG,  STATS_NULL}
//};

#ifndef IDE_DISK_MAJOR
#define IDE_DISK_MAJOR(M) ((M) == IDE0_MAJOR || (M) == IDE1_MAJOR || \
        (M) == IDE2_MAJOR || (M) == IDE3_MAJOR || \
        (M) == IDE4_MAJOR || (M) == IDE5_MAJOR || \
        (M) == IDE6_MAJOR || (M) == IDE7_MAJOR || \
        (M) == IDE8_MAJOR || (M) == IDE9_MAJOR)
#endif  /* !IDE_DISK_MAJOR */

#ifndef SCSI_DISK_MAJOR
#ifndef SCSI_DISK8_MAJOR
#define SCSI_DISK8_MAJOR 128
#endif
#ifndef SCSI_DISK15_MAJOR
#define SCSI_DISK15_MAJOR 135
#endif
#define SCSI_DISK_MAJOR(M) ((M) == SCSI_DISK0_MAJOR || \
        ((M) >= SCSI_DISK1_MAJOR && \
         (M) <= SCSI_DISK7_MAJOR) || \
        ((M) >= SCSI_DISK8_MAJOR && \
         (M) <= SCSI_DISK15_MAJOR))
#endif  /* !SCSI_DISK_MAJOR */

#ifndef DEVMAP_MAJOR
#define DEVMAP_MAJOR 253
#endif

#ifndef COMPAQ_MAJOR
#define COMPAQ_CISS_MAJOR   104
#define COMPAQ_CISS_MAJOR7  111
#define COMPAQ_SMART2_MAJOR 72
#define COMPAQ_SMART2_MAJOR7    79
#define COMPAQ_MAJOR(M) (((M) >= COMPAQ_CISS_MAJOR && \
            (M) <= COMPAQ_CISS_MAJOR7) || \
        ((M) >= COMPAQ_SMART2_MAJOR && \
         (M) <= COMPAQ_SMART2_MAJOR7))
#endif /* !COMPAQ_MAJOR */

void
handle_error(const char *string, int error)
{
    if (error) {
        fputs("iostat: ", stderr);
        if (errno)
            perror(string);
        else
            fprintf(stderr, "%s\n", string);
        exit(EXIT_FAILURE);
    }
}

int
printable(unsigned int major, unsigned int minor)
{
    if (IDE_DISK_MAJOR(major)) {
        return (!(minor & 0x3F) && print_device)
            || ((minor & 0x3F) && print_partition);
    } else if (SCSI_DISK_MAJOR(major)) {
        return (!(minor & 0x0F) && print_device)
            || ((minor & 0x0F) && print_partition);
    } else if(COMPAQ_MAJOR(major)){
        return (!(minor & 0x0F) && print_device)
            || ((minor & 0x0F) && print_partition);
    }
    /*
    } else if(DEVMAP_MAJOR == major){
        return 0;
    }
    */
    return 1;   /* if uncertain, print it */
}

/* Get partition names.  Check against match list */
void
initialize()
{
    const char *scan_fmt = NULL;

    scan_fmt = "%4d %4d %31s %u";

    while (fgets(buffer, sizeof(buffer), iofp)) {
        unsigned int reads = 0;
        struct PartInfo curr;

        if (sscanf(buffer, scan_fmt, &curr.major, &curr.minor,
                    curr.name, &reads) == 4) {
            unsigned int p;

            for (p = 0; p < n_partitions
                    && (aPartition[p].major != curr.major
                        || aPartition[p].minor != curr.minor);
                    p++);

            if (p == n_partitions && p < MAX_PARTITIONS) {
                //if (reads && printable(curr.major, curr.minor)) {
                if (reads ){
                    aPartition[p] = curr;
                    n_partitions = p + 1;
                }
            }
        }
    }
}

void
get_kernel_stats()
{
    const char *scan_fmt = NULL;

    scan_fmt = "%4d %4d %*s %u %u %llu %u %u %u %llu %u %*u %u %u";

    rewind(iofp);
    while (fgets(buffer, sizeof(buffer), iofp)) {
        int items;
        struct PartInfo curr;
        struct blkio_info blkio;
        memset(&blkio, 0, STATS_IO_SIZE);
        items = sscanf(buffer, scan_fmt,
                &curr.major, &curr.minor,
                &blkio.rd_ios, &blkio.rd_merges,
                &blkio.rd_sectors, &blkio.rd_ticks,
                &blkio.wr_ios, &blkio.wr_merges,
                &blkio.wr_sectors, &blkio.wr_ticks,
                &blkio.ticks, &blkio.aveq);

        /*
         * Unfortunately, we can report only transfer rates
         * for partitions in 2.6 kernels, all other I/O
         * statistics are unavailable.
         */
        if (items == 6) {
            blkio.rd_sectors = blkio.rd_merges;
            blkio.wr_sectors = blkio.rd_ticks;
            blkio.rd_ios = 0;
            blkio.rd_merges = 0;
            blkio.rd_ticks = 0;
            blkio.wr_ios = 0;
            blkio.wr_merges = 0;
            blkio.wr_ticks = 0;
            blkio.ticks = 0;
            blkio.aveq = 0;
            items = 12;
        }

        if (items == 12) {
            unsigned int p;

            /* Locate partition in data table */
            for (p = 0; p < n_partitions; p++) {
                if (aPartition[p].major == curr.major
                        && aPartition[p].minor == curr.minor) {
                    new_blkio[p] = blkio;
                    break;
                }
            }
        }
    }
}

/*
 * Print out statistics.
 * extended form is:
 *   read merges
 *   write merges
 *   read io requests
 *   write io requests
 *   kilobytes read
 *   kilobytes written
 *   average queue length
 *   average waiting time (queue + service)
 *   average service time at disk
 *   average disk utilization.
 */

void report()
{
    char buf[LEN_4096];
    memset(buf, 0, LEN_4096);
    unsigned int p;

    string strRead = "BLK_READ_PER=";
    string strWrite = "BLK_WRITE_PER=";
    string strTps = "TPS=";
    string strSubObj = "SUBOBJ=";

    for (p = 0; p < n_partitions; p++) {
        if(p == 0)
        {
            strSubObj += aPartition[p].name;
            snprintf(buf,sizeof(buf)-1,"%llu",new_blkio[p].rd_ios+new_blkio[p].wr_ios);
            strTps += buf;

            snprintf(buf,sizeof(buf)-1,"%llu",new_blkio[p].rd_sectors/2);
            strRead += buf;

            snprintf(buf,sizeof(buf)-1,"%llu",new_blkio[p].wr_sectors/2);
            strWrite += buf;
        }
        else
        {
            strSubObj += ",";
            strSubObj += aPartition[p].name;
            snprintf(buf,sizeof(buf)-1,",%llu",new_blkio[p].rd_ios+new_blkio[p].wr_ios);
            strTps += buf;

            snprintf(buf,sizeof(buf)-1,",%llu",new_blkio[p].rd_sectors*512);
            strRead += buf;

            snprintf(buf,sizeof(buf)-1,",%llu",new_blkio[p].wr_sectors*512);
            strWrite += buf;
        }
    }

    if(p > 0)
    {
        snprintf(buf, sizeof(buf)-1, "/usr/local/cagent/bin/cagent -i CE.IO -V \"%s||%s||%s||%s\" > /dev/null 2>&1",
            strSubObj.c_str(),
            strRead.c_str(),
            strWrite.c_str(),
            strTps.c_str());
        printf("%s\n",buf);
        system(buf);
    }

    return;
}

void read_io_stat()
{
    aPartition = new PartInfo[MAX_PARTITIONS];
    new_blkio = new blkio_info[MAX_PARTITIONS];
    setlinebuf(stdout);
    /*open current io statistics file*/
    iofp = fopen(IO_FILE, "r");
    handle_error("Can't open /proc/diskstats", !iofp);
    initialize();
    get_kernel_stats();

    rewind(iofp);
    if (NULL != iofp) {
        if (fclose(iofp) < 0) {
            return;
        }
        iofp =NULL;
    }

    report();
    delete aPartition;
    delete new_blkio;
}


int main(int argc, char* argv[])
{
    read_io_stat();
    return 0;
}
