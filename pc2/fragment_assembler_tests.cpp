#include <GUnit.h>
#include "GUnit/GMock.h"
#include "gmock/gmock.h"

#include "pc2/fragment_assembler.hpp"

using ::testing::StrictGMock;

GTEST(PC2MessageFragmentAssembler) {
    SHOULD("Construct and destruct") {
        PC2MessageFragmentAssembler assembler;
    }

    SHOULD("Have no complete message on construction") {
        PC2MessageFragmentAssembler assembler;

        ASSERT_EQ(false, assembler.has_complete_message());
    }

    SHOULD("Have no complete message when partial message submitted") {
        PC2MessageFragmentAssembler assembler;

        std::vector<uint8_t> partial = {0x60, 0x05, 0x00};
        assembler << partial;

        ASSERT_EQ(false, assembler.has_complete_message());
    }

    SHOULD("Return a complete message when a self-contained message is submitted") {
        PC2MessageFragmentAssembler assembler;

        std::vector<uint8_t> complete = {0x60, 0x00, 0x61};
        assembler << complete;

        ASSERT_EQ(true, assembler.has_complete_message());

        auto returned = assembler.get_message();

        ASSERT_EQ(false, assembler.has_complete_message());
        ASSERT_EQ(complete, returned);
    }

    SHOULD("Throw an exception if given a message not starting with 0x60") {
        PC2MessageFragmentAssembler assembler;
        PC2Message bad_start = {0x00, 0x10, 0xE0, 0xC0, 0xC1, 0x01};

        ASSERT_ANY_THROW(assembler << bad_start); 
        ASSERT_EQ(false, assembler.has_complete_message());
    }

    SHOULD("Return a complete message from multiple fragments") {
        PC2MessageFragmentAssembler assembler;
        PC2Message part1 = {0x60, 0x10, 0xE0, 0xC0, 0xC1, 0x01};
        PC2Message part2 = {0x0B, 0x00, 0x00, 0x00, 0x04, 0x03, 0x04, 0x01, 0x01};
        PC2Message part3 = {0x00, 0x9A, 0x00, 0x61};

        assembler << part1; 
        ASSERT_EQ(false, assembler.has_complete_message());
        assembler << part2; 
        ASSERT_EQ(false, assembler.has_complete_message());
        assembler << part3; 
        ASSERT_EQ(true, assembler.has_complete_message());
    }

    SHOULD("Throw an exception if completed message does not end on 0x61") {
        PC2MessageFragmentAssembler assembler;
        PC2Message part1 = {0x60, 0x10, 0xE0, 0xC0, 0xC1, 0x01};
        PC2Message part2 = {0x0B, 0x00, 0x00, 0x00, 0x04, 0x03, 0x04, 0x01, 0x01};
        PC2Message part3 = {0x00, 0x9A, 0x00, 0x62};

        assembler << part1; 
        ASSERT_EQ(false, assembler.has_complete_message());
        assembler << part2; 
        ASSERT_EQ(false, assembler.has_complete_message());
        ASSERT_ANY_THROW(assembler << part3); 
        ASSERT_EQ(false, assembler.has_complete_message());
    }

}
