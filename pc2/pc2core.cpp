#include "pc2/beo4.hpp"
#include "pc2/pc2.hpp"

/*! \brief Process a key press
 */
void MediaCore::keypress(Beo4::keycode keycode) {
    // Is this LIGHT or CONTROL?
    // Update timestamp for last LIGHT command
    // Are we expecting a LIGHT or CONTROL command?
    // then send on to home control
    if(this->lc_handler != nullptr) {
        this->lc_handler->handle_command(keycode);
    }
    // Are we expecting a mixer control signal?
    // then send on to our mixer control processing
    // is it a shutdown? 
    // then shut down.
    this->standby();
    // is it a long shutdown?
    // then shut down and send Masterlink telegram.
    // Is it a source selection?
    // then switch source.
    // Is it a command for a source?
    // current_source->keypress(keycode);
};

/*! \brief Get a Beolink::Source instance for a given Masterlink::source
 */
Beolink::Source * MediaCore::SourceFactory(Masterlink::source source) {
    // is source local?
    // get the class
    // is source remote?
    // is source video?
    // request from video master
    // is source audio?
    // request from audio master
    // create a remote source class
    // return the pointer
    // if source not available, return nullptr
};
