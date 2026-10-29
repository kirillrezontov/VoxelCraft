#include "engine/platform/tcp.hpp"
#include <algorithm>
#include <climits>
#include <stdexcept>
#include <string>
#include <utility>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace vc::net {
namespace {
#ifdef _WIN32
using Socket=SOCKET;
using SocketLength=int;
constexpr Socket invalidSocket=INVALID_SOCKET;
int lastError() { return WSAGetLastError(); }
bool wouldBlock(int error) { return error==WSAEWOULDBLOCK; }
bool interrupted(int error) { return error==WSAEINTR; }
bool connecting(int error) { return error==WSAEWOULDBLOCK||error==WSAEINPROGRESS||error==WSAEALREADY; }
void closeSocket(Socket socket) { (void)closesocket(socket); }
struct Runtime {
    Runtime() { WSADATA data{};if(const auto result=WSAStartup(MAKEWORD(2,2),&data)) throw std::runtime_error("WinSock startup failed: "+std::to_string(result)); }
    ~Runtime() { (void)WSACleanup(); }
};
#else
using Socket=int;
using SocketLength=socklen_t;
constexpr Socket invalidSocket=-1;
int lastError() { return errno; }
bool wouldBlock(int error) { return error==EAGAIN||error==EWOULDBLOCK; }
bool interrupted(int error) { return error==EINTR; }
bool connecting(int error) { return error==EINPROGRESS||error==EALREADY; }
void closeSocket(Socket socket) { (void)::close(socket); }
struct Runtime {};
#endif
std::shared_ptr<Runtime> runtime() { static const auto shared=std::make_shared<Runtime>();return shared; }
std::runtime_error failure(std::string_view operation,int error=lastError()) {
    return std::runtime_error("TCP "+std::string(operation)+" failed (socket error "+std::to_string(error)+')');
}
struct SocketOwner {
    Socket socket{invalidSocket};
    ~SocketOwner() { if(socket!=invalidSocket) closeSocket(socket); }
    Socket release() { return std::exchange(socket,invalidSocket); }
};
void nonblocking(Socket socket) {
#ifdef _WIN32
    u_long value=1;if(ioctlsocket(socket,FIONBIO,&value)!=0) throw failure("nonblocking configuration");
#else
    const auto status=fcntl(socket,F_GETFL,0),descriptor=fcntl(socket,F_GETFD,0);
    if(status<0||descriptor<0||fcntl(socket,F_SETFL,status|O_NONBLOCK)<0||fcntl(socket,F_SETFD,descriptor|FD_CLOEXEC)<0)
        throw failure("nonblocking configuration");
#endif
}
void noDelay(Socket socket) {
    const int enabled=1;
    if(setsockopt(socket,IPPROTO_TCP,TCP_NODELAY,reinterpret_cast<const char*>(&enabled),sizeof(enabled))!=0)
        throw failure("TCP_NODELAY");
}
void validateHost(std::string_view host) {
    if(host.empty()||host.size()>255||std::any_of(host.begin(),host.end(),[](unsigned char c){return c<=32||c==127;}))
        throw std::invalid_argument("TCP address must be a nonempty host/IP without whitespace or control characters");
}
using Addresses=std::unique_ptr<addrinfo,decltype(&freeaddrinfo)>;
Addresses resolve(std::string_view host,std::uint16_t port,bool passive) {
    validateHost(host);addrinfo hints{};hints.ai_family=AF_UNSPEC;hints.ai_socktype=SOCK_STREAM;hints.ai_protocol=IPPROTO_TCP;
    hints.ai_flags=AI_NUMERICSERV|(passive?AI_PASSIVE:0);
    addrinfo* result{};const auto service=std::to_string(port),name=std::string(host);
    if(const auto error=getaddrinfo(name.c_str(),service.c_str(),&hints,&result)) throw failure("address resolution",error);
    return {result,freeaddrinfo};
}
bool waitConnect(Socket socket,std::chrono::steady_clock::time_point deadline) {
    for(;;) {
        const auto left=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();
        if(left<=0) return false;
#ifdef _WIN32
        WSAPOLLFD descriptor{};descriptor.fd=socket;descriptor.events=POLLOUT;
        const auto result=WSAPoll(&descriptor,1,static_cast<int>(std::min<std::int64_t>(left,INT_MAX)));
#else
        pollfd descriptor{};descriptor.fd=socket;descriptor.events=POLLOUT;
        const auto result=poll(&descriptor,1,static_cast<int>(std::min<std::int64_t>(left,INT_MAX)));
#endif
        if(result>0) return true;
        if(result==0) return false;
        const auto error=lastError();if(!interrupted(error)) throw failure("connect polling",error);
    }
}
}
struct TcpStream::Impl {
    std::shared_ptr<Runtime> network=runtime();
    Socket socket{invalidSocket};
    ~Impl() { if(socket!=invalidSocket) closeSocket(socket); }
};
struct TcpListener::Impl {
    std::shared_ptr<Runtime> network=runtime();
    Socket socket{invalidSocket};
    std::uint16_t localPort{};
    ~Impl() { if(socket!=invalidSocket) closeSocket(socket); }
};
TcpStream::TcpStream()=default;
TcpStream::TcpStream(std::unique_ptr<Impl> impl):impl_(std::move(impl)) {}
TcpStream::~TcpStream()=default;
TcpStream::TcpStream(TcpStream&&) noexcept=default;
TcpStream& TcpStream::operator=(TcpStream&&) noexcept=default;
bool TcpStream::open() const { return impl_&&impl_->socket!=invalidSocket; }
void TcpStream::close() { impl_.reset(); }
TcpStream TcpStream::connect(std::string_view host,std::uint16_t port,std::chrono::milliseconds timeout) {
    if(!port||timeout.count()<1||timeout>std::chrono::seconds(60)) throw std::invalid_argument("TCP connect needs a port and a 1ms..60s timeout");
    auto impl=std::make_unique<Impl>();const auto addresses=resolve(host,port,false);
    const auto deadline=std::chrono::steady_clock::now()+timeout;int error=0;
    for(auto* address=addresses.get();address;address=address->ai_next) {
        SocketOwner candidate{::socket(address->ai_family,address->ai_socktype,address->ai_protocol)};
        if(candidate.socket==invalidSocket) { error=lastError();continue; }
        nonblocking(candidate.socket);
        const auto connected=::connect(candidate.socket,address->ai_addr,static_cast<SocketLength>(address->ai_addrlen));
        if(connected!=0) {
            error=lastError();if(!connecting(error)) continue;
            if(!waitConnect(candidate.socket,deadline)) throw std::runtime_error("TCP connection timed out");
            SocketLength size=sizeof(error);
            if(getsockopt(candidate.socket,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&error),&size)!=0) throw failure("connect status");
            if(error) continue;
        }
        noDelay(candidate.socket);impl->socket=candidate.release();return TcpStream(std::move(impl));
    }
    throw failure("connection",error);
}
IoResult TcpStream::receive(std::span<std::uint8_t> buffer) {
    if(!open()) throw std::logic_error("Cannot receive on a closed TCP stream");
    if(buffer.empty()) return {};
    for(;;) {
        const auto count=std::min<std::size_t>(buffer.size(),INT_MAX);
#ifdef _WIN32
        const auto result=::recv(impl_->socket,reinterpret_cast<char*>(buffer.data()),static_cast<int>(count),0);
#else
        const auto result=::recv(impl_->socket,buffer.data(),count,0);
#endif
        if(result>0) return {static_cast<std::size_t>(result),false};
        if(result==0) return {0,true};
        const auto error=lastError();if(interrupted(error)) continue;
        if(wouldBlock(error)) return {};
        throw failure("receive",error);
    }
}
IoResult TcpStream::send(std::span<const std::uint8_t> buffer) {
    if(!open()) throw std::logic_error("Cannot send on a closed TCP stream");
    if(buffer.empty()) return {};
#ifdef MSG_NOSIGNAL
    constexpr int flags=MSG_NOSIGNAL;
#else
    constexpr int flags=0;
#endif
    for(;;) {
        const auto count=std::min<std::size_t>(buffer.size(),INT_MAX);
#ifdef _WIN32
        const auto result=::send(impl_->socket,reinterpret_cast<const char*>(buffer.data()),static_cast<int>(count),flags);
#else
        const auto result=::send(impl_->socket,buffer.data(),count,flags);
#endif
        if(result>0) return {static_cast<std::size_t>(result),false};
        if(result==0) return {0,true};
        const auto error=lastError();if(interrupted(error)) continue;
        if(wouldBlock(error)) return {};
        throw failure("send",error);
    }
}
TcpListener::TcpListener(std::string_view host,std::uint16_t port):impl_(std::make_unique<Impl>()) {
    const auto addresses=resolve(host,port,true);int error=0;
    for(auto* address=addresses.get();address;address=address->ai_next) {
        SocketOwner candidate{::socket(address->ai_family,address->ai_socktype,address->ai_protocol)};
        if(candidate.socket==invalidSocket) { error=lastError();continue; }
        const int enabled=1;
#ifdef _WIN32
        constexpr int reuse=SO_EXCLUSIVEADDRUSE;
#else
        constexpr int reuse=SO_REUSEADDR;
#endif
        if(setsockopt(candidate.socket,SOL_SOCKET,reuse,reinterpret_cast<const char*>(&enabled),sizeof(enabled))!=0) throw failure("listen configuration");
        if(::bind(candidate.socket,address->ai_addr,static_cast<SocketLength>(address->ai_addrlen))!=0||::listen(candidate.socket,16)!=0) { error=lastError();continue; }
        nonblocking(candidate.socket);sockaddr_storage local{};SocketLength length=sizeof(local);
        if(getsockname(candidate.socket,reinterpret_cast<sockaddr*>(&local),&length)!=0) throw failure("local address");
        impl_->localPort=ntohs(local.ss_family==AF_INET?reinterpret_cast<const sockaddr_in*>(&local)->sin_port:reinterpret_cast<const sockaddr_in6*>(&local)->sin6_port);
        impl_->socket=candidate.release();return;
    }
    throw failure("listen",error);
}
TcpListener::~TcpListener()=default;
TcpListener::TcpListener(TcpListener&&) noexcept=default;
TcpListener& TcpListener::operator=(TcpListener&&) noexcept=default;
std::optional<TcpStream> TcpListener::accept() {
    if(!impl_) throw std::logic_error("Cannot accept on a moved TCP listener");
    for(;;) {
        SocketOwner candidate{::accept(impl_->socket,nullptr,nullptr)};
        if(candidate.socket!=invalidSocket) {
            nonblocking(candidate.socket);noDelay(candidate.socket);auto stream=std::make_unique<TcpStream::Impl>();
            stream->socket=candidate.release();return TcpStream(std::move(stream));
        }
        const auto error=lastError();if(interrupted(error)) continue;
        if(wouldBlock(error)) return {};
        throw failure("accept",error);
    }
}
std::uint16_t TcpListener::port() const { if(!impl_) throw std::logic_error("Moved TCP listener has no port");return impl_->localPort; }
}
