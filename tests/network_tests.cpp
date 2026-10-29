#include "engine/network/channel.hpp"
#include "engine/network/address.hpp"
#include <gtest/gtest.h>
#include <array>
#include <thread>

namespace {
vc::binary::Bytes frame(std::string_view body) {
    vc::binary::Writer out;out.u32(static_cast<std::uint32_t>(body.size()));out.raw(body);return out.bytes;
}
TEST(NetworkAddress, IPv4HostnamesAndBracketedIpv6HaveUnambiguousPorts) {
    const auto local=vc::net::parseEndpoint("localhost");EXPECT_EQ(local.host,"localhost");EXPECT_EQ(local.port,25565);
    const auto custom=vc::net::parseEndpoint("127.0.0.1:1234");EXPECT_EQ(custom.host,"127.0.0.1");EXPECT_EQ(custom.port,1234);
    const auto v6=vc::net::parseEndpoint("[::1]:2345");EXPECT_EQ(v6.host,"::1");EXPECT_EQ(v6.port,2345);
    EXPECT_EQ(vc::net::parseEndpoint("::1").port,25565);EXPECT_EQ(vc::net::parseEndpoint("[::1]").host,"::1");
    for(const auto* invalid:{"",":123","host:","host:0","host:65536","host:-1","[::1","[::1]bad","host/path","bad host","[localhost]"})
        EXPECT_THROW(vc::net::parseEndpoint(invalid),std::invalid_argument);
}
vc::net::TcpStream accept(vc::net::TcpListener& listener) {
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    do { if(auto stream=listener.accept()) return std::move(*stream);std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    while(std::chrono::steady_clock::now()<end);
    throw std::runtime_error("Framed loopback accept timed out");
}
}
TEST(NetworkFrames, ArbitraryFragmentationAndCoalescingPreserveMessages) {
    const auto first=frame("hello"),second=frame("world!");auto both=first;both.insert(both.end(),second.begin(),second.end());
    for(std::size_t step=1;step<=both.size();++step) {
        vc::net::FrameDecoder decoder;
        for(std::size_t begin=0;begin<both.size();begin+=step)
            decoder.feed(std::span(both).subspan(begin,std::min(step,both.size()-begin)));
        auto a=decoder.take(),b=decoder.take();ASSERT_TRUE(a&&b);
        EXPECT_EQ(std::string(a->begin(),a->end()),"hello");EXPECT_EQ(std::string(b->begin(),b->end()),"world!");
        EXPECT_FALSE(decoder.take());EXPECT_EQ(decoder.bufferedBytes(),0u);EXPECT_NO_THROW(decoder.finish());
    }
}
TEST(NetworkFrames, InvalidLengthsTruncationAndFloodsFailBeforeLargeAllocation) {
    vc::net::FrameDecoder zero;const std::array<std::uint8_t,4> empty{};EXPECT_THROW(zero.feed(empty),std::runtime_error);
    vc::net::FrameDecoder oversized;vc::binary::Writer length;length.u32(static_cast<std::uint32_t>(vc::net::maxFrameBytes+1));
    EXPECT_THROW(oversized.feed(length.bytes),std::runtime_error);
    vc::net::FrameDecoder partial;const auto encoded=frame("abc");partial.feed(std::span(encoded).first(5));EXPECT_THROW(partial.finish(),std::runtime_error);
    vc::net::FrameDecoder flood;const auto tiny=frame("x");
    for(std::size_t i=0;i<vc::net::maxQueuedFrames;++i) flood.feed(tiny);
    EXPECT_THROW(flood.feed(tiny),std::runtime_error);
    vc::net::FrameDecoder large;EXPECT_THROW(large.feed(vc::binary::Bytes(vc::net::maxBufferedBytes+1)),std::runtime_error);
}
TEST(NetworkFrames, LoopbackBackpressureAndBidirectionalMessagesDrainWithSmallBudgets) {
    vc::net::TcpListener listener("127.0.0.1",0);
    vc::net::FramedConnection client(vc::net::TcpStream::connect("127.0.0.1",listener.port()));
    vc::net::FramedConnection server(accept(listener));
    const vc::binary::Bytes payload(4096,123),response{4,3,2,1};std::size_t count=0;
    while(client.queue(payload)) ++count;
    EXPECT_EQ(count,vc::net::maxQueuedFrames);EXPECT_LE(client.queuedBytes(),vc::net::maxBufferedBytes);EXPECT_TRUE(server.queue(response));
    std::size_t received=0;bool answered=false;const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while((received<count||!answered)&&std::chrono::steady_clock::now()<end) {
        client.pump(8192);server.pump(8192);
        while(auto message=server.take()) { EXPECT_EQ(*message,payload);++received; }
        if(auto message=client.take()) { EXPECT_EQ(*message,response);answered=true; }
    }
    EXPECT_EQ(received,count);EXPECT_TRUE(answered);EXPECT_EQ(client.queuedBytes(),0u);EXPECT_TRUE(client.queue(response));
}
TEST(NetworkFrames, DisconnectRejectsTruncatedFrameAndKeepsCompleteMessages) {
    vc::net::TcpListener listener("127.0.0.1",0);auto client=vc::net::TcpStream::connect("127.0.0.1",listener.port());
    vc::net::FramedConnection server(accept(listener));const auto encoded=frame("complete");
    ASSERT_EQ(client.send(encoded).bytes,encoded.size());client.close();
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(server.open()&&std::chrono::steady_clock::now()<end) { server.pump();if(server.open()) std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    EXPECT_FALSE(server.open());const auto message=server.take();ASSERT_TRUE(message);EXPECT_EQ(std::string(message->begin(),message->end()),"complete");
    auto partialClient=vc::net::TcpStream::connect("127.0.0.1",listener.port());vc::net::FramedConnection partialServer(accept(listener));
    ASSERT_EQ(partialClient.send(std::span(encoded).first(2)).bytes,2u);partialClient.close();bool failed=false;
    const auto partialEnd=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(!failed&&std::chrono::steady_clock::now()<partialEnd) {
        try { partialServer.pump(); } catch(const std::runtime_error&) { failed=true; }
        if(!failed) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    EXPECT_TRUE(failed);EXPECT_FALSE(partialServer.open());
}
