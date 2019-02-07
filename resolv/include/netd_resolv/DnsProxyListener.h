/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _DNSPROXYLISTENER_H__
#define _DNSPROXYLISTENER_H__

#include <sysutils/FrameworkCommand.h>
#include <sysutils/FrameworkListener.h>

#include "resolv.h"  // android_net_context

namespace android {
namespace net {

class DnsProxyListener : public FrameworkListener {
  public:
    DnsProxyListener();
    virtual ~DnsProxyListener() {}

    bool setCallbacks(const dnsproxylistener_callbacks& callbacks);

    static constexpr const char* SOCKET_NAME = "dnsproxyd";

    // TODO: Considering putting this callbacks structure in its own file.
    dnsproxylistener_callbacks mCallbacks{};

  private:
    class GetAddrInfoCmd : public FrameworkCommand {
      public:
        GetAddrInfoCmd();
        virtual ~GetAddrInfoCmd() {}
        int runCommand(SocketClient* c, int argc, char** argv) override;
    };

    /* ------ getaddrinfo ------*/
    class GetAddrInfoHandler {
      public:
        // Note: All of host, service, and hints may be NULL
        GetAddrInfoHandler(SocketClient* c, char* host, char* service, addrinfo* hints,
                           const android_net_context& netcontext);
        ~GetAddrInfoHandler();

        void run();

      private:
        void doDns64Synthesis(int32_t* rv, addrinfo** res);

        SocketClient* mClient;  // ref counted
        char* mHost;            // owned. TODO: convert to std::string.
        char* mService;         // owned. TODO: convert to std::string.
        addrinfo* mHints;       // owned
        android_net_context mNetContext;
    };

    /* ------ gethostbyname ------*/
    class GetHostByNameCmd : public FrameworkCommand {
      public:
        GetHostByNameCmd();
        virtual ~GetHostByNameCmd() {}
        int runCommand(SocketClient* c, int argc, char** argv) override;
    };

    class GetHostByNameHandler {
      public:
        GetHostByNameHandler(SocketClient* c, char* name, int af,
                             const android_net_context& netcontext);
        ~GetHostByNameHandler();

        void run();

      private:
        void doDns64Synthesis(int32_t* rv, hostent** hpp);

        SocketClient* mClient; //ref counted
        char* mName;           // owned. TODO: convert to std::string.
        int mAf;
        android_net_context mNetContext;
    };

    /* ------ gethostbyaddr ------*/
    class GetHostByAddrCmd : public FrameworkCommand {
      public:
        GetHostByAddrCmd();
        virtual ~GetHostByAddrCmd() {}
        int runCommand(SocketClient* c, int argc, char** argv) override;
    };

    class GetHostByAddrHandler {
      public:
        GetHostByAddrHandler(SocketClient* c, void* address, int addressLen, int addressFamily,
                             const android_net_context& netcontext);
        ~GetHostByAddrHandler();

        void run();

      private:
        void doDns64ReverseLookup(hostent** hpp);

        SocketClient* mClient;  // ref counted
        void* mAddress;    // address to lookup; owned
        int mAddressLen; // length of address to look up
        int mAddressFamily;  // address family
        android_net_context mNetContext;
    };

    /* ------ resnsend ------*/
    class ResNSendCommand : public FrameworkCommand {
      public:
        ResNSendCommand();
        ~ResNSendCommand() override {}
        int runCommand(SocketClient* c, int argc, char** argv) override;
    };

    class ResNSendHandler {
      public:
        ResNSendHandler(SocketClient* c, std::string msg, uint32_t flags,
                        const android_net_context& netcontext);
        ~ResNSendHandler();

        void run();

      private:
        SocketClient* mClient;  // ref counted
        std::string mMsg;
        uint32_t mFlags;
        android_net_context mNetContext;
    };
};

}  // namespace net
}  // namespace android

#endif
