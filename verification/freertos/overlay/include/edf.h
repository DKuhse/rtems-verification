#ifndef VERIFY_EDF_H
#define VERIFY_EDF_H

#ifndef INC_FREERTOS_H
    #error "FreeRTOS.h must be included before edf.h"
#endif

#include "list.h"

/*@
  // Invariant for the ready list
  predicate xItemValue_matches_deadline(struct xLIST *pxList) =
    \forall struct xLIST_ITEM *pxItem;
      in_list(pxItem, pxList) ==>
      pxItem->xItemValue ==
      ((struct tskTaskControlBlock *)pxItem->pvOwner)->xDeadline;

  predicate edf_property(struct xLIST *pxList,
                         struct tskTaskControlBlock *pxRunning) =
    \exists struct xLIST_ITEM *pxItem;
      in_list(pxItem, pxList) &&
      pxItem->pvOwner == pxRunning &&
      (\forall struct xLIST_ITEM *other;
        in_list(other, pxList) ==>
        pxRunning->xDeadline
        <= ((struct tskTaskControlBlock *)other->pvOwner)->xDeadline);
*/

#endif /* VERIFY_EDF_H */
