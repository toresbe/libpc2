#include <list>
#include <memory>
#include <exception>
#include <future>
#include <cstdio>
#include <ctime>
#include <string>
#include <iomanip>
#include <boost/log/trivial.hpp>
#include <boost/format.hpp>
#include <vector>
#include <array>

#include "ml/telegram.hpp"
#include "ml/pprint.hpp"
#include "pc2/pc2device.hpp"
#include "pc2/pc2.hpp"

static bool is_response_to(MasterlinkTelegram &incoming_tgram, MasterlinkTelegram &waiting_tgram) {
    // If...
    // 1) the payload type matches the request's,
    if(incoming_tgram.payload_type != waiting_tgram.payload_type) return false;
    //BOOST_LOG_TRIVIAL(debug) << "Payload type matches.";
    // 2) the telegram destination node matches the request's source,
    if(incoming_tgram.dest_node != waiting_tgram.src_node) return false;
    //BOOST_LOG_TRIVIAL(debug) << "Payload node matches.";
    // 3) the incoming tgram is of type status (TODO: this should be resolved through typing)
    if(incoming_tgram.telegram_type != MasterlinkTelegram::telegram_types::status) return false;
    //BOOST_LOG_TRIVIAL(debug) << "Telegram type matches.";
    // ...then return true
    return true;
};

void PC2Beolink::handle_ml_status(MasterlinkTelegram &mlt) {
    BOOST_LOG_TRIVIAL(info) << "Handling status telegram";
    for (auto x: this->pending_request_queue) {
        if(is_response_to(mlt, *x->first)) {
            pending_request_queue.remove(x);
            BOOST_LOG_TRIVIAL(info) << "Found waiting telegram request!";
            x->second->set_value(mlt);
            return;
        }
    }
    BOOST_LOG_TRIVIAL(info) << "Did not find waiting telegram request!";
};

std::shared_future<MasterlinkTelegram> PC2Beolink::send_request(std::shared_ptr<MasterlinkTelegram> tgram) {
    auto promised_reply = std::make_shared<std::promise<MasterlinkTelegram>>();
    pending_request_t * request = new pending_request_t(tgram, promised_reply);
    this->pending_request_queue.push_back(request);
    this->send_telegram(*tgram);
    return promised_reply->get_future();
};

MasterlinkTelegram PC2Beolink::interrogate(MasterlinkTelegram & tgram) {
    auto future = send_request(std::make_shared<MasterlinkTelegram>(tgram));
    int i = 0;
    // FIXME: I'm faking threaded code flow here; disaster waiting to happen
    while (i++ <= 5) {
        this->yield(); // will wait 200 milliseconds
        if(future.wait_for(std::chrono::milliseconds(10)) == std::future_status::ready)
            break;
    }
    // TODO: Determine precise timeout used by B&O audio masters
    auto retval = future.wait_for(std::chrono::milliseconds(10));
    if(retval == std::future_status::timeout) {
        BOOST_LOG_TRIVIAL(info) << "Timeout waiting for telegram!";
        throw TelegramRequestTimeout();
    } else if (retval == std::future_status::ready) {
        try {
            return future.get();
        } catch (const std::future_error& e) {
            BOOST_LOG_TRIVIAL(error) << "Caught a future_error with code \"" << e.code()
                << "\"\nMessage: \"" << e.what() << "\"\n";
        }
    }
}

void PC2Beolink::handle_ml_request(MasterlinkTelegram & mlt) {
    if(mlt.payload_type == mlt.payload_types::master_present) {
        BOOST_LOG_TRIVIAL(info) << "Master present request seen";
        DecodedTelegram::MasterPresent reply = DecodedTelegram::MasterPresent::reply_from_request(mlt);
        this->send_telegram(reply);
    } else if (mlt.payload_type == mlt.payload_types::goto_source) {
        BOOST_LOG_TRIVIAL(info) << "Source goto request seen";
        DecodedTelegram::GotoSource goto_source(mlt);
        if(goto_source.tgram_meaning == DecodedTelegram::GotoSource::tgram_meanings::request_source) {
            // TODO: check if video master is present
            // TODO: if present, check if casting
            // TODO: if casting, ask to cease and desist
            DecodedTelegram::StatusInfo reply(goto_source.requested_source);
            reply.src_node = goto_source.dest_node;
            this->send_telegram(reply);

            DecodedTelegram::TrackInfo track_reply(goto_source.requested_source);
            track_reply.src_node = goto_source.dest_node;
            track_reply.dest_node = goto_source.src_node;
            this->send_telegram(track_reply);

            // TODO: Signal interface that a source has been requested
            //this->pc2->mixer->ml_distribute(true);
            BOOST_LOG_TRIVIAL(error) << "Not distributing on ML because rewrite pending!";
        }
    }
};

void PC2Beolink::process_ml_telegram(PC2Telegram & tgram) {
    Masterlink ml;
    MasterlinkTelegram mlt(tgram);
    for (auto c : tgram) {
        printf(" %02X", c);
    }
    printf(".\n");
    TelegramPrinter::print(mlt);

    if(this->pc2->interface->address_mask == PC2Interface::address_mask_t::promisc) {
        if(mlt.dest_node == 0xc0) {
            BOOST_LOG_TRIVIAL(debug) << "Not processing packet addressed to V_MASTER";
            return;
        }
    }

    switch(mlt.telegram_type) {
        case(mlt.telegram_types::request):
            handle_ml_request(mlt);
            break;
        case(mlt.telegram_types::status):
            handle_ml_status(mlt);
            break;
        default:
            BOOST_LOG_TRIVIAL(warning) << "So far I can only handle request telegrams";
    };


    /*
    switch (tgram[10]) {
        case(0x04):
            {
                // Some sort of audio setup
                uint8_t requesting_node = tgram[4];
                if ((tgram[11] == 0x03) && (tgram[12] == 0x04) && (tgram[14] == 0x01) && (tgram[15] == 0x00)) {
                    if (tgram[13] == 0x08) {
                        BOOST_LOG_TRIVIAL(info) << "Link device ping, sending pong";
                    }
                    else if (tgram[13] == 0x02) {
                        BOOST_LOG_TRIVIAL(info) << "Video device ping? Sending pong";
                    }
                    this->device->send_telegram({ 0xe0, requesting_node, 0xc1, 0x01, 0x14, 0x00, 0x00, 0x00, 0x04, 0x03, 0x04, 0x01, 0x01, 0x01, 0xe9, 0x00 });
                    expect_ack();
                    //this->mixer->transmit(true);
                    //this->mixer->set_parameters(0x22, 0, 0, 0);
                }
            }
            break;
        case(0x08):
            {
                if ((tgram[11] == 0x00) && (tgram[12] == 0x01)) {
                    BOOST_LOG_TRIVIAL(info) << boost::format("Node #%02X sent one of those 0x08 requests we don't know what are yet") % (unsigned int)tgram[4];
                    // Not sure what this means but link room products will sometimes need this reply
                    this->device->send_telegram({ 0xe0, tgram[4], 0xc1, 0x01, 0x14, 0x00, 0x00, 0x00, 0x08, 0x00, 0x04, 0xe4, 0x00 });
                    //							A casting AM will yield this reply in stead
                    //         			         '0xe0, 0xc0,     0xc1, 0x01, 0x14, 0x00, 0x00, 0x00, 0x08, 0x04, 0x06, 0x02, 0x01, 0x00, 0x79, 0x24, 0x00'

                    expect_ack();
                }
                else {
                    BOOST_LOG_TRIVIAL(warning) << boost::format("Node #%02X sent us a 0x08 request we don't know how to handle!") % (unsigned int)tgram[4];
                }
            }
            break;
        case(0x82):
            {
                std::string source_name;
                if(ml.source_name.count((Masterlink::source)tgram[14])) {
                    source_name = ml.source_name[(Masterlink::source)tgram[14]];
                } else {
                    source_name = "unknown source";
                }

                std::string state;
                if (tgram[16] == ML_STATE_STOPPED) state = "stopped";
                else if (tgram[16] == ML_STATE_PLAYING) state = "playing";
                else if (tgram[16] == ML_STATE_PLAYING) state = "fast-forwarding";
                else if (tgram[16] == ML_STATE_PLAYING) state = "rewinding";
                else state = "unknown";
                //14 = source, 15 = track, 16 = playstate

                BOOST_LOG_TRIVIAL(info) << boost::format("Node #%02X is %s source %s (track %d)") % (unsigned int)tgram[4] % state % source_name % (unsigned int) tgram[15];

                //        60 14 00 01 C1 01 14 00 00 00 82 09 01 04 8D 06 02 00 00 01 FF FF 61
            }
            break;
        case(0x10):
            if (!memcmp("\x10\x03\x03\x01\x00\x01", (void *)(tgram.data() + 10), 6)) {
                BOOST_LOG_TRIVIAL(info) << "Video master is requesting audio control back. Relinquishing...";
                this->mixer->ml_distribute(false);
            }
            break;
        case(0x11):
            {
                if ((tgram[11] == 0x02) && (tgram[12] == 0x02) && (tgram[13] == 0x01) && (tgram[14] == 0x00)) {
                    BOOST_LOG_TRIVIAL(info) << boost::format("Node #%02X powered down") % (unsigned int)tgram[4];
                }
                else {
                    BOOST_LOG_TRIVIAL(warning) << boost::format("Node #%02X sent us a 0x11 telegram we don't know how to handle!") % (unsigned int)tgram[4];
                }
            }
            break;
        case(0x30):
            {
                if ((tgram[11] == 0x00) && (tgram[12] == 0x02)) {
                    BOOST_LOG_TRIVIAL(info) << boost::format("Node #%02X sent one of those 0x30 requests we don't know what are yet") % (unsigned int)tgram[4];
                    // Not sure what this means but link room products will sometimes need this reply
                    this->device->send_telegram({ 0xe0, tgram[4], 0xc1, 0x01, 0x14, 0x00, 0x00, 0x00, 0x30, 0x00, 0x04, 0x0c, 0x00 });
                    expect_ack();
                }
                else {
                    BOOST_LOG_TRIVIAL(warning) << boost::format("Node #%02X sent us a 0x30 request we don't know how to handle!") % (unsigned int)tgram[4];
                }
            }
            break;
        case(0x40):
            printf("| Time is: %02X:%02X:%02X on 20%02X-%02X-%02X.\n", tgram[16], tgram[17], tgram[18], tgram[22], tgram[21], tgram[20]);
            break;
        case(0x44):
            // So, I don't know what this telegram type actually means yet or how to decode it, however I have observed it in
            // contexts where audio bus ownership is handed from one device to another. Erring on the side of caution I've decided
            // to just cut the feed if this should ever occur (it's not in any "supported" configuration for this code).
            //BOOST_LOG_TRIVIAL(warning) << "Disabling audio output because we got a 0x44 telegram and I don't know what that means (see code)";
            //this->mixer->ml_distribute(false);
            break;
        case(0x45):
            {
                uint8_t source_num = tgram[14];
                uint8_t requesting_node = tgram[4];
                BOOST_LOG_TRIVIAL(info) << boost::format("Source %02X requested by node #%02X") % (unsigned int)source_num % (unsigned int)requesting_node;
                this->listener_count++;
                this->mixer->ml_distribute(true);
                this->mixer->set_parameters(0x22, 0, 0, 0, false);
                this->device->send_telegram({ 0xe0, 0x83, 0xc1, 0x01, 0x14, 0x00, source_num, 0x00, 0x87, 0x1f, 0x04, source_num, 0x01, 0x00, 0x00, 0x1f, 0xbe, 0x01, 0x00, 0x00, 0x00, 0xff, 0x02, 0x01, 0x00, 0x03, 0x01, 0x01, 0x01, 0x03, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe2, 0x00 });
                expect_ack();
                this->device->send_telegram({ 0xe0, requesting_node, 0xc1, 0x01, 0x14, 0x00, 0x00, 0x00, 0x44, 0x08, 0x05, 0x02, source_num, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00, 0xaa, 0x00 });
                expect_ack();
                this->device->send_telegram({ 0xe0, requesting_node, 0xc1, 0x01, 0x14, 0x00, 0x00, 0x00, 0x82, 0x0a, 0x01, 0x06, source_num, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xea, 0x00 });
                expect_ack();
            }
            break;
        case(0x87):
            printf("| Active source is: 0x%02X.\n", tgram[13]);
            break;
    }

    */

};
