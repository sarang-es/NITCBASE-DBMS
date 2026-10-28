#include "BPlusTree.h"
#include <cstring>

RecId BPlusTree::bPlusSearch(int relId, char attrName[ATTR_SIZE],
                             Attribute attrVal, int op) {

    
    
    // Store the search index for attrName.
    IndexId searchIndex;

    // Get the current search index.
    int ret = AttrCacheTable::getSearchIndex(relId, attrName, &searchIndex);
    if (ret != SUCCESS) {
        return RecId{-1, -1};
    }

    // Load the attribute catalog entry.
    AttrCatEntry attrCatEntry;
    ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if (ret != SUCCESS) {
        return RecId{-1, -1};
    }

    // Variables used during the search.
    int block, index;

    if (searchIndex.block == -1 && searchIndex.index == -1) {
        // Search is being performed for the first time.
        block = attrCatEntry.rootBlock;
        index = 0;

        // No B+ tree exists for this attribute.
        if (block == -1) {
            return RecId{-1, -1};
        }
    } else {
        // Resume searching from the next entry.
        block = searchIndex.block;
        index = searchIndex.index + 1;

        // Load the current leaf block.
        IndLeaf leaf(block);
        HeadInfo leafHead;

        ret = leaf.getHeader(&leafHead);
        if (ret != SUCCESS) {
            return RecId{-1, -1};
        }

        // If the current leaf is exhausted, move to the next leaf.
        if (index >= leafHead.numEntries) {
            block = leafHead.rblock;
            index = 0;

            // End of the linked list.
            if (block == -1) {
                return RecId{-1, -1};
            }
        }
    }

    /************************************************************
     * Traverse the internal nodes to reach the appropriate leaf.
     ************************************************************/

    while (StaticBuffer::getStaticBlockType(block) == IND_INTERNAL) {
        // Load the internal block.
        IndInternal internalBlk(block);
        HeadInfo intHead;

        ret = internalBlk.getHeader(&intHead);
        if (ret != SUCCESS) {
            return RecId{-1, -1};
        }

        InternalEntry intEntry;

        if (op == NE || op == LT || op == LE) {
            /*
             * NE, LT and LE start from the leftmost leaf.
             * Therefore, follow the leftmost child.
             */

            ret = internalBlk.getEntry(&intEntry, 0);
            if (ret != SUCCESS) {
                return RecId{-1, -1};
            }

            block = intEntry.lChild;
        } else {
            /*
             * For EQ and GE, find the first entry whose key
             * is greater than or equal to attrVal.
             *
             * For GT, find the first entry whose key is
             * strictly greater than attrVal.
             */

            bool found = false;

            for (int i = 0; i < intHead.numEntries; i++) {
                ret = internalBlk.getEntry(&intEntry, i);
                if (ret != SUCCESS) {
                    return RecId{-1, -1};
                }
                
                int cmpVal = compareAttrs(
                    intEntry.attrVal,
                    attrVal,
                    attrCatEntry.attrType
                );
                

                if (((op == EQ || op == GE) && cmpVal >= 0) ||
                    (op == GT && cmpVal > 0)) {

                    // Follow the left child of the matching entry.
                    block = intEntry.lChild;
                    found = true;
                    break;
                }
            }

            if (!found) {
                /*
                 * No suitable separator was found.
                 * Follow the right child of the last entry.
                 */

                if (intHead.numEntries <= 0) {
                    return RecId{-1, -1};
                }

                ret = internalBlk.getEntry(
                    &intEntry,
                    intHead.numEntries - 1
                );

                if (ret != SUCCESS) {
                    return RecId{-1, -1};
                }

                block = intEntry.rChild;
            }
        }
    }

    /************************************************************
     * Search the leaf entries for a matching record.
     ************************************************************/

    while (block != -1) {
        // Load the current leaf block.
        IndLeaf leafBlk(block);
        HeadInfo leafHead;

        ret = leafBlk.getHeader(&leafHead);
        if (ret != SUCCESS) {
            return RecId{-1, -1};
        }

        // Read each entry from the current leaf.
        Index leafEntry;

        while (index < leafHead.numEntries) {
            ret = leafBlk.getEntry(&leafEntry, index);
            if (ret != SUCCESS) {
                return RecId{-1, -1};
            }

            // Compare the indexed attribute with the search value.
            int cmpVal = compareAttrs(
                leafEntry.attrVal,
                attrVal,
                attrCatEntry.attrType
            );
            

            // Check whether this entry satisfies the operator.
            if ((op == EQ && cmpVal == 0) ||
                (op == LE && cmpVal <= 0) ||
                (op == LT && cmpVal < 0) ||
                (op == GT && cmpVal > 0) ||
                (op == GE && cmpVal >= 0) ||
                (op == NE && cmpVal != 0)) {

                // Save the location of this matching index entry.
                searchIndex.block = block;
                searchIndex.index = index;

                ret = AttrCacheTable::setSearchIndex(
                    relId,
                    attrName,
                    &searchIndex
                );

                if (ret != SUCCESS) {
                    return RecId{-1, -1};
                }
            
                

                // Return the location of the actual table record.
                return RecId{leafEntry.block, leafEntry.slot};
            } else if ((op == EQ || op == LE || op == LT) &&
                       cmpVal > 0) {
                /*
                 * Leaf keys are sorted in ascending order.
                 * Once a key exceeds attrVal, no later key
                 * can satisfy EQ, LE or LT.
                 */
                return RecId{-1, -1};
            }

            // Examine the next entry.
            ++index;
        }


        // EQ, LT and LE can stop when no match exists in this leaf.
        // GT and GE may need to continue into the next leaf.
        if (op == EQ || op == LT || op == LE) {
            break;
        }

        block = leafHead.rblock;
        index = 0;

        // NE must examine the remaining leaf blocks.
        block = leafHead.rblock;
        index = 0;
    }

    // No matching record was found.
    return RecId{-1, -1};
}

