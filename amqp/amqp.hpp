#ifndef __AMQP_HPP
#define __AMQP_HPP

#include <amqp.h>
#include <amqp_tcp_socket.h>
#include <amqp_framing.h>
#include "amqp.hpp"
class AMQP {
    private:
        amqp_socket_t *socket = NULL;
	void die(const char *fmt, ...);
	void die_on_error(int x, char const *context);
	void die_on_amqp_error(amqp_rpc_reply_t x, char const *context);
        amqp_connection_state_t conn;
        uint8_t active_source = 0;
    public:
        void set_active_source(uint8_t source_id);
        void send_message(const char *source_name, const char *messagebody);
        AMQP(char *hostname = (char *)"localhost", int port = 5672);
        ~AMQP();
};
#endif