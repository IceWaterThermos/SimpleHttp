#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/config.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using  header = http::field;
using tcp = boost::asio::ip::tcp;

beast::string_view mime_type(beast::string_view path) {
    using beast::iequals;
    auto const ext = [&path] {
        auto const pos = path.rfind(".");
        if(pos == beast::string_view::npos) { // .이 없을경우
            return beast::string_view{};
        } else {
            return path.substr(pos);
        }
    }();
    if(iequals(ext, ".htm"))    return "text/html";
    if(iequals(ext, ".html"))   return "text/html";
    if(iequals(ext, ".php"))    return "text/html";
    if(iequals(ext, ".css"))    return "text/css";
    if(iequals(ext, ".txt"))    return "text/plain";
    if(iequals(ext, ".js"))     return "application/javascript";
    if(iequals(ext, ".json"))   return "application/json";
    if(iequals(ext, ".xml"))    return "application/xml";
    if(iequals(ext, ".swf"))    return "application/x-shockwave-flash";
    if(iequals(ext, ".flv"))    return "video/x-flv";
    if(iequals(ext, ".png"))    return "image/png";
    if(iequals(ext, ".jpe"))    return "image/jpeg";
    if(iequals(ext, ".jpeg"))   return "image/jpeg";
    if(iequals(ext, ".jpg"))    return "image/jpeg";
    if(iequals(ext, ".gif"))    return "image/gif";
    if(iequals(ext, ".bmp"))    return "image/bmp";
    if(iequals(ext, ".ico"))    return "image/vnd.microsoft.icon";
    if(iequals(ext, ".tiff"))   return "image/tiff";
    if(iequals(ext, ".tif"))    return "image/tiff";
    if(iequals(ext, ".svg"))    return "image/svg+xml";
    if(iequals(ext, ".svgz"))   return "image/svg+xml";
    return "application/text";
}

//로컬 파일 시스템 경로에 HTTP rel-path 추가
//반환된 경로는 플랫폼에 맞게 정규화됩니다.
std::string path_cat(beast::string_view base,
                     beast::string_view path) {
    if(base.empty())    return std::string(path) ;
    std::string result(base);
#ifdef BOOST_MSVC
    char constexpr path_separator = "\\";
    if(result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
    for(auto& c : result) {
        if(c == '/')    c = path_separator;
    }
#else
    char constexpr path_separator = '/';
    if(result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
#endif
    return result;
}

template<class Body, class Allocator, class Send>
void handle_request(beast::string_view doc_root,
                    http::request<Body, http::basic_fields<Allocator>>&& req,
                    Send&& send) {
    auto const bad_request =
            [&req](beast::string_view why) {
                http::response<http::string_body> res{http::status::bad_request, req.version()};
                res.set(header::server, BOOST_BEAST_VERSION_STRING);
                res.set(header::content_type, "text/html");
                res.keep_alive(req.keep_alive());
                res.body() = std::string(why);
                res.prepare_payload();
                return res;
            };
    auto const not_found =
            [&req](beast::string_view target) {
                http::response<http::string_body> res{http::status::not_found, req.version()};
                res.set(header::server, BOOST_BEAST_VERSION_STRING);
                res.set(header::content_type, "text/html");
                res.keep_alive(req.keep_alive());
                res.body() = "The resouce '" + std::string(target) + "' was not found.";
                res.prepare_payload();
                return res;
            };
    auto const server_error =
            [&req](beast::string_view what) {
                http::response<http::string_body> res{http::status::internal_server_error, req.version()};
                res.set(header::server, BOOST_BEAST_VERSION_STRING);
                res.set(header::content_type, "text/html");
                res.keep_alive(req.keep_alive());
                res.body() = "An error occurred: '" + std::string(what) + "'";
                res.prepare_payload();
                return res;
            };


    if(req.method() == http::verb::get ||
        req.method() == http::verb::head ||
        req.method() == http::verb::post ||
        req.method() == http::verb::put) {
        std::cout << "Method is : " << req.method() << std::endl;
    } else {
        return send(bad_request("Not Allowed HTTP-method. Only get, head, post, put"));
    }
    //전체 헤더 값 출력
    for(const auto& header : req.base()) {
        std::cout << header.name_string() << ": " << header.value() << "\n";
    }
    std::cout << "\n";
    //특정 헤더 값 출력
    auto header_ = req.base().find(http::field::connection);
    if(header_ != req.base().end()) {
        std::cout <<"connection: " << header_->value() << "\n";
    }

    std::cout << "\n";

    //특정 헤더 값 제거
    req.base().erase(http::field::connection);

    for(const auto& header : req.base()) {
        std::cout << header.name_string() << ": " << header.value() << "\n";
    }


    //path값
    if(req.target().empty() || // 값은 없어도 될듯
        req.target()[0] != '/' ||
        req.target().find("..") != beast::string_view::npos) {
        return send(bad_request("Illegal request-target"));
    } else {
        std::cout << "target is : " << req.target() << "\n";
    }

    std::string path = path_cat(doc_root, req.target());
    if(req.target().back() == '/')
        path.append("index.html");

    beast::error_code ec;
    http::file_body::value_type body;
    body.open(path.c_str(), beast::file_mode::scan, ec);

    if(ec == beast::errc::no_such_file_or_directory)
        return send(not_found(req.target()));

    if(ec)
        return send(server_error(ec.message()));

    auto const size = body.size();

    //HEAD 요청일 경우
    if(req.method() == http::verb::head) {
        http::response<http::empty_body> res{http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, mime_type(path));
        res.content_length(size);
        res.keep_alive(req.keep_alive());
        return send(std::move(res));
    }

    http::response<http::file_body> res {
        std::piecewise_construct,
        std::make_tuple(std::move(body)),
        std::make_tuple(http::status::ok, req.version())};

    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, mime_type(path));
    res.content_length(size);
    res.keep_alive(req.keep_alive());
    return send(std::move(res));
}

void fail(beast::error_code ec, char const* what) {
    std::cerr << what << ": " << ec.message() << std::endl;
}

template<class Stream>
struct send_lambda {
    Stream& stream_;
    bool& close_;
    beast::error_code& ec_;

    explicit send_lambda(
            Stream& stream,
            bool& close,
            beast::error_code& ec)
            : stream_(stream),
            close_(close),
            ec_(ec)
    {}

    template<bool isRequest, class Body, class Fields>
    void operator()(http::message<isRequest, Body, Fields>&& msg) const {
        //다음에 연결을 종료할지 결정합니다.
        close_ = msg.need_eof();


        //http:write가 const에서만 동작하기 때문에 직렬화 해주어야 합니다.
        http::serializer<isRequest, Body, Fields> sr{msg};
        http::write(stream_, sr, ec_);
    }
};

//HTTP Server Connection Handling
void do_session(tcp::socket& socket,
                std::shared_ptr<std::string const> const& doc_root) {
    bool close = false;
    beast::error_code ec;

    //이 버퍼는 읽기를 지속하는 데 필요합니다.
    beast::flat_buffer buffer;

    send_lambda<tcp::socket> lambda{socket, close, ec};

    for(;;) {
        http::request<http::string_body> req;
        http::read(socket, buffer, req, ec);
        if(ec == http::error::end_of_stream)
            break;
        if(ec)
            return fail(ec, "read");

        handle_request(*doc_root, std::move(req), lambda);
        if(ec)
            return fail(ec, "write");
        if(close) {
            //응답에 Connection: close가 있기 때문에 연결을 끊어야한다.
            break;
        }
    }
    //Send a TCP Shutdown
    socket.shutdown(tcp::socket::shutdown_send, ec);
}
#define __DEBUG__
int main(int argc, char* argv[]) {
    try {

#ifndef __DEBUG__
        std::cout << "Simple Http Running" << std::endl;
        if(argc != 4) {
            std::cerr <<
                "Usage : http-server-sync <address> <port> <doc_root>\n" <<
                "Example: \n" <<
                " http-server-sync 0.0.0.0 8080 /root\n";
                return EXIT_FAILURE;
        }

        auto const address = net::ip::make_address(argv[1]);
        auto const port = static_cast<unsigned short>(std::atoi(argv[2]));
        auto const doc_root = std::make_shared<std::string>(argv[3]);
#else
        auto const address = net::ip::make_address("0.0.0.0");
        auto const port = static_cast<unsigned short>(4040);
        auto const doc_root = std::make_shared<std::string>(".");
#endif
        net::io_context ioc{1};

        tcp::acceptor acceptor{ioc, {address, port}};
        for(;;) {
            tcp::socket socket{ioc};
            acceptor.accept(socket);
            std::thread{std::bind(&do_session, std::move(socket), doc_root)}.detach();
        }
    } catch(const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
