#include "experimental_mode.h"

#if EXPERIMENTAL_MODE_TICTOC_DISK
#   include "transaction_impl/transaction_tictoc_disk.h"
#else
#   if EXPERIMENTAL_MODE_ATOMIC_WORD
#      include "transaction_impl/transaction_fantasticc_nolock.h"
#   else
#      include "transaction_impl/transaction_fantasticc.h"
#   endif
#endif