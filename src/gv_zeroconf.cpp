#include <time.h>
#include <sys/select.h>
#include <assert.h>

#include <future>
#include <functional>

#include "gv_zeroconf.hpp"
#include "gv_util.hpp"

namespace grapevine {

void
MDNSHandler::browseCallback(
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
    IN DNSServiceRef service,
    IN DNSServiceFlags flags,
    IN uint32_t interfaceIndex,
    IN DNSServiceErrorType errorCode,
    IN const char *name,
    IN const char *type,
    IN const char *domain,
    IN void *context
#pragma clang diagnostic pop
    )
{
    MDNSHandler *self = reinterpret_cast<MDNSHandler *>(context);
    GV_DEBUG_PRINT("Browse callback initiated");
    if (nullptr == self)
    {
        GV_DEBUG_PRINT("No context given.");
        return;
    }
    //else if (nullptr == self->_mCallback)
    //{
    //    GV_DEBUG_PRINT("No callback set.");
    //    return;
    //}
}

GV_ERROR
MDNSHandler::setBrowseCallback(
    IN gv_browse_callback callback
    )
{
    _mBrowseCallback = callback;
    return GV_ERROR_SUCCESS;
}

GV_ERROR
MDNSHandler::enableBrowse()
{
    DNSServiceErrorType error;
    GV_DEBUG_PRINT("About to call zeroconf browse");
    error = DNSServiceBrowse(
            &_mServiceRef,   // sdRef,
            0,              // flags,
            0,              // interfaceIndex,
            "_grapevine._tcp", // regtype,
            // FIXME cevans87: Implement more than link-local
            "local",       // domain,
            _mBrowseCallback,
            reinterpret_cast<void *>(this)            // context
            );
    GV_DEBUG_PRINT("DNSServiceBrowse returned with error: %d", error);
    if (!error)
    {
        printf("about to call async\n");
        _mFutureHandleEvents =
                std::async(std::launch::async, handleEvents, _mServiceRef);
        printf("Called async\n");
        DNSServiceRefDeallocate(_mServiceRef);
        _mServiceRef = reinterpret_cast<DNSServiceRef>(NULL);
    }
    return GV_ERROR_SUCCESS;
}

void
MDNSHandler::handleEvents(
    IN DNSServiceRef serviceRef
    )
{
    int err = 0;
    struct timeval tv;
    fd_set readfds;
    int dns_sd_fd = DNSServiceRefSockFD(serviceRef);
    int nfds = dns_sd_fd + 1; // FIXME, this info should come from somewhere else.
    while (!err)
    {
        FD_ZERO(&readfds);
        FD_SET(dns_sd_fd, &readfds);
        tv.tv_sec = 20;
        tv.tv_usec = 0;
        GV_DEBUG_PRINT("handleEvents loop");
        int result = select(nfds, &readfds, reinterpret_cast<fd_set *>(NULL),
                reinterpret_cast<fd_set *>(NULL), &tv);
        if (result > 0 && FD_ISSET(dns_sd_fd, &readfds))
        {
            // FIXME future destructor at end of loop is blocking. Create
            // thread without blocking.
            std::future<int> handle = std::async(std::launch::async,
                    DNSServiceProcessResult, serviceRef);
            err = DNSServiceProcessResult(serviceRef);
        }
    }
}

} // namespace grapevine
