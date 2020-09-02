## MySQL8

This is the MySQL shared branch of kunpengcompute. The mysql-cluster-8.0.19-arm branch is optimized for ARM based on the [mysql-cluster-8.0.19](https://github.com/mysql/mysql-server/tree/mysql-cluster-8.0.19). Other branches of this project are source code released by Oracle. 

Opt-in features will be provided as patch files in the patch directory. Currently the patch directory includes 3 patch files:
1. The 0001-ARM-TRX_SYS-Hash-Table.patch
The trx_sys is the transaction system central memory data structure. In the past a single latch protected most fields in this structure. In this patch, a lock-free hash is used to optimize a key field named rw_trx_ids, which frees the readview management from the latch competition. [read more](https://support.huaweicloud.com/fg-kunpengdbs/kunpengpatch_20_0007.html)

2. The 0001-MYSQL-BINLOG-CRC32-ARM-INNOBASE-CRC32C-ARM.patch
In this patch, a crc32 function accelerated by ARM specific instrument is provided. Scenarios in ARM with binlog enabled will benefit from this.

3. The 0001-ARM-Buffer-Pool-NUMA-Aware.patch
Under NUMA, a processor can access its own local memory faster than non-local memory. In this patch, we bring the benefits of NUMA for particular workloads, where the data is associated strongly with certain tasks or users. [read more](https://support.huaweicloud.com/fg-kunpengdbs/kunpengpatch_20_0004.html)


## License

This project is licensed by the GPL-2.0 authorization agreement in the [MySQL 8 LICENSE](https://github.com/mysql/mysql-server/blob/8.0/LICENSE) file. For details about the agreement, see the LICENSE file in the project.