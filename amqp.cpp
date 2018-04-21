#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include <amqp.h>
#include <amqp_tcp_socket.h>
#include <amqp_framing.h>
#include "amqp.hpp"

void AMQP::die(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

void AMQP::die_on_error(int x, char const *context) {
    if (x < 0) {
        fprintf(stderr, "%s: %s\n", context, amqp_error_string2(x));
        exit(1);
    }
}

void AMQP::die_on_amqp_error(amqp_rpc_reply_t x, char const *context) {
    switch (x.reply_type) {
        case AMQP_RESPONSE_NORMAL:
            return;

        case AMQP_RESPONSE_NONE:
            fprintf(stderr, "%s: missing RPC reply type!\n", context);
            break;

        case AMQP_RESPONSE_LIBRARY_EXCEPTION:
            fprintf(stderr, "%s: %s\n", context, amqp_error_string2(x.library_error));
            break;

        case AMQP_RESPONSE_SERVER_EXCEPTION:
            switch (x.reply.id) {
                case AMQP_CONNECTION_CLOSE_METHOD: {
                                                       amqp_connection_close_t *m =
                                                           (amqp_connection_close_t *)x.reply.decoded;
                                                       fprintf(stderr, "%s: server connection error %uh, message: %.*s\n",
                                                               context, m->reply_code, (int)m->reply_text.len,
                                                               (char *)m->reply_text.bytes);
                                                       break;
                                                   }
                case AMQP_CHANNEL_CLOSE_METHOD: {
                                                    amqp_channel_close_t *m = (amqp_channel_close_t *)x.reply.decoded;
                                                    fprintf(stderr, "%s: server channel error %uh, message: %.*s\n",
                                                            context, m->reply_code, (int)m->reply_text.len,
                                                            (char *)m->reply_text.bytes);
                                                    break;
                                                }
                default:
                                                fprintf(stderr, "%s: unknown server error, method id 0x%08X\n",
                                                        context, x.reply.id);
                                                break;
            }
            break;
    }

    exit(1);
}
AMQP::AMQP(char *hostname, int port) {
    int status;

    this->conn = amqp_new_connection();

    this->socket = amqp_tcp_socket_new(conn);
    if (!this->socket) {
        die("creating TCP socket");
    }

    status = amqp_socket_open(this->socket, hostname, port);
    if (status) {
        die("opening TCP socket");
    }

    die_on_amqp_error(amqp_login(this->conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, "guest", "guest"), "Logging in");
    amqp_channel_open(this->conn, 1);
    die_on_amqp_error(amqp_get_rpc_reply(this->conn), "Opening channel");
}

void AMQP::set_active_source(uint8_t source_id) {
    char const *exchange = "PC2";
    char const *routingkey = "new_source";
    if (source_id == this->active_source) {
        return;
    }
    this->active_source = source_id;
    char *messagebody = (char *)malloc(3);
    sprintf(messagebody, "%02X", source_id);
    {
        amqp_basic_properties_t props;
        props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
        props.content_type = amqp_cstring_bytes("text/plain");
        props.delivery_mode = 2; /* persistent delivery mode */
        die_on_error(amqp_basic_publish(this->conn, 1, amqp_cstring_bytes(exchange),
                    amqp_cstring_bytes(routingkey), 0, 0,
                    &props, amqp_cstring_bytes(messagebody)),
                "Publishing");
    }
    free(messagebody);
}

void AMQP::send_message(const char *source_name, const char *messagebody) {
    char const *exchange = "PC2";
    char *routingkey = (char *)malloc(30);
    sprintf(routingkey, "%s.Beo4", source_name);
    {
        amqp_basic_properties_t props;
        props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
        props.content_type = amqp_cstring_bytes("text/plain");
        props.delivery_mode = 2; /* persistent delivery mode */
        die_on_error(amqp_basic_publish(this->conn, 1, amqp_cstring_bytes(exchange),
                    amqp_cstring_bytes(routingkey), 0, 0,
                    &props, amqp_cstring_bytes(messagebody)),
                "Publishing");
    }
    free(routingkey);
}

AMQP::~AMQP() {
    die_on_amqp_error(amqp_channel_close(this->conn, 1, AMQP_REPLY_SUCCESS), "Closing channel");
    die_on_amqp_error(amqp_connection_close(this->conn, AMQP_REPLY_SUCCESS), "Closing connection");
    die_on_error(amqp_destroy_connection(this->conn), "Ending connection");
}
