#include <time.h>
#include <sys/select.h>

#include <future>

#include "gv_browser.hpp"
#include "gv_util.hpp"

GV_ERROR
GV_MDNSHander setBrowseCallback(
    IN std::function<void()> callback
    )
{
    self._browseCallback = callback;
    return GV_ERROR_SUCCESS;
}


void
GV_Browser::DNSServiceBrowseCallback(
    IN DNSServiceRef service,
    IN DNSServiceFlags flags,
    IN uint32_t interfaceIndex,
    IN DNSServiceErrorType errorCode,
    IN const char *name,
    IN const char *type,
    IN const char *domain,
    IN void *context
    )
{
    if (NULL == context)
    {
        GV_DEBUG_PRINT("No context given.");
        return;
    }
    else if (nullptr == reinterpret_cast<GV_Browser *>(context)->_mCallback)
    {
        GV_DEBUG_PRINT("No callback set.");
        return;
    }

    reinterpret_cast<GV_Browser *>(context)->_mCallback(
            service,
            flags,
            interfaceIndex,
            errorCode,
            name,
            type,
            domain,
            reinterpret_cast<GV_Browser *>(context)->_mContext);
}

void
GV_Browser::DNSServiceBrowseCallback(
    IN DNSServiceRef service,
    IN DNSServiceFlags flags,
    IN uint32_t interfaceIndex,
    IN DNSServiceErrorType errorCode,
    IN const char *name,
    IN const char *type,
    IN const char *domain
    )
{
    if (errorCode != kDNSServiceErr_NoError)
    {
        GV_DEBUG_PRINT("DNSServiceErrorType error (%d) on browse", errorCode);
        return;
    }

    GV_DEBUG_PRINT("%-7s iface:%d %s.%s%s%s",
            (flags & kDNSServiceFlagsAdd) ? "ADD" : "REMOVE",
            interfaceIndex,
            name,
            type,
            domain,
            (flags & kDNSServiceFlagsMoreComing) ? "MORE" : "");
}

DNSServiceErrorType
GV_Browser::browse()
{
    DNSServiceErrorType error;
    GV_DEBUG_PRINT("About to call zeroconf browse");
    error = DNSServiceBrowse(
            &_serviceRef,   // sdRef,
            0,              // flags,
            0,              // interfaceIndex,
            "_grapevine._tcp", // regtype,
            // FIXME cevans87: Implement more than link-local
            "local",       // domain,
            DNSServiceBrowseCallback,
            reinterpret_cast<void *>(this)            // context
            );
    GV_DEBUG_PRINT("DNSServiceBrowse returned with error: %d", error);
    if (!error)
    {
        handleEvents(_serviceRef);
        DNSServiceRefDeallocate(_serviceRef);
        _serviceRef = reinterpret_cast<DNSServiceRef>(NULL);
    }
    return error;
}

void
GV_Browser::handleEvents(
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
            auto handle = std::async(std::launch::async,
                    DNSServiceProcessResult, serviceRef);
            //err = DNSServiceProcessResult(serviceRef);
        }
    }
}
