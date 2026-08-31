#include "BlockAccess.h"
#include <cstring>

RecId BlockAccess::linearSearch(
    int relId,
    char attrName[ATTR_SIZE],
    union Attribute attrVal,
    int op
) {

    RecId prevRecId;

    int status =RelCacheTable::getSearchIndex(relId,&prevRecId);

    if (status != SUCCESS) {
        return RecId{-1, -1};
    }

    int block, slot;

    if (prevRecId.block == -1 && prevRecId.slot == -1) {

        RelCatEntry relCatEntry;

        status =RelCacheTable::getRelCatEntry(relId,&relCatEntry);

        if (status != SUCCESS) {
            return RecId{-1, -1};
        }

        block = relCatEntry.firstBlk;
        slot = 0;
    }
    else {

        block = prevRecId.block;
        slot = prevRecId.slot + 1;
    }

    AttrCatEntry attrCatEntry;

    status =AttrCacheTable::getAttrCatEntry(relId,attrName,&attrCatEntry);

    if (status != SUCCESS) {
        return RecId{-1, -1};
    }

    while (block != -1) {

        RecBuffer recBlock(block);

        HeadInfo head;

        recBlock.getHeader(&head);

        unsigned char slotMap[head.numSlots];

        recBlock.getSlotMap(slotMap);

        if (slot >= head.numSlots) {

            block = head.rblock;
            slot = 0;

            continue;
        }

        if (slotMap[slot] == SLOT_UNOCCUPIED) {

            slot++;

            continue;
        }

        Attribute record[head.numAttrs];

        recBlock.getRecord(
            record,
            slot
        );

        Attribute recordAttr =
            record[attrCatEntry.offset];

        int cmpVal =compareAttrs(recordAttr,attrVal,attrCatEntry.attrType);

        bool cond = false;

        if (
            (op == NE && cmpVal != 0) ||
            (op == LT && cmpVal < 0) ||
            (op == LE && cmpVal <= 0) ||
            (op == EQ && cmpVal == 0) ||
            (op == GT && cmpVal > 0) ||
            (op == GE && cmpVal >= 0)
        ) {

            RecId foundRecId;

            foundRecId.block = block;
            foundRecId.slot = slot;

            RelCacheTable::setSearchIndex(relId,&foundRecId);

            return foundRecId;
        }

        slot++;
    }

    RelCacheTable::resetSearchIndex(relId);

    return RecId{-1, -1};
}
