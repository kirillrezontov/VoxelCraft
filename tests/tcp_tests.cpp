#include "engine/platform/tcp.hpp"
#include <gtest/gtest.h>
#include <array>
#include <thread>
#include <vector>

namespace {
vc::net::TcpStream accept(vc::net::TcpListener& listener) {
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    do {
        if(auto stream=listener.accept()) return std::move(*stream);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while(std::chrono::steady_clock::now()<end);
    throw std::runtime_error("Loopback TCP accept timed out");
}
}
TEST(Tcp, LoopbackTransfersPartialBuffersAndReportsWouldBlock) {
    vc::net::TcpListener listener("127.0.0.1",0);ASSERT_NE(listener.port(),0);
    EXPECT_FALSE(listener.accept());auto client=vc::net::TcpStream::connect("127.0.0.1",listener.port());auto server=accept(listener);
    std::array<std::uint8_t,8> empty{};const auto idle=server.receive(empty);EXPECT_EQ(idle.bytes,0u);EXPECT_FALSE(idle.closed);
    std::vector<std::uint8_t> sent(2*1024*1024),received(sent.size());
    for(std::size_t i=0;i<sent.size();++i) sent[i]=static_cast<std::uint8_t>(i%251);
    std::size_t written=0,read=0;const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(read<received.size()&&std::chrono::steady_clock::now()<end) {
        if(written<sent.size()) written+=client.send(std::span(sent).subspan(written)).bytes;
        const auto result=server.receive(std::span(received).subspan(read));ASSERT_FALSE(result.closed);read+=result.bytes;
        if(!result.bytes) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    EXPECT_EQ(written,sent.size());EXPECT_EQ(read,sent.size());EXPECT_EQ(received,sent);
    EXPECT_EQ(client.send({}).bytes,0u);EXPECT_FALSE(client.receive({}).closed);
}
TEST(Tcp, CloseMoveAndPortOwnershipReleaseResources) {
    vc::net::TcpListener listener("127.0.0.1",0);const auto port=listener.port();
    EXPECT_THROW((vc::net::TcpListener("127.0.0.1",port)),std::runtime_error);
    auto client=vc::net::TcpStream::connect("127.0.0.1",port);auto server=accept(listener);
    auto moved=std::move(client);EXPECT_FALSE(client.open());EXPECT_TRUE(moved.open());server.close();
    bool closed=false;std::array<std::uint8_t,16> buffer{};const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(!closed&&std::chrono::steady_clock::now()<end) { closed=moved.receive(buffer).closed;if(!closed) std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    EXPECT_TRUE(closed);moved.close();EXPECT_FALSE(moved.open());EXPECT_THROW(moved.receive(buffer),std::logic_error);
    auto relocated=std::move(listener);EXPECT_EQ(relocated.port(),port);EXPECT_THROW(listener.accept(),std::logic_error);
}
TEST(Tcp, InvalidArgumentsAndRefusedConnectionsFailExplicitly) {
    EXPECT_THROW((vc::net::TcpListener("",0)),std::invalid_argument);
    EXPECT_THROW((vc::net::TcpListener("127.0.0.1\n",0)),std::invalid_argument);
    EXPECT_THROW(vc::net::TcpStream::connect("127.0.0.1",0),std::invalid_argument);
    EXPECT_THROW(vc::net::TcpStream::connect("127.0.0.1",1,std::chrono::milliseconds(0)),std::invalid_argument);
    std::uint16_t unused{};{ vc::net::TcpListener listener("127.0.0.1",0);unused=listener.port(); }
    EXPECT_THROW(vc::net::TcpStream::connect("127.0.0.1",unused,std::chrono::milliseconds(500)),std::runtime_error);
}
