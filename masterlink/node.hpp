#include <exception>
#include <future>
#include <thread>

#include "pc2/pc2.hpp"

class MasterlinkNode {
};

class VideoMaster: public MasterlinkNode {
    public:
    static bool is_present(PC2 *pc2);
};
