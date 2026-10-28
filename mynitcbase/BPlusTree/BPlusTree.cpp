#include "BPlusTree.h"

#include <cstring>


RecId BPlusTree::bPlusSearch(int relId, char attrName[], Attribute attrVal, int op) {
    // declare searchIndex variable to store the search index
    IndexId searchIndex;

    int ret = AttrCacheTable::getSearchIndex(relId, attrName, &searchIndex);
    if(ret != SUCCESS) {
        return RecId{-1,-1};
    }

    // get searchIndex of attrName for rootblock
    AttrCatEntry attrCatEntry;
    ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if(ret != SUCCESS) {
        return RecId{-1,-1};
    }

    int block=-1, index=-1;

    if(searchIndex.block == -1 && searchIndex.index == -1) {
        // search is done for the first time
        block = attrCatEntry.rootBlock;
        index = 0;
        
        // if the attribute is not indexed
        if(block == -1) {
            return RecId{-1,-1};
        }
    } else {
        /*a valid searchIndex points to an entry in the leaf index of the attribute's
        B+ Tree which had previously satisfied the op for the given attrVal.*/

        block = searchIndex.block;
        index = searchIndex.index + 1; // resume from NEXT index

        // load that block into leaf
        IndLeaf leaf(block);

        // declare leafHead for the header of this leaf block
        HeadInfo leafHead;

        leaf.getHeader(&leafHead);

        if(index >= leafHead.numEntries) {
            // this block is completely searched, go to next block
            block = leafHead.rblock;
            index = 0;

            //if there is no block left
            if(block == -1) {
                return RecId{-1,-1};
            }
        }
    }

    /******  Traverse through all the internal nodes according to value
             of attrVal and the operator op                             ******/

    /* (This section is only needed when
        - search restarts from the root block (when searchIndex is reset by caller)
        - root is not a leaf
        If there was a valid search index, then we are already at a leaf block
        and the test condition in the following loop will fail)
    */

    while(StaticBuffer::getStaticBlockType(block)==IND_INTERNAL) {
        
        IndInternal internalBlk(block);

        HeadInfo intHead;
        internalBlk.getHeader(&intHead);

        //declare intEntry which will be used to store an entry of internalBlk.
        InternalEntry intEntry;

        if(op == NE || op == LT || op == LE) {
             /*
            - NE: need to search the entire linked list of leaf indices of the B+ Tree,
            starting from the leftmost leaf index. Thus, always move to the left.

            - LT and LE: the attribute values are arranged in ascending order in the
            leaf indices of the B+ Tree. Values that satisfy these conditions, if
            any exist, will always be found in the left-most leaf index. Thus,
            always move to the left.
            */

            // load entry in the first slot of the block into intEntry
            // using IndInternal::getEntry().

            internalBlk.getEntry(&intEntry, 0);
            block = intEntry.lChild;
        } else {

             /*
            - EQ, GT and GE: move to the left child of the first entry that is
            greater than (or equal to) attrVal
            (we are trying to find the first entry that satisfies the condition.
            since the values are in ascending order we move to the left child which
            might contain more entries that satisfy the condition)
            */

            /*
             traverse through all entries of internalBlk and find an entry that
             satisfies the condition.
             if op == EQ or GE, then intEntry.attrVal >= attrVal
             if op == GT, then intEntry.attrVal > attrVal
             Hint: the helper function compareAttrs() can be used for comparing
            */

            int i = 0;
            bool entryFound = false;
            while(i < intHead.numEntries) {

                int ret = internalBlk.getEntry(&intEntry, i);
                int cmpVal = compareAttrs(intEntry.attrVal, attrVal, attrCatEntry.attrType);
                 if (
                    (op == EQ && cmpVal >= 0) ||
                    (op == GE && cmpVal >= 0) ||
                    (op == GT && cmpVal > 0)
                )
                {
                    entryFound = true;
                    break;
                }

                    i++;
            }
                if(entryFound) {
                    // move to the left side to find the first entry that satisfies this condition
                    block = intEntry.lChild;
                } else {
                    // nothing satisfies the condition. move out of the block
                    internalBlk.getEntry(&intEntry, intHead.numEntries - 1);
                    block = intEntry.rChild;
                }
        }
    }

    // NOTE: `block` now has the block number of a leaf index block.

    // on a fresh search, internal-node descent leaves `index` at a position
    // from the internal block's entry array; reset it before scanning the leaf.
    // on a resume search, index = searchIndex.index + 1 is already correct.


     /******  Identify the first leaf index entry from the current position
                that satisfies our condition (moving right)             ******/

     while (block != -1) {
        // load the block into leafBlk using IndLeaf::IndLeaf().
        IndLeaf leafBlk(block);

        // load the header to leafHead using BlockBuffer::getHeader().
        HeadInfo leafHead;
        leafBlk.getHeader(&leafHead);

        // declare leafEntry which will be used to store an entry from leafBlk
        Index leafEntry;

        // while (/*index < numEntries in leafBlk*/) 
        while (index < leafHead.numEntries)
        {
            // load entry corresponding to block and index into leafEntry
            // using IndLeaf::getEntry().
            leafBlk.getEntry(&leafEntry, index);

            // comparison between leafEntry's attribute value and input attrVal using compareAttrs()
            int cmpVal = compareAttrs(leafEntry.attrVal, attrVal, attrCatEntry.attrType); 

            if (
                (op == EQ && cmpVal == 0) ||
                (op == LE && cmpVal <= 0) ||
                (op == GE && cmpVal >= 0) ||
                (op == LT && cmpVal < 0) ||
                (op == GT && cmpVal > 0) ||
                (op == NE && cmpVal != 0)
            ) {
                // (entry satisfying the condition found)

                // set search index to {block, index}
                searchIndex = IndexId{block, index};
                AttrCacheTable::setSearchIndex(relId, attrName, &searchIndex);

                // return the recId {leafEntry.block, leafEntry.slot}.
                return RecId{leafEntry.block, leafEntry.slot};
            } 
            else if ((op == EQ || op == LE || op == LT) && cmpVal > 0) 
            {
                // future entries will not satisfy EQ, LE, LT since the values
                // are arranged in ascending order in the leaves 

                return RecId {-1, -1};
            }

            // search next index.
            ++index;
        }

        // only for NE operation do we have to check the entire linked list;
        // for all the other op it is guaranteed that the block being searched
        // will have an entry, if it exists, satisying that op. 
        if (op != NE) {
            break;
        }

        // block = next block in the linked list, i.e., the rblock in leafHead.
        // update index to 0.
        block = leafHead.rblock;
        index = 0;
    }

    // no entry satisying the op was found; return the recId {-1,-1}
    return RecId{-1, -1};
}