#include "BindingApi.h"
#include "compatibility/private/ClipboardCompat_p.h"

bool clipboardWriteSucceededForBinding()
{
    return ::compatibility::detail::clipboardWriteSucceeded();
}
