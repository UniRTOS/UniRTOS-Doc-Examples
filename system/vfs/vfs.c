#include "qosa_log.h"
#include "qosa_def.h"
#include "qosa_sys.h"
#include "qosa_virtual_file.h"

#define QOS_LOG_TAG                   LOG_TAG

#define UNIR_VFS_DEMO_TASK_STACK_SIZE 4096

/**
 * @brief 遍历指定目录下所有文件和子目录，输出详细信息
 */
static void unir_vfs_demo_dir_list(char *file_path)
{
    QOSA_VFS_DIR             *dir = QOSA_NULL;
    struct qosa_vfs_dirent_t *entry = QOSA_NULL;
    struct qosa_vfs_stat_t    st = {0};
    qosa_int64_t              size = 0;
    qosa_int32_t              ret = 0;
    char                      child[QOSA_VFS_PATH_MAX] = {0};

    /* 打开目录 */
    dir = qosa_vfs_opendir(file_path);
    if (dir == QOSA_NULL)
    {
        QLOGE("dir open err=%d", qosa_get_errno());
        return;
    }

    /* 循环读取目录条目 */
    while ((entry = qosa_vfs_readdir(dir)) != QOSA_NULL)
    {
        QLOGV("%s\n", entry->d_name);

        if (entry->d_type == QOSA_VFS_DT_REG)
        {
            /* 普通文件：获取文件状态信息 */
            char path[1024] = {0};
            qosa_snprintf(path, sizeof(path), "./%s", entry->d_name);

            if (qosa_vfs_stat(path, &st) == -1)
            {
                QLOGV("stat");
            }
            else
            {
                QLOGV("File name: %s", entry->d_name);
                QLOGV("File size: %ld bytes", (long)st.st_size);
                QLOGV("File mode: %o", (int)st.st_mode);
            }
        }
        else if (entry->d_type == QOSA_VFS_DT_DIR)
        {
            /* 子目录：获取目录总大小 */
            qosa_memset(child, 0, sizeof(child));
            qosa_snprintf(child, sizeof(child), "%s/%s", file_path, entry->d_name);
            size = qosa_vfs_dir_total_size(child);
            QLOGV("Dir name: [%s],[%s]", entry->d_name, child);
            qosa_int32_t high = ((size >> 32) & 0xFFFFFFFF);
            qosa_int32_t low = (size & 0xFFFFFFFF);
            QLOGV("Dir size: %d,%d bytes", high, low);
        }
    }
    ret = qosa_vfs_closedir(dir);
    QLOGD("close ret=%d", ret);
}

/**
 * @brief 目录操作测试：创建目录和文件、遍历、删除
 */
static void unir_vfs_demo_dir_test(void)
{
    QOSA_VFS_DIR *dir = QOSA_NULL;
    qosa_int32_t  ret = 0;

    QLOGV("test dir");

    /* 尝试打开目录，不存在则创建 */
    dir = qosa_vfs_opendir("./testdir");
    if (dir == QOSA_NULL)
    {
        QLOGE("dir not exist");
        ret = qosa_vfs_mkdir("./testdir", 0);
        if (ret != 0)
        {
            QLOGE("mkdir err=%d", qosa_get_errno());
            return;
        }
    }

    /* 创建测试文件和子目录 */
    ret = qosa_vfs_creat("./testdir/vfs_test1.txt", 0);
    if (ret < 0)
    {
        QLOGE("creat file err=%d", qosa_get_errno());
    }
    ret = qosa_vfs_creat("./testdir/vfs_test2.txt", 0);
    if (ret < 0)
    {
        QLOGE("creat file2 err=%d", qosa_get_errno());
    }
    ret = qosa_vfs_mkdir("./testdir/subdir", 0);
    if (ret != 0)
    {
        QLOGE("subdir mkdir err=%d", qosa_get_errno());
    }

    /* 遍历目录内容 */
    unir_vfs_demo_dir_list("./testdir");

    /* 关闭目录 */
    dir = qosa_vfs_opendir("./testdir");
    if (dir == QOSA_NULL)
    {
        QLOGE("dir open err=%d", qosa_get_errno());
        return;
    }
    ret = qosa_vfs_closedir(dir);
    QLOGD("close dir ret=%d", ret);

    /* 尝试删除非空目录（预期失败） */
    ret = qosa_vfs_rmdir("./testdir");
    QLOGD("remove dir ret=%d", ret);

    /* 递归删除目录及所有内容 */
    ret = qosa_vfs_rmdir_recursive("./testdir");
    QLOGD("remove dir ret=%d", ret);
}

/**
 * @brief 文件操作测试：创建、写入、读取、状态查询、删除
 */
static void unir_vfs_demo_file_test(void)
{
    int                    fd = 0;
    int                    ret = 0;
    char                   data[10 + 1] = {0};
    struct qosa_vfs_stat_t stat = {0};

    QLOGV("test file");

    /* 打开或创建文件，读写模式 */
    fd = qosa_vfs_open("./vfs_test.txt", QOSA_VFS_O_CREAT | QOSA_VFS_O_RDWR);
    if (fd < 0)
    {
        QLOGE("open dir error!!");
        return;
    }

    /* 写入测试数据 */
    qosa_snprintf(data, 10, "%s", "1234567890");
    ret = qosa_vfs_write(fd, data, 10);
    QLOGD("write ret=%d", ret);

    /* 获取文件状态信息 */
    ret = qosa_vfs_fstat(fd, &stat);
    if (ret == 0)
    {
        QLOGD("size=%d", stat.st_size);
    }

    /* 将文件指针移回起始位置 */
    qosa_vfs_lseek(fd, 0, QOSA_VFS_SEEK_SET);

    /* 读取文件数据 */
    qosa_memset(data, 0, sizeof(data));
    ret = qosa_vfs_read(fd, data, 10);
    QLOGD("read ret=%d,data=[%s]", ret, data);

    /* 关闭并删除测试文件 */
    qosa_vfs_close(fd);
    qosa_vfs_unlink("./vfs_test.txt");
}

static void unir_vfs_task_handler(void *argv)
{
    QOSA_UNUSED(argv);
    qosa_task_sleep_sec(10);
    unir_vfs_demo_file_test();
    unir_vfs_demo_dir_test();
}

void unir_vfs_demo_init(void)
{
    int         err = 0;
    qosa_task_t vfs_task = QOSA_NULL;

    err = qosa_task_create(&vfs_task, UNIR_VFS_DEMO_TASK_STACK_SIZE, QOSA_PRIORITY_NORMAL,
                           "vfs_demo", unir_vfs_task_handler, QOSA_NULL);
    if (err != QOSA_OK)
    {
        QLOGE("task create error");
        return;
    }
}