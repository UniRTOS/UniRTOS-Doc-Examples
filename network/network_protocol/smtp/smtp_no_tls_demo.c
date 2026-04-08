#include "qosa_sys.h"
#include "qosa_def.h"
#include "qosa_log.h"
#include "qurl.h"
#include "qosa_virtual_file.h"
#include "qcm_base64.h"

#define QOS_LOG_TAG                    LOG_TAG

#define QURL_SMTP_DEMO_PDPID           1    /*!< PDP context ID for network connection */

/* Please modify the following SMTP server related parameters and login parameters according to actual situation */
/* SMTP server login credentials */
#define QURL_SMTP_DEMO_USERNAME        "xxx@163.com"  /*!< SMTP login username */
#define QURL_SMTP_DEMO_PASSWARD        "xxx"          /*!< SMTP login password, authorization code */
#define QURL_SMTP_DEMO_EMAILADDR       "smtp.163.com" /*!< 163 mail server address */
#define QURL_SMTP_DEMO_NICKNAME        "xxx"          /*!< Sender nickname */
#define QURL_SMTP_DEMO_SENDER_EMAIL    "xxx@163.com"  /*!< Sender email address */

/* Email recipient configuration */
#define QURL_SMTP_DEMO_TO_PERSON       "xxx@163.com"  /*!< Primary recipient email address */
#define QURL_SMTP_DEMO_CC_PERSON       "xxx@sina.com" /*!< CC recipient email address */
#define QURL_SMTP_DEMO_BCC_PERSON      "xxx@sohu.com" /*!< BCC recipient email address */

/* SMTP server port configuration */
#define QURL_SMTP_DEMO_NO_TLS_PORT     25  /*!< Normal SMTP service port without TLS encryption */

/* SMTP connection timeout in seconds */
#define QURL_SMTP_DEMO_TIMEOUT         60

/* Email body content */
#define QURL_SMTP_DEMO_TEXT            "Hello, welcome to test SMTP protocol"
/* Email attachment filename */
#define QURL_SMTP_DEMO_FILENAME        "/user/smtp_test.txt"

// SMTP email boundary string definition, should not exceed 70 bytes and be as unique as possible
#define QURL_SMTP_BOUNDARY_STRING      ("===Quectel_1755834449018_Quectel===")
// Mixed mode email header, typically sent before the body, can also be omitted and send body directly
#define QURL_SMTP_MIXED_SPECIFICATION  ("\r\nThis is a multi-part message in MIME format.\r\n\r\n")
// multipart/mixed type indicates the email content consists of multiple independent parts
#define QURL_SMTP_MULTIPART_MIXED      ("multipart/mixed;\r\n")

/**
 * @brief SMTP sending status enumeration definition
 *
 * This enumeration defines the various status stages during the SMTP email sending process
 *
 * @note For qurl there is only one state, uploading email content, the division here is just for easy packet understanding
 */
typedef enum
{
    QURL_SMTP_SEND_HEAD = 0,  /*!< Email header */
    QURL_SMTP_SEND_TEXT_HEAD, /*!< Text body header */
    QURL_SMTP_SEND_TEXT_BODY, /*!< Text body content */
    QURL_SMTP_SEND_ATTA_HEAD, /*!< Attachment header */
    QURL_SMTP_SEND_ATTA_BODY, /*!< Attachment content */
    QURL_SMTP_SEND_TAIL,      /*!< Email ending */
    QURL_SMTP_SEND_NONE,      /*!< Email completed */
} qurl_smtp_send_statu_e;

/**
 * @brief SMTP email encoding format enumeration definition
 *
 * Defines the character encoding format types supported in SMTP email sending,
 * used to specify the encoding method for email subject and body
 */
typedef enum
{
    QURL_SMTP_CHARSET_ASCII = 0, /*!< ASCII code */
    QURL_SMTP_CHARSET_UTF8,      /*!< UTF8 encoding */
    QURL_SMTP_CHARSET_GB2312,    /*!< GB2312 encoding */
    QURL_SMTP_CHARSET_BIG5,      /*!< BIG5 encoding */
    QURL_SMTP_CHARSET_MAX,       /*!< Maximum encoding format value, used for boundary check */
} qurl_smtp_charset_e;           /*!< Subject and body encoding format */

/**
 * @brief SMTP SSL encryption type enumeration definition
 *
 * Defines the SSL/TLS encryption methods used in the SMTP protocol, for configuring
 * the secure connection type when sending emails
 */
typedef enum
{
    QURL_SMTP_SSL_TYPE_NO = 0,       /*!< SMTP connection without SSL encryption */
    QURL_SMTP_SSL_TYPE_SSL = 1,      /*!< SMTPS encrypted connection, SSL encryption during handshake phase */
    QURL_SMTP_SSL_TYPE_STARTTLS = 2, /*!< SMTP+TLS encrypted connection, upgrade to TLS encryption after handshake */
} qurl_smtp_ssl_type_e;

/**
 * SMTP client structure definition
 * Used to store SMTP email sending related client information and status
 * Contains email subject, content, attachment file information and sending status and SSL configuration
 */
typedef struct
{
    char                  *subject;         /*!< Email subject string pointer */
    qurl_smtp_charset_e    subject_charset; /*!< Email subject character set encoding */
    qurl_smtp_charset_e    text_charset;    /*!< Email body character set type */
    char                  *text;            /*!< Email body content pointer */
    qosa_uint32_t          text_len;        /*!< Email body length */
    qosa_uint32_t          text_used;       /*!< Email body processed size */
    char                  *file_name;       /*!< Attachment file name pointer */
    qosa_uint32_t          file_size;       /*!< Attachment file size */
    qosa_uint32_t          file_used;       /*!< Attachment file processed size */
    int                    fd;              /*!< File descriptor */
    qurl_smtp_send_statu_e status;          /*!< Email sending status */
    qurl_smtp_ssl_type_e   ssl_type;        /*!< SSL connection type configuration */
} qurl_smtp_client;

// "Hello" in UTF-8 hexadecimal string
static char g_hello_utf8_hex[] = {0xE4, 0xBD, 0xA0, 0xE5, 0xA5, 0xBD};

/**
 * SMTP character set mapping table
 *
 * This array defines the character set encoding types supported in the SMTP protocol,
 * used for character encoding conversion during email transmission.
 * The array index corresponds to the QURL_SMTP_CHARSET enumeration value,
 * and the string value is the actual character set identifier.
 *
 * Supported character sets include:
 * - us-ascii: American Standard Code for Information Interchange
 * - utf-8: Unicode character encoding
 * - gb2312: Simplified Chinese character set
 * - big5: Traditional Chinese character set
 */
static const char *qurl_smtp_charset[QURL_SMTP_CHARSET_MAX] = {"us-ascii", "utf-8", "gb2312", "big5"};

/**
 * Read specified length of data from file descriptor to buffer
 * @param buf: Buffer pointer for storing read data
 * @param fd: File descriptor identifying the file to read
 * @param len: Length of data to read
 * @return: Returns 0 on success, -1 on failure
 */
static int qurl_smtp_file_read(qosa_uint8_t *buf, int fd, int len)
{
    qosa_size_t temp_len = 0;
    int         ret = 0;

    // Loop to read data until all is read
    while (temp_len < len)
    {
        ret = qosa_vfs_read(fd, buf + temp_len, len - temp_len);
        // Handle file reading error
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
 * @brief Encode SMTP attachment content, use Base64 encoding to write file content to buffer.
 *
 * This function reads the file content specified in the current SMTP client context and
 * writes it to the provided buffer in Base64 format.
 * It also handles line breaks and buffer space limitations to ensure the output meets

 * SMTP attachment format requirements.
 *
 * @param[in,out] ctx_ptr Pointer to SMTP client context structure, used to get file
 * information and update status.
 * @param[out]    ptr     Buffer pointer for storing encoded data.
 * @param[in]     buf_len Total buffer length (in bytes).
 *
 * @return Returns the actual length of data written to the buffer (in bytes).
 */
static unsigned int qurl_smtp_encode_atta_body(qurl_smtp_client *ctx_ptr, char *ptr, unsigned int buf_len)
{
    qosa_uint32_t len = 0;              /*!< Length of added content */
    qosa_uint32_t free_len = 0;         /*!< Remaining space length */
    int           left_len = 0;         /*!< Remaining file length */
    int           copy_len = 0;         /*!< Actual read size */
    qosa_uint32_t line_character = 0;   /*!< Space occupied by line breaks */
    qosa_uint32_t can_read_len = 0;     /*!< Maximum readable data length */
    qosa_uint8_t *file_buf = QOSA_NULL; /*!< Buffer for storing read file content */
    int           base64_dst_len = 0;   /*!< Encoding space size */
    int           base64_result = 0;    /*!< Encoding result */
    qosa_uint32_t ret = 0;              /*!< Return value */

    QLOGD("buf_len=%d", buf_len);

    // Check if remaining buffer is sufficient for one Base64 encoding (at least 4 bytes required)
    if (buf_len - len < 4)
    {
        return len;
    }
    free_len = buf_len;

    // Roughly estimate space occupied by line breaks (insert CRLF every 76 characters)
    line_character = (free_len / 76) * 2;
    free_len -= line_character;

    // Calculate readable raw data length (before Base64 encoding), and adjust to multiple of 3
    // to avoid padding characters in the middle
    can_read_len = (free_len * 3 / 4);
    can_read_len = (can_read_len - can_read_len % 3);
    QLOGD("can_read_len=%d,%d", can_read_len, line_character);

    // Calculate remaining unread file length and determine bytes to read this time
    left_len = ctx_ptr->file_size - ctx_ptr->file_used;
    copy_len = MIN((int)can_read_len, left_len);

    QLOGD("copy_len=%d,%d", copy_len, ctx_ptr->file_used);

    // If there is still data to read and encode
    if (0 != copy_len)
    {
        // Allocate temporary buffer for storing content read from file
        file_buf = qosa_malloc(copy_len + 1);
        if (file_buf == QOSA_NULL)
        {
            QLOGE("malloc err");
            goto exit;
        }
        qosa_memset(file_buf, 0, copy_len + 1);

        // Read specified length data from file descriptor
        ret = qurl_smtp_file_read(file_buf, ctx_ptr->fd, copy_len);
        if (ret != 0)
        {
            QLOGE("read err");
            goto exit;
        }

        // Base64 encode the read data and write to target buffer
        base64_dst_len = buf_len;
        base64_result = qcm_base64_encode((char *)file_buf, copy_len, ptr + len, &base64_dst_len, QOSA_TRUE);
        if (base64_result < 0)
        {
            QLOGE("base64 err");
            goto exit;
        }
        else
        {
            // Update read file size and buffer write length
            ctx_ptr->file_used += copy_len;
            QLOGD("file_used=%d", ctx_ptr->file_used);
            len += base64_dst_len;

            // If file has been completely read, switch status to send tail
            if (ctx_ptr->file_used >= ctx_ptr->file_size)
            {
                ctx_ptr->status = QURL_SMTP_SEND_TAIL;
            }
        }
    }
    else
    {
        // When no more data to read, switch status to send tail
        ctx_ptr->status = QURL_SMTP_SEND_TAIL;
    }

    return len;

exit:
    // Set status and close file handle on error or completion
    ctx_ptr->status = QURL_SMTP_SEND_TAIL;
    if (ctx_ptr->fd > 0)
    {
        qosa_vfs_close(ctx_ptr->fd);
    }
    return len;
}

/**
 * @brief Encode SMTP email attachment header information and update client status
 *
 * This function is used to construct the MIME header information for email attachments,
 * including Content-Type, Content-Transfer-Encoding and Content-Disposition fields.
 * If an attachment file exists, it opens the file and obtains its size and other information
 *
 * @param[in,out] ctx_ptr SMTP client context pointer, containing current processing
 * status and attachment related information
 * @param[out]    ptr     Buffer for storing generated header data
 * @param[in]     buf_len Buffer length
 *
 * @return Returns the length of data written to the buffer (in bytes)
 */
static qosa_uint32_t qurl_smtp_encode_atta_head(qurl_smtp_client *ctx_ptr, char *ptr, unsigned int buf_len)
{
    int                    len = 0;    /*!< Length of added content */
    int                    ret = 0;    /*!< Return value */
    struct qosa_vfs_stat_t stat = {0}; /*!< File information */

    if (QOSA_NULL != ctx_ptr->file_name)
    {
        // Attachment exists, ensure buffer is large enough to accommodate attachment header content
        if (buf_len < 512)
        {
            return len;
        }
        QLOGD("file_name=%s", ctx_ptr->file_name);

        // Open attachment file
        ctx_ptr->fd = qosa_vfs_open(ctx_ptr->file_name, QOSA_VFS_O_RDONLY);
        if (ctx_ptr->fd < 0)
        {
            ctx_ptr->status = QURL_SMTP_SEND_TAIL;
            return len;
        }
        // Get file status information
        ret = qosa_vfs_fstat(ctx_ptr->fd, &stat);
        if (ret != 0)
        {
            ctx_ptr->status = QURL_SMTP_SEND_TAIL;
            qosa_vfs_close(ctx_ptr->fd);
            return len;
        }
        /* Record file size */
        ctx_ptr->file_size = (qosa_uint32_t)stat.st_size;
        QLOGD("size=%d", stat.st_size);

        // Extract filename without path
        char *tmp_ptr = qosa_strrchr(ctx_ptr->file_name, '/');
        char *file_name = QOSA_NULL;
        if (QOSA_NULL == tmp_ptr)
        {
            file_name = ctx_ptr->file_name;
        }
        else
        {
            file_name = (tmp_ptr + 1);
        }

        // Construct attachment MIME header information, specify as attachment
        len += qosa_snprintf(ptr + len, buf_len - len, "\r\n--%s\r\n", QURL_SMTP_BOUNDARY_STRING);
        len += qosa_snprintf(ptr + len, buf_len - len, "Content-Type: %s\tname=\"%s\"\r\n", "application/octet-stream;\r\n", file_name);
        len += qosa_snprintf(ptr + len, buf_len - len, "Content-Transfer-Encoding: %s\r\n", "base64");
        len += qosa_snprintf(ptr + len, buf_len - len, "Content-Disposition: attachment;\r\n\tfile_name=\"%s\"\r\n\r\n", file_name);

        // Set status to send attachment body
        ctx_ptr->status = QURL_SMTP_SEND_ATTA_BODY;
    }
    else
    {
        // No attachment, directly enter email tail sending phase
        ctx_ptr->status = QURL_SMTP_SEND_TAIL;
    }

    return len;
}

/**
 * @brief Base64 encode SMTP email text body content and fill into input buffer
 *
 * This function is used to Base64 encode the text body content of the email as needed
 * and write it to the specified buffer.
 * It supports segmented processing of large text content to avoid occupying too much
 * memory at one time. When the text processing is completed,
 * the state machine switches to the attachment header sending stage.
 *
 * @param[in,out] ctx_ptr SMTP client context pointer, containing text information to
 * be processed and current processing status
 * @param[out] ptr Buffer for storing encoded data
 * @param[in] buf_len Total length of the buffer (in bytes)
 *
 * @return Returns the actual length of data written to the buffer (in bytes), returns
 * the written length if there is an error or insufficient buffer
 */
static qosa_uint32_t qurl_smtp_encode_text_body(qurl_smtp_client *ctx_ptr, char *ptr, unsigned int buf_len)
{
    qosa_uint32_t len = 0;            /*!< Length of added content */
    qosa_uint32_t free_len = 0;       /*!< Remaining buffer length */
    qosa_uint32_t line_character = 0; /*!< Space occupied by line breaks after base64 encoding */
    qosa_uint32_t can_read_len = 0;   /*!< Maximum readable length this time */
    int           base64_dst_len = 0; /*!< Encoding space size, will fail if space is insufficient to store encoding */
    int           base64_result = 0;  /*!< Encoding result */

    QLOGD("buf_len=%d", buf_len);

    // If no body content, directly jump to attachment header sending status
    if (ctx_ptr->text_len <= 0)
    {
        ctx_ptr->status = QURL_SMTP_SEND_ATTA_HEAD;
        return len;
    }

    // At least 4 bytes of space are required for one encoding (Base64 minimum unit),
    // otherwise processing cannot continue
        if (buf_len - len < 4)
    {
        return len;
    }
    free_len = (buf_len - len);

    /*
     * Base64 encoding description:
     * 1. Every 3 bytes of original data are encoded as 4 bytes of output, this is the encoding specification
     * 2. Insert a line break \r\n every 76 characters, this is based on RFC 2045 recommendation,
     * but here it is simplified, only ensuring that the data uploaded in a single time conforms,
     * the junction of two data may be less than or exceed 76
     * 3. Large data can be encoded in multiple Base64 encodings and then summarized,
     * but ensure that before the last Base64, all are encoded in multiples of 3
     */

    // Estimate space occupied by line breaks
    line_character = (free_len / 76) * 2;
    free_len -= line_character;

    // Calculate maximum readable raw data length
    can_read_len = (free_len * 3 / 4);
    // Align to 3 bytes to avoid intermediate padding
    can_read_len = (can_read_len - can_read_len % 3);
    QLOGD("can_read_len=%d,%d", can_read_len, line_character);

    // Calculate remaining unprocessed text length and maximum copyable length this time
    int left_len = ctx_ptr->text_len - ctx_ptr->text_used;
    int copy_len = MIN((int)can_read_len, left_len);

    // If there is still data to process
    if (0 != copy_len)
    {
        base64_dst_len = (buf_len - len);

        // Execute Base64 encoding operation, QOSA_TRUE represents adding \r\n every 76 bytes
        base64_result = qcm_base64_encode(ctx_ptr->text + ctx_ptr->text_used, copy_len, ptr + len, &base64_dst_len, QOSA_TRUE);
        if (base64_result < 0)
        {
            QLOGE("BASE64 err");
            return len;
        }
        else
        {
            // Update processed text offset and write length, after successful encoding,
            // base64_dst_len is modified to the actual length occupied by encoding
            ctx_ptr->text_used += copy_len;
            len += base64_dst_len;

            // If all text has been processed, switch status to attachment header sending
            if (ctx_ptr->text_used >= ctx_ptr->text_len)
            {
                ctx_ptr->status = QURL_SMTP_SEND_ATTA_HEAD;
            }
        }
    }
    else
    {
        // Also switch status to attachment header sending when no data to process
        ctx_ptr->status = QURL_SMTP_SEND_ATTA_HEAD;
    }

    return len;
}


/**
 * @brief Encode SMTP email text header information
 *
 * This function is used to build the header information of the text body,
 * including boundary separator, content type and transfer encoding information.
 *
 * @param[in,out] ctx_ptr SMTP client context pointer, containing text information to
 * be processed and current processing status
 * @param[out] ptr Buffer for storing encoded data
 * @param[in] buf_len Total length of the buffer (in bytes)
 * @return Returns the length of newly added field data, returns the added length if
 * the buffer is insufficient
 */
static unsigned int qurl_smtp_encode_text_head(qurl_smtp_client *ctx_ptr, char *ptr, unsigned int buf_len)
{
    qosa_uint32_t len = 0; /*!< Length of added content */

    QLOGD("buf_len=%d", buf_len);

    // Check if buffer is sufficient to store header information, including boundary
    // string length and reserved space
    // This contains a knowledge point, because this example uses multipart/mixed,
    // so even if there is no text and attachment at the same time, it must ensure a boundary
    if (buf_len < (100 + qosa_strlen(QURL_SMTP_BOUNDARY_STRING)))
    {
        // The purpose here is just to facilitate one-time upload of header
        return len;
    }

    // Build multipart email text part format information
    // Add boundary separator
    len += qosa_snprintf(ptr + len, buf_len - len, "--%s\r\n", QURL_SMTP_BOUNDARY_STRING);
    // Add content type and character set information
    len += qosa_snprintf(ptr + len, buf_len - len, "Content-Type: %s\tcharset=\"%s\"\r\n", "text/plain;\r\n", qurl_smtp_charset[ctx_ptr->text_charset]);
    // Add transfer encoding method
    len += qosa_snprintf(ptr + len, buf_len - len, "Content-Transfer-Encoding: base64\r\n\r\n");

    // Update SMTP client status to send text body
    ctx_ptr->status = QURL_SMTP_SEND_TEXT_BODY;

    return len;
}

/**
 * @brief Encode SMTP email header information and fill into specified buffer
 *
 * This function is used to construct the header fields of SMTP emails, including From,
 * To, Cc, Bcc, MIME-Version, Subject and Content-Type fields. According to whether the
 * subject content is ASCII or needs Base64 encoding,
 * construct the appropriate email header format.
 *
 * @param[in,out] ctx_ptr SMTP client context pointer, containing text information to
 * be processed and current processing status
 * @param[out] ptr Buffer for storing encoded data
 * @param[in] buf_len Total length of the buffer (in bytes)
 *
 * @return Returns the number of bytes written to the buffer; returns 0 if an error occurs
 */
static unsigned int qurl_smtp_encode_mail_header(qurl_smtp_client *ctx_ptr, char *ptr, unsigned int buf_len)
{
    unsigned int len = 0;                  /*!< Length of added content */
    char         subject_title[128] = {0}; /*!< Buffer for storing encoded Subject */
    int          subject_title_len = 128;  /*!< Length of buffer */
    int          base64_result = 0;        /*!< Encoding result */

    QLOGV("enter");

    /* Construct From field */
    len += qosa_snprintf(ptr + len, buf_len - len, "From: \"%s\" <%s>\r\n", QURL_SMTP_DEMO_NICKNAME, QURL_SMTP_DEMO_SENDER_EMAIL);

    // To Cc Bcc can be absent, but will usually be marked as spam or abnormal email,
    // the real recipients have been configured through the previous RCPT
    /* Construct recipient field To */
    len += qosa_snprintf(ptr + len, buf_len - len, "To: <%s>\r\n", QURL_SMTP_DEMO_TO_PERSON);

    /* Construct carbon copy field Cc */
    len += qosa_snprintf(ptr + len, buf_len - len, "Cc: <%s>\r\n", QURL_SMTP_DEMO_CC_PERSON);

    /* Construct blind carbon copy field Bcc */
    len += qosa_snprintf(ptr + len, buf_len - len, "Bcc: <%s>\r\n", QURL_SMTP_DEMO_BCC_PERSON);

    /* Add MIME version identifier */
    len += qosa_snprintf(ptr + len, buf_len - len, "%s\r\n", "MIME-Version: 1.0");

    /* Construct email subject field Subject */
    if (ctx_ptr->subject == QOSA_NULL)
    {
        // If no subject is set, insert empty subject line
        len += qosa_snprintf(ptr + len, buf_len - len, "%s", "Subject: \r\n");
    }
    else
    {
        // Encoding method can be B (Base64) or Q (Quoted-Printable),
        // here only Base64 method and API are provided, if you want to use Q,
        // users need to add encoding API themselves
        if (ctx_ptr->subject_charset == QURL_SMTP_CHARSET_ASCII)
        {
            // If ASCII character set, can directly use original text as subject,
            // can also adopt Base64 encoding
            len += qosa_snprintf(ptr + len, buf_len - len, "Subject: %s\r\n", (const char *)ctx_ptr->subject);
        }
        else
        {
            // Base64 encode non-ASCII subject
            base64_result = qcm_base64_encode((const char *)ctx_ptr->subject, qosa_strlen(ctx_ptr->subject), subject_title, &subject_title_len, QOSA_FALSE);
            if (base64_result < 0)
            {
                QLOGE("base64 err");
                return 0;
            }
            len += qosa_snprintf(ptr + len, buf_len - len, "Subject: =?%s?B?%s?=\r\n", qurl_smtp_charset[ctx_ptr->subject_charset], subject_title);
        }
    }

    /* Set email content type to multipart/mixed, used to contain multiple different
     * types of parts in one email message, even if there is only one part,
     * a boundary parameter must be included */
    // Here \r\n and \t are added after content-type, this is a way of line breaking,
    // it's also fine not to break the line
    len += qosa_snprintf(ptr + len, buf_len - len, "Content-Type: %s\tboundary=\"%s\"\r\n\r\n", QURL_SMTP_MULTIPART_MIXED, QURL_SMTP_BOUNDARY_STRING);

    /* Update SMTP status to send text header */
    ctx_ptr->status = QURL_SMTP_SEND_TEXT_HEAD;

    return len;
}

/**
 * @brief SMTP data upload callback function, used to upload email content.
 *
 * This function fills the buffer according to the current SMTP sending status
 * (such as email header, body, attachment, etc.),
 * and returns the actual length of data written.
 *
 * @param buf  [out] Pointer to the output buffer for storing data to be sent.
 * @param size [in]  Maximum allowed bytes of the buffer.
 * @param arg  [in]  User-defined parameter, usually a qurl_smtp_client context
 * structure pointer.
 *
 * @return Returns the actual number of bytes written to the buffer; returns 0 if no
 * data is available to send. If the email content has been completely uploaded,
 * return 0 again on the next callback
 * @note   1. If the email upload is complete, you need to return on the next callback
 *         2. Pay attention to the size when uploading data, do not exceed it, if it is
 * inconvenient to upload single data (for example, at the critical point of packet
 * assembly), you can first return the completed content and wait for the next
 *         continue uploading, generally single size is 2048.
 */
unsigned int qurl_smtp_upload_data_cb(unsigned char *buf, long size, void *arg)
{
    unsigned int      len = 0;
    unsigned int      buf_len = size;
    qurl_smtp_client *ctx_ptr = (qurl_smtp_client *)arg;

    if (0 == buf_len)
    {
        return 0;
    }

    QLOGV("len:%d, buf_len:%d,statu:%d", len, buf_len, ctx_ptr->status);
    qosa_memset(buf, 0x00, size);

    // Process email header encoding
    if (ctx_ptr->status == QURL_SMTP_SEND_HEAD)
    {
        len += qurl_smtp_encode_mail_header(ctx_ptr, (char *)buf + len, buf_len - len);
        QLOGV("len:%d, buf_len:%d,statu:%d", len, buf_len, ctx_ptr->status);
    }

    // Process text body header encoding
    if (ctx_ptr->status == QURL_SMTP_SEND_TEXT_HEAD)
    {
        len += qurl_smtp_encode_text_head(ctx_ptr, (char *)buf + len, buf_len - len);
        QLOGV("len:%d, buf_len:%d,statu:%d", len, buf_len, ctx_ptr->status);
    }

    // Process text body content encoding
    if (ctx_ptr->status == QURL_SMTP_SEND_TEXT_BODY)
    {
        len += qurl_smtp_encode_text_body(ctx_ptr, (char *)buf + len, buf_len - len);
        QLOGV("len:%d, buf_len:%d,statu:%d", len, buf_len, ctx_ptr->status);
    }

    // Process attachment header encoding
    if (ctx_ptr->status == QURL_SMTP_SEND_ATTA_HEAD)
    {
        len += qurl_smtp_encode_atta_head(ctx_ptr, (char *)buf + len, buf_len - len);
        QLOGV("len:%d, buf_len:%d,statu:%d", len, buf_len, ctx_ptr->status);
    }

    // Process attachment content encoding
    if (ctx_ptr->status == QURL_SMTP_SEND_ATTA_BODY)
    {
        len += qurl_smtp_encode_atta_body(ctx_ptr, (char *)buf + len, buf_len - len);
        QLOGV("len:%d, buf_len:%d,statu:%d", len, buf_len, ctx_ptr->status);
    }

    // Process email end boundary
    if (ctx_ptr->status == QURL_SMTP_SEND_TAIL)
    {
        if ((buf_len - len) > (qosa_strlen(QURL_SMTP_BOUNDARY_STRING) + 10))
        {
            len += qosa_snprintf((char *)buf + len, buf_len - len, "\r\n--%s--\r\n", QURL_SMTP_BOUNDARY_STRING);
            ctx_ptr->status = QURL_SMTP_SEND_NONE;
            QLOGV("upload end");
        }
        return len;
    }

    // All content has been sent
    if (ctx_ptr->status == QURL_SMTP_SEND_NONE)
    {
        return 0;
    }

    return len;
}

/**
 * @brief Free dynamically allocated memory in SMTP client structure
 *
 * This function is used to free the dynamically allocated string memory in the SMTP
 * client structure,
 * including email subject, email body and attachment file name fields.
 *
 * @param client_ptr Pointer to SMTP client structure
 * @return No return value
 */
static void qurl_smtp_client_deinit(qurl_smtp_client *client_ptr)
{
    /* Free email subject string memory */
    if (client_ptr->subject)
    {
        qosa_free(client_ptr->subject);
    }
    /* Free email body string memory */
    if (client_ptr->text)
    {
        qosa_free(client_ptr->text);
    }
    /* Free attachment file name string memory */
    if (client_ptr->file_name)
    {
        qosa_free(client_ptr->file_name);
    }
}


static int qurl_smtp_client_init(qurl_smtp_client *client_ptr)
{
    // Initialize SMTP client custom fields
    client_ptr->status = QURL_SMTP_SEND_HEAD;
    client_ptr->subject_charset = QURL_SMTP_CHARSET_UTF8;

    // Allocate and set email subject
    client_ptr->subject = qosa_malloc(qosa_strlen(g_hello_utf8_hex) + 1);
    if (client_ptr->subject == QOSA_NULL)
    {
        return -1;
    }
    qosa_memset(client_ptr->subject, 0, qosa_strlen(g_hello_utf8_hex) + 1);
    qosa_snprintf(client_ptr->subject, qosa_strlen(g_hello_utf8_hex) + 1, "%s", g_hello_utf8_hex);

    // Allocate and set email body content
    client_ptr->text = qosa_malloc(qosa_strlen(QURL_SMTP_DEMO_TEXT) + 1);
    if (client_ptr->text == QOSA_NULL)
    {
        return -1;
    }
    qosa_memset(client_ptr->text, 0, qosa_strlen(QURL_SMTP_DEMO_TEXT) + 1);
    client_ptr->text_len = qosa_snprintf((char *)client_ptr->text, qosa_strlen(QURL_SMTP_DEMO_TEXT) + 1, "%s", QURL_SMTP_DEMO_TEXT);
    client_ptr->subject_charset = QURL_SMTP_CHARSET_UTF8;

    // Allocate and set attachment file name, to test attachment, need to ensure the
    // specified file exists in the file system
    client_ptr->file_name = qosa_malloc(qosa_strlen(QURL_SMTP_DEMO_FILENAME) + 1);
    if (client_ptr->file_name == QOSA_NULL)
    {
        return -1;
    }
    qosa_memset(client_ptr->file_name, 0, qosa_strlen(QURL_SMTP_DEMO_FILENAME) + 1);
    qosa_snprintf(client_ptr->file_name, qosa_strlen(QURL_SMTP_DEMO_FILENAME) + 1, "%s", QURL_SMTP_DEMO_FILENAME);
    return 0;
}

/**
 * @brief SMTP non-TLS mode email sending example function
 *
 * This function demonstrates how to use the qurl library to send emails through the SMTP
 * protocol without enabling TLS encryption.
 * Including configuring network parameters, setting sender and recipient information,
 * constructing email content and executing sending operations.
 *
 * @note This example includes body and one attachment
 */
static void smtp_no_tls_demo(void)
{
    int              ret = 0;               /*!< Function call return value */
    char            *url = QOSA_NULL;       /*!< SMTP server URL string */
    qurl_core_t      core = QOSA_NULL;      /*!< Core handle */
    qurl_smtp_client client_ptr = {0};      /*!< SMTP client structure */
    qurl_slist_t     rcpt_list = QOSA_NULL; /*!< Recipient list */

    // Global initialization and core handle creation
    qurl_global_init();
    ret = qurl_core_create(&core);

    // Set pdp id
    qurl_core_setopt(core, QURL_OPT_NETWORK_ID, QURL_SMTP_DEMO_PDPID);
    // Username
    qurl_core_setopt(core, QURL_OPT_USERNAME, QURL_SMTP_DEMO_USERNAME);
    // Password, most mailboxes use authorization password
    qurl_core_setopt(core, QURL_OPT_PASSWORD, QURL_SMTP_DEMO_PASSWARD);
    // Sender email
    qurl_core_setopt(core, QURL_OPT_SMTP_MAIL_FROM, QURL_SMTP_DEMO_SENDER_EMAIL);

    // Construct SMTP URL and set related options
    url = (char *)qosa_malloc(qosa_strlen(QURL_SMTP_DEMO_EMAILADDR) + 10);
    if (QOSA_NULL == core || QOSA_NULL == url)
    {
        QLOGE("core:%p, url:%p", core, url);
        goto exit_put;
    }
    qosa_memset(url, 0, qosa_strlen(QURL_SMTP_DEMO_EMAILADDR) + 10);
    // Construct SMTP URL, only QURL_SMTP_SSL_TYPE_SSL configures URL as smtps,
    // the other two methods are smtp
    // starttls also uses tls, but the authentication phase is still smtp plaintext,
    // tls handshake will only be performed after authentication
    qosa_sprintf(url, "smtp://%s", QURL_SMTP_DEMO_EMAILADDR);
    QLOGV("url: %s", url);

    ret = qurl_core_setopt(core, QURL_OPT_URL, url);
    // Configure server port
    ret = qurl_core_setopt(core, QURL_OPT_PORT, QURL_SMTP_DEMO_NO_TLS_PORT);
    qurl_core_setopt(core, QURL_OPT_TLS_USETLS, 0);  // Disable TLS
    // Set read callback function and parameters, used to upload email content
    qurl_core_setopt(core, QURL_OPT_READ_CB, qurl_smtp_upload_data_cb);  // Set data read callback
    qurl_core_setopt(core, QURL_OPT_READ_CB_ARG, (void *)&client_ptr);   // Callback parameter
    // Set timeout, if attachment is large, increase time to prevent sending failure
    qurl_core_setopt(core, QURL_OPT_TIMEOUT_MS, QURL_SMTP_DEMO_TIMEOUT * 1000);  // Timeout

    // Add multiple recipient addresses to the list, recipients, CC, BCC all need to be configured
    rcpt_list = qurl_slist_add_strdup(rcpt_list, QURL_SMTP_DEMO_TO_PERSON);
    rcpt_list = qurl_slist_add_strdup(rcpt_list, QURL_SMTP_DEMO_CC_PERSON);
    rcpt_list = qurl_slist_add_strdup(rcpt_list, QURL_SMTP_DEMO_BCC_PERSON);
    qurl_core_setopt(core, QURL_OPT_SMTP_MAIL_RCPT, rcpt_list);

    // Initialize client custom content
    ret = qurl_smtp_client_init(&client_ptr);
    if (ret != 0)
    {
        goto exit_put;
    }

    // Execute SMTP sending operation
    ret = qurl_core_perform(core);
    QLOGV("ret:%x", ret);

exit_put:
    // Clean up resources
    if (qurl_core_delete(core) != QURL_OK)
    {
        QLOGE("%x\r\n", ret);
    }

    qurl_global_deinit();
    qurl_slist_del_all(rcpt_list);
    qurl_smtp_client_deinit(&client_ptr);
    if (url)
    {
        qosa_free(url);
    }

    QLOGV("ret:%x", ret);
    return;
}