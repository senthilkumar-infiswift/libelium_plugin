#ifndef CLOUDINFISWIFT_SYNC_H
#define CLOUDINFISWIFT_SYNC_H

/* ============================================================
 * define directives
 * ============================================================ */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <mysql/mysql.h>

/* ============================================================
 * define definitions
 * ============================================================ */
#define SSL_DATA_PENDING(A) 0
#define PROTOCOL_NAME_v31 "MQIsdp"
#define PROTOCOL_NAME_v311 "MQTT"
#define MQTT_PROTOCOL_V31 3
#define MQTT_PROTOCOL_V311 4
#define CONNECT 0x10
#define CONNACK 0x20
#define PUBLISH 0x30
#define PUBACK 0x40
#define PUBREC 0x50
#define PUBREL 0x60
#define PUBCOMP 0x70
#define PINGREQ 0xC0
#define PINGRESP 0xD0
#define DISCONNECT 0xE0
#define MQTT_MAX_PAYLOAD 268435455
#define MQTT_ID_MAX_LENGTH 23
#define MSGMODE_NONE 0
#define MSGMODE_CMD 1
#define STREMPTY(str) (str[0] == '\0')
#define COMPAT_CLOSE(a) close(a)
#define COMPAT_ECONNRESET ECONNRESET
#define COMPAT_EWOULDBLOCK EWOULDBLOCK
#define MSB(A) (uint8_t)((A & 0xFF00) >> 8)
#define LSB(A) (uint8_t)(A & 0x00FF)
#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif
#if !defined(WIN32) && !defined(__SYMBIAN32__)
#define HAVE_PSELECT
#endif
#ifndef EPROTO
#define EPROTO ECONNABORTED
#endif
#define MEMSQL_SERVER_IP "localhost"
#define USERNAME "root"
#define PASSWORD "libelium2007"
//#define MEMSQL_SERVER_IP "52.91.114.249"
//#define USERNAME "MeshliumDB"
//#define PASSWORD "MeshliumDB$User"
#define DATABASENAME "MeshliumDB"
#define ID 0
#define ID_WASP 1
#define ID_SECRET 2
#define FRAME_TYPE 3
#define FRAME_NUMBER 4
#define SENSOR 5
#define VALUE 6
#define TIMESTAMP 7
#define SYNC 8
#define RAW 9
#define PARSER_TYPE 10
#define SELECT_SENSOR_PARSER_QUERY "select * from sensorParser where sync &64=false order by  timestamp desc limit %s"
#define UPDATE_SENSOR_PARSER_QUERY "update sensorParser set sync = 64 where id = %s"
#define MAXBUF 1024
#define DELIM "= "
#define LOG_ALL 0
#define LOG_ERROR 1
#define LOG_WARNING 2
#define LOG_INFO 3
/* ============================================================
 * enumeration types
 * ============================================================ */
enum infi_connack {
    connack_accepted = 0,
    connack_refused_protocol_version = 1,
    connack_refused_identifier_rejected = 2,
    connack_refused_server_unavailable = 3,
    connack_refused_bad_username_password = 4,
    connack_refused_not_authorized = 5,
};

enum infi_err_t {
    err_conn_pending = -1,
    err_success = 0,
    err_nomem = 1,
    err_protocol = 2,
    err_inval = 3,
    err_no_conn = 4,
    err_conn_refused = 5,
    err_not_found = 6,
    err_conn_lost = 7,
    err_tls = 8,
    err_payload_size = 9,
    err_not_supported = 10,
    err_auth = 11,
    err_acl_denied = 12,
    err_unknown = 13,
    err_errno = 14,
    err_eai = 15,
    err_proxy = 16
};

enum infi_opt_t {
    opt_protocol_version = 1,
};

enum infi_msg_direction {
    md_in = 0, md_out = 1
};

enum infi_msg_state {
    ms_invalid = 0,
    ms_wait_for_puback = 3,
    ms_wait_for_pubrec = 5,
    ms_wait_for_pubrel = 7,
    ms_wait_for_pubcomp = 9
};

enum infi_client_state {
    cs_new = 0,
    cs_connected = 1,
    cs_disconnecting = 2,
    cs_connect_async = 3,
    cs_connect_pending = 4
};

enum infi_protocol {
    p_mqtt31 = 1, p_mqtt311 = 2
};

enum file_type {
    file_topic = 1, file_message = 2
};

/* ============================================================
 * typedef definitions
 * ============================================================ */
typedef struct sensor_parser {
    char *id;
    char *id_wasp;
    char *id_secret;
    char *frame_type;
    char *frame_number;
    char *sensor;
    char *value;
    char *timestamp;
    char *sync;
    char *raw;
    char *parser_type;
} sensor_parser_t;

typedef struct setup {
    char *mqttserver;
    char *mqttport;
    char *mqttuser;
    char *mqttpassword;
    char *mqttsleeping;
    char *measuresync;
    char *qos;
    char *limit;
    char *log_level;
    char *interval;
    char *client_id;
} setup_t;

typedef int sock_t;

/* ============================================================
 * struct types
 * ============================================================ */
struct sensor_parser_data {
    struct sensor_parser *data;
    struct sensor_parser_data *next;
};

struct infi_message {
    int mid;
    char *topic;
    void *payload;
    int payloadlen;
    int qos;bool retain;
};

struct infi_packet {
    uint8_t *payload;
    struct infi_packet *next;
    uint32_t remaining_mult;
    uint32_t remaining_length;
    uint32_t packet_length;
    uint32_t to_process;
    uint32_t pos;
    uint16_t mid;
    uint8_t command;
    int8_t remaining_count;
};

struct infi_message_all {
    struct infi_message_all *next;
    time_t timestamp;
    enum infi_msg_state state;bool dup;
    struct infi_message msg;
};

struct infi {
    sock_t sock;
    sock_t sockpairR, sockpairW;
    enum infi_protocol protocol;
    char *address;
    char *id;
    char *username;
    char *password;
    uint16_t keepalive;
    uint16_t last_mid;
    enum infi_client_state state;
    time_t last_msg_in;
    time_t last_msg_out;
    time_t ping_t;
    struct infi_packet in_packet;
    struct infi_packet *current_out_packet;
    struct infi_packet *out_packet;
    struct infi_message *will;bool clean_session;
    void *userdata;bool in_callback;
    unsigned int message_retry;
    time_t last_retry_check;
    struct infi_message_all *in_messages;
    struct infi_message_all *in_messages_last;
    struct infi_message_all *out_messages;
    struct infi_message_all *out_messages_last;
    void (*on_connect)(struct infi *, int result);
    void (*on_disconnect)(struct infi *);
    void (*on_publish)(struct infi *);
    void (*on_message)(struct infi *, void *userdata,
            const struct infi_message *message);
    char *host;
    int port;
    int in_queue_len;
    int out_queue_len;
    char *bind_address;
    unsigned int reconnect_delay;
    unsigned int reconnect_delay_max;bool reconnect_exponential_backoff;bool threaded;
    struct infi_packet *out_packet_last;
    int inflight_messages;
    int max_inflight_messages;
    char *publish_message;
};

struct infi_config {
    char *id;
    char *id_prefix;
    int protocol_version;
    int keepalive;
    char *host;
    int port;
    int qos;bool retain;
    int pub_mode;
    char *message;
    long msglen;
    char *topic;
    char *bind_address;bool debug;bool quiet;
    unsigned int max_inflight;
    char *username;
    char *password;
    char *will_topic;
    char *will_payload;
    long will_payloadlen;
    int will_qos;bool will_retain;bool clean_session;
    char **topics;
    int topic_count;bool no_retain;
    char **filter_outs;
    int filter_out_count;bool verbose;bool eol;
    int msg_count;
    setup_t setup;
};

/* ============================================================
 * infi function declaration
 * ============================================================ */
int infi_lib_init(void);
struct infi *infi_new(struct infi_config *cfg, bool clean_session, void *obj);
void infi_destroy(struct infi *inf);
int infi_reinitialise(struct infi *inf, const char *id,
bool clean_session, void *obj);
int infi_will_clear(struct infi *inf);
int infi_username_pw_set(struct infi *inf, const char *username,
        const char *password);
int infi_connect_bind(struct infi *inf, const char *host, int port,
        int keepalive, const char *bind_address);
int infi_reconnect(struct infi *inf);
int infi_disconnect(struct infi *inf);
int infi_publish(struct infi *inf, int *mid, const char *topic, int payloadlen,
        const void *payload, int qos, bool retain);
int infi_loop(struct infi *inf, int timeout, int max_packets);
int infi_loop_read(struct infi *inf, int max_packets);
int infi_loop_write(struct infi *inf, int max_packets);
int infi_loop_misc(struct infi *inf);
int infi_opts_set(struct infi *inf, enum infi_opt_t option, void *value);
void infi_connect_callback_set(struct infi *inf,
        void (*on_connect)(struct infi *, int));
void infi_disconnect_callback_set(struct infi *inf,
        void (*on_disconnect)(struct infi *));
void infi_publish_callback_set(struct infi *inf,
        void (*on_publish)(struct infi *));
void infi_message_callback_set(struct infi *inf,
        void (*on_message)(struct infi *, void *, const struct infi_message *));
const char *infi_strerror(int inf_errno);
const char *infi_connack_string(int connack_code);
int infi_pub_topic_check(const char *topic);
time_t infi_time(void);
int infi_packet_alloc(struct infi_packet *packet);
void infi_check_keepalive(struct infi *inf);
uint16_t infi_mid_generate(struct infi *inf);
int infi_connect_init(struct infi *inf, const char *host, int port,
        int keepalive, const char *bind_address);
int infi_reconnect_blocking(struct infi *inf, bool blocking);

/* ============================================================
 * Memory function declaration
 * ============================================================ */
void *infi_calloc(size_t nmemb, size_t size);
void infi_free(void *mem);
void *infi_malloc(size_t size);
char *infi_strdup(const char *s);

/* ============================================================
 * Messages function declaration
 * ============================================================ */
void infi_message_cleanup(struct infi_message_all **message);
int infi_message_delete(struct infi *inf, uint16_t mid,
        enum infi_msg_direction dir);
int infi_message_queue(struct infi *inf, struct infi_message_all *message,
        enum infi_msg_direction dir);
void infi_messages_reconnect_reset(struct infi *inf);
int infi_message_remove(struct infi *inf, uint16_t mid,
        enum infi_msg_direction dir, struct infi_message_all **message);
void infi_message_retry_check(struct infi *inf);
int infi_message_out_update(struct infi *inf, uint16_t mid,
        enum infi_msg_state state);

/* ============================================================
 * Read_handle function declaration
 * ============================================================ */
void infi_packet_cleanup(struct infi_packet *packet);
int infi_packet_queue(struct infi *inf, struct infi_packet *packet);
int infi_socket_connect(struct infi *inf, const char *host, uint16_t port,
        const char *bind_address, bool blocking);
int infi_socket_close(struct infi *inf);
int infi_try_connect(struct infi *inf, const char *host, uint16_t port,
        sock_t *sock, const char *bind_address, bool blocking);
int infi_socket_nonblock(sock_t sock);
int infi_socketpair(sock_t *sp1, sock_t *sp2);
int infi_read_byte(struct infi_packet *packet, uint8_t *byte);
int infi_read_bytes(struct infi_packet *packet, void *bytes, uint32_t count);
int infi_read_string(struct infi_packet *packet, char **str);
int infi_read_uint16(struct infi_packet *packet, uint16_t *word);
void infi_write_byte(struct infi_packet *packet, uint8_t byte);
void infi_write_bytes(struct infi_packet *packet, const void *bytes,
        uint32_t count);
void infi_write_string(struct infi_packet *packet, const char *str,
        uint16_t length);
void infi_write_uint16(struct infi_packet *packet, uint16_t word);
ssize_t infi_net_read(struct infi *inf, void *buf, size_t count);
ssize_t infi_net_write(struct infi *inf, void *buf, size_t count);
int infi_packet_write(struct infi *inf);
int infi_packet_read(struct infi *inf);
int infi_packet_handle(struct infi *inf);
int infi_handle_connack(struct infi *inf);
int infi_handle_pingreq(struct infi *inf);
int infi_handle_pingresp(struct infi *inf);
int infi_handle_pubackcomp(struct infi *inf, const char *type);
int infi_handle_publish(struct infi *inf);
int infi_handle_pubrec(struct infi *inf);
int infi_handle_pubrel(struct infi *inf);

/* ============================================================
 * Send function declaration
 * ============================================================ */
int infi_send_simple_command(struct infi *inf, uint8_t command);
int infi_send_command_with_mid(struct infi *inf, uint8_t command, uint16_t mid,
bool dup);
int infi_send_real_publish(struct infi *inf, uint16_t mid, const char *topic,
        uint32_t payloadlen, const void *payload, int qos, bool retain,
        bool dup);
int infi_send_connect(struct infi *inf, uint16_t keepalive,
bool clean_session);
int infi_send_disconnect(struct infi *inf);
int infi_send_pingreq(struct infi *inf);
int infi_send_pingresp(struct infi *inf);
int infi_send_puback(struct infi *inf, uint16_t mid);
int infi_send_pubcomp(struct infi *inf, uint16_t mid);
int infi_send_publish(struct infi *inf, uint16_t mid, const char *topic,
        uint32_t payloadlen, const void *payload, int qos,
        bool retain, bool dup);
int infi_send_pubrec(struct infi *inf, uint16_t mid);
int infi_send_pubrel(struct infi *inf, uint16_t mid);

/* ============================================================
 * infi_client_shared function declaration
 * ============================================================ */
int client_config_load(struct infi_config *config);
void client_config_cleanup(struct infi_config *cfg);
int client_opts_set(struct infi *inf, struct infi_config *cfg);
int client_id_generate(struct infi_config *cfg, const char *id_base);
int client_connect(struct infi *inf, struct infi_config *cfg);

/* ============================================================
 * get file information function declaration
 * ============================================================ */
void get_setup_from_file(struct infi_config *cfg, char *fname);
char *get_template_information_from_file(char *fname, int ftype);

/* ============================================================
 * mysql function declaration
 * ============================================================ */
void get_sensor_parser_from_db(struct infi_config *cfg);
void finish_with_error(MYSQL *con);
void mysql_db_connect(MYSQL* con);
void mysql_db_query(MYSQL* con, char *str);
void finish_with_error(MYSQL *con);
void persist_sensor_parser_mask_in_db(MYSQL *con, char *id);
void db_update_sensor_parser_mask_for_id(MYSQL *con, char *id);
void db_select_sensor_parser_based_on_limit(MYSQL *con,
        struct infi_config *cfg);
/* ============================================================
 * utility function declaration
 * ============================================================ */
void push(struct sensor_parser_data ** start, struct sensor_parser *custdata);
char *str_replace(char *orig, char *rep, char *with);
void cleanup_for_str(char *str);
char *remove_space(char *text);
char *duplicate_the_original_string(char *publish_context,
        char *template_replace_key[], char *template_replace_with_key[]);
int infi_log_messages(int priority, const char *charfmt, ...);
#endif


