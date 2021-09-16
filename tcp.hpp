#pragma once

#include "debug.hpp"
#include "net.hpp"

namespace tcp {
  static debugging::Debug* debug = new debugging::Debug("tcp", debugging::colors::GREEN);

  void enableDebug() {
    debug->enable();
  }

  void disableDebug() {
    debug->disable();
  }

  template <typename ...A>
    void log(pstd::vstring str, A ...args) {
      debug->log(str, args ...);
    }

  // ----

  struct exception : public pstd::exception {
    public:
      template <typename ...A>
        exception(pstd::vstring str, A ...args) : pstd::exception(str, args ...) {

        }
  };

  enum class event {
    ERROR = EOF,
    DISCONNECTED
  };

  enum class status {
    FAIL = EOF,
    OK
  };

  class Socket {
    friend class Server;

    protected:
      addrinfo* ai;
      uint id;

      // used by Server
      Socket(std::pair<uint, addrinfo*> pair) : id(pair.first), ai(pair.second) {
        log("socket created by server");
      }

    public:
      static void *operator new      (size_t) = delete;
      static void *operator new[]    (size_t) = delete;
      static void  operator delete   (void*)  = delete;
      static void  operator delete[] (void*)  = delete;

      ~Socket() {
        log("destroying socket");

        std::free(ai);
      }

      Socket() {
        log("creating socket");

        ai = new addrinfo();

        net::set_type(ai, net::type::STREAM);
        net::set_proto(ai, net::proto::TCP);
      }

      std::variant<event, std::string> recv() {
        log("recieving data");

        auto out = std::variant<event, std::string>();

        auto vari = net::recv(id);

        if (pstd::has<std::string>(vari))
          out = std::get<std::string>(vari);

        else {
          auto evnt = std::get<net::event>(vari);

          if (evnt == net::event::DISCONNECTED)
            out = event::DISCONNECTED;

          if (evnt == net::event::ERROR)
            out = event::ERROR;
        }

        return out;
      }

      void close() {
        log("closing connection");

        net::close(id);
      }

      status connect(uint16_t port, pstd::vstring host, net::family family = net::family::Unspecified) {
        log("attempting connection to %s (%i)", &host[0], port);

        auto list = net::lookup(host, family);

        auto it = pstd::until(list, [&](auto next) -> bool {
          log("attempting address %s", next.data());

          net::set_addr(ai, next);
          net::set_port(ai, port);

          auto fd = net::open(ai);

          if (!fd)
            return !1;

          log("socket created");

          auto i = net::connect(*fd, ai);

          if (i == net::status::FAIL) {
            net::close(*fd);
            return !1;
          }

          log("socket connected to %s", next.data());

          id = *fd;

          log("socket id set to %i", id);

          return !0;
        });

        return it ?
          status::OK : status::FAIL;
      }
  };

  class Server {
    protected:
      addrinfo* ai;
      uint id;

    public:
      static void *operator new      (size_t) = delete;
      static void *operator new[]    (size_t) = delete;
      static void  operator delete   (void*)  = delete;
      static void  operator delete[] (void*)  = delete;

      ~Server() {
        log("destroying addrinfo");
        std::free(ai);
      }

      Server() {
        log("creating server");

        ai = new addrinfo();

        net::set_type(ai, net::type::STREAM);
        net::set_proto(ai, net::proto::TCP);
      }

      status bind(uint16_t port, pstd::vstring host, net::family family = net::family::Unspecified) {
        auto list = net::lookup(host, family);

        auto it = pstd::until(list, [&](auto next) -> bool {
          net::set_addr(ai, next);
          net::set_port(ai, port);

          auto fd = net::open(ai);

          if (!fd)
            return !1;

          auto i = net::bind(*fd, ai);

          if (i == net::status::FAIL) {
            net::close(*fd);
            return !1;
          }

          net::option(*fd, SO_REUSEADDR);
          net::option(*fd, SO_REUSEPORT);

          id = *fd;

          return !0;
        });

        return it ?
          status::OK : status::FAIL;
      }

      auto accept() {
        auto i = net::listen(id);

        if (i == net::status::FAIL)
          throw exception("unable to listen");

        auto pair = net::accept(id, ai);

        return Socket(pair);
      }
  };
}
