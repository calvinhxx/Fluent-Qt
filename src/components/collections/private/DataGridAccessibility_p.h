#ifndef FLUENTQT_COMPONENTS_COLLECTIONS_PRIVATE_DATAGRIDACCESSIBILITY_P_H
#define FLUENTQT_COMPONENTS_COLLECTIONS_PRIVATE_DATAGRIDACCESSIBILITY_P_H

namespace fluent::collections::detail {

// Installs the private logical table adapter used by DataGrid. The adapter is
// intentionally absent from installed headers.
void ensureDataGridAccessibilityFactory();

} // namespace fluent::collections::detail

#endif // FLUENTQT_COMPONENTS_COLLECTIONS_PRIVATE_DATAGRIDACCESSIBILITY_P_H
