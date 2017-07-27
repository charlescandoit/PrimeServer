//
//  primserv.cpp
//  
//  HTTP server to check if a given integer is prime
//  Created by Charles on 7/26/17.
//
//

#include <stdexcept>
#include <iostream>
#include <string>
#include <cmath>
#include <memory>
#include <chrono>
#include <thread>
#include <cstdint>
#include <vector>
#include <evhttp.h>
int main(int argc, char** argv)
{
    char *SrvAddress = argv[1]; //"10.0.0.35";
    std::uint16_t const SrvPort = 8555;
    int const SrvThreadCount = 4;
    try
    {
        void (*OnRequest)(evhttp_request *, void *) = [] (evhttp_request *req, void *)
        {
            const char *uri = evhttp_request_get_uri(req);
            auto *OutBuf = evhttp_request_get_output_buffer(req);
            if (!OutBuf)
                return;
            
            std::cout << std::endl << "GET " << uri << std::endl;
            std::string sin = uri;
            bool isPrime = true;
            
            if(sin.at(0) == '/') {
                sin.erase (0,1);
            }
            // validate input
            if( !sin.empty() && std::find_if(sin.begin(),sin.end(), [](char c) { return !std::isdigit(c); }) == sin.end())
            {
                // input is indeed integer
                std::cout << "Calculating " << sin << "...";
                std::string::size_type sz;   // alias of size_t
                uintmax_t i_dec = std::stoi (sin, &sz);
                
                if(i_dec == 1) {
                    isPrime = false;
                }
                else {
                    uintmax_t sr = sqrt(i_dec);
                    for(uintmax_t i = 2; i <= sr; ++i)
                    {
                        if(i_dec % i == 0)
                        {
                            isPrime = false;
                            break;
                        }
                    }
                }
                std::cout << " is " << i_dec << " prime? " << isPrime << std::endl;

                evbuffer_add_printf(OutBuf, "<html><body><center><h1> is %ld prime? %d </h1></center></body></html>", i_dec, isPrime);
            }
            else {
                std::cout << "input not valid: " << sin << std::endl;
                evbuffer_add_printf(OutBuf, "<html><body><center><h1> invalid input. </h1></center></body></html>");
            }
            evhttp_send_reply(req, HTTP_OK, "", OutBuf);
        };
        std::exception_ptr InitExcept;
        bool volatile IsRun = true;
        evutil_socket_t Socket = -1;
        auto ThreadFunc = [&] ()
        {
            try
            {
                std::unique_ptr<event_base, decltype(&event_base_free)> EventBase(event_base_new(), &event_base_free);
                if (!EventBase)
                    throw std::runtime_error("Failed to create new base_event.");
                std::unique_ptr<evhttp, decltype(&evhttp_free)> EvHttp(evhttp_new(EventBase.get()), &evhttp_free);
                if (!EvHttp)
                    throw std::runtime_error("Failed to create new evhttp.");
                
                evhttp_set_gencb(EvHttp.get(), OnRequest, nullptr);
                if (Socket == -1)
                {
                    auto *BoundSock = evhttp_bind_socket_with_handle(EvHttp.get(), SrvAddress, SrvPort);
                    if (!BoundSock)
                        throw std::runtime_error("Failed to bind server socket.");
                    if ((Socket = evhttp_bound_socket_get_fd(BoundSock)) == -1)
                        throw std::runtime_error("Failed to get server socket for next instance.");
                }
                else
                {
                    if (evhttp_accept_socket(EvHttp.get(), Socket) == -1)
                        throw std::runtime_error("Failed to bind server socket for new instance.");
                }
                for ( ; IsRun ; )
                {
                    event_base_loop(EventBase.get(), EVLOOP_NONBLOCK);
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
            catch (...)
            {
                InitExcept = std::current_exception();
            }
        };
        auto ThreadDeleter = [&] (std::thread *t) { IsRun = false; t->join(); delete t; };
        typedef std::unique_ptr<std::thread, decltype(ThreadDeleter)> ThreadPtr;
        typedef std::vector<ThreadPtr> ThreadPool;
        ThreadPool Threads;
        for (int i = 0 ; i < SrvThreadCount ; ++i)
        {
            ThreadPtr Thread(new std::thread(ThreadFunc), ThreadDeleter);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            if (InitExcept != std::exception_ptr())
            {
                IsRun = false;
                std::rethrow_exception(InitExcept);
            }
            Threads.push_back(std::move(Thread));
        }
        std::cout << "Enter to quit prime server" << std::endl;
        std::cin.get();
        IsRun = false;
    }
    catch (std::exception const &e)
    {
        std::cerr << "Error in prime server: " << e.what() << std::endl;
    }
    return 0;
}
