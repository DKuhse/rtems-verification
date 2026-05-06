#ifndef VERIFY_EDF_H
#define VERIFY_EDF_H

#ifndef INC_FREERTOS_H
    #error "FreeRTOS.h must be included before edf.h"
#endif

#include "list.h"

/*@
  predicate edf_property(struct xLIST *pxList, void *pxRunning) =
    \exists struct xLIST_ITEM *pxItem;
      in_list(pxItem, pxList) &&
      pxItem->pvOwner == pxRunning &&
      min(pxList, pxItem->xItemValue);
*/

#endif /* VERIFY_EDF_H */
