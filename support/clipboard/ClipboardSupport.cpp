#include "ClipboardSupport.h"
#include "compatibility/private/ClipboardCompat_p.h"

bool fluent::support::clipboard::writeSucceeded()
{
    return ::compatibility::detail::clipboardWriteSucceeded();
}
