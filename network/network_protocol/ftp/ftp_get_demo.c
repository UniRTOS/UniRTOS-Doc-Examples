#include "qosa_sys.h"
#include "qosa_def.h"
#include "qosa_log.h"
#include "qurl.h"
#include "qosa_virtual_file.h"

#define QOS_LOG_TAG                     LOG_TAG

#define UNIR_FTP_DEMP_PDPID             1

 /*
 * FTP server connection configuration information
 *
 * The following macro definitions are used to configure FTP server connection parameters,
 * including host address, username, password, and port number.
 * These parameters will be used to establish a connection with the FTP server.
 */
#define QURL_FTP_DEMO_HOSTNAME          "xxx"  /* FTP server host IP address */
#define QURL_FTP_DEMO_USERNAME          "test" /* FTP server login username */
#define QURL_FTP_DEMO_PASSNAME          "test" /* FTP server login password */
#define QURL_FTP_DEMO_SERVPORT          21     /* FTP server port number */

#define QURL_FTP_HOSTNAME_MAX_LEN       256    /* Maximum length definition for FTP hostname */
#define QURL_FTP_USERNAME_MAX_LEN       256    /* Maximum length definition for FTP username */
#define QURL_FTP_PASSNAME_MAX_LEN       256    /* Maximum length definition for FTP password */
#define QURL_FTP_DEMO_TIMEOUT           90     /* FTP connection timeout definition */  

/*
 * FTP test related macro definitions
 * Defines directory paths and filename constants used for FTP testing
 */
#define QURL_FTP_DEMO_TEST_DIR          "ftp_test_dir"        /* FTP test directory name */
#define QURL_FTP_DEMO_SERVER_FILENAME   "ftp_test1.txt"       /* Test filename on FTP server */
#define QURL_FTP_DEMO_DOWNLOAD_FILENAME "/user/ftp_test2.txt" /* Source path for download file */

/**
 * @brief FTP connection SSL encryption type enumeration definition
 */
typedef enum
{
    QURL_FTP_DEMO_SSL_TYPE_NONE, /* Connect to FTP server */
    QURL_FTP_DEMO_SSL_TYPE_IMP,  /* Implicit encrypted connection to FTPS server */
    QURL_FTP_DEMO_SSL_TYPE_EXP,  /* Explicit encrypted connection to FTPS server */
} QURL_FTP_DEMO_SSL_TYPE_E;

/**
 * @brief FTP transfer mode enumeration definition
 *
 * Defines two working modes for FTP connection:
 * - Passive Mode: Client connects to the data port specified by the server
 * - Active Mode: Server connects to the data port specified by the client
 */
typedef enum
{
    QURL_FTP_DEMO_MODE_PASSIVE, /* Passive mode */
    QURL_FTP_DEMO_MODE_ACTIVE,  /* Active mode */
} QURL_FTP_DEMO_MODE_E;

/**
 * @brief QURL demonstration data structure
 *
 * This structure is used to store QURL demonstration related data information,
 * including data buffer, file descriptor, and usage status information.
 */
typedef struct
{
    char        *data_buf;  /*!< Pointer to buffer address */
    qosa_int32_t total_len; /*!< Total size of buffer */
    qosa_int32_t used_len;  /*!< Length of buffer already used */
    int          fd;        /*!< File descriptor */
    qosa_bool_t  is_file;   /*!< File identifier flag */
} qurl_demo_data_t;

/**
 * FTP client configuration structure
 *
 * This structure is used to store various configuration parameters required for FTP client connection and operations,
 * including server connection information, authentication information, transmission configuration, and status information.
 */
typedef struct
{
    qurl_core_t              qurl;                                /*!< URL core configuration information */
    qurl_tls_cfg_t           qurl_tls_cfg;                        /*!< TLS security configuration information */
    char                     hostname[QURL_FTP_HOSTNAME_MAX_LEN]; /*!< FTP server hostname */
    char                     username[QURL_FTP_USERNAME_MAX_LEN]; /*!< Username */
    char                     password[QURL_FTP_PASSNAME_MAX_LEN]; /*!< Password */
    int                      timeout;                             /*!< Connection timeout time (seconds) */
    int                      server_port;                         /*!< FTP server port number */
    int                      transfer_type;                       /*!< Data transmission mode */
    int                      skip_pasv_ip;                        /*!< Whether to skip passive mode IP check */
    int                      status;                              /*!< Current connection status */
    int                      sslCtx;                              /*!< SSL context */
    int                      pdp_cid;                             /*!< PDP context ID */
    int                      ftp_port;                            /*!< FTP data port, configured in active mode */
    int                      ipv6_extend;                         /*!< IPv6 extension support flag */
    int                      last_reply_code;                     /*!< Last server response code */
    char                    *currentDir;                          /*!< Current working directory */
    char                    *last_reply_Dir;                      /*!< Directory information in last response */
    char                    *server_ap;                           /*!< FTP server absolute path */
    char                    *url;                                 /*!< URL */
    qurl_demo_data_t         data;                                /*!< Upload or download data */
    QURL_FTP_DEMO_SSL_TYPE_E ssl_enable;                          /*!< Encryption method */
    QURL_FTP_DEMO_MODE_E     mode;                                /*!< Working mode */
} qurl_demo_ftp_t;

/**
 * @brief Read specified length data from FTP file descriptor to buffer
 * @param buf Target buffer pointer for data reading
 * @param fd  File descriptor, used to identify the file to read
 * @param len Length of data to read
 * @return 0 indicates successful reading, -1 indicates reading failure
 */
static int qurl_ftp_demo_file_read(qosa_uint8_t *buf, int fd, int len)
{
    qosa_size_t temp_len = 0;
    int         ret = 0;

    // Loop to read data until all is read
    while (temp_len < len)
    {
        ret = qosa_vfs_read(fd, buf + temp_len, len - temp_len);
        // Handle file read error
        if (ret <= 0)
        {
            qosa_vfs_close(fd);
            fd = 0;
            // If file reading fails, no need to continue processing
            return -1;
        }
        temp_len += ret;
    }
    return 0;
}

/**
 * @brief FTP file write function, write buffer data to specified file descriptor
 *
 * @param fd File descriptor, used to identify the file to write to
 * @param buf Pointer to the buffer containing data to write
 * @param size Total size of data to write
 * @return qosa_int32_t Returns 0 on successful write, -1 on write failure
 */
static qosa_int32_t qurl_ftp_demo_vfs_write(int fd, char *buf, long size)
{
    qosa_size_t temp_len = 0;
    int         ret = 0;

    // Loop to write data until all is written
    while (temp_len < size)
    {
        ret = qosa_vfs_write(fd, buf + temp_len, size - temp_len);
        // Handle file write error
        if (ret <= 0)
        {
            qosa_vfs_close(fd);
            // If file writing fails, no need to continue processing
            return -1;
        }
        temp_len += ret;
    }
    return 0;
}

static long qurl_ftp_demo_write_cb(void *ptr, long size, void *stream)
{
    return size;
}

/**
 * @brief Initialize FTP demonstration configuration parameters
 *
 * This function is used to initialize FTP demonstration related configuration parameters,
 * including basic information such as hostname, username, password, etc.
 */
static int qurl_ftp_demo_init(qurl_demo_ftp_t *ftp_ptr)
{
    /* Check input parameter validity */
    if (ftp_ptr == QOSA_NULL)
    {
        return -1;
    }

    /* Initialize FTP server basic information */
    qosa_snprintf(ftp_ptr->hostname, QURL_FTP_HOSTNAME_MAX_LEN, "%s", QURL_FTP_DEMO_HOSTNAME);
    qosa_snprintf(ftp_ptr->username, QURL_FTP_USERNAME_MAX_LEN, "%s", QURL_FTP_DEMO_USERNAME);
    qosa_snprintf(ftp_ptr->password, QURL_FTP_PASSNAME_MAX_LEN, "%s", QURL_FTP_DEMO_PASSNAME);

    /* Set FTP connection parameters */
    ftp_ptr->timeout = QURL_FTP_DEMO_TIMEOUT;
    ftp_ptr->server_port = QURL_FTP_DEMO_SERVPORT;
    ftp_ptr->pdp_cid = UNIR_FTP_DEMP_PDPID;

    /* Set FTP transmission mode and security options */
    ftp_ptr->transfer_type = 0;
    ftp_ptr->skip_pasv_ip = 1;
    ftp_ptr->ssl_enable = QURL_FTP_DEMO_SSL_TYPE_NONE;
    ftp_ptr->mode = QURL_FTP_DEMO_MODE_ACTIVE;

    return 0;
}


/**
 * FTP download data callback function
 *
 * @param buf: Pointer to received data buffer
 * @param size: Size of data chunk
 * @param arg: User-defined parameter
 *
 * @return Returns actual processed data size, returns 0 if processing fails
 */
unsigned long qurl_ftp_download_data_cb(unsigned char *buf, long size, void *arg)
{
    qurl_demo_ftp_t *ftp_ptr = QOSA_NULL;
    int              ret = 0;

    /* Get FTP related information structure pointer */
    ftp_ptr = (qurl_demo_ftp_t *)arg;

    /* Write downloaded data to file */
    ret = qurl_ftp_demo_vfs_write(ftp_ptr->data.fd, (char *)buf, size);
    if (ret < 0)
    {
        return 0;
    }

    /* Update total length of downloaded data and record log */
    ftp_ptr->data.used_len += size;
    QLOGD("size=%d,%d", size, ftp_ptr->data.used_len);
    return size;
}

/**
 * @brief FTP file download demonstration function, used to download specified file from FTP server to local.
 *
 * This function constructs FTP URL and configures qurl_core module to complete file download operation.
 *
 * @param ftp_ptr Pointer to FTP demonstration control structure, containing host information, authentication information, URL, etc.
 * @param filename Filename to download (without path).
 *
 * @note This demonstrates getting files from current directory
 */
void qurl_ftp_demo_get(qurl_demo_ftp_t *ftp_ptr, char *filename)
{
    qurl_ecode_e      res = QURL_OK;         // Operation result status code
    qurl_core_t       qurl = ftp_ptr->qurl;  // qurl core handle
    qurl_demo_data_t *data = QOSA_NULL;      // Data structure pointer
    int               url_len = 0;           // Length required to construct URL

    QLOGV("enter");
    data = &ftp_ptr->data;

    // Set to write to local file
    data->is_file = QOSA_TRUE;

    // Open local file for writing
    if (data->is_file)
    {
        data->fd = qosa_vfs_open(QURL_FTP_DEMO_DOWNLOAD_FILENAME, QOSA_VFS_O_CREAT | QOSA_VFS_O_RDWR);
        if (data->fd < 0)
        {
            QLOGE("open file err");
            return;
        }
    }

    // Free old URL string
    qosa_free(ftp_ptr->url);

    // Calculate length required to construct new URL
    url_len = qosa_strlen(ftp_ptr->hostname) + qosa_strlen(ftp_ptr->currentDir) + qosa_strlen(filename) + 64;

    // Allocate memory to store new URL
    ftp_ptr->url = (char *)qosa_malloc(url_len);
    if (QOSA_NULL == ftp_ptr->url)
    {
        QLOGE("mem err");
        goto exit;
    }

    // Initialize URL memory and construct complete FTP URL
    qosa_memset(ftp_ptr->url, 0, url_len);
    qosa_snprintf(ftp_ptr->url, url_len, "ftp://%s%s/%s", ftp_ptr->hostname, ftp_ptr->currentDir, filename);

    // Reset qurl core module status
    qurl_core_reset(qurl);
    QLOGV("qurl_core_reset");
    QLOGV("url:%s", ftp_ptr->url);

    // Set qurl core module parameters
        qurl_core_setopt(qurl, QURL_OPT_URL, ftp_ptr->url);                           // Set target URL
    qurl_core_setopt(qurl, QURL_OPT_PORT, ftp_ptr->server_port);                  // Set port number
    qurl_core_setopt(qurl, QURL_OPT_USERNAME, ftp_ptr->username);                 // Set username
    qurl_core_setopt(qurl, QURL_OPT_PASSWORD, ftp_ptr->password);                 // Set password
    qurl_core_setopt(qurl, QURL_OPT_READ_CB, QOSA_NULL);                          // Don't set read callback
    qurl_core_setopt(qurl, QURL_OPT_READ_CB_ARG, QOSA_NULL);                      // Don't set read callback parameter
    qurl_core_setopt(qurl, QURL_OPT_READ_HEAD_CB, QOSA_NULL);                     // Don't set read header callback
    qurl_core_setopt(qurl, QURL_OPT_READ_HEAD_CB_ARG, QOSA_NULL);                 // Don't set read header callback parameter
    qurl_core_setopt(qurl, QURL_OPT_WRITE_CB, qurl_ftp_download_data_cb);         // Set write callback function, mandatory, other CBs optional
    qurl_core_setopt(qurl, QURL_OPT_WRITE_CB_ARG, ftp_ptr);                       // Set write callback parameter
    qurl_core_setopt(qurl, QURL_OPT_WRITE_HEAD_CB, QOSA_NULL);                    // Don't set write header callback
    qurl_core_setopt(qurl, QURL_OPT_WRITE_HEAD_CB_ARG, QOSA_NULL);                // Don't set write header callback parameter
    qurl_core_setopt(qurl, QURL_OPT_TIMEOUT_MS, 0L);                              // Set transmission timeout time (0 means use default value)
    qurl_core_setopt(qurl, QURL_OPT_IDLE_TIMEOUT_MS, (ftp_ptr->timeout) * 1000);  // Set idle timeout time
    qurl_core_setopt(qurl, QURL_OPT_NETWORK_ID, ftp_ptr->pdp_cid);                // Set network ID (PDP context)
    qurl_core_setopt(qurl, QURL_OPT_NOBODY, 0L);                                  // Need body
    qurl_core_setopt(qurl, QURL_OPT_UPLOAD_SIZE, -1L);                            // Download mode, don't set upload size

    // If not disabling skip PASV IP, set to default behavior
    if (0 == ftp_ptr->skip_pasv_ip)
    {
        qurl_core_setopt(qurl, QURL_OPT_FTP_SKIP_PASV_IP, 0L);
    }

    // If active mode, set PORT option to "-", can also specify port
    if (ftp_ptr->mode == QURL_FTP_DEMO_MODE_ACTIVE)
    {
        qurl_core_setopt(qurl, QURL_OPT_FTP_PORT, "-");
    }

    // Execute FTP download operation
    res = qurl_core_perform(qurl);
    QLOGD("res=%x", res);

    // Check execution result
    if (res != QURL_OK)
    {
        goto exit;
    }

exit:
    // Close local file handle
    if (data->is_file)
    {
        qosa_vfs_close(data->fd);
    }
    return;
}

/**
 * @brief Get size of specified file on FTP server
 *
 * @param[in,out] ftp_ptr Pointer to FTP demonstration control structure, containing connection information and result storage area
 * @param[in] filename Filename to query size
 *
 * @note This function will not download file content, only get file size. Here the size is obtained through URL
 */
void qurl_ftp_demo_size(qurl_demo_ftp_t *ftp_ptr, char *filename)
{
    qurl_ecode_e      res = QURL_OK;
    qurl_core_t       qurl = ftp_ptr->qurl;
    qurl_demo_data_t *data = QOSA_NULL;
    int               url_len = 0;
    long              filesize = 0;

    QLOGV("enter");
    data = &ftp_ptr->data;
    qosa_memset(data, 0, sizeof(qurl_demo_data_t));

    // Free old URL memory and reconstruct new FTP URL
    qosa_free(ftp_ptr->url);
    url_len = qosa_strlen(ftp_ptr->hostname) + qosa_strlen(ftp_ptr->currentDir) + qosa_strlen(filename) + 64;
    ftp_ptr->url = (char *)qosa_malloc(url_len);
    if (QOSA_NULL == ftp_ptr->url)
    {
        QLOGE("mem err");
        return;
    }
    qosa_memset(ftp_ptr->url, 0, url_len);
    qosa_snprintf(ftp_ptr->url, url_len, "ftp://%s%s%s", ftp_ptr->hostname, ftp_ptr->currentDir, filename);

    // Reset qurl core status and set related options
    qurl_core_reset(qurl);
    QLOGV("qurl_core_reset");
    QLOGV("url:%s", ftp_ptr->url);
    qurl_core_setopt(qurl, QURL_OPT_URL, ftp_ptr->url);
    qurl_core_setopt(qurl, QURL_OPT_PORT, ftp_ptr->server_port);
    qurl_core_setopt(qurl, QURL_OPT_USERNAME, ftp_ptr->username);
    qurl_core_setopt(qurl, QURL_OPT_PASSWORD, ftp_ptr->password);
    qurl_core_setopt(qurl, QURL_OPT_READ_CB, QOSA_NULL);
    qurl_core_setopt(qurl, QURL_OPT_READ_CB_ARG, QOSA_NULL);
    qurl_core_setopt(qurl, QURL_OPT_READ_HEAD_CB, QOSA_NULL);
    qurl_core_setopt(qurl, QURL_OPT_READ_HEAD_CB_ARG, QOSA_NULL);
    qurl_core_setopt(qurl, QURL_OPT_WRITE_CB, QOSA_NULL);
    qurl_core_setopt(qurl, QURL_OPT_WRITE_CB_ARG, QOSA_NULL);
    qurl_core_setopt(qurl, QURL_OPT_WRITE_HEAD_CB, QOSA_NULL);
    qurl_core_setopt(qurl, QURL_OPT_WRITE_HEAD_CB_ARG, QOSA_NULL);
    qurl_core_setopt(qurl, QURL_OPT_TIMEOUT_MS, 0L);
    qurl_core_setopt(qurl, QURL_OPT_IDLE_TIMEOUT_MS, (ftp_ptr->timeout) * 1000);
    qurl_core_setopt(qurl, QURL_OPT_NETWORK_ID, ftp_ptr->pdp_cid);
    qurl_core_setopt(qurl, QURL_OPT_NOBODY, 1L);  // Only get header information, don't transfer data body
    qurl_core_setopt(qurl, QURL_OPT_UPLOAD_SIZE, -1L);

    // Set whether to skip PASV mode returned IP address
    if (0 == ftp_ptr->skip_pasv_ip)
    {
        qurl_core_setopt(qurl, QURL_OPT_FTP_SKIP_PASV_IP, 0L);
    }

    // If active mode, set PORT parameter to "-"
    if (ftp_ptr->mode == QURL_FTP_DEMO_MODE_ACTIVE)
    {
        //Default FTP operations are passive, and thus will not use PORT.
        qurl_core_setopt(qurl, QURL_OPT_FTP_PORT, "-");
    }

    // Execute network request
    res = qurl_core_perform(qurl);

    QLOGD("res=%x", res);
    // Check request result and get file size
    if (QURL_OK == res)
    {
        res = qurl_core_getinfo(qurl, QURL_INFO_RESP_CONTENT_LENGTH, &filesize);
        QLOGD("res=%x,filesize=%ld", res, filesize);
        if ((QURL_OK == res) && (filesize >= 0))
        {
            data->total_len = filesize;
            return;
        }
    }
    return;
}

/**
 * @brief FTP response processing function, used to get current directory information
 *
 * This function parses FTP server's "257" command response, extracts current directory path.
 * When receiving response in format "257 \"path\"", it will parse the path part and store it in data pointed variable.
 *
 * @param ptr Pointer to received data buffer
 * @param size Size of each data item (usually 1)
 * @param data Pointer to variable used to store current directory string
 * @return Returns processed data size
 */
static qosa_size_t qurl_ftp_demo_open_get_current_dir(void *ptr, qosa_size_t size, void *data)
{
    char **current_dir = QOSA_NULL;
    char  *str = (char *)ptr;
    int    realsize = (size);
    int    i = 0;

    /* Check if response data is long enough and starts with "257 \"" indicating current directory response */
    if (realsize > 5)
    {
        if (qosa_memcmp(str, "257 \"", 5) == 0)
        {
            QLOGV("......");
            /* Find position of ending double quote for directory path */
            for (i = 5; i < realsize; i++)
            {
                if (str[i] == '\"')
                {
                    break;
                }
            }
            QLOGD("i=%d", i);
            /* If ending quote is found, extract directory path */
            if (i < realsize)
            {
                /* Check if output parameter is valid */
                if (QOSA_NULL == data)
                {
                    QLOGV("data QOSA_NULL");
                    return realsize;
                }
                current_dir = (char **)data;
                /* Allocate memory to store directory path */
                char *temp_dir = (char *)qosa_malloc(realsize);
                if (QOSA_NULL == temp_dir)
                {
                    QLOGE("mem err");
                    return realsize;
                }
                /* Free old directory string and update to new path */
                qosa_free(*current_dir);
                *current_dir = temp_dir;
                qosa_memset(*current_dir, 0x00, realsize);
                /* Copy directory path (remove quotes) */
                qosa_memcpy(*current_dir, str + 5, i - 5);
                QLOGD("current_dir=%s", *current_dir);
            }
        }
    }

    QLOGD("size=%d", size);
    return realsize;

}

/**
 * @brief Callback function for processing FTP response header data, used to parse FTP server returned status code and directory information.
 *
 * This function is called during FTP communication process, used to process response header data received from FTP server.
 * It parses status code, and under specific status codes (such as 257) extracts current directory information and updates to FTP control structure.
 *
 * @param ptr Pointer to received data buffer
 * @param size Size of received data (unit: bytes)
 * @param stream Pointer to FTP control structure (qurl_demo_ftp_t type)
 * @return Returns processed data size (unit: bytes)
 */
static qosa_size_t qurl_ftp_demo_open_header(void *ptr, qosa_size_t size, void *stream)
{
    qurl_demo_ftp_t *ftp_ptr = (qurl_demo_ftp_t *)stream;
    qosa_size_t      realsize = size;
    char             code_str[4];
    int              code = 0;

    // Ensure data length is sufficient to parse status code (at least 4 bytes)
    if (realsize > 4)
    {
        // Extract first 3 characters as status code string, and convert to integer
        qosa_memcpy(code_str, ptr, 3);
        code_str[3] = '\0';

        code = qosa_atoi(code_str);
        QLOGD("code=%d", code);

        // Check if status code is within legal range (100~600)
        if (code < 100 || code > 600)
        {
            return realsize;
        }

        // Check if stream pointer is null
        if (QOSA_NULL == stream)
        {
            QLOGV("null");
            return realsize;
        }
        ftp_ptr = (qurl_demo_ftp_t *)stream;

        // If status code is 257, indicates current directory information returned
        if (257 == code)
        {
            // Get current directory
            QLOGD("size=%d,ptr=%s", size, ptr);
            qurl_ftp_demo_open_get_current_dir(ptr, size, (void *)(&(ftp_ptr->last_reply_Dir)));

            // Check if directory information obtained successfully
            if (QOSA_NULL == ftp_ptr->last_reply_Dir)
            {
                QLOGD("no mem=%d\n", realsize);
                return realsize;
            }
            else
            {
                QLOGD("last_reply_Dir=%s", ftp_ptr->last_reply_Dir);

                // If old directory information exists, free old memory
                if (ftp_ptr->currentDir != QOSA_NULL)
                {
                    qosa_free(ftp_ptr->currentDir);
                }

                // Allocate new memory and copy current directory information
                ftp_ptr->currentDir = qosa_malloc(qosa_strlen(ftp_ptr->last_reply_Dir) + 1);
                qosa_memset(ftp_ptr->currentDir, 0, qosa_strlen(ftp_ptr->last_reply_Dir) + 1);
                qosa_memcpy(ftp_ptr->currentDir, ftp_ptr->last_reply_Dir, qosa_strlen(ftp_ptr->last_reply_Dir));
            }
        }
    }

    QLOGD("realsize=%d", realsize);
    return realsize;
}

/**
 * @brief Send FTP command request and execute FTP operation.
 *
 * This function is used to send specified command to FTP server, and execute FTP transmission operation based on parameter configuration.
 *
 * @param ftp_ptr Pointer to FTP control structure, containing basic information required for FTP connection.
 * @param pCommand Command string to send to FTP server
 * @param cb Custom write callback function, used to process received data. If QOSA_NULL, use default callback.
 * @param opt_nobody Whether to execute command without downloading data (similar to HEAD request), 1 means don't download data, 0 means download.
 *
 * @note This executes original, custom, non-standard control commands, implemented through QURL_OPT_QUOTE
 */
int qurl_ftp_demo_request_command_quote(qurl_demo_ftp_t *ftp_ptr, char *pCommand, qurl_write_cb cb, int opt_nobody)            
{
    qurl_ecode_e res = QURL_OK;
    qurl_core_t  qurl = ftp_ptr->qurl;
    qurl_slist_t headerlist = QOSA_NULL;

    // Reset core handle status
    qurl_core_reset(qurl);

    // Print and set URL
    QLOGV("url:%s", ftp_ptr->url);
    qurl_core_setopt(qurl, QURL_OPT_URL, ftp_ptr->url);

    // Set basic authentication information like port, username, password
    qurl_core_setopt(qurl, QURL_OPT_PORT, ftp_ptr->server_port);
    qurl_core_setopt(qurl, QURL_OPT_USERNAME, ftp_ptr->username);
    qurl_core_setopt(qurl, QURL_OPT_PASSWORD, ftp_ptr->password);

    // Clear read related callback settings
    qurl_core_setopt(qurl, QURL_OPT_READ_CB, QOSA_NULL);
    qurl_core_setopt(qurl, QURL_OPT_READ_CB_ARG, QOSA_NULL);
    qurl_core_setopt(qurl, QURL_OPT_READ_HEAD_CB, QOSA_NULL);
    qurl_core_setopt(qurl, QURL_OPT_READ_HEAD_CB_ARG, QOSA_NULL);

    // Set default write callback and parameters
    qurl_core_setopt(qurl, QURL_OPT_WRITE_CB, qurl_ftp_demo_write_cb);
    qurl_core_setopt(qurl, QURL_OPT_WRITE_CB_ARG, (void *)ftp_ptr);

    // Clear write header callback settings
    qurl_core_setopt(qurl, QURL_OPT_WRITE_HEAD_CB, QOSA_NULL);
    qurl_core_setopt(qurl, QURL_OPT_WRITE_HEAD_CB_ARG, QOSA_NULL);

    // Set timeout time
    qurl_core_setopt(qurl, QURL_OPT_TIMEOUT_MS, 0L);
    qurl_core_setopt(qurl, QURL_OPT_IDLE_TIMEOUT_MS, (ftp_ptr->timeout) * 1000);

    // Set network ID
    qurl_core_setopt(qurl, QURL_OPT_NETWORK_ID, ftp_ptr->pdp_cid);

    // Set whether to only get response header
    QLOGD("opt_nobody=%d", opt_nobody);
    qurl_core_setopt(qurl, QURL_OPT_NOBODY, opt_nobody);

    // If command is provided, add to QUOTE option
    if (pCommand != QOSA_NULL)
    {
        headerlist = qurl_slist_add_strdup(headerlist, pCommand);
        QLOGV("pCommand:%s", pCommand);
        qurl_core_setopt(qurl, QURL_OPT_QUOTE, headerlist);
    }

    // Decide whether to skip PASV IP check based on configuration
    if (0 == ftp_ptr->skip_pasv_ip)
    {
        qurl_core_setopt(qurl, QURL_OPT_FTP_SKIP_PASV_IP, 0L);
    }

    // If active mode, set PORT to "-" to enable active mode
    if (ftp_ptr->mode == QURL_FTP_DEMO_MODE_ACTIVE)
    {
        // Default FTP operations are passive, and thus will not use PORT.
        qurl_core_setopt(qurl, QURL_OPT_FTP_PORT, "-");
    }

    // If custom write callback function is passed in, replace default callback
    if (cb != QOSA_NULL)
    {
        qurl_core_setopt(qurl, QURL_OPT_WRITE_CB, cb);
        qurl_core_setopt(qurl, QURL_OPT_WRITE_CB, ftp_ptr);
    }

    // Execute FTP request
    res = qurl_core_perform(qurl);
    QLOGD("res=%x", res);

    // Clean up QUOTE list resources
    if (headerlist != QOSA_NULL)
    {
        qurl_core_setopt(qurl, QURL_OPT_QUOTE, QOSA_NULL);
        qurl_slist_del_all(headerlist);  //free the list again
    }

    // Check execution result
    if (res != QURL_OK)
    {
        return res;
    }

    return 0;
}

/**
 * @brief Execute FTP standard command processing flow.
 *
 * This function is used to send a custom command to FTP server, and execute related operations based on configuration.
 * Supports setting callback function to process response data, while supporting control whether to receive response body.
 *
 * @param ftp_ptr       Pointer to FTP session structure, containing basic information required for connection (URL, port, username, etc.).
 * @param pCommand      FTP custom command string to execute.
 * @param cb            Data write callback function pointer, used to process data received from server. If QOSA_NULL, don't set callback.
 * @param opt_nobody    Whether to ignore response body content: 1 means ignore, 0 means receive.
 *
 * @note This executes some FTP regular standard commands
 */
int qurl_ftp_demo_request_command_std(qurl_demo_ftp_t *ftp_ptr, char *pCommand, qurl_write_cb cb, int opt_nobody)
{
    qurl_ecode_e res = QURL_OK;
    qurl_core_t  qurl = ftp_ptr->qurl;

    // Reset core handle status and set basic connection parameters
    qurl_core_reset(qurl);
    QLOGV("url:%s", ftp_ptr->url);
    qurl_core_setopt(qurl, QURL_OPT_URL, ftp_ptr->url);
    qurl_core_setopt(qurl, QURL_OPT_PORT, ftp_ptr->server_port);
    qurl_core_setopt(qurl, QURL_OPT_USERNAME, ftp_ptr->username);
    qurl_core_setopt(qurl, QURL_OPT_PASSWORD, ftp_ptr->password);

    // Set custom FTP command
    QLOGD("pCommand=%s", pCommand);
    qurl_core_setopt(qurl, QURL_OPT_CUSTOMREQUEST, pCommand);

    // Clear all read-write related callback functions and parameter settings
    qurl_core_setopt(qurl, QURL_OPT_READ_HEAD_CB, QOSA_NULL);
    qurl_core_setopt(qurl, QURL_OPT_READ_HEAD_CB_ARG, QOSA_NULL);
    qurl_core_setopt(qurl, QURL_OPT_READ_CB, QOSA_NULL);
    qurl_core_setopt(qurl, QURL_OPT_READ_CB_ARG, QOSA_NULL);

    qurl_core_setopt(qurl, QURL_OPT_WRITE_HEAD_CB, QOSA_NULL);
    qurl_core_setopt(qurl, QURL_OPT_WRITE_HEAD_CB_ARG, QOSA_NULL);
    qurl_core_setopt(qurl, QURL_OPT_WRITE_CB, QOSA_NULL);
    qurl_core_setopt(qurl, QURL_OPT_WRITE_CB_ARG, QOSA_NULL);

    // Set timeout and network parameters
    qurl_core_setopt(qurl, QURL_OPT_TIMEOUT_MS, 0L);
    qurl_core_setopt(qurl, QURL_OPT_IDLE_TIMEOUT_MS, (ftp_ptr->timeout) * 1000);
    qurl_core_setopt(qurl, QURL_OPT_NETWORK_ID, ftp_ptr->pdp_cid);

    // Set whether to ignore response body content
    QLOGD("opt_nobody=%d", opt_nobody);
    qurl_core_setopt(qurl, QURL_OPT_NOBODY, opt_nobody);

    // Decide whether to skip IP address check in PASV mode based on configuration
    if (0 == ftp_ptr->skip_pasv_ip)
    {
        qurl_core_setopt(qurl, QURL_OPT_FTP_SKIP_PASV_IP, 0L);
    }

    // If ACTIVE mode, enable PORT command
    if (ftp_ptr->mode == QURL_FTP_DEMO_MODE_ACTIVE)
    {
        //Default FTP operations are passive, and thus will not use PORT.
        qurl_core_setopt(qurl, QURL_OPT_FTP_PORT, "-");
    }

    // If callback function is provided, set it
    if (cb != QOSA_NULL)
    {
        qurl_core_setopt(qurl, QURL_OPT_WRITE_CB, cb);
        qurl_core_setopt(qurl, QURL_OPT_WRITE_CB_ARG, ftp_ptr);
    }

    // Execute FTP request
    res = qurl_core_perform(qurl);
    // Check for errors
    if (res != QURL_OK)
    {
        return res;
    }
    return 0;

}

/**
 * @brief Execute FTP PWD command to get current working directory
 *
 * This function sends PWD command to FTP server, used to get current working directory path.
 *
 * @param ftp_ptr Pointer to FTP demonstration structure, containing FTP connection related information
 *
 */
void qurl_ftp_demo_pwd(qurl_demo_ftp_t *ftp_ptr)
{
    /* Check if FTP connection handle is valid */
    if (QOSA_NULL == ftp_ptr->qurl)
    {
        QLOGE("path QOSA_NULL");
        return;
    }

    /* Send PWD command to FTP server */
    qurl_ftp_demo_request_command_quote(ftp_ptr, "PWD", QOSA_NULL, 1);
}

/**
 * @brief Delete specified file on FTP server
 *
 * This function is used to send delete file command to FTP server. Based on whether the path is absolute or relative,
 *
 * @param ftp_ptr Pointer to FTP control structure, containing FTP connection related information
 * @param path    File path to delete (can be absolute path or relative path)
 *
 */
void qurl_ftp_demo_delete(qurl_demo_ftp_t *ftp_ptr, char *path)
{
    char *szComand = QOSA_NULL;
    int   szComand_size = 0;
    int   url_len = 0;

    // Parameter validity check: FTP control structure and path cannot be null
    if (QOSA_NULL == ftp_ptr->qurl || path == QOSA_NULL)
    {
        QLOGE("path QOSA_NULL");
        return;
    }

    // Calculate buffer size required to construct command
    if (path != QOSA_NULL)
    {
        szComand_size = qosa_strlen(ftp_ptr->server_ap) + qosa_strlen(path) + 64;
    }
    else
    {
        szComand_size = qosa_strlen(ftp_ptr->server_ap) + 64;
    }

    // Allocate command buffer memory
    szComand = qosa_malloc(szComand_size);
    if (szComand == QOSA_NULL)
    {
        QLOGE("mem err");
        return;
    }
    qosa_memset(szComand, 0, szComand_size);

    // Reallocate and construct current FTP URL path
    qosa_free(ftp_ptr->url);
    url_len = qosa_strlen(ftp_ptr->hostname) + qosa_strlen(ftp_ptr->currentDir) + 64;
    ftp_ptr->url = (char *)qosa_malloc(url_len);
    if (QOSA_NULL == ftp_ptr->url)
    {
        QLOGE("mem err");
        return;
    }
    qosa_memset(ftp_ptr->url, 0, url_len);
    QLOGD("ftp_ptr->currentDir=%s", ftp_ptr->currentDir);
    qosa_snprintf(ftp_ptr->url, url_len, "FTP://%s%s", ftp_ptr->hostname, ftp_ptr->currentDir);

    // Construct DELE command based on whether path starts with '/' to determine absolute or relative path
    if (path[0] == '/')
    {
        // Get list under absolute path
        qosa_snprintf(szComand, szComand_size, "DELE %s", path);
    }
    else
    {
        // Get list under relative path
        qosa_snprintf(szComand, szComand_size, "DELE %s%s", ftp_ptr->currentDir, path);
    }

    // Print constructed command and send FTP request
    QLOGD("szComand=%s", szComand);
    qurl_ftp_demo_request_command_quote(ftp_ptr, szComand, QOSA_NULL, 1);
}

/**
 * @brief FTP client change current working directory (CWD) operation demonstration function
 *
 * @param ftp_ptr Pointer to FTP demonstration control structure, containing connection information and current status
 * @param path    Target path to switch to, can be relative path or absolute path
 *
 * @note Here directory switching is done directly through URL, can also use cwd command
 */
void qurl_ftp_demo_cwd(qurl_demo_ftp_t *ftp_ptr, char *path)
{
    qurl_ecode_e res = QURL_OK;
    qurl_core_t  qurl = ftp_ptr->qurl;
    qurl_slist_t cmd_slist = QOSA_NULL;
    int          url_len = 0;
    int          server_ap_len = 0;
    int          currentdir_len = 0;
    int          ret = 0;

    // Check input parameter validity
    if (QOSA_NULL == ftp_ptr->qurl || QOSA_NULL == path || 0 == qosa_strlen(path))
    {
        QLOGE("path QOSA_NULL");
        return;
    }

    // Free old URL and reallocate memory
    qosa_free(ftp_ptr->url);
    url_len = qosa_strlen(ftp_ptr->hostname) + qosa_strlen(ftp_ptr->currentDir) + qosa_strlen(path) + 64;
    ftp_ptr->url = (char *)qosa_malloc(url_len);
    if (QOSA_NULL == ftp_ptr->url)
    {
        QLOGE("mem err");
        return;
    }
    qosa_memset(ftp_ptr->url, 0, url_len);

    // Handle server application path ending slash
    server_ap_len = qosa_strlen(ftp_ptr->server_ap);
    if ('/' == ftp_ptr->server_ap[server_ap_len - 1])
    {
        server_ap_len = server_ap_len - 1;
    }

    // Construct different FTP URL based on whether path starts with '/'
    if ('/' != path[0])
    {
        // User input relative path, relative to current CWD path
        {
            qosa_snprintf(ftp_ptr->url, url_len, "FTP://%s/%s/", ftp_ptr->hostname, path);
        }
    }
    else
    {
        // User input contains absolute path of server file system
        {
            qosa_snprintf(ftp_ptr->url, url_len, "FTP://%s%s", ftp_ptr->hostname, path);
        }
    }

    // Reset and configure FTP connection parameters
    qurl_core_reset(qurl);
    QLOGV("url:%s", ftp_ptr->url);
    qurl_core_setopt(qurl, QURL_OPT_URL, ftp_ptr->url);
    qurl_core_setopt(qurl, QURL_OPT_PORT, ftp_ptr->server_port);
    qurl_core_setopt(qurl, QURL_OPT_USERNAME, ftp_ptr->username);
    qurl_core_setopt(qurl, QURL_OPT_PASSWORD, ftp_ptr->password);
    qurl_core_setopt(qurl, QURL_OPT_READ_CB, QOSA_NULL);
    qurl_core_setopt(qurl, QURL_OPT_READ_CB_ARG, QOSA_NULL);
    qurl_core_setopt(qurl, QURL_OPT_READ_HEAD_CB, QOSA_NULL);
    qurl_core_setopt(qurl, QURL_OPT_READ_HEAD_CB_ARG, QOSA_NULL);
    qurl_core_setopt(qurl, QURL_OPT_WRITE_CB, qurl_ftp_demo_write_cb);
    qurl_core_setopt(qurl, QURL_OPT_WRITE_CB_ARG, (void *)ftp_ptr);
    qurl_core_setopt(qurl, QURL_OPT_WRITE_HEAD_CB, qurl_ftp_demo_open_header);
    qurl_core_setopt(qurl, QURL_OPT_WRITE_HEAD_CB_ARG, (void *)(ftp_ptr));
    qurl_core_setopt(qurl, QURL_OPT_TIMEOUT_MS, 0L);
    qurl_core_setopt(qurl, QURL_OPT_IDLE_TIMEOUT_MS, (ftp_ptr->timeout) * 1000);
    qurl_core_setopt(qurl, QURL_OPT_NETWORK_ID, ftp_ptr->pdp_cid);
    qurl_core_setopt(qurl, QURL_OPT_NOBODY, 1);

    // Set whether to skip IP address check in PASV mode
    if (0 == ftp_ptr->skip_pasv_ip)
    {
        qurl_core_setopt(qurl, QURL_OPT_FTP_SKIP_PASV_IP, 0L);
    }

    // Set FTP transmission mode (active or passive)
    if (ftp_ptr->mode == QURL_FTP_DEMO_MODE_ACTIVE)
    {
        // Default FTP operations are passive, and thus will not use PORT.
        qurl_core_setopt(qurl, QURL_OPT_FTP_PORT, "-");
    }

    // Execute FTP directory switching operation
    res = qurl_core_perform(qurl);
    QLOGD("res=%x", res);

    // If directory switching successful, execute PWD command to get current directory
    if (res == QURL_OK)
    {
        cmd_slist = qurl_slist_add_strdup(cmd_slist, "PWD");
        qurl_core_setopt(qurl, QURL_OPT_QUOTE, cmd_slist);
        res = qurl_core_perform(qurl);
        if (res == QURL_OK)
        {
            // Clean command list and update current directory path
            qurl_core_setopt(qurl, QURL_OPT_QUOTE, QOSA_NULL);
            qurl_slist_del_all(cmd_slist);

            currentdir_len = qosa_strlen(ftp_ptr->currentDir);
            QLOGD("currentDir=%s", ftp_ptr->currentDir);
            if ('/' != ftp_ptr->currentDir[currentdir_len - 1])
            {
                ftp_ptr->currentDir[currentdir_len] = '/';
                ftp_ptr->currentDir[currentdir_len + 1] = '\0';
            }
        }
    }
    QLOGD("ret=%d", ret);
}

/**
 * @brief FTP file list callback function, used to process FTP server returned file list data
 *
 * @param ptr Pointer to received data buffer pointer
 * @param size Size of received data
 * @param stream Pointer to FTP related information structure pointer
 *
 * @return Returns actual processed data size, returns 0 indicates processing failure or no data to process
 */
static long qurl_ftp_demo_list_cb(unsigned char *ptr, long size, void *stream)
{
    /* Initialize FTP information structure pointer */
    qurl_demo_ftp_t *ftp_ptr = QOSA_NULL;

    /* Type conversion and verify input parameter validity */
    ftp_ptr = (qurl_demo_ftp_t *)stream;
    if (ftp_ptr == QOSA_NULL || size == 0)
    {
        return 0;
    }

    /* Record received FTP list data information */
    QLOGD("size=%d,%s", size, ptr);
    return size;
}

/**
 * @brief Get file list of specified directory via FTP protocol (using NLST command).
 *
 * This function constructs an NLST command request, used to get filename list under a directory on FTP server,
 * and processes results through callback function. Supports specifying directory location through absolute path and relative path.
 *
 * @param ftp_ptr Pointer to FTP control structure, containing connection information and current working directory status information
 * @param dir_path Directory path to list files, can be absolute path or relative path; if QOSA_NULL, list current directory
 *
 */
void qurl_ftp_demo_nlist(qurl_demo_ftp_t *ftp_ptr, char *dir_path)
{
    int   url_len = 0;
    int   server_ap_len = 0;
    int   ret = 0;
    char *szComand = QOSA_NULL;
    int   szComand_size = 0;

    // Check if URL is valid
    if (QOSA_NULL == ftp_ptr->qurl)
    {
        QLOGE("path QOSA_NULL");
        return;
    }

    // Calculate command string memory size required based on whether path exists
    if (dir_path != QOSA_NULL)
    {
        szComand_size = qosa_strlen(ftp_ptr->server_ap) + qosa_strlen(dir_path) + 64;
    }
    else
    {
        szComand_size = qosa_strlen(ftp_ptr->server_ap) + 64;
    }

    // Allocate command string memory
    szComand = qosa_malloc(szComand_size);
    if (szComand == QOSA_NULL)
    {
        QLOGE("mem err");
        return;
    }

    // Reallocate and build FTP URL
    qosa_free(ftp_ptr->url);
    url_len = qosa_strlen(ftp_ptr->hostname) + qosa_strlen(ftp_ptr->currentDir) + 64;
    ftp_ptr->url = (char *)qosa_malloc(url_len);
    if (QOSA_NULL == ftp_ptr->url)
    {
        QLOGE("mem err");
        return;
    }
    qosa_memset(ftp_ptr->url, 0, url_len);
    qosa_snprintf(ftp_ptr->url, url_len, "FTP://%s%s", ftp_ptr->hostname, ftp_ptr->currentDir);

    // Handle path ending slash issue
    server_ap_len = qosa_strlen(ftp_ptr->server_ap);
    if ('/' == ftp_ptr->server_ap[server_ap_len - 1])
    {
        server_ap_len = server_ap_len - 1;
    }

    // Construct NLST command, choose different format based on path type
    if (dir_path == QOSA_NULL || qosa_strlen(dir_path))
    {
        // Get current directory list
        qosa_snprintf(szComand, szComand_size, "NLST %s", ftp_ptr->currentDir);
    }
    else
    {
        if (dir_path[0] == '/')
        {
            // Get list under absolute path
            qosa_snprintf(szComand, szComand_size, "NLST %s", dir_path);
        }
        else
        {
            // Get list under relative path
            qosa_snprintf(szComand, szComand_size, "NLST %s%s", ftp_ptr->currentDir, dir_path);
        }
    }

    // Send FTP command and register callback function to process response
    qurl_ftp_demo_request_command_std(ftp_ptr, szComand, qurl_ftp_demo_list_cb, 0);
    QLOGD("ret=%d", ret);
}


/**
 * @brief List contents of specified directory on FTP server
 *
 * This function is used to send LIST command to FTP server to get file and subdirectory list under specified directory. It will construct corresponding FTP command based on passed path parameter (absolute path or relative path),
 * and process response results through callback mechanism. Before execution, it will rebuild FTP URL and allocate necessary memory space to construct command string.
 *
 * @param ftp_ptr Pointer to FTP control structure, containing current connection status information, hostname, current directory, etc.
 * @param dir_path Directory path to list contents, can be absolute path or relative path; if QOSA_NULL, list current directory contents
 *
 * @return No return value
 */
void qurl_ftp_demo_list(qurl_demo_ftp_t *ftp_ptr, char *dir_path)
{
    int   url_len = 0;           // Store new URL required length
    int   server_ap_len = 0;     // Store server path length
    int   ret = 0;               // Function call return value (not used)
    char *szComand = QOSA_NULL;  // Store constructed FTP command string
    int   szComand_size = 0;     // Command string buffer size

    // Check if FTP URL is valid
    if (QOSA_NULL == ftp_ptr->qurl)
    {
        QLOGE("path QOSA_NULL");
        return;
    }

    // Calculate command string buffer size required based on path parameter
    if (dir_path != QOSA_NULL)
    {
        szComand_size = qosa_strlen(ftp_ptr->server_ap) + qosa_strlen(dir_path) + 64;
    }
    else
    {
        szComand_size = qosa_strlen(ftp_ptr->server_ap) + 64;
    }

    // Allocate command string buffer memory
    szComand = qosa_malloc(szComand_size);
    if (szComand == QOSA_NULL)
    {
        QLOGE("mem err");
        return;
    }

    // Free old URL and reallocate memory
    qosa_free(ftp_ptr->url);
    url_len = qosa_strlen(ftp_ptr->hostname) + qosa_strlen(ftp_ptr->currentDir) + 64;
    ftp_ptr->url = (char *)qosa_malloc(url_len);
    if (QOSA_NULL == ftp_ptr->url)
    {
        QLOGE("mem err");
        return;
    }
    qosa_memset(ftp_ptr->url, 0, url_len);  // Clear newly allocated URL memory
    QLOGD("ftp_ptr->currentDir=%s", ftp_ptr->currentDir);

    // Construct new FTP URL
    qosa_snprintf(ftp_ptr->url, url_len, "FTP://%s%s", ftp_ptr->hostname, ftp_ptr->currentDir);

    server_ap_len = qosa_strlen(ftp_ptr->server_ap);
    // If path ends with '/', remove the last slash character
    if ('/' == ftp_ptr->server_ap[server_ap_len - 1])
    {
        server_ap_len = server_ap_len - 1;
    }

    // Construct LIST command based on path parameter
    if (dir_path == QOSA_NULL || qosa_strlen(dir_path))
    {
        // Get current directory list
        qosa_snprintf(szComand, szComand_size, "LIST %s", ftp_ptr->currentDir);
    }
    else
    {
        if (dir_path[0] == '/')
        {
            // Get list under absolute path
            qosa_snprintf(szComand, szComand_size, "LIST %s", dir_path);
        }
        else
        {
            // Get list under relative path
            qosa_snprintf(szComand, szComand_size, "LIST %s%s", ftp_ptr->currentDir, dir_path);
        }
    }

    QLOGD("szComand=%s", szComand);
    // Send constructed LIST command to FTP server and register callback function to process response
    qurl_ftp_demo_request_command_std(ftp_ptr, szComand, qurl_ftp_demo_list_cb, 0);
    QLOGD("ret=%d", ret);
}

/**
 * @brief Create directory on FTP server
 *
 * This function is used to create directory with specified path on FTP server. Supports two ways: relative path and absolute path:
 * - Relative path: Relative to current working directory (CWD)
 * - Absolute path: Complete path relative to server root directory
 *
 * @param ftp_ptr FTP connection control structure pointer, containing server connection information
 * @param path Directory path to create, can be relative path or absolute path
 *
 * @note Note, when creating directory, need to ensure parent directory exists
 */
void qurl_ftp_demo_mkdir(qurl_demo_ftp_t *ftp_ptr, char *path)
{
    char *szComand = QOSA_NULL;
    int   server_ap_len = 0;
    int   ret = 0;

    // Check input parameter validity
    if (QOSA_NULL == ftp_ptr->qurl || QOSA_NULL == path || 0 == qosa_strlen(path))
    {
        QLOGE("path QOSA_NULL");
        return;
    }

    // Allocate command buffer memory
    szComand = (char *)qosa_malloc(qosa_strlen(ftp_ptr->server_ap) + qosa_strlen(path) + 64);
    if (QOSA_NULL == szComand)
    {
        QLOGE("mem err");
        return;
    }
    qosa_memset(szComand, 0, qosa_strlen(ftp_ptr->server_ap) + qosa_strlen(path) + 64);

    // Calculate server base path length, handle ending slash
    server_ap_len = qosa_strlen(ftp_ptr->server_ap);
    if ('/' == ftp_ptr->server_ap[server_ap_len - 1])
    {
        server_ap_len = server_ap_len - 1;
    }

    // Construct MKD command based on path type
    if ('/' != path[0])
    {
        //User input relative path, relative to current CWD path
        {
            qosa_sprintf(szComand, "MKD %s", path);
        }
    }
    else
    {
        //User input contains absolute path of server file system
        {
            qosa_sprintf(szComand, "MKD %s%s", ftp_ptr->server_ap, path);
        }
    }

    // Send FTP MKD command and process response
    ret = qurl_ftp_demo_request_command_quote(ftp_ptr, szComand, QOSA_NULL, 1);
    QLOGD("ret=%d", ret);
    qosa_free(szComand);
}

/**
 * @brief Send RMD (remove directory) command to FTP server via FTP protocol
 *
 * @param ftp_ptr Pointer to FTP control structure, containing FTP connection related information (such as hostname, current directory, etc.)
 * @param path    Directory path to delete, can be absolute path (starting with '/') or relative path
 *
 * @note Deleting directory requires ensuring it's an empty directory
 */
void qurl_ftp_demo_rmd(qurl_demo_ftp_t *ftp_ptr, char *path)
{
    int   url_len = 0;
    char *szComand = QOSA_NULL;
    int   szComand_size = 0;

    // Parameter validity check: FTP structure pointer, path pointer and path length
    if (QOSA_NULL == ftp_ptr->qurl || QOSA_NULL == path || 0 == qosa_strlen(path))
    {
        QLOGE("path QOSA_NULL");
        return;
    }

    // Calculate command buffer size required based on whether path is null
    if (path != QOSA_NULL)
    {
        szComand_size = qosa_strlen(ftp_ptr->server_ap) + qosa_strlen(path) + 64;
    }
    else
    {
        szComand_size = qosa_strlen(ftp_ptr->server_ap) + 64;
    }

    szComand = qosa_malloc(szComand_size);
    if (szComand == QOSA_NULL)
    {
        QLOGE("mem err");
        return;
    }
    qosa_memset(szComand, 0, szComand_size);

    qosa_free(ftp_ptr->url);
    url_len = qosa_strlen(ftp_ptr->hostname) + qosa_strlen(ftp_ptr->currentDir) + 64;
    ftp_ptr->url = (char *)qosa_malloc(url_len);
    if (QOSA_NULL == ftp_ptr->url)
    {
        QLOGE("mem err");
        qosa_free(szComand);
        return;
    }
    qosa_memset(ftp_ptr->url, 0, url_len);
    // Construct new FTP URL
    qosa_snprintf(ftp_ptr->url, url_len, "FTP://%s%s", ftp_ptr->hostname, ftp_ptr->currentDir);
    QLOGD("ftp_ptr->currentDir=%s", ftp_ptr->currentDir);

    // Construct RMD command based on whether path is absolute or relative path
    if (path[0] == '/')
    {
        // Get list under absolute path
        qosa_snprintf(szComand, szComand_size, "RMD %s", path);
    }
    else
    {
        // Get list under relative path
        qosa_snprintf(szComand, szComand_size, "RMD %s%s", ftp_ptr->currentDir, path);
    }

    QLOGD("szComand=%s", szComand);
    // Send constructed RMD command
    qurl_ftp_demo_request_command_quote(ftp_ptr, szComand, QOSA_NULL, 1);
}  

/**
 * @brief Close FTP demonstration connection and release related resources
 */
void qurl_ftp_demo_close(qurl_demo_ftp_t *ftp_ptr)
{
    /* Check if input parameter is null pointer */
    if (ftp_ptr == QOSA_NULL)
    {
        return;
    }
    /* Delete qurl core object, release related resources */
    qurl_core_delete(ftp_ptr->qurl);
}

 * @brief Initialize and open FTP connection, configure related parameters and execute FTP operation.
 *
 * This function is used to initialize qurl core module, create FTP connection context, and configure FTP connection options based on passed parameters,
 * including server address, port, username, password, etc. Then initiate FTP connection request and process return results.
 * Simultaneously parse FTP server returned directory information and save.
 *
 */
static void qurl_ftp_demo_open(qurl_demo_ftp_t *ftp_ptr)
{
    qurl_core_t qurl = QOSA_NULL;
    int         ret = 0;

    // Global initialization of qurl module
    qurl_global_init();

    // Create qurl core handle
    qurl_core_create(&ftp_ptr->qurl);
    qurl = ftp_ptr->qurl;
    if (qurl == QOSA_NULL)
    {
        goto exit;
    }

    // Allocate and construct FTP URL string
    ftp_ptr->url = (char *)qosa_malloc(qosa_strlen(ftp_ptr->hostname) + 64);
    if (QOSA_NULL == ftp_ptr->url)
    {
        QLOGE("mem err");
        goto exit;
    }
    qosa_memset(ftp_ptr->url, 0, qosa_strlen(ftp_ptr->hostname) + 64);
    qosa_sprintf(ftp_ptr->url, "ftp://%s/", ftp_ptr->hostname);

    // Configure qurl parameters including url, username, password, etc.
    QLOGD("user=%s,pwd=%s,port=%d", ftp_ptr->username, ftp_ptr->password, ftp_ptr->server_port);
    qurl_core_setopt(qurl, QURL_OPT_NETWORK_ID, ftp_ptr->pdp_cid);
    qurl_core_setopt(qurl, QURL_OPT_USERNAME, ftp_ptr->username);
    qurl_core_setopt(qurl, QURL_OPT_PASSWORD, ftp_ptr->password);
    qurl_core_setopt(qurl, QURL_OPT_PORT, ftp_ptr->server_port);
    qurl_core_setopt(qurl, QURL_OPT_URL, ftp_ptr->url);
    qurl_core_setopt(qurl, QURL_OPT_WRITE_CB, qurl_ftp_demo_write_cb);
    qurl_core_setopt(qurl, QURL_OPT_WRITE_CB_ARG, ftp_ptr);
    qurl_core_setopt(qurl, QURL_OPT_WRITE_HEAD_CB, qurl_ftp_demo_open_header);
    qurl_core_setopt(qurl, QURL_OPT_WRITE_HEAD_CB_ARG, (void *)ftp_ptr);
    qurl_core_setopt(qurl, QURL_OPT_TIMEOUT_MS, 0L);
    qurl_core_setopt(qurl, QURL_OPT_IDLE_TIMEOUT_MS, (ftp_ptr->timeout) * 1000);
    qurl_core_setopt(qurl, QURL_OPT_NOBODY, 1L);

    // If not disabling PASV mode IP skip, set corresponding option
    if (ftp_ptr->skip_pasv_ip == 0)
    {
        qurl_core_setopt(qurl, QURL_OPT_FTP_SKIP_PASV_IP, 0L);
    }

    // Execute FTP connection operation
    ret = qurl_core_perform(qurl);
    if (ret != QURL_OK)
    {
        QLOGE("%x\r\n", ret);
        goto exit;
    }

    QLOGV("end");
    QLOGD("dir=%s", ftp_ptr->last_reply_Dir);

    // Update server current directory information
    if (ftp_ptr->server_ap != QOSA_NULL)
    {
        qosa_free(ftp_ptr->server_ap);
    }
    if (ftp_ptr->last_reply_Dir != QOSA_NULL)
    {
        ftp_ptr->server_ap = (char *)qosa_malloc(qosa_strlen(ftp_ptr->last_reply_Dir) + 1);
        if (QOSA_NULL == ftp_ptr->server_ap)
        {
            QLOGE("mem err");
            goto exit;
        }
        qosa_memset(ftp_ptr->server_ap, 0, qosa_strlen(ftp_ptr->last_reply_Dir) + 1);
        qosa_memcpy(ftp_ptr->server_ap, ftp_ptr->last_reply_Dir, qosa_strlen(ftp_ptr->last_reply_Dir) + 1);
        QLOGD("server_ap=%s", ftp_ptr->server_ap);
    }


exit:
    QLOGV("end");
}

/**
 * @brief FTP download test function
 *
 * This function is used to initialize FTP test environment, and sequentially execute FTP common instruction test process,
 * including connection, directory operations, file download, list acquisition and deletion operations.
 * All resources will be released after use.
 *
 * @note This only demonstrates basic usage, all operations are based on current directory. If cross-directory operations are needed, please modify accordingly
 * @note File download is just a simple demonstration. If files are large, segmented download can be used, and file writing operations can also be optimized for efficiency
 */
static void ftp_get_demo(void *argv)
{
    qurl_demo_ftp_t *ftp_ptr = QOSA_NULL;
    int              ret = 0;

    // Wait for system initialization to complete
    qosa_task_sleep_sec(10);

    // Allocate FTP control structure memory
    ftp_ptr = qosa_malloc(sizeof(qurl_demo_ftp_t));
    if (ftp_ptr == QOSA_NULL)
    {
        return;
    }
    qosa_memset(ftp_ptr, 0, sizeof(qurl_demo_ftp_t));

    // Initialize FTP module
    ret = qurl_ftp_demo_init(ftp_ptr);
    if (ret != 0)
    {
        goto exit;
    }

    // Establish FTP connection
    qurl_ftp_demo_open(ftp_ptr);

    // Create test directory
    qurl_ftp_demo_mkdir(ftp_ptr, QURL_FTP_DEMO_TEST_DIR);

    // Switch to test directory
    qurl_ftp_demo_cwd(ftp_ptr, QURL_FTP_DEMO_TEST_DIR);

    // Get current working directory   
    qurl_ftp_demo_pwd(ftp_ptr);

    // Query server file size
    qurl_ftp_demo_size(ftp_ptr, QURL_FTP_DEMO_SERVER_FILENAME);

    // Download file test
    qurl_ftp_demo_get(ftp_ptr, QURL_FTP_DEMO_SERVER_FILENAME);

    // Get detailed file list
    qurl_ftp_demo_list(ftp_ptr, QOSA_NULL);

    // Get simple filename list
    qurl_ftp_demo_nlist(ftp_ptr, QOSA_NULL);

    // Delete file on server
    qurl_ftp_demo_delete(ftp_ptr, QURL_FTP_DEMO_SERVER_FILENAME);

    // Switch back to root directory and delete test directory
    qurl_ftp_demo_cwd(ftp_ptr, "/");
    qurl_ftp_demo_rmd(ftp_ptr, QURL_FTP_DEMO_TEST_DIR);

    // Close FTP connection
    qurl_ftp_demo_close(ftp_ptr);
    QLOGV("download test end");

exit:
    // Release allocated memory resources
    qosa_free(ftp_ptr->currentDir);
    qosa_free(ftp_ptr->last_reply_Dir);
    qosa_free(ftp_ptr->server_ap);
    qosa_free(ftp_ptr->url);
    qosa_free(ftp_ptr);
}