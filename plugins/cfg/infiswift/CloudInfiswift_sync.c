#include "CloudInfiswift_sync.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/time.h>
#define FILESETUP "setup.ini"
#define FILEMESSAGE "message_template"
#define FILETOPIC "topic_template"

struct sensor_parser_data *global;
long msglen = 0;
static int qos = 0;
static int retain = 0;
static int mode = MSGMODE_NONE;
static int mid_sent = 0;
static char *username = NULL;
static char *password = NULL;
static char *topic = NULL;
char *message = NULL;
static bool connected = true;
static bool disconnect_sent = false;
static bool quiet = false;
int msg_count = 0;
bool process_messages = true;
bool connack_set = false;
char charlog_type[15];
char charlog_destination[15];
char log_file_location[250];

void my_connect_callback(struct infi *inf, int results) {
    int result = err_success;

    if (!results) {
        switch (mode) {
        case MSGMODE_CMD:
            connack_set = true;
            break;
        }
        if (result) {
            if (!quiet) {
                switch (result) {
                case err_inval:
                    infi_log_messages(LOG_ERROR,
                            " Invalid input. Does your topic contain '+' or '#'?\n");
                    break;
                case err_nomem:
                    infi_log_messages(LOG_ERROR,
                            " Out of memory when trying to publish message.\n");
                    break;
                case err_no_conn:
                    infi_log_messages(LOG_ERROR,
                            " Client not connected when trying to publish.\n");
                    break;
                case err_protocol:
                    infi_log_messages(LOG_ERROR,
                            " Protocol error when communicating with broker.\n");
                    break;
                case err_payload_size:
                    infi_log_messages(LOG_ERROR,
                            " Message payload is too large.\n");
                    break;
                }
            }
            infi_disconnect(inf);
        }
    } else {
        if (results && !quiet) {
            infi_log_messages(LOG_INFO, "%s\n", infi_connack_string(results));
        }
    }
}

void my_disconnect_callback(struct infi *inf) {
    connected = false;
}

void my_publish_callback(struct infi *inf) {
    if (connack_set == false && disconnect_sent == false) {
        infi_disconnect(inf);
        disconnect_sent = true;
    }
}

int main() {

    struct sensor_parser_data *sp = NULL, *temp = NULL;
    char hostname[256];
    memset(hostname, 0, 256);
    gethostname(hostname, 256);
    char *template_replace_key[9] = { "#TIMESTAMP#", "#MESHLIUM#", "#ID#",
            "#ID_WASP#", "#ID_SECRET#", "#FRAME_TYPE#", "#FRAME_NUMBER#",
            "#SENSOR#", "#VALUE#" };

    struct infi *inf = NULL;
    struct infi_config cfg;
    int result;
    infi_log_messages(LOG_INFO, "Init...");
    result = client_config_load(&cfg);
    if (result) {
        client_config_cleanup(&cfg);
        return 1;
    }

    char *publish_topic = get_template_information_from_file(FILETOPIC,
                file_topic);
    char *publish_message = get_template_information_from_file(FILEMESSAGE,
                file_message);

    qos = cfg.qos;
    retain = cfg.retain;
    mode = cfg.pub_mode;
    username = cfg.username;
    password = cfg.password;
    quiet = cfg.quiet;

    if (mode == MSGMODE_NONE) {
        infi_log_messages(LOG_ERROR,
                " Both topic and message must be supplied.\n");
        return 1;
    }
    if (strcmp(cfg.id, "\0") == 0) {
        if (client_id_generate(&cfg, "Meshlium")) {
            return 1;
        }
    }
    inf = infi_new(&cfg, true, NULL);

    if (!inf) {
        switch (errno) {
        case ENOMEM:
            if (!quiet)
                infi_log_messages(LOG_ERROR, " Out of memory.\n");
            break;
        case EINVAL:
            if (!quiet)
                infi_log_messages(LOG_ERROR, " Invalid id.\n");
            break;
        }
        return err_success;
        return 1;
    }

    infi_connect_callback_set(inf, my_connect_callback);
    infi_disconnect_callback_set(inf, my_disconnect_callback);
    infi_publish_callback_set(inf, my_publish_callback);

    if (client_opts_set(inf, &cfg)) {
        return 1;
    }

    result = client_connect(inf, &cfg);
    if (result)
        return result;
    sp = global;
    MYSQL* con = mysql_init(NULL);
    mysql_db_connect(con);
    do {
        while (sp != NULL && connack_set == true) {
            char *template_replace_with_key[9] = { sp->data->timestamp,
                    hostname, sp->data->id, sp->data->id_wasp,
                    sp->data->id_secret, sp->data->frame_type,
                    sp->data->frame_number, sp->data->sensor, sp->data->value };
            topic = duplicate_the_original_string(publish_topic,
                    template_replace_key, template_replace_with_key);

            cfg.topic = topic;

            message = duplicate_the_original_string(publish_message,
                    template_replace_key, template_replace_with_key);

            msglen = strlen(message);
            cfg.message = message;
            sp = sp->next;
            if (sp == NULL) {
                connack_set = false;
            }
            infi_publish(inf, &mid_sent, topic, msglen, message, qos, retain);
            infi_loop(inf, -1, 1);
            infi_log_messages(LOG_INFO, "Mark sync..");
            persist_sensor_parser_mask_in_db(con, template_replace_with_key[2]);
            infi_free(topic);
            infi_free(message);

        }
        result = infi_loop(inf, -1, 1);
    } while (result == err_success && connected);
    mysql_close(con);
    infi_log_messages(LOG_INFO, "*********Sleeping %s seconds*********\n",
            cfg.setup.interval);
    while (global != NULL) {
        temp = global;
        global = global->next;
        infi_free(temp->data);
        infi_free(temp);
    }
    infi_free(publish_topic);
    infi_free(publish_message);
    infi_free(inf->host);
    infi_free(inf->id);
    infi_free(inf->username);
    infi_free(inf->password);
    infi_destroy(inf);
    free(global);
    client_config_cleanup(&cfg);
    return err_success;

    if (result) {
        infi_log_messages(LOG_ERROR, " %s\n", infi_strerror(result));
    }
    return result;
}

void client_config_cleanup(struct infi_config *cfg) {
    int i;
    if (cfg->id)
        infi_free(cfg->id);
    if (cfg->id_prefix)
        infi_free(cfg->id_prefix);
    if (cfg->host)
        infi_free(cfg->host);
    if (cfg->bind_address)
        infi_free(cfg->bind_address);
    if (cfg->username)
        infi_free(cfg->username);
    if (cfg->password)
        infi_free(cfg->password);
    if (cfg->will_topic)
        infi_free(cfg->will_topic);
    if (cfg->will_payload)
        infi_free(cfg->will_payload);
    if (cfg->topics) {
        for (i = 0; i < cfg->topic_count; i++) {
            infi_free(cfg->topics[i]);
        }
        infi_free(cfg->topics);
    }
    if (cfg->filter_outs) {
        for (i = 0; i < cfg->filter_out_count; i++) {
            infi_free(cfg->filter_outs[i]);
        }
        infi_free(cfg->filter_outs);
    }
    if (cfg->setup.interval)
        infi_free(cfg->setup.interval);
    if (cfg->setup.limit)
        infi_free(cfg->setup.limit);
    if (cfg->setup.log_level)
        infi_free(cfg->setup.log_level);
    if (cfg->setup.measuresync)
        infi_free(cfg->setup.measuresync);
    if (cfg->setup.mqttport)
        infi_free(cfg->setup.mqttport);
    if (cfg->setup.mqttsleeping)
        infi_free(cfg->setup.mqttsleeping);
    if (cfg->setup.qos)
        infi_free(cfg->setup.qos);
}

int client_config_load(struct infi_config *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->port = 1883;
    cfg->max_inflight = 20;
    cfg->keepalive = 60;
    cfg->clean_session = true;
    cfg->eol = true;
    cfg->protocol_version = MQTT_PROTOCOL_V31;
    get_setup_from_file(cfg, FILESETUP);
    if (cfg->setup.log_level) {
        switch (atoi(cfg->setup.log_level)) {
        case 0:
            break;
        case 1:
            strcpy(charlog_type, "ERROR");
            strcpy(charlog_destination, "TEXTFILE");
            break;
        case 2:
            strcpy(charlog_type, "INFORMATION");
            strcpy(charlog_destination, "TEXTFILE");
            break;
        case 3:
            strcpy(charlog_type, "ALL");
            strcpy(charlog_destination, "ALL");
            break;
        }
    }
    get_sensor_parser_from_db(cfg);
    cfg->host = cfg->setup.mqttserver;
    cfg->port = atoi(cfg->setup.mqttport);
    cfg->username = cfg->setup.mqttuser;
    cfg->password = cfg->setup.mqttpassword;
    cfg->qos = atoi(cfg->setup.qos);
    cfg->id = cfg->setup.client_id;
    cfg->pub_mode = MSGMODE_CMD;

    if (cfg->password && !cfg->username) {
        if (!cfg->quiet)
            infi_log_messages(LOG_ERROR,
                    "Warning: Not using password since username not set.\n");
    }
    if (!cfg->host) {
        cfg->host = "localhost";
    }
    return err_success;

    return 1;
}

int client_opts_set(struct infi *inf, struct infi_config *cfg) {
    if (cfg->username
            && infi_username_pw_set(inf, cfg->username, cfg->password)) {
        if (!cfg->quiet)
            infi_log_messages(LOG_ERROR,
                    " Problem setting username and password.\n");
        return err_success;
        return 1;
    }

    inf->max_inflight_messages = cfg->max_inflight;

    infi_opts_set(inf, opt_protocol_version, &(cfg->protocol_version));
    return err_success;
}

int client_id_generate(struct infi_config *cfg, const char *id_base) {
    int len;
    char hostname[256];

    if (cfg->id_prefix) {
        cfg->id = malloc(strlen(cfg->id_prefix) + 10);
        if (!cfg->id) {
            if (!cfg->quiet)
                infi_log_messages(LOG_ERROR, " Out of memory.\n");
            return err_success;
            return 1;
        }
        snprintf(cfg->id, strlen(cfg->id_prefix) + 10, "%s%d", cfg->id_prefix,
                getpid());
    } else if (strcmp(cfg->id, "\0") == 0) {
        hostname[0] = '\0';
        gethostname(hostname, 256);
        hostname[255] = '\0';
        len = strlen(id_base) + strlen("/-") + 6 + strlen(hostname);
        cfg->id = malloc(len);
        if (!cfg->id) {
            if (!cfg->quiet)
                infi_log_messages(LOG_ERROR, " Out of memory.\n");
            return err_success;
            return 1;
        }
        snprintf(cfg->id, len, "%s/%d-%s", id_base, getpid(), hostname);
        if (strlen(cfg->id) > MQTT_ID_MAX_LENGTH) {
            cfg->id[MQTT_ID_MAX_LENGTH] = '\0';
        }
    }
    return err_success;
}

int client_connect(struct infi *inf, struct infi_config *cfg) {
    char err[1024];
    int result;
    result = infi_connect_bind(inf, cfg->host, cfg->port, cfg->keepalive,
            cfg->bind_address);

    if (result) {
        if (!cfg->quiet) {
            if (result == err_errno) {
                strerror_r(errno, err, 1024);
                infi_log_messages(LOG_ERROR, " %s\n", err);
            } else {
                infi_log_messages(LOG_ERROR, "Unable to connect (%s).\n",
                        infi_strerror(result));
            }
        }
        return result;
    }
    return err_success;
}

int infi_lib_init(void) {
    struct timeval tv;

    gettimeofday(&tv, NULL);
    srand(tv.tv_sec * 1000 + tv.tv_usec / 1000);

    return err_success;
}

struct infi *infi_new(struct infi_config *cfg, bool clean_session,
        void *userdata) {
    struct infi *inf = NULL;
    int result;

    if (clean_session == false && cfg->id == NULL) {
        errno = EINVAL;
        return NULL;
    }

    signal(SIGPIPE, SIG_IGN);

    inf = (struct infi *) infi_calloc(1, sizeof(struct infi));
    if (inf) {
        inf->sock = INVALID_SOCKET;
        inf->sockpairR = INVALID_SOCKET;
        inf->sockpairW = INVALID_SOCKET;

        result = infi_reinitialise(inf, cfg->id, clean_session, userdata);
        if (result) {
            infi_destroy(inf);
            if (result == err_inval) {
                errno = EINVAL;
            } else if (result == err_nomem) {
                errno = ENOMEM;
            }
            return NULL;
        }
    } else {
        errno = ENOMEM;
    }
    return inf;
}

int infi_reinitialise(struct infi *inf, const char *id,
bool clean_session, void *userdata) {
    int i;

    if (!inf)
        return err_inval;

    if (clean_session == false && id == NULL) {
        return err_inval;
    }

    memset(inf, 0, sizeof(struct infi));

    if (userdata) {
        inf->userdata = userdata;
    } else {
        inf->userdata = inf;
    }
    inf->protocol = p_mqtt31;
    inf->sock = INVALID_SOCKET;
    inf->sockpairR = INVALID_SOCKET;
    inf->sockpairW = INVALID_SOCKET;
    inf->keepalive = 60;
    inf->message_retry = 20;
    inf->last_retry_check = 0;
    inf->clean_session = clean_session;
    if (id) {
        if (STREMPTY(id)) {
            return err_inval;
        }
        inf->id = infi_strdup(id);
    } else {
        inf->id = (char *) infi_calloc(24, sizeof(char));
        if (!inf->id) {
            return err_nomem;
        }
        inf->id[0] = 'M';
        inf->id[1] = 'e';
        inf->id[2] = 's';
        inf->id[3] = 'h';
        inf->id[4] = 'l';
        inf->id[5] = 'i';
        inf->id[6] = 'u';
        inf->id[7] = 'm';
        inf->id[8] = '/';

        for (i = 9; i < 23; i++) {
            inf->id[i] = (rand() % 73) + 48;
        }
    }
    inf->in_packet.payload = NULL;
    infi_packet_cleanup(&inf->in_packet);
    inf->out_packet = NULL;
    inf->current_out_packet = NULL;
    inf->last_msg_in = infi_time();
    inf->last_msg_out = infi_time();
    inf->ping_t = 0;
    inf->last_mid = 0;
    inf->state = cs_new;
    inf->in_messages = NULL;
    inf->in_messages_last = NULL;
    inf->out_messages = NULL;
    inf->out_messages_last = NULL;
    inf->max_inflight_messages = 20;
    inf->will = NULL;
    inf->on_connect = NULL;
    inf->on_publish = NULL;
    inf->on_message = NULL;
    inf->host = NULL;
    inf->port = 1883;
    inf->in_callback = false;
    inf->in_queue_len = 0;
    inf->out_queue_len = 0;
    inf->reconnect_delay = 1;
    inf->reconnect_delay_max = 1;
    inf->reconnect_exponential_backoff = false;
    inf->threaded = false;

    return err_success;
}

int infi_username_pw_set(struct infi *inf, const char *username,
        const char *password) {
    if (!inf)
        return err_inval;

    if (inf->username) {
        infi_free(inf->username);
        inf->username = NULL;
    }
    if (inf->password) {
        infi_free(inf->password);
        inf->password = NULL;
    }

    if (username) {
        inf->username = infi_strdup(username);
        if (!inf->username)
            return err_nomem;
        if (password) {
            inf->password = infi_strdup(password);
            if (!inf->password) {
                infi_free(inf->username);
                inf->username = NULL;
                return err_nomem;
            }
        }
    }
    return err_success;
}

void infi_destroy(struct infi *inf) {
    if (!inf)
        return;
    infi_free(inf);
}

int infi_connect_init(struct infi *inf, const char *host, int port,
        int keepalive, const char *bind_address) {
    if (!inf)
        return err_inval;
    if (!host || port <= 0)
        return err_inval;

    if (inf->host)
        infi_free(inf->host);
    inf->host = infi_strdup(host);
    if (!inf->host)
        return err_nomem;
    inf->port = port;

    if (inf->bind_address)
        infi_free(inf->bind_address);
    if (bind_address) {
        inf->bind_address = infi_strdup(bind_address);
        if (!inf->bind_address)
            return err_nomem;
    }

    inf->keepalive = keepalive;

    if (inf->sockpairR != INVALID_SOCKET) {
        COMPAT_CLOSE(inf->sockpairR);
        inf->sockpairR = INVALID_SOCKET;
    }
    if (inf->sockpairW != INVALID_SOCKET) {
        COMPAT_CLOSE(inf->sockpairW);
        inf->sockpairW = INVALID_SOCKET;
    }

    if (infi_socketpair(&inf->sockpairR, &inf->sockpairW)) {
        infi_log_messages(LOG_ERROR,
                "Warning: Unable to open socket pair, outgoing publish commands may be delayed.");
    }

    return err_success;
}

int infi_connect_bind(struct infi *inf, const char *host, int port,
        int keepalive, const char *bind_address) {
    int result;
    result = infi_connect_init(inf, host, port, keepalive, bind_address);
    if (result)
        return result;
    inf->state = cs_new;
    return infi_reconnect(inf);
}

int infi_reconnect(struct infi *inf) {
    return infi_reconnect_blocking(inf, true);
}

int infi_reconnect_blocking(struct infi *inf, bool blocking) {
    int rc;
    struct infi_packet *packet;
    if (!inf)
        return err_inval;
    if (!inf->host || inf->port <= 0)
        return err_inval;
    inf->state = cs_new;

    inf->last_msg_in = infi_time();
    inf->last_msg_out = infi_time();

    inf->ping_t = 0;

    infi_packet_cleanup(&inf->in_packet);

    if (inf->out_packet && !inf->current_out_packet) {
        inf->current_out_packet = inf->out_packet;
        inf->out_packet = inf->out_packet->next;
    }

    while (inf->current_out_packet) {
        packet = inf->current_out_packet;
        /* Free data and reset values */
        inf->current_out_packet = inf->out_packet;
        if (inf->out_packet) {
            inf->out_packet = inf->out_packet->next;
        }

        infi_packet_cleanup(packet);
        infi_free(packet);
    }
    infi_messages_reconnect_reset(inf);

    rc = infi_socket_connect(inf, inf->host, inf->port, inf->bind_address,
            blocking);

    if (rc > 0) {
        return rc;
    }

    return infi_send_connect(inf, inf->keepalive, inf->clean_session);
}

int infi_disconnect(struct infi *inf) {
    if (!inf)
        return err_inval;

    inf->state = cs_disconnecting;

    if (inf->sock == INVALID_SOCKET)
        return err_no_conn;
    return infi_send_disconnect(inf);
}

int infi_publish(struct infi *inf, int *mid, const char *topic, int payloadlen,
        const void *payload, int qos, bool retain) {
    struct infi_message_all *message;
    uint16_t local_mid;
    int queue_status;

    if (!inf || !topic || qos < 0 || qos > 2)
        return err_inval;
    if (STREMPTY(topic))
        return err_inval;
    if (payloadlen < 0 || payloadlen > MQTT_MAX_PAYLOAD)
        return err_payload_size;

    if (infi_pub_topic_check(topic) != err_success) {
        return err_inval;
    }

    local_mid = infi_mid_generate(inf);
    if (mid) {
        *mid = local_mid;
    }

    if (qos == 0) {
        return infi_send_publish(inf, local_mid, topic, payloadlen, payload,
                qos, retain, false);
    } else {
        message = infi_calloc(1, sizeof(struct infi_message_all));
        if (!message)
            return err_nomem;

        message->next = NULL;
        message->timestamp = infi_time();
        message->msg.mid = local_mid;
        message->msg.topic = infi_strdup(topic);
        if (!message->msg.topic) {
            infi_message_cleanup(&message);
            return err_nomem;
        }
        if (payloadlen) {
            message->msg.payloadlen = payloadlen;
            message->msg.payload = infi_malloc(payloadlen * sizeof(uint8_t));
            if (!message->msg.payload) {
                infi_message_cleanup(&message);
                return err_nomem;
            }
            memcpy(message->msg.payload, payload, payloadlen * sizeof(uint8_t));
        } else {
            message->msg.payloadlen = 0;
            message->msg.payload = NULL;
        }
        message->msg.qos = qos;
        message->msg.retain = retain;
        message->dup = false;

        queue_status = infi_message_queue(inf, message, md_out);
        if (queue_status == 0) {
            if (qos == 1) {
                message->state = ms_wait_for_puback;
            } else if (qos == 2) {
                message->state = ms_wait_for_pubrec;
            }
            return infi_send_publish(inf, message->msg.mid, message->msg.topic,
                    message->msg.payloadlen, message->msg.payload,
                    message->msg.qos, message->msg.retain, message->dup);
        } else {
            message->state = ms_invalid;
            return err_success;
        }
    }
}

int infi_loop(struct infi *inf, int timeout, int max_packets) {
#ifdef HAVE_PSELECT
    struct timespec local_timeout;
#else
    struct timeval local_timeout;
#endif
    fd_set readfds, writefds;
    int fdcount;
    int result;
    char pairbuf;
    int maxfd = 0;

    if (!inf || max_packets < 1)
        return err_inval;
    if (inf->sock >= FD_SETSIZE || inf->sockpairR >= FD_SETSIZE) {
        return err_inval;
    }

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    if (inf->sock != INVALID_SOCKET) {
        maxfd = inf->sock;
        FD_SET(inf->sock, &readfds);
        if (inf->out_packet || inf->current_out_packet) {
            FD_SET(inf->sock, &writefds);
        }
    } else {
        return err_no_conn;
    }
    if (inf->sockpairR != INVALID_SOCKET) {
        FD_SET(inf->sockpairR, &readfds);
        if (inf->sockpairR > maxfd) {
            maxfd = inf->sockpairR;
        }
    }

    if (timeout >= 0) {
        local_timeout.tv_sec = timeout / 1000;
#ifdef HAVE_PSELECT
        local_timeout.tv_nsec = (timeout - local_timeout.tv_sec * 1000) * 1e6;
#else
        local_timeout.tv_usec = (timeout-local_timeout.tv_sec*1000)*1000;
#endif
    } else {
        local_timeout.tv_sec = 1;
#ifdef HAVE_PSELECT
        local_timeout.tv_nsec = 0;
#else
        local_timeout.tv_usec = 0;
#endif
    }

#ifdef HAVE_PSELECT
    fdcount = pselect(maxfd + 1, &readfds, &writefds, NULL, &local_timeout,
    NULL);
#else
    fdcount = select(maxfd+1, &readfds, &writefds, NULL, &local_timeout);
#endif
    if (fdcount == -1) {
        if (errno == EINTR) {
            return err_success;
        } else {
            return err_errno;
        }
    } else {
        if (inf->sock != INVALID_SOCKET) {
            if (FD_ISSET(inf->sock, &readfds)) {
                do {
                    result = infi_loop_read(inf, max_packets);
                    if (result || inf->sock == INVALID_SOCKET) {
                        return result;
                    }
                } while (SSL_DATA_PENDING(inf));
            }
            if (inf->sockpairR != INVALID_SOCKET
                    && FD_ISSET(inf->sockpairR, &readfds)) {
                if (read(inf->sockpairR, &pairbuf, 1) == 0) {
                }

                FD_SET(inf->sock, &writefds);
            }
            if (FD_ISSET(inf->sock, &writefds)) {
                result = infi_loop_write(inf, max_packets);
                if (result || inf->sock == INVALID_SOCKET) {
                    return result;
                }
            }
        }
    }
    return infi_loop_misc(inf);
}

int infi_loop_misc(struct infi *inf) {
    time_t now;

    if (!inf)
        return err_inval;
    if (inf->sock == INVALID_SOCKET)
        return err_no_conn;

    infi_check_keepalive(inf);
    now = infi_time();
    if (inf->last_retry_check + 1 < now) {
        infi_message_retry_check(inf);
        inf->last_retry_check = now;
    }
    if (inf->ping_t && now - inf->ping_t >= inf->keepalive) {

        infi_socket_close(inf);

        if (inf->on_disconnect) {
            inf->in_callback = true;
            inf->on_disconnect(inf);
            inf->in_callback = false;
        }
        return err_conn_lost;
    }
    return err_success;
}

static int infi_loop_rc_handle(struct infi *inf, int result) {
    if (result) {
        infi_socket_close(inf);
        if (inf->state == cs_disconnecting) {
            result = err_success;
        }
        if (inf->on_disconnect) {
            inf->in_callback = true;
            inf->on_disconnect(inf);
            inf->in_callback = false;
        }
        return result;
    }
    return result;
}

int infi_loop_read(struct infi *inf, int max_packets) {
    int result;
    int i;
    if (max_packets < 1)
        return err_inval;

    max_packets = inf->out_queue_len;
    max_packets += inf->in_queue_len;
    if (max_packets < 1)
        max_packets = 1;

    for (i = 0; i < max_packets; i++) {

        result = infi_packet_read(inf);

        if (result || errno == EAGAIN || errno == COMPAT_EWOULDBLOCK) {
            return infi_loop_rc_handle(inf, result);
        }
    }
    return result;
}

int infi_loop_write(struct infi *inf, int max_packets) {
    int result;
    int i;
    if (max_packets < 1)
        return err_inval;

    max_packets = inf->out_queue_len;
    max_packets += inf->in_queue_len;

    if (max_packets < 1)
        max_packets = 1;

    for (i = 0; i < max_packets; i++) {
        result = infi_packet_write(inf);
        if (result || errno == EAGAIN || errno == COMPAT_EWOULDBLOCK) {
            return infi_loop_rc_handle(inf, result);
        }
    }
    return result;
}

int infi_opts_set(struct infi *inf, enum infi_opt_t option, void *value) {
    int ival;

    if (!inf || !value)
        return err_inval;

    switch (option) {
    case opt_protocol_version:
        ival = *((int *) value);
        if (ival == MQTT_PROTOCOL_V31) {
            inf->protocol = p_mqtt31;
        } else if (ival == MQTT_PROTOCOL_V311) {
            inf->protocol = p_mqtt311;
        } else {
            return err_inval;
        }
        break;
    default:
        return err_inval;
    }
    return err_success;
}

void infi_connect_callback_set(struct infi *inf,
        void (*on_connect)(struct infi *, int)) {
    inf->on_connect = on_connect;
}

void infi_disconnect_callback_set(struct infi *inf,
        void (*on_disconnect)(struct infi *)) {
    inf->on_disconnect = on_disconnect;
}

void infi_publish_callback_set(struct infi *inf,
        void (*on_publish)(struct infi *)) {
    inf->on_publish = on_publish;
}

void infi_message_callback_set(struct infi *inf,
        void (*on_message)(struct infi *, void *, const struct infi_message *)) {
    inf->on_message = on_message;
}

const char *infi_strerror(int inf_errno) {
    switch (inf_errno) {
    case err_conn_pending:
        return "Connection pending.";
    case err_success:
        return "No error.";
    case err_nomem:
        return "Out of memory.";
    case err_protocol:
        return "A network protocol error occurred when communicating with the broker.";
    case err_inval:
        return "Invalid function arguments provided.";
    case err_no_conn:
        return "The client is not currently connected.";
    case err_conn_refused:
        return "The connection was refused.";
    case err_not_found:
        return "Message not found (internal error).";
    case err_conn_lost:
        return "The connection was lost.";
    case err_tls:
        return "A TLS error occurred.";
    case err_payload_size:
        return "Payload too large.";
    case err_not_supported:
        return "This feature is not supported.";
    case err_auth:
        return "Authorisation failed.";
    case err_acl_denied:
        return "Access denied by ACL.";
    case err_unknown:
        return "Unknown error.";
    case err_errno:
        return strerror(errno);
    case err_eai:
        return "Lookup error.";
    case err_proxy:
        return "Proxy error.";
    default:
        return "Unknown error.";
    }
}

const char *infi_connack_string(int connack_code) {
    switch (connack_code) {
    case connack_accepted:
        return "Connection Accepted.";
    case connack_refused_protocol_version:
        return "Connection Refused: unacceptable protocol version.";
    case connack_refused_identifier_rejected:
        return "Connection Refused: identifier rejected.";
    case connack_refused_server_unavailable:
        return "Connection Refused: broker unavailable.";
    case connack_refused_bad_username_password:
        return "Connection Refused: bad user name or password.";
    case connack_refused_not_authorized:
        return "Connection Refused: not authorised.";
    default:
        return "Connection Refused: unknown reason.";
    }
}

int infi_packet_handle(struct infi *inf) {
    assert(inf);

    switch ((inf->in_packet.command) & 0xF0) {
    case PINGREQ:
        return infi_handle_pingreq(inf);
    case PINGRESP:
        return infi_handle_pingresp(inf);
    case PUBACK:
        return infi_handle_pubackcomp(inf, "PUBACK");
    case PUBCOMP:
        return infi_handle_pubackcomp(inf, "PUBCOMP");
    case PUBLISH:
        return infi_handle_publish(inf);
    case PUBREC:
        return infi_handle_pubrec(inf);
    case PUBREL:
        return infi_handle_pubrel(inf);
    case CONNACK:
        return infi_handle_connack(inf);
    default:
        infi_log_messages(LOG_ERROR, " Unrecognised command %d\n",
                (inf->in_packet.command) & 0xF0);
        return err_protocol;
    }
}

int infi_handle_publish(struct infi *inf) {
    uint8_t header;
    struct infi_message_all *message;
    int result = 0;
    uint16_t mid;

    assert(inf);

    message = infi_calloc(1, sizeof(struct infi_message_all));
    if (!message)
        return err_nomem;

    header = inf->in_packet.command;

    message->dup = (header & 0x08) >> 3;
    message->msg.qos = (header & 0x06) >> 1;
    message->msg.retain = (header & 0x01);

    result = infi_read_string(&inf->in_packet, &message->msg.topic);
    if (result) {
        infi_message_cleanup(&message);
        return result;
    }
    if (!strlen(message->msg.topic)) {
        infi_message_cleanup(&message);
        return err_protocol;
    }

    if (message->msg.qos > 0) {
        result = infi_read_uint16(&inf->in_packet, &mid);
        if (result) {
            infi_message_cleanup(&message);
            return result;
        }
        message->msg.mid = (int) mid;
    }

    message->msg.payloadlen = inf->in_packet.remaining_length
            - inf->in_packet.pos;
    if (message->msg.payloadlen) {
        message->msg.payload = infi_calloc(message->msg.payloadlen + 1,
                sizeof(uint8_t));
        if (!message->msg.payload) {
            infi_message_cleanup(&message);
            return err_nomem;
        }
        result = infi_read_bytes(&inf->in_packet, message->msg.payload,
                message->msg.payloadlen);
        if (result) {
            infi_message_cleanup(&message);
            return result;
        }
    }
    infi_log_messages(LOG_INFO,
            "Client %s received PUBLISH (d%d, q%d, r%d, m%d, '%s', ... (%ld bytes))",
            inf->id, message->dup, message->msg.qos, message->msg.retain,
            message->msg.mid, message->msg.topic,
            (long) message->msg.payloadlen);

    message->timestamp = infi_time();
    switch (message->msg.qos) {
    case 0:
        if (inf->on_message) {
            inf->in_callback = true;
            inf->on_message(inf, inf->userdata, &message->msg);
            inf->in_callback = false;
        }
        infi_message_cleanup(&message);
        return err_success;
    case 1:
        result = infi_send_puback(inf, message->msg.mid);
        if (inf->on_message) {
            inf->in_callback = true;
            inf->on_message(inf, inf->userdata, &message->msg);
            inf->in_callback = false;
        }
        infi_message_cleanup(&message);
        return result;
    case 2:
        result = infi_send_pubrec(inf, message->msg.mid);
        message->state = ms_wait_for_pubrel;
        infi_message_queue(inf, message, md_in);
        return result;
    default:
        infi_message_cleanup(&message);
        return err_protocol;
    }
}

int infi_handle_connack(struct infi *inf) {
    uint8_t byte;
    uint8_t results;
    int result;

    assert(inf);
    infi_log_messages(LOG_INFO, "Client %s received CONNACK", inf->id);
    result = infi_read_byte(&inf->in_packet, &byte);
    if (result)
        return result;
    result = infi_read_byte(&inf->in_packet, &results);
    if (result)
        return result;
    infi_log_messages(LOG_INFO, "Broker Connected to %s", inf->host);
    if (inf->on_connect) {
        inf->in_callback = true;
        inf->on_connect(inf, results);
        inf->in_callback = false;
    }
    if (inf->state != cs_disconnecting) {
        inf->state = cs_connected;
    }
    return err_success;
    return err_conn_refused;
    return err_protocol;
}

int infi_handle_pingreq(struct infi *inf) {
    assert(inf);
    infi_log_messages(LOG_INFO, "Client %s received PINGREQ", inf->id);
    return infi_send_pingresp(inf);
}

int infi_handle_pingresp(struct infi *inf) {
    assert(inf);
    inf->ping_t = 0;
    infi_log_messages(LOG_INFO, "Client %s received PINGRESP", inf->id);
    return err_success;
}

int infi_handle_pubackcomp(struct infi *inf, const char *type) {
    uint16_t mid;
    int result;

    assert(inf);
    result = infi_read_uint16(&inf->in_packet, &mid);
    if (result)
        return result;
    infi_log_messages(LOG_INFO, "Client %s received %s (Mid: %d)", inf->id,
            type, mid);

    if (!infi_message_delete(inf, mid, md_out)) {

        if (inf->on_publish) {
            inf->in_callback = true;
            inf->on_publish(inf);
            inf->in_callback = false;
        }
    }
    return err_success;
}

int infi_handle_pubrec(struct infi *inf) {
    uint16_t mid;
    int result;

    assert(inf);
    result = infi_read_uint16(&inf->in_packet, &mid);
    if (result)
        return result;
    infi_log_messages(LOG_INFO, "Client %s received PUBREC (Mid: %d)", inf->id,
            mid);
    result = infi_message_out_update(inf, mid, ms_wait_for_pubcomp);
    if (result)
        return result;
    result = infi_send_pubrel(inf, mid);
    if (result)
        return result;
    return err_success;
}

int infi_handle_pubrel(struct infi *inf) {
    uint16_t mid;
    struct infi_message_all *message = NULL;
    int result;
    assert(inf);
    if (inf->protocol == p_mqtt311) {
        if ((inf->in_packet.command & 0x0F) != 0x02) {
            return err_protocol;
        }
    }
    result = infi_read_uint16(&inf->in_packet, &mid);
    if (result)
        return result;
    infi_log_messages(LOG_INFO, "Client %s received PUBREL (Mid: %d)", inf->id,
            mid);

    if (!infi_message_remove(inf, mid, md_in, &message)) {

        if (inf->on_message) {
            inf->in_callback = true;
            inf->on_message(inf, inf->userdata, &message->msg);
            inf->in_callback = false;
        }
        infi_message_cleanup(&message);
    }
    result = infi_send_pubcomp(inf, mid);
    if (result)
        return result;

    return err_success;
}

int infi_send_pingreq(struct infi *inf) {
    int result;
    assert(inf);
    infi_log_messages(LOG_INFO, "Client %s sending PINGREQ", inf->id);
    result = infi_send_simple_command(inf, PINGREQ);
    if (result == err_success) {
        inf->ping_t = infi_time();
    }
    return result;
}

int infi_send_pingresp(struct infi *inf) {
    if (inf)
        infi_log_messages(LOG_INFO, "Client %s sending PINGRESP", inf->id);
    return infi_send_simple_command(inf, PINGRESP);
}

int infi_send_puback(struct infi *inf, uint16_t mid) {
    if (inf)
        infi_log_messages(LOG_INFO, "Client %s sending PUBACK (Mid: %d)",
                inf->id, mid);
    return infi_send_command_with_mid(inf, PUBACK, mid, false);
}

int infi_send_pubcomp(struct infi *inf, uint16_t mid) {
    if (inf)
        infi_log_messages(LOG_INFO, "Client %s sending PUBCOMP (Mid: %d)",
                inf->id, mid);
    return infi_send_command_with_mid(inf, PUBCOMP, mid, false);
}

int infi_send_publish(struct infi *inf, uint16_t mid, const char *topic,
        uint32_t payloadlen, const void *payload, int qos,
        bool retain, bool dup) {
    assert(inf);
    assert(topic);
    if (inf->sock == INVALID_SOCKET)
        return err_no_conn;
    infi_log_messages(LOG_INFO,
            "Client %s sending PUBLISH (d%d, q%d, r%d, m%d, '%s', ... (%ld bytes))",
            inf->id, dup, qos, retain, mid, topic, (long) payloadlen);
    return infi_send_real_publish(inf, mid, topic, payloadlen, payload, qos,
            retain, dup);
}

int infi_send_pubrec(struct infi *inf, uint16_t mid) {
    if (inf)
        infi_log_messages(LOG_INFO, "Client %s sending PUBREC (Mid: %d)",
                inf->id, mid);
    return infi_send_command_with_mid(inf, PUBREC, mid, false);
}

int infi_send_pubrel(struct infi *inf, uint16_t mid) {
    if (inf)
        infi_log_messages(LOG_INFO, "Client %s sending PUBREL (Mid: %d)",
                inf->id, mid);
    return infi_send_command_with_mid(inf, PUBREL | 2, mid, false);
}

int infi_send_command_with_mid(struct infi *inf, uint8_t command, uint16_t mid,
bool dup) {
    struct infi_packet *packet = NULL;
    int result;

    assert(inf);
    packet = infi_calloc(1, sizeof(struct infi_packet));
    if (!packet)
        return err_nomem;

    packet->command = command;
    if (dup) {
        packet->command |= 8;
    }
    packet->remaining_length = 2;
    result = infi_packet_alloc(packet);
    if (result) {
        infi_free(packet);
        return result;
    }

    packet->payload[packet->pos + 0] = MSB(mid);
    packet->payload[packet->pos + 1] = LSB(mid);

    return infi_packet_queue(inf, packet);
}

int infi_send_simple_command(struct infi *inf, uint8_t command) {
    struct infi_packet *packet = NULL;
    int result;

    assert(inf);
    packet = infi_calloc(1, sizeof(struct infi_packet));
    if (!packet)
        return err_nomem;

    packet->command = command;
    packet->remaining_length = 0;

    result = infi_packet_alloc(packet);
    if (result) {
        infi_free(packet);
        return result;
    }

    return infi_packet_queue(inf, packet);
}

int infi_send_real_publish(struct infi *inf, uint16_t mid, const char *topic,
        uint32_t payloadlen, const void *payload, int qos, bool retain,
        bool dup) {
    struct infi_packet *packet = NULL;
    int packetlen;
    int result;

    assert(inf);
    assert(topic);

    packetlen = 2 + strlen(topic) + payloadlen;
    if (qos > 0)
        packetlen += 2;
    packet = infi_calloc(1, sizeof(struct infi_packet));
    if (!packet)
        return err_nomem;

    packet->mid = mid;
    packet->command = PUBLISH | ((dup & 0x1) << 3) | (qos << 1) | retain;
    packet->remaining_length = packetlen;
    result = infi_packet_alloc(packet);
    if (result) {
        infi_free(packet);
        return result;
    }

    infi_write_string(packet, topic, strlen(topic));
    if (qos > 0) {
        infi_write_uint16(packet, mid);
    }

    if (payloadlen) {
        infi_write_bytes(packet, payload, payloadlen);
    }

    return infi_packet_queue(inf, packet);
}

int infi_send_connect(struct infi *inf, uint16_t keepalive,
bool clean_session) {
    struct infi_packet *packet = NULL;
    int payloadlen;
    uint8_t will = 0;
    uint8_t byte;
    int result;
    uint8_t version;
    char *clientid, *username, *password;
    int headerlen;

    assert(inf);
    assert(inf->id);

    clientid = inf->id;
    username = inf->username;
    password = inf->password;

    if (inf->protocol == p_mqtt31) {
        version = MQTT_PROTOCOL_V31;
        headerlen = 12;
    } else if (inf->protocol == p_mqtt311) {
        version = MQTT_PROTOCOL_V311;
        headerlen = 10;
    } else {
        return err_inval;
    }

    packet = infi_calloc(1, sizeof(struct infi_packet));
    if (!packet)
        return err_nomem;

    payloadlen = 2 + strlen(clientid);
    if (inf->will) {
        will = 1;
        assert(inf->will->topic);

        payloadlen += 2 + strlen(inf->will->topic) + 2 + inf->will->payloadlen;
    }
    if (username) {
        payloadlen += 2 + strlen(username);
        if (password) {
            payloadlen += 2 + strlen(password);
        }
    }

    packet->command = CONNECT;
    packet->remaining_length = headerlen + payloadlen;
    result = infi_packet_alloc(packet);
    if (result) {
        infi_free(packet);
        return result;
    }

    if (version == MQTT_PROTOCOL_V31) {
        infi_write_string(packet, PROTOCOL_NAME_v31, strlen(PROTOCOL_NAME_v31));
    } else if (version == MQTT_PROTOCOL_V311) {
        infi_write_string(packet, PROTOCOL_NAME_v311,
                strlen(PROTOCOL_NAME_v311));
    }
    infi_write_byte(packet, version);
    byte = (clean_session & 0x1) << 1;
    if (will) {
        byte = byte | ((inf->will->retain & 0x1) << 5)
                | ((inf->will->qos & 0x3) << 3) | ((will & 0x1) << 2);
    }
    if (username) {
        byte = byte | 0x1 << 7;
        if (inf->password) {
            byte = byte | 0x1 << 6;
        }
    }
    infi_write_byte(packet, byte);
    infi_write_uint16(packet, keepalive);

    infi_write_string(packet, clientid, strlen(clientid));
    if (will) {
        infi_write_string(packet, inf->will->topic, strlen(inf->will->topic));
        infi_write_string(packet, (const char *) inf->will->payload,
                inf->will->payloadlen);
    }
    if (username) {
        infi_write_string(packet, username, strlen(username));
        if (password) {
            infi_write_string(packet, password, strlen(password));
        }
    }

    inf->keepalive = keepalive;
    infi_log_messages(LOG_INFO, "Client %s sending CONNECT", clientid);
    return infi_packet_queue(inf, packet);
}

int infi_send_disconnect(struct infi *inf) {
    assert(inf);
    infi_log_messages(LOG_INFO, "Client %s sending DISCONNECT", inf->id);
    return infi_send_simple_command(inf, DISCONNECT);
}

void infi_packet_cleanup(struct infi_packet *packet) {
    if (!packet)
        return;

    packet->command = 0;
    packet->remaining_count = 0;
    packet->remaining_mult = 1;
    packet->remaining_length = 0;
    if (packet->payload)
        infi_free(packet->payload);
    packet->payload = NULL;
    packet->to_process = 0;
    packet->pos = 0;
}

int infi_packet_queue(struct infi *inf, struct infi_packet *packet) {
    char sockpair_data = 0;
    assert(inf);
    assert(packet);

    packet->pos = 0;
    packet->to_process = packet->packet_length;

    packet->next = NULL;
    if (inf->out_packet) {
        inf->out_packet_last->next = packet;
    } else {
        inf->out_packet = packet;
    }
    inf->out_packet_last = packet;

    if (inf->sockpairW != INVALID_SOCKET) {
        if (write(inf->sockpairW, &sockpair_data, 1)) {
        }
    }
    if (inf->in_callback == false && inf->threaded == false) {
        return infi_packet_write(inf);
    } else {
        return err_success;
    }

}

int infi_socket_close(struct infi *inf) {
    int result = 0;

    assert(inf);

    if ((int) inf->sock >= 0) {
        result = COMPAT_CLOSE(inf->sock);
        inf->sock = INVALID_SOCKET;
    }

    return result;
}

int infi_try_connect(struct infi *inf, const char *host, uint16_t port,
        sock_t *sock, const char *bind_address,
        bool blocking) {
    struct addrinfo hints;
    struct addrinfo *ainfo, *rp;
    struct addrinfo *ainfo_bind, *rp_bind;
    int s;
    int result = err_success;
    *sock = INVALID_SOCKET;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = PF_UNSPEC;
    hints.ai_flags = AI_ADDRCONFIG;
    hints.ai_socktype = SOCK_STREAM;

    s = getaddrinfo(host, NULL, &hints, &ainfo);
    if (s) {
        errno = s;
        return err_eai;
    }

    if (bind_address) {
        s = getaddrinfo(bind_address, NULL, &hints, &ainfo_bind);
        if (s) {
            freeaddrinfo(ainfo);
            errno = s;
            return err_eai;
        }
    }

    for (rp = ainfo; rp != NULL; rp = rp->ai_next) {
        *sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (*sock == INVALID_SOCKET)
            continue;

        if (rp->ai_family == PF_INET) {
            ((struct sockaddr_in *) rp->ai_addr)->sin_port = htons(port);
        } else if (rp->ai_family == PF_INET6) {
            ((struct sockaddr_in6 *) rp->ai_addr)->sin6_port = htons(port);
        } else {
            COMPAT_CLOSE(*sock);
            continue;
        }

        if (bind_address) {
            for (rp_bind = ainfo_bind; rp_bind != NULL;
                    rp_bind = rp_bind->ai_next) {
                if (bind(*sock, rp_bind->ai_addr, rp_bind->ai_addrlen) == 0) {
                    break;
                }
            }
            if (!rp_bind) {
                COMPAT_CLOSE(*sock);
                continue;
            }
        }

        if (!blocking) {

            if (infi_socket_nonblock(*sock)) {
                COMPAT_CLOSE(*sock);
                continue;
            }
        }

        result = connect(*sock, rp->ai_addr, rp->ai_addrlen);
        if (result == 0 || errno == EINPROGRESS || errno == COMPAT_EWOULDBLOCK) {
            if (result < 0
                    && (errno == EINPROGRESS || errno == COMPAT_EWOULDBLOCK)) {
                result = err_conn_pending;
            }

            if (blocking) {

                if (infi_socket_nonblock(*sock)) {
                    COMPAT_CLOSE(*sock);
                    continue;
                }
            }
            break;
        }

        COMPAT_CLOSE(*sock);
        *sock = INVALID_SOCKET;
    }
    freeaddrinfo(ainfo);
    if (bind_address) {
        freeaddrinfo(ainfo_bind);
    }
    if (!rp) {
        return err_errno;
    }
    return result;
}

int infi_socket_connect(struct infi *inf, const char *host, uint16_t port,
        const char *bind_address, bool blocking) {
    sock_t sock = INVALID_SOCKET;
    int result;

    if (!inf || !host || !port)
        return err_inval;

    result = infi_try_connect(inf, host, port, &sock, bind_address, blocking);
    if (result > 0)
        return result;
    inf->sock = sock;

    return result;
}

int infi_read_byte(struct infi_packet *packet, uint8_t *byte) {
    assert(packet);
    if (packet->pos + 1 > packet->remaining_length)
        return err_protocol;

    *byte = packet->payload[packet->pos];
    packet->pos++;

    return err_success;
}

void infi_write_byte(struct infi_packet *packet, uint8_t byte) {
    assert(packet);
    assert(packet->pos + 1 <= packet->packet_length);

    packet->payload[packet->pos] = byte;
    packet->pos++;
}

int infi_read_bytes(struct infi_packet *packet, void *bytes, uint32_t count) {
    assert(packet);
    if (packet->pos + count > packet->remaining_length)
        return err_protocol;

    memcpy(bytes, &(packet->payload[packet->pos]), count);
    packet->pos += count;

    return err_success;
}

void infi_write_bytes(struct infi_packet *packet, const void *bytes,
        uint32_t count) {
    assert(packet);
    assert(packet->pos + count <= packet->packet_length);

    memcpy(&(packet->payload[packet->pos]), bytes, count);
    packet->pos += count;
}

int infi_read_string(struct infi_packet *packet, char **str) {
    uint16_t len;
    int result;

    assert(packet);
    result = infi_read_uint16(packet, &len);
    if (result)
        return result;

    if (packet->pos + len > packet->remaining_length)
        return err_protocol;

    *str = infi_malloc(len + 1);
    if (*str) {
        memcpy(*str, &(packet->payload[packet->pos]), len);
        (*str)[len] = '\0';
        packet->pos += len;
    } else {
        return err_nomem;
    }

    return err_success;
}

void infi_write_string(struct infi_packet *packet, const char *str,
        uint16_t length) {
    assert(packet);
    infi_write_uint16(packet, length);
    infi_write_bytes(packet, str, length);
}

int infi_read_uint16(struct infi_packet *packet, uint16_t *word) {
    uint8_t msb, lsb;

    assert(packet);
    if (packet->pos + 2 > packet->remaining_length)
        return err_protocol;

    msb = packet->payload[packet->pos];
    packet->pos++;
    lsb = packet->payload[packet->pos];
    packet->pos++;

    *word = (msb << 8) + lsb;

    return err_success;
}

void infi_write_uint16(struct infi_packet *packet, uint16_t word) {
    infi_write_byte(packet, MSB(word));
    infi_write_byte(packet, LSB(word));
}

ssize_t infi_net_read(struct infi *inf, void *buf, size_t count) {
    assert(inf);
    errno = 0;
    return read(inf->sock, buf, count);
}

ssize_t infi_net_write(struct infi *inf, void *buf, size_t count) {
    assert(inf);
    errno = 0;
    return write(inf->sock, buf, count);
}

int infi_packet_write(struct infi *inf) {
    ssize_t write_length;
    struct infi_packet *packet;

    if (!inf)
        return err_inval;
    if (inf->sock == INVALID_SOCKET)
        return err_no_conn;

    if (inf->out_packet && !inf->current_out_packet) {
        inf->current_out_packet = inf->out_packet;
        inf->out_packet = inf->out_packet->next;
        if (!inf->out_packet) {
            inf->out_packet_last = NULL;
        }
    }
    if (inf->state == cs_connect_pending) {
        return err_success;
    }

    while (inf->current_out_packet) {
        packet = inf->current_out_packet;

        while (packet->to_process > 0) {
            write_length = infi_net_write(inf, &(packet->payload[packet->pos]),
                    packet->to_process);
            if (write_length > 0) {
                packet->to_process -= write_length;
                packet->pos += write_length;
            } else {
                if (errno == EAGAIN || errno == COMPAT_EWOULDBLOCK) {
                    return err_success;
                } else {
                    switch (errno) {
                    case COMPAT_ECONNRESET:
                        return err_conn_lost;
                    default:
                        return err_errno;
                    }
                }
            }
        }

        if (((packet->command) & 0xF6) == PUBLISH) {
            if (inf->on_publish) {

                inf->in_callback = true;
                inf->on_publish(inf);
                inf->in_callback = false;
            }
        } else if (((packet->command) & 0xF0) == DISCONNECT) {

            infi_socket_close(inf);
            inf->current_out_packet = inf->out_packet;
            if (inf->out_packet) {
                inf->out_packet = inf->out_packet->next;
                if (!inf->out_packet) {
                    inf->out_packet_last = NULL;
                }
            }
            infi_packet_cleanup(packet);
            infi_free(packet);

            inf->last_msg_out = infi_time();
            if (inf->on_disconnect) {
                inf->in_callback = true;
                inf->on_disconnect(inf);
                inf->in_callback = false;
            }
            return err_success;
        }

        inf->current_out_packet = inf->out_packet;
        if (inf->out_packet) {
            inf->out_packet = inf->out_packet->next;
            if (!inf->out_packet) {
                inf->out_packet_last = NULL;
            }
        }
        infi_packet_cleanup(packet);
        infi_free(packet);

        inf->last_msg_out = infi_time();
    }
    return err_success;
}

int infi_packet_read(struct infi *inf) {
    uint8_t byte;
    ssize_t read_length;
    int result = 0;

    if (!inf)
        return err_inval;
    if (inf->sock == INVALID_SOCKET)
        return err_no_conn;
    if (inf->state == cs_connect_pending) {
        return err_success;
    }
    if (!inf->in_packet.command) {
        read_length = infi_net_read(inf, &byte, 1);
        if (read_length == 1) {
            inf->in_packet.command = byte;
        } else {
            if (read_length == 0)
                return err_conn_lost;
            if (errno == EAGAIN || errno == COMPAT_EWOULDBLOCK) {
                return err_success;
            } else {
                switch (errno) {
                case COMPAT_ECONNRESET:
                    return err_conn_lost;
                default:
                    return err_errno;
                }
            }
        }
    }

    if (inf->in_packet.remaining_count <= 0) {
        do {
            read_length = infi_net_read(inf, &byte, 1);
            if (read_length == 1) {
                inf->in_packet.remaining_count--;

                if (inf->in_packet.remaining_count < -4)
                    return err_protocol;
                inf->in_packet.remaining_length += (byte & 127)
                        * inf->in_packet.remaining_mult;
                inf->in_packet.remaining_mult *= 128;
            } else {
                if (read_length == 0)
                    return err_conn_lost;
                if (errno == EAGAIN || errno == COMPAT_EWOULDBLOCK) {
                    return err_success;
                } else {
                    switch (errno) {
                    case COMPAT_ECONNRESET:
                        return err_conn_lost;
                    default:
                        return err_errno;
                    }
                }
            }
        } while ((byte & 128) != 0);

        inf->in_packet.remaining_count *= -1;

        if (inf->in_packet.remaining_length > 0) {
            inf->in_packet.payload = infi_malloc(
                    inf->in_packet.remaining_length * sizeof(uint8_t));
            if (!inf->in_packet.payload)
                return err_nomem;
            inf->in_packet.to_process = inf->in_packet.remaining_length;
        }
    }
    while (inf->in_packet.to_process > 0) {
        read_length = infi_net_read(inf,
                &(inf->in_packet.payload[inf->in_packet.pos]),
                inf->in_packet.to_process);
        if (read_length > 0) {
            inf->in_packet.to_process -= read_length;
            inf->in_packet.pos += read_length;
        } else {
            if (errno == EAGAIN || errno == COMPAT_EWOULDBLOCK) {
                if (inf->in_packet.to_process > 1000) {

                    inf->last_msg_in = infi_time();
                }
                return err_success;
            } else {
                switch (errno) {
                case COMPAT_ECONNRESET:
                    return err_conn_lost;
                default:
                    return err_errno;
                }
            }
        }
    }

    inf->in_packet.pos = 0;
    result = infi_packet_handle(inf);

    infi_packet_cleanup(&inf->in_packet);

    inf->last_msg_in = infi_time();
    return result;
}

int infi_socket_nonblock(sock_t sock) {
    int opt;

    opt = fcntl(sock, F_GETFL, 0);
    if (opt == -1) {
        COMPAT_CLOSE(sock);
        return 1;
    }
    if (fcntl(sock, F_SETFL, opt | O_NONBLOCK) == -1) {

        COMPAT_CLOSE(sock);
        return 1;
    }
    return 0;
}

int infi_socketpair(sock_t *pairR, sock_t *pairW) {
    int sv[2];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1) {
        return err_errno;
    }
    if (infi_socket_nonblock(sv[0])) {
        COMPAT_CLOSE(sv[0]);
        COMPAT_CLOSE(sv[1]);
        return err_errno;
    }
    if (infi_socket_nonblock(sv[1])) {
        COMPAT_CLOSE(sv[0]);
        COMPAT_CLOSE(sv[1]);
        return err_errno;
    }
    *pairR = sv[0];
    *pairW = sv[1];
    return err_success;
}

void *infi_calloc(size_t nmemb, size_t size) {
    void *mem = calloc(nmemb, size);
    return mem;
}

void infi_free(void *mem) {
    if (!mem)
        return;
    free(mem);
    mem = NULL;
}

void *infi_malloc(size_t size) {
    void *mem = malloc(size);
    return mem;
}

char *infi_strdup(const char *s) {
    char *str = strdup(s);
    return str;
}

void infi_message_cleanup(struct infi_message_all **message) {
    struct infi_message_all *msg;

    if (!message || !*message)
        return;

    msg = *message;

    if (msg->msg.topic)
        infi_free(msg->msg.topic);
    if (msg->msg.payload)
        infi_free(msg->msg.payload);
    infi_free(msg);
}

int infi_message_delete(struct infi *inf, uint16_t mid,
        enum infi_msg_direction dir) {
    struct infi_message_all *message;
    int result;
    assert(inf);

    result = infi_message_remove(inf, mid, dir, &message);
    if (result == err_success) {
        infi_message_cleanup(&message);
    }
    return result;
}

int infi_message_queue(struct infi *inf, struct infi_message_all *message,
        enum infi_msg_direction dir) {
    int result = 0;

    assert(inf);
    assert(message);

    if (dir == md_out) {
        inf->out_queue_len++;
        message->next = NULL;
        if (inf->out_messages_last) {
            inf->out_messages_last->next = message;
        } else {
            inf->out_messages = message;
        }
        inf->out_messages_last = message;
        if (message->msg.qos > 0) {
            if (inf->max_inflight_messages == 0
                    || inf->inflight_messages < inf->max_inflight_messages) {
                inf->inflight_messages++;
            } else {
                result = 1;
            }
        }
    } else {
        inf->in_queue_len++;
        message->next = NULL;
        if (inf->in_messages_last) {
            inf->in_messages_last->next = message;
        } else {
            inf->in_messages = message;
        }
        inf->in_messages_last = message;
    }
    return result;
}

void infi_messages_reconnect_reset(struct infi *inf) {
    struct infi_message_all *message;
    struct infi_message_all *prev = NULL;
    assert(inf);

    message = inf->in_messages;
    inf->in_queue_len = 0;
    while (message) {
        inf->in_queue_len++;
        message->timestamp = 0;
        if (message->msg.qos != 2) {
            if (prev) {
                prev->next = message->next;
                infi_message_cleanup(&message);
                message = prev;
            } else {
                inf->in_messages = message->next;
                infi_message_cleanup(&message);
                message = inf->in_messages;
            }
        } else {

        }
        prev = message;
        message = message->next;
    }
    inf->in_messages_last = prev;
    inf->inflight_messages = 0;
    message = inf->out_messages;
    inf->out_queue_len = 0;
    while (message) {
        inf->out_queue_len++;
        message->timestamp = 0;

        if (message->msg.qos > 0) {
            inf->inflight_messages++;
        }
        if (inf->max_inflight_messages == 0
                || inf->inflight_messages < inf->max_inflight_messages) {
            if (message->msg.qos == 1) {
                message->state = ms_wait_for_puback;
            } else if (message->msg.qos == 2) {

            }
        } else {
            message->state = ms_invalid;
        }
        prev = message;
        message = message->next;
    }
    inf->out_messages_last = prev;
}

int infi_message_remove(struct infi *inf, uint16_t mid,
        enum infi_msg_direction dir, struct infi_message_all **message) {
    struct infi_message_all *cur, *prev = NULL;
    bool found = false;
    int result;
    assert(inf);
    assert(message);

    if (dir == md_out) {
        cur = inf->out_messages;
        while (cur) {
            if (cur->msg.mid == mid) {
                if (prev) {
                    prev->next = cur->next;
                } else {
                    inf->out_messages = cur->next;
                }
                *message = cur;
                inf->out_queue_len--;
                if (cur->next == NULL) {
                    inf->out_messages_last = prev;
                } else if (!inf->out_messages) {
                    inf->out_messages_last = NULL;
                }
                if (cur->msg.qos > 0) {
                    inf->inflight_messages--;
                }
                found = true;
                break;
            }
            prev = cur;
            cur = cur->next;
        }

        if (found) {
            cur = inf->out_messages;
            while (cur) {
                if (inf->max_inflight_messages == 0
                        || inf->inflight_messages
                                < inf->max_inflight_messages) {
                    if (cur->msg.qos > 0 && cur->state == ms_invalid) {
                        inf->inflight_messages++;
                        if (cur->msg.qos == 1) {
                            cur->state = ms_wait_for_puback;
                        } else if (cur->msg.qos == 2) {
                            cur->state = ms_wait_for_pubrec;
                        }
                        result = infi_send_publish(inf, cur->msg.mid,
                                cur->msg.topic, cur->msg.payloadlen,
                                cur->msg.payload, cur->msg.qos, cur->msg.retain,
                                cur->dup);
                        if (result) {
                            return result;
                        }
                    }
                } else {
                    return err_success;
                }
                cur = cur->next;
            }
            return err_success;
        } else {
            return err_not_found;
        }
    } else {
        cur = inf->in_messages;
        while (cur) {
            if (cur->msg.mid == mid) {
                if (prev) {
                    prev->next = cur->next;
                } else {
                    inf->in_messages = cur->next;
                }
                *message = cur;
                inf->in_queue_len--;
                if (cur->next == NULL) {
                    inf->in_messages_last = prev;
                } else if (!inf->in_messages) {
                    inf->in_messages_last = NULL;
                }
                found = true;
                break;
            }
            prev = cur;
            cur = cur->next;
        }

        if (found) {
            return err_success;
        } else {
            return err_not_found;
        }
    }
}

void infi_message_retry_check_actual(struct infi *inf,
        struct infi_message_all *messages) {
    time_t now = infi_time();
    assert(inf);

    while (messages) {
        if (messages->timestamp + inf->message_retry < now) {
            switch (messages->state) {
            case ms_wait_for_puback:
            case ms_wait_for_pubrec:
                messages->timestamp = now;
                messages->dup = true;
                infi_send_publish(inf, messages->msg.mid, messages->msg.topic,
                        messages->msg.payloadlen, messages->msg.payload,
                        messages->msg.qos, messages->msg.retain, messages->dup);
                break;
            case ms_wait_for_pubrel:
                messages->timestamp = now;
                messages->dup = true;
                infi_send_pubrec(inf, messages->msg.mid);
                break;
            case ms_wait_for_pubcomp:
                messages->timestamp = now;
                messages->dup = true;
                infi_send_pubrel(inf, messages->msg.mid);
                break;
            default:
                break;
            }
        }
        messages = messages->next;
    }
}

void infi_message_retry_check(struct infi *inf) {
    infi_message_retry_check_actual(inf, inf->out_messages);
    infi_message_retry_check_actual(inf, inf->in_messages);
}

int infi_message_out_update(struct infi *inf, uint16_t mid,
        enum infi_msg_state state) {
    struct infi_message_all *message;
    assert(inf);

    message = inf->out_messages;
    while (message) {
        if (message->msg.mid == mid) {
            message->state = state;
            message->timestamp = infi_time();
            return err_success;
        }
        message = message->next;
    }
    return err_not_found;
}

time_t infi_time(void) {
#ifdef WIN32
    if(tick64) {
        return GetTickCount64()/1000;
    } else {
        return GetTickCount()/1000;
    }
#elif _POSIX_TIMERS>0 && defined(_POSIX_MONOTONIC_CLOCK)
    struct timespec tp;

    clock_gettime(CLOCK_MONOTONIC, &tp);
    return tp.tv_sec;
#else
    return time(NULL);
#endif
}

int infi_packet_alloc(struct infi_packet *packet) {
    uint8_t remaining_bytes[5], byte;
    uint32_t remaining_length;
    int i;

    assert(packet);

    remaining_length = packet->remaining_length;
    packet->payload = NULL;
    packet->remaining_count = 0;
    do {
        byte = remaining_length % 128;
        remaining_length = remaining_length / 128;

        if (remaining_length > 0) {
            byte = byte | 0x80;
        }
        remaining_bytes[packet->remaining_count] = byte;
        packet->remaining_count++;
    } while (remaining_length > 0 && packet->remaining_count < 5);
    if (packet->remaining_count == 5)
        return err_payload_size;
    packet->packet_length = packet->remaining_length + 1
            + packet->remaining_count;
    packet->payload = infi_malloc(sizeof(uint8_t) * packet->packet_length);
    if (!packet->payload)
        return err_nomem;

    packet->payload[0] = packet->command;
    for (i = 0; i < packet->remaining_count; i++) {
        packet->payload[i + 1] = remaining_bytes[i];
    }
    packet->pos = 1 + packet->remaining_count;

    return err_success;
}

void infi_check_keepalive(struct infi *inf) {
    time_t last_msg_out;
    time_t last_msg_in;
    time_t now = infi_time();
    assert(inf);
    last_msg_out = inf->last_msg_out;
    last_msg_in = inf->last_msg_in;
    if (inf->keepalive && inf->sock != INVALID_SOCKET
            && (now - last_msg_out >= inf->keepalive
                    || now - last_msg_in >= inf->keepalive)) {

        if (inf->state == cs_connected && inf->ping_t == 0) {
            infi_send_pingreq(inf);

            inf->last_msg_in = now;
            inf->last_msg_out = now;
        } else {
            infi_socket_close(inf);
            infi_log_messages(LOG_ERROR, "Broker is Out of Range\n");
            if (inf->on_disconnect) {
                inf->in_callback = true;
                inf->on_disconnect(inf);
                inf->in_callback = false;
            }
        }
    }
}

uint16_t infi_mid_generate(struct infi *inf) {
    uint16_t mid;
    assert(inf);

    inf->last_mid++;
    if (inf->last_mid == 0)
        inf->last_mid++;
    mid = inf->last_mid;

    return mid;
}

int infi_pub_topic_check(const char *str) {
    int len = 0;
    while (str && str[0]) {
        if (str[0] == '+' || str[0] == '#') {
            return err_inval;
        }
        len++;
        str = &str[1];
    }
    if (len > 65535)
        return err_inval;

    return err_success;
}
int infi_log_messages(int priority, const char *charfmt, ...) {

    va_list va;
    va_start(va, charfmt);
    char *chars;
    int intlen;
    FILE *ptr_file;
    char filenameandpath[250];
    char timestamp[100];

    intlen = strlen(charfmt) + 500;
    chars = infi_malloc(intlen * sizeof(char));
    if (!chars) {
        va_end(va);
        return err_nomem;
    }

    vsnprintf(chars, intlen, charfmt, va);
    chars[intlen - 1] = '\0';

    char priorityname[50];
    switch (priority) {
    case LOG_ERROR:
        strcpy(priorityname, "ERROR");
        break;
    case LOG_WARNING:
        strcpy(priorityname, "WARNING");
        break;
    case LOG_INFO:
        strcpy(priorityname, "INFORMATION");
        break;
    }

    if (!strcmp(charlog_type, "ALL") || !strcmp(charlog_type, priorityname)) {
        if (!strcmp(charlog_destination, "ALL")
                || !strcmp(charlog_destination, "CONSOLE")) {
            // WRITE LOG OVER CONSOLE
            time_t now = time(0);
            strftime(timestamp, 100, "%Y-%m-%d %H:%M:%S", localtime(&now));
            fprintf(stdout, "%s: %s %s\n", timestamp, priorityname, chars);
            fflush(stdout);
            filenameandpath[0] = '\0';
            snprintf(filenameandpath, 150, "%s/%s", "/mnt/user/logs",
                    "infiswift.log");
            ptr_file = fopen(filenameandpath, "a+");
            if (ptr_file == NULL) {
                printf("File does not exist,please check!\n");
            }
            fprintf(ptr_file, "%s: %s %s\n", timestamp, priorityname, chars);
            fclose(ptr_file);
        }
    }

    infi_free(chars);
    va_end(va);
    return err_success;
}

void get_setup_from_file(struct infi_config *cfg, char *fname) {
    FILE *fp;
    if ((fp = fopen(fname, "r")) != NULL) {
        char *line = (char *) malloc(MAXBUF);
        int i = 0;
        while (fgets(line, MAXBUF, fp) != NULL) {
            char *cfline, *str, *p;
            int len = 0;
            cfline = strstr((char *) line, DELIM);
            if (cfline) {
                cfline = cfline + strlen(DELIM);
                if (strstr((char *) cfline, "\"")) {
                    cfline = cfline + strlen("\"");
                }
                while (cfline[len]) {
                    len++;
                }
                str = malloc(len + 1);
                p = str;
                while (*cfline) {
                    *p++ = *cfline++;
                }
                p--;
                *p = '\0';
                if (strstr((char *) str, "\"")) {
                    p--;
                    *p = '\0';
                }
                switch (i) {
                case 0:
                    cfg->setup.mqttserver = str;
                    break;
                case 1:
                    cfg->setup.mqttport = str;
                    break;
                case 2:
                    cfg->setup.mqttuser = str;
                    break;
                case 3:
                    cfg->setup.mqttpassword = str;
                    break;
                case 4:
                    cfg->setup.mqttsleeping = str;
                    break;
                case 5:
                    cfg->setup.measuresync = str;
                    break;
                case 6:
                    cfg->setup.qos = str;
                    break;
                case 7:
                    cfg->setup.limit = str;
                    break;
                case 8:
                    cfg->setup.log_level = str;
                    break;
                case 9:
                    cfg->setup.interval = str;
                    break;
                case 10:
                    cfg->setup.client_id = str;
                    break;
                }
                i++;
            }
        }
        fclose(fp);
        cleanup_for_str(line);
    }
}

char *get_template_information_from_file(char *fname, int ftype) {
    FILE *fp;
    char *temp = (char*) malloc(MAXBUF);
    memset(temp, 0, MAXBUF);
    char *reply = NULL;
    if (ftype == file_message) {
        if ((fp = fopen(fname, "r")) != NULL) {
            if (fgets(temp, MAXBUF, fp) != NULL) {
                reply = (char *) malloc(strlen(temp) + 1);
                memset(reply, 0, strlen(temp) + 1);
                strcat(reply, temp);
            }
            while (fgets(temp, MAXBUF, fp) != NULL) {
                reply = (char *) realloc(reply,
                        strlen(reply) + strlen(temp) + 1);
                strcat(reply, temp);
            }
            fclose(fp);
        } else {

            strcpy(reply,
                    "{\n  \"sensor\": \"#SENSOR#\",\n  \"value\": \"#VALUE#\",\n  \"datetime\": \"#TIMESTAMP#\"\n}\n");
        }
    } else if (ftype == file_topic) {
        if ((fp = fopen(fname, "r")) != NULL) {
            if (fgets(temp, MAXBUF, fp) != NULL) {
                reply = (char *) malloc(strlen(temp) + 1);
                memset(reply, 0, strlen(temp) + 1);
                strcat(reply, temp);
            }
            while (fgets(temp, MAXBUF, fp) != NULL) {
                reply = (char *) realloc(reply,
                        strlen(reply) + strlen(temp) + 1);
                strcat(reply, temp);
            }
            fclose(fp);
        } else {
            strcpy(reply, "#MESHLIUM#/#ID_WASP#");
        }
    }
    cleanup_for_str(temp);
  printf("publish_string: %s",reply);
    char *publish_string = remove_space(reply);
    return publish_string;
}

char *get_topic_from_file(char *fname) {
    FILE *fp;
    char *topic = (char*) malloc(MAXBUF);
    if ((fp = fopen(fname, "r")) != NULL) {
        if (fgets(topic, MAXBUF, fp) != NULL) {
        }
        fclose(fp);
    }
    return topic;
}

char *str_replace(char *orig, char *rep, char *with) {
    if (strstr(orig, rep)) {
        if (strlen(with) > strlen(rep)) {
            orig = (char *) realloc(orig, (strlen(orig) + strlen(with)));
        }
    }
//    char *ret,
    char *original;
    original = orig;
    char ret[MAXBUF];
    int i, count = 0;
    int withlen = strlen(with);
    int replen = strlen(rep);
    for (i = 0; orig[i] != '\0'; i++) {
        if (strstr(&orig[i], rep) == &orig[i]) {
            count++;
            i += replen - 1;
        }
    }
//    ret = (char *) malloc(i + count * (withlen - replen));
    if (ret == NULL)
        exit(EXIT_FAILURE);
    i = 0;
    while (*orig) {
        if (strstr(orig, rep) == orig) {
            strcpy(&ret[i], with);
            i += withlen;
            orig += replen;
        } else
            ret[i++] = *orig++;
    }
    ret[i] = '\0';
    strcpy(original, ret);
    return original;
//    cleanup_for_str(ret);
}

void push(struct sensor_parser_data ** start, struct sensor_parser *custdata) {
    struct sensor_parser_data* ele = (struct sensor_parser_data*) malloc(
            sizeof(struct sensor_parser_data));
    ele->data = custdata;
    ele->next = (*start);
    (*start) = ele;
}

void get_sensor_parser_from_db(struct infi_config *cfg) {
    sensor_parser_t *sensor_parser;

    MYSQL* con = mysql_init(NULL);

    mysql_db_connect(con);

    db_select_sensor_parser_based_on_limit(con, cfg);

    MYSQL_RES* result = mysql_store_result(con);
    if (result == NULL) {
        finish_with_error(con);
    }
    MYSQL_ROW row;
    int num_fields = mysql_num_fields(result);
    int fieldno;
    while ((row = mysql_fetch_row(result))) {
        sensor_parser = (sensor_parser_t *) malloc(sizeof(sensor_parser_t));
        for (fieldno = 0; fieldno < num_fields; fieldno++) {
            switch (fieldno) {
            case ID:
                sensor_parser->id = row[ID];
                break;
            case ID_WASP:
                sensor_parser->id_wasp = row[ID_WASP];
                break;
            case ID_SECRET:
                sensor_parser->id_secret = row[ID_SECRET];
                break;
            case FRAME_TYPE:
                sensor_parser->frame_type = row[FRAME_TYPE];
                break;
            case FRAME_NUMBER:
                sensor_parser->frame_number = row[FRAME_NUMBER];
                break;
            case SENSOR:
                sensor_parser->sensor = row[SENSOR];
                break;
            case VALUE:
                sensor_parser->value = row[VALUE];
                break;
            case TIMESTAMP:
                sensor_parser->timestamp = row[TIMESTAMP];
                break;
            case SYNC:
                sensor_parser->sync = row[SYNC];
                break;
            case RAW:
                sensor_parser->raw = row[RAW];
                break;
            case PARSER_TYPE:
                sensor_parser->parser_type = row[PARSER_TYPE];
                break;
            }
        }
        push(&global, sensor_parser);
    }
    mysql_free_result(result);
    mysql_close(con);
}

/*
 *
 * For the connection specified by mysql, This function
 * returns a null-terminated string containing the error
 * message for the most recently invoked API function
 * that failed. If a function did not fail, the return
 * value of this function may be the previous error or an
 * empty string to indicate no error.
 *
 */
void finish_with_error(MYSQL *con) {
    infi_log_messages(LOG_ERROR, " %s.", mysql_error(con));
    mysql_close(con);
    exit(1);
}

/*
 *
 *  This function attempts to establish a connection to a
 *  MySQL database engine running on host and it complete
 *  successfully before you can execute any other API functions
 *  that require a valid MYSQL connection handle structure
 *
 */
void mysql_db_connect(MYSQL* con) {
    if (con == NULL) {
        infi_log_messages(LOG_ERROR, " %s.", mysql_error(con));
        exit(1);
    }
    if (mysql_real_connect(con, MEMSQL_SERVER_IP, USERNAME, PASSWORD,
    DATABASENAME, 0, NULL, 0) == NULL) {
        infi_log_messages(LOG_ERROR, " %s.", mysql_error(con));
        mysql_close(con);
        exit(1);
    }
}

/*
 *
 * This function executes the SQL statement pointed to by the
 * null-terminated string str. Normally, the string must consist of a
 * single SQL statement without a terminating semicolon (;) or \g.
 * If multiple-statement execution has been enabled, the
 * string can contain several statements separated by semicolons
 *
 */
void mysql_db_query(MYSQL* con, char *str) {
    if (mysql_query(con, str)) {
        infi_log_messages(LOG_ERROR, " %s.", mysql_error(con));
        mysql_close(con);
        exit(1);
    }
}

void persist_sensor_parser_mask_in_db(MYSQL *con, char *id) {
    db_update_sensor_parser_mask_for_id(con, id);
}

void db_select_sensor_parser_based_on_limit(MYSQL *con, struct infi_config *cfg) {
    int statement_length = strlen(SELECT_SENSOR_PARSER_QUERY)
            + strlen(cfg->setup.limit) + 1;
    char *statement_str = (char*) malloc(statement_length + 1);
    snprintf(statement_str, statement_length, SELECT_SENSOR_PARSER_QUERY,
            cfg->setup.limit);
    mysql_db_query(con, statement_str);
    cleanup_for_str(statement_str);
}

void db_update_sensor_parser_mask_for_id(MYSQL *con, char *id) {
    int statement_length = strlen(UPDATE_SENSOR_PARSER_QUERY) + strlen(id) + 1;
    char *statement_str = (char*) malloc(statement_length + 1);
    snprintf(statement_str, statement_length, UPDATE_SENSOR_PARSER_QUERY, id);
    mysql_db_query(con, statement_str);
    cleanup_for_str(statement_str);
}

void cleanup_for_str(char *str) {
    if (!str)
        return;
    free(str);
    str = NULL;
}

char *remove_space(char *text) {
    int length, c, d;
    char *start, *original;
    original = text;
    c = d = 0;

    length = strlen(text);

    start = (char*) malloc(length + 1);
    memset(start, 0, length + 1);
    if (start == NULL)
        exit(EXIT_FAILURE);

    while (*(text + c) != '\0') {
        if (*(text + c) == ' ') {
            int temp = c + 1;
            if (*(text + temp) != '\0') {
                while (*(text + temp) == ' ' && *(text + temp) != '\0') {
                    if (*(text + temp) == ' ') {
                        c++;
                    }
                    temp++;
                }
            }
        }
        *(start + d) = *(text + c);
        c++;
        d++;
    }
    *(start + d) = '\0';
    strcpy(original, start);
    cleanup_for_str(start);
    return original;
}

char *duplicate_the_original_string(char *publish_context,
        char *template_replace_key[], char *template_replace_with_key[]) {
    int data;
    char *string = (char *) malloc(strlen(publish_context) + 1);
    memset(string, 0, strlen(publish_context) + 1);
    strcpy(string, publish_context);

    for (data = 0; data < 9; data++) {
        string = str_replace(string, template_replace_key[data],
                template_replace_with_key[data]);
    }
    return string;
}


