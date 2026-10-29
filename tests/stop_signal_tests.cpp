#include "engine/platform/stop_signal.hpp"
#include <gtest/gtest.h>

namespace {
volatile std::sig_atomic_t previousCalled=0;
void previous(int) { previousCalled=1; }
}
TEST(StopSignal, RepeatedRequestsResetAndRestorePreviousHandlers) {
    const auto old=std::signal(SIGINT,previous);
    {
        vc::StopSignal stop;EXPECT_FALSE(stop.requested());EXPECT_THROW((vc::StopSignal{}),std::logic_error);
        EXPECT_EQ(std::raise(SIGINT),0);EXPECT_TRUE(stop.requested());EXPECT_EQ(previousCalled,0);
        stop.reset();EXPECT_FALSE(stop.requested());EXPECT_EQ(std::raise(SIGINT),0);EXPECT_TRUE(stop.requested());
    }
    EXPECT_EQ(std::raise(SIGINT),0);EXPECT_EQ(previousCalled,1);(void)std::signal(SIGINT,old);
}
